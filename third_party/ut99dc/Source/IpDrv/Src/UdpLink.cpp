/*=============================================================================
	IpDrv.cpp: Unreal TCP/IP driver.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
	* Additions by Brandon Reinhart.
=============================================================================*/

#include "IpDrvPrivate.h"

/*-----------------------------------------------------------------------------
	AUdpLink implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(AUdpLink);

#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <android/log.h>

// UT99_ANDROID_V108_LAN_BEACON_DIRECTED_SCAN:
// v106 repeated the stock LAN broadcast, but OUYA/Android Wi-Fi stacks and some
// access points still drop 255.255.255.255.  Keep the original script behavior,
// but add a native LAN-only fallback:
//   * send to limited broadcast and directed interface broadcasts
//   * client LAN browser also probes the local /24 with REPORT packets
//   * server beacon announces on directed broadcasts, not only 255.255.255.255
// This does not touch the gameplay NetDriver or Internet/master-server logic.
static AUdpLink* GAndroidLanRepeatLinks[64];
static DOUBLE    GAndroidLanRepeatTimes[64];

static void AndroidLanLog( const TCHAR* Text )
{
	if( !Text )
		return;
	debugf( NAME_Log, TEXT("%s"), Text );
	__android_log_print( ANDROID_LOG_INFO, "UT99LAN", "%s", appToAnsi(Text) );
}

static void AndroidLanRepeatRemove( AUdpLink* Link )
{
	for( INT i=0; i<ARRAY_COUNT(GAndroidLanRepeatLinks); i++ )
	{
		if( GAndroidLanRepeatLinks[i] == Link )
		{
			GAndroidLanRepeatLinks[i] = NULL;
			GAndroidLanRepeatTimes[i] = 0.0;
		}
	}
}

static UBOOL AndroidLanRepeatDue( AUdpLink* Link, DOUBLE PeriodSeconds )
{
	DOUBLE Now = appSeconds();
	INT Empty = -1;
	for( INT i=0; i<ARRAY_COUNT(GAndroidLanRepeatLinks); i++ )
	{
		if( GAndroidLanRepeatLinks[i] == Link )
		{
			if( Now - GAndroidLanRepeatTimes[i] >= PeriodSeconds )
			{
				GAndroidLanRepeatTimes[i] = Now;
				return 1;
			}
			return 0;
		}
		if( Empty < 0 && GAndroidLanRepeatLinks[i] == NULL )
		{
			Empty = i;
		}
	}
	if( Empty >= 0 )
	{
		GAndroidLanRepeatLinks[Empty] = Link;
		GAndroidLanRepeatTimes[Empty] = Now;
		return 1;
	}
	return 0;
}

static UBOOL AndroidLanClassIs( AUdpLink* Link, const TCHAR* ClassName )
{
	return Link
		&& Link->GetClass()
		&& appStricmp( Link->GetClass()->GetName(), ClassName ) == 0;
}

static UBOOL AndroidLanClassContains( AUdpLink* Link, const TCHAR* Fragment )
{
	return Link
		&& Link->GetClass()
		&& Fragment
		&& appStrfind( Link->GetClass()->GetName(), Fragment ) != NULL;
}

static UBOOL AndroidLanClassLooksRelevant( AUdpLink* Link )
{
	return AndroidLanClassContains(Link,TEXT("Browser"))
		|| AndroidLanClassContains(Link,TEXT("Beacon"))
		|| AndroidLanClassContains(Link,TEXT("Local"));
}

static INT AndroidLanAddTarget( DWORD* Targets, INT Count, INT MaxTargets, DWORD Target )
{
	if( !Targets || MaxTargets <= 0 || Target == 0 )
		return Count;
	for( INT i=0; i<Count; i++ )
		if( Targets[i] == Target )
			return Count;
	if( Count < MaxTargets )
		Targets[Count++] = Target;
	return Count;
}

static INT AndroidLanBuildTargets( DWORD* Targets, INT MaxTargets, UBOOL IncludeHostSweep )
{
	INT Count = 0;
	Count = AndroidLanAddTarget( Targets, Count, MaxTargets, 0xffffffff );

	SOCKET ProbeSocket = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if( ProbeSocket == INVALID_SOCKET )
		return Count;

	char Buffer[4096];
	appMemzero( Buffer, sizeof(Buffer) );
	struct ifconf IfConf;
	appMemzero( &IfConf, sizeof(IfConf) );
	IfConf.ifc_len = sizeof(Buffer);
	IfConf.ifc_buf = Buffer;

	if( ioctl( ProbeSocket, SIOCGIFCONF, &IfConf ) == 0 )
	{
		char* End = Buffer + IfConf.ifc_len;
		for( struct ifreq* IfReq = (struct ifreq*)Buffer; (char*)IfReq + sizeof(struct ifreq) <= End; ++IfReq )
		{
			if( IfReq->ifr_addr.sa_family != AF_INET )
				continue;

			struct ifreq FlagsReq;
			appMemzero( &FlagsReq, sizeof(FlagsReq) );
			strncpy( FlagsReq.ifr_name, IfReq->ifr_name, IFNAMSIZ-1 );
			if( ioctl( ProbeSocket, SIOCGIFFLAGS, &FlagsReq ) != 0 )
				continue;
			if( !(FlagsReq.ifr_flags & IFF_UP) || (FlagsReq.ifr_flags & IFF_LOOPBACK) )
				continue;

			struct sockaddr_in* IpAddr = (struct sockaddr_in*)&IfReq->ifr_addr;
			DWORD IpHost = ntohl( IpAddr->sin_addr.s_addr );
			if( IpHost == 0 || (IpHost >> 24) == 127 )
				continue;

			DWORD MaskHost = 0xffffff00;
			struct ifreq MaskReq;
			appMemzero( &MaskReq, sizeof(MaskReq) );
			strncpy( MaskReq.ifr_name, IfReq->ifr_name, IFNAMSIZ-1 );
			if( ioctl( ProbeSocket, SIOCGIFNETMASK, &MaskReq ) == 0 )
			{
				struct sockaddr_in* MaskAddr = (struct sockaddr_in*)&MaskReq.ifr_addr;
				DWORD CandidateMask = ntohl( MaskAddr->sin_addr.s_addr );
				if( CandidateMask != 0 )
					MaskHost = CandidateMask;
			}

			DWORD DirectedBroadcast = IpHost | (~MaskHost);
			Count = AndroidLanAddTarget( Targets, Count, MaxTargets, DirectedBroadcast );

			// Some Android 4.x/OUYA networks report unusable masks or drop directed
			// broadcasts.  A bounded /24 probe is small enough for a LAN menu refresh
			// and reliably finds the other console when broadcast is filtered.
			DWORD ClassCBroadcast = (IpHost & 0xffffff00) | 0x000000ff;
			Count = AndroidLanAddTarget( Targets, Count, MaxTargets, ClassCBroadcast );
			if( IncludeHostSweep )
			{
				DWORD Base = IpHost & 0xffffff00;
				for( INT Host=1; Host<=254 && Count<MaxTargets; Host++ )
				{
					DWORD Target = Base | (DWORD)Host;
					if( Target != IpHost )
						Count = AndroidLanAddTarget( Targets, Count, MaxTargets, Target );
				}
			}
		}
	}

	closesocket( ProbeSocket );
	return Count;
}

static void AndroidLanSendText( SOCKET Socket, DWORD Addr, INT ToPort, const FString& Text )
{
	if( Socket == INVALID_SOCKET || Socket == 0 || ToPort <= 0 || Text.Len() <= 0 )
		return;

	sockaddr_in ToAddr;
	appMemzero( &ToAddr, sizeof(ToAddr) );
	ToAddr.sin_family      = AF_INET;
	ToAddr.sin_port        = htons(ToPort);
	ToAddr.sin_addr.s_addr = htonl(Addr);

	sendto( Socket, (char*)appToAnsi(*Text), sizeof(ANSICHAR)*Text.Len(), 0, (sockaddr*)&ToAddr, sizeof(ToAddr) );
}

static INT AndroidLanSendToTargets( SOCKET Socket, INT ToPort, const FString& Text, UBOOL IncludeHostSweep )
{
	DWORD Targets[384];
	INT Count = AndroidLanBuildTargets( Targets, ARRAY_COUNT(Targets), IncludeHostSweep );
	for( INT i=0; i<Count; i++ )
		AndroidLanSendText( Socket, Targets[i], ToPort, Text );
	return Count;
}

static INT AndroidLanSendToTargetsPortRange( SOCKET Socket, INT FirstPort, INT PortCount, const FString& Text, UBOOL IncludeHostSweep )
{
	if( PortCount <= 0 )
		return 0;

	DWORD Targets[384];
	INT TargetCount = AndroidLanBuildTargets( Targets, ARRAY_COUNT(Targets), IncludeHostSweep );
	INT SendCount = 0;
	for( INT p=0; p<PortCount; p++ )
	{
		INT ToPort = FirstPort + p;
		for( INT i=0; i<TargetCount; i++ )
		{
			AndroidLanSendText( Socket, Targets[i], ToPort, Text );
			SendCount++;
		}
	}
	return SendCount;
}

static UBOOL AndroidLanLooksLikeServerInfo( const FString& Text )
{
	// GameSpy/IpServer replies are backslash key-value packets.  If the LAN
	// browser receives one directly from our native scan, synthesize the normal
	// UBrowserLocalLink beacon reply so the stock UI can add only real servers.
	return Text.InStr( TEXT("\\hostname\\") ) >= 0
		|| Text.InStr( TEXT("\\hostport\\") ) >= 0
		|| Text.InStr( TEXT("\\gamename\\") ) >= 0
		|| Text.InStr( TEXT("\\mapname\\") ) >= 0;
}

static INT AndroidLanSendDirectInfoScan( SOCKET Socket )
{
	// v110: bypass fragile REPORTQUERY broadcasts.  Query the real IpServer
	// listener directly on the normal UT99 local query range.  UdpServerQuery
	// usually binds to the game port or the next free one, commonly 7778.
	return AndroidLanSendToTargetsPortRange( Socket, 7777, 11, TEXT("\\info\\"), 1 );
}

static void AndroidLanFanOutReportQuery( SOCKET Socket, INT ToPort, const FString& Text )
{
	if( appStricmp( *Text, TEXT("REPORTQUERY") ) != 0 || ToPort <= 0 )
		return;

	INT Sends = AndroidLanSendToTargets( Socket, ToPort, Text, 1 );
	AndroidLanLog( *FString::Printf( TEXT("UT99_ANDROID_V112_LAN SendText REPORTQUERY fanout sends=%i port=%i"), Sends, ToPort ) );
}


static void AndroidLanNativeBeaconReply( AUdpLink* Link, const FIpAddr& Addr, const FString& Text )
{
	if( !Link || !Link->GetSocket() || !AndroidLanClassContains(Link,TEXT("Beacon")) )
		return;
	if( appStricmp(*Text,TEXT("REPORTQUERY"))!=0 && appStricmp(*Text,TEXT("REPORT"))!=0 )
		return;

	INT ServerPort = 7777;
	INT QueryPort = 7778;
	FString BeaconText = TEXT("UT99 Android LAN 0/16");
	ULevel* Level = Link->GetLevel();
	if( Level )
	{
		if( Level->URL.Port > 0 )
			ServerPort = Level->URL.Port;
		ALevelInfo* Info = Level->GetLevelInfo();
		if( Info && Info->Game )
		{
			FString ScriptBeaconText = Info->Game->eventGetBeaconText();
			if( ScriptBeaconText.Len() > 0 )
				BeaconText = ScriptBeaconText;
		}
	}

	FString ReplyUt;
	FString ReplyUnreal;
	if( ServerPort > 0 )
		QueryPort = ServerPort + 1;
	if( appStricmp(*Text,TEXT("REPORTQUERY"))==0 )
	{
		ReplyUt = FString::Printf(TEXT("ut %i"), QueryPort);
		ReplyUnreal = FString::Printf(TEXT("unreal %i"), QueryPort);
	}
	else
	{
		ReplyUt = FString::Printf(TEXT("ut %i %s"), ServerPort, *BeaconText);
		ReplyUnreal = FString::Printf(TEXT("unreal %i %s"), ServerPort, *BeaconText);
	}
	AndroidLanSendText( Link->GetSocket(), Addr.Addr, Addr.Port, ReplyUt );
	AndroidLanSendText( Link->GetSocket(), Addr.Addr, Addr.Port, ReplyUnreal );
	AndroidLanLog( *FString::Printf(TEXT("UT99_ANDROID_V128_LAN native UdpBeacon reply class=%s text=%s to=%u.%u.%u.%u:%i port=%i"),
		Link->GetClass()->GetName(), *Text, (Addr.Addr>>24)&255, (Addr.Addr>>16)&255, (Addr.Addr>>8)&255, Addr.Addr&255, Addr.Port, QueryPort) );
}

static void AndroidLanRepeatTick( AUdpLink* Link )
{
	if( !Link || !Link->GetSocket() || !Link->GetLevel() )
		return;

	// UT99's visible LAN tab does not use ClientBeaconReceiver.  It creates an
	// UBrowserLocalLink, binds a random UDP port and sends REPORTQUERY to server
	// beacon ports 8777..8796.  v107 only repeated REPORT/9777 traffic, which
	// helped the old helper class but missed the real UT99 browser menu.
	if( AndroidLanClassIs( Link, TEXT("UBrowserLocalLink") ) || AndroidLanClassContains( Link, TEXT("LocalLink") ) )
	{
		if( AndroidLanRepeatDue( Link, 3.0 ) )
		{
			INT ReportSends = AndroidLanSendToTargetsPortRange( Link->GetSocket(), 8777, 20, TEXT("REPORTQUERY"), 1 );
			INT InfoSends   = AndroidLanSendDirectInfoScan( Link->GetSocket() );
			AndroidLanLog( *FString::Printf( TEXT("UT99_ANDROID_V112_LAN browser scan reportquery=%i info=%i ports=8777-8796/7777-7787"), ReportSends, InfoSends ) );
		}
		return;
	}

	// Keep the older ClientBeaconReceiver path alive too; it is used by some
	// stock/Unreal-family menus even though UT99's UBrowserLAN tab uses the
	// UBrowserLocalLink REPORTQUERY path above.
	if( AndroidLanClassIs( Link, TEXT("ClientBeaconReceiver") ) || AndroidLanClassContains( Link, TEXT("BeaconReceiver") ) )
	{
		if( AndroidLanRepeatDue( Link, 2.0 ) )
		{
			INT Targets = AndroidLanSendToTargets( Link->GetSocket(), 8777, TEXT("REPORT"), 1 );
			AndroidLanLog( *FString::Printf( TEXT("UT99_ANDROID_V112_LAN client REPORT probes targets=%i port=8777"), Targets ) );
		}
		return;
	}

	// Server LAN beacon: announce periodically on all useful LAN broadcasts, not
	// only 255.255.255.255.  Query replies are still handled by UdpBeacon script.
	if( (AndroidLanClassIs( Link, TEXT("UdpBeacon") ) || AndroidLanClassContains( Link, TEXT("UdpBeacon") )) && Link->Port >= 8777 && Link->Port <= 8797 )
	{
		if( AndroidLanRepeatDue( Link, 1.0 ) )
		{
			INT ServerPort = 7777;
			INT QueryPort = 7778;
			FString BeaconText = TEXT("UT99 Android LAN 0/16");

			ULevel* Level = Link->GetLevel();
			if( Level )
			{
				if( Level->URL.Port > 0 )
					ServerPort = Level->URL.Port;

				ALevelInfo* Info = Level->GetLevelInfo();
				if( Info && Info->Game )
				{
					FString ScriptBeaconText = Info->Game->eventGetBeaconText();
					if( ScriptBeaconText.Len() > 0 )
						BeaconText = ScriptBeaconText;
				}
			}

			if( ServerPort > 0 )
				QueryPort = ServerPort + 1;
			FString PacketUt = FString::Printf( TEXT("ut %i %s"), QueryPort, *BeaconText );
			FString PacketUnreal = FString::Printf( TEXT("unreal %i %s"), QueryPort, *BeaconText );
			INT TargetsUt = AndroidLanSendToTargets( Link->GetSocket(), 9777, PacketUt, 0 );
			INT TargetsUnreal = AndroidLanSendToTargets( Link->GetSocket(), 9777, PacketUnreal, 0 );
			AndroidLanLog( *FString::Printf( TEXT("UT99_ANDROID_V112_LAN server beacon targets=%i/%i port=9777 %s"), TargetsUt, TargetsUnreal, *PacketUt ) );
		}
		return;
	}
}
#endif

#define MAXRECVDATASIZE 4096

//
// Constructor.
//
AUdpLink::AUdpLink()
{
	guard(AUdpLink::AUdpLink);
	unguard;
}

//
// Destroy.
//
void AUdpLink::Destroy()
{
	guard(AUdpLink::Destroy);
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
	AndroidLanRepeatRemove( this );
#endif
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
	if( GetSocket() > 2 )
		closesocket(GetSocket());
	GetSocket() = INVALID_SOCKET;
#else
	if( GetSocket() )
		closesocket(GetSocket());
#endif
	Super::Destroy();
	unguard;
}

//
// BindPort: Binds a free port or optional port specified in argument one.
//
void AUdpLink::execBindPort( FFrame& Stack, RESULT_DECL )
{
	guard(AUdpLink::execBindPort);
	P_GET_INT_OPTX(InPort,0);
	P_GET_UBOOL_OPTX(bUseNextAvailable,0);
	P_FINISH;
	if( GInitialized )
	{
		if( GetSocket()==INVALID_SOCKET )
		{
			Socket = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
			if( GetSocket() != INVALID_SOCKET )
			{
				UBOOL TrueBuffer=1;
#ifdef PLATFORM_DREAMCAST
				if( true )
#else
				if( setsockopt( GetSocket(), SOL_SOCKET, SO_BROADCAST, (char*)&TrueBuffer, sizeof(TrueBuffer) )==0 )
#endif
				{
					sockaddr_in Addr;
					Addr.sin_family      = AF_INET;
					Addr.sin_addr        = getlocalbindaddr( Stack );
					Addr.sin_port        = htons(InPort);
					INT boundport = bindnextport( Socket, &Addr, bUseNextAvailable ? 20 : 1, 1 );
					if( boundport )
					{
						#if __GNUG__
						INT pd_flags;
						pd_flags = fcntl( Socket, F_GETFL, 0 );
						pd_flags |= O_NONBLOCK;
						if( fcntl( Socket, F_SETFL, pd_flags ) == 0 )
						#else
						DWORD NoBlock = 1;
						if( ioctlsocket( Socket, FIONBIO, &NoBlock )==0 )
						#endif
						{
							// Success.
							*(INT*)Result = boundport;
							Port = ntohs( Addr.sin_port );
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
							if( AndroidLanClassLooksRelevant( this ) || InPort==8777 || InPort==9777 || InPort==0 )
							{
								AndroidLanLog( *FString::Printf( TEXT("UT99_ANDROID_V112_LAN bind class=%s requested=%i bound=%i next=%i"), GetClass()->GetName(), InPort, Port, bUseNextAvailable ? 1 : 0 ) );
							}
#endif
							return;
						}
						else Stack.Logf( TEXT("BindPort: ioctlsocket failed") );
					}
					else Stack.Logf( TEXT("BindPort: bind failed") );
				}
				else Stack.Logf( TEXT("BindPort: setsockopt failed") );
			}
			else Stack.Logf( TEXT("BindPort: socket failed") );
			closesocket(GetSocket());
			GetSocket()=0;
		}
		else Stack.Logf( TEXT("BindPort: already bound") );
	}
	else Stack.Logf( TEXT("BindPort: winsock failed") );
	*(INT*)Result = 0;
	unguard;
}

//
// Send text in a UDP packet.
//
void AUdpLink::execSendText( FFrame& Stack, RESULT_DECL )
{
	guard(AUdpLink::execSendText);
	P_GET_STRUCT(FIpAddr,IpAddr);
	P_GET_STR(Str);
	P_FINISH;
	if( GetSocket() )
	{
		sockaddr_in Addr;
		Addr.sin_family      = AF_INET;
		Addr.sin_port        = htons(IpAddr.Port);
		Addr.sin_addr.s_addr = htonl(IpAddr.Addr);
		INT SentBytes = sendto( Socket, (char*)appToAnsi(*Str), sizeof(ANSICHAR)*Str.Len(), 0, (sockaddr*)&Addr, sizeof(Addr) );
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
		AndroidLanFanOutReportQuery( Socket, IpAddr.Port, Str );
#endif
		if ( SentBytes == 0 )
		{
			Stack.Logf( TEXT("SentText: sendto failed") );
			*(DWORD*)Result = 0;
			return;
		}
		else
		{
			//debugf("Sent %i bytes.", SentBytes);
		}
	}
	*(DWORD*)Result = 1;
	unguard;
}

//
// Send binary data.
//
void AUdpLink::execSendBinary( FFrame& Stack, RESULT_DECL )
{
	guard(AUdpLink::execSendBinary);
	P_GET_STRUCT(FIpAddr,IpAddr);
	P_GET_INT(Count);
	P_GET_ARRAY_REF(BYTE,B);
	P_FINISH;
	if( GetSocket() )
	{
		sockaddr_in Addr;
		Addr.sin_family      = AF_INET;
		Addr.sin_port        = htons(IpAddr.Port);
		Addr.sin_addr.s_addr = htonl(IpAddr.Addr);
		if( sendto( Socket, (char*)B, Count, 0, (sockaddr*)&Addr, sizeof(Addr) )==0 )
		{
			Stack.Logf( TEXT("SendBinary: sendto failed") );
			*(DWORD*)Result = 1;
			return;
		}
	}
	*(DWORD*)Result = 0;
	unguard;
}

//
// Time passes...
//
UBOOL AUdpLink::Tick( FLOAT DeltaTime, enum ELevelTick TickType )
{
	guard(AUdpLink::Tick);
	UBOOL Result = Super::Tick( DeltaTime, TickType );
	if( GetSocket() )
	{
		if( ReceiveMode == RMODE_Event )
		{
			BYTE Buffer[MAXRECVDATASIZE];
			sockaddr_in FromAddr;
			socklen_t FromSize = sizeof(FromAddr);
			INT Count = recvfrom( GetSocket(), (char*)Buffer, ARRAY_COUNT(Buffer)-1, 0, (sockaddr*)&FromAddr, &FromSize );
			if( Count!=SOCKET_ERROR )
			{
				FIpAddr Addr;
				Addr.Addr = ntohl( FromAddr.sin_addr.s_addr );
				Addr.Port = ntohs( FromAddr.sin_port );
				if( LinkMode == MODE_Text )
				{
					Buffer[Count]=0;
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
					FString IncomingText = appFromAnsi((ANSICHAR*)Buffer);
					AndroidLanNativeBeaconReply( this, Addr, IncomingText );
					if( (AndroidLanClassIs( this, TEXT("UBrowserLocalLink") ) || AndroidLanClassContains( this, TEXT("LocalLink") )) && AndroidLanLooksLikeServerInfo( IncomingText ) )
					{
						FString SynthBeacon = FString::Printf( TEXT("ut %i"), Addr.Port );
						AndroidLanLog( *FString::Printf( TEXT("UT99_ANDROID_V112_LAN direct info reply %i.%i.%i.%i:%i -> %s"),
							(Addr.Addr >> 24) & 255, (Addr.Addr >> 16) & 255, (Addr.Addr >> 8) & 255, Addr.Addr & 255, Addr.Port, *SynthBeacon ) );
						eventReceivedText( Addr, SynthBeacon );
					}
					else
					{
						eventReceivedText( Addr, IncomingText );
					}
#else
					eventReceivedText( Addr, appFromAnsi((ANSICHAR*)Buffer) );
#endif
				}
				else if ( LinkMode == MODE_Line )
				{
					Buffer[Count]=0;
					eventReceivedLine( Addr, appFromAnsi((ANSICHAR*)Buffer) );
				}
				else if( LinkMode == MODE_Binary )
				{
					eventReceivedBinary( Addr, Count, (BYTE*)Buffer );
				}
			}
		}
		else if( ReceiveMode == RMODE_Manual )
		{
			fd_set SocketSet;
			TIMEVAL SelectTime = {0, 0};
			INT Error;

			FD_ZERO( &SocketSet );
			FD_SET( Socket, &SocketSet );
			Error = select( Socket + 1, &SocketSet, 0, 0, &SelectTime);
			if( Error==0 || Error==SOCKET_ERROR )
			{
				DataPending = 0;
			}
			else
			{
				DataPending = 1;
			}
		}
	}

#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
	AndroidLanRepeatTick( this );
#endif
	return Result;
	unguard;
}

//
// Read text.
//
void AUdpLink::execReadText( FFrame& Stack, RESULT_DECL )
{
	guard(AUdpLink::execReadText);
	P_GET_STRUCT_REF( FIpAddr, Addr );
	P_GET_STR_REF( Str );
	P_FINISH;
	*Str = TEXT("");
	if( GetSocket() )
	{
		BYTE Buffer[MAXRECVDATASIZE];
		sockaddr_in FromAddr;
		socklen_t FromSize = sizeof(FromAddr);
		INT BytesReceived = recvfrom( (SOCKET)Socket, (char*)Buffer, sizeof(Buffer), 0, (sockaddr*)&FromAddr, &FromSize );
		if( BytesReceived != SOCKET_ERROR )
		{
			Addr->Addr = ntohl( FromAddr.sin_addr.s_addr );
			Addr->Port = ntohs( FromAddr.sin_port );
			*Str = appFromAnsi((ANSICHAR*)Buffer);
			*(DWORD*)Result = BytesReceived;
		}
		else
		{
			*(DWORD*) Result = 0;
			if ( WSAGetLastError() != WSAEWOULDBLOCK )
				debugf( NAME_Log, TEXT("ReadText: Error reading text.") );
			return;
		}
		return;
	}
	*(DWORD*)Result = 0;

	unguardexec;
}

//
// Read Binary.
//
void AUdpLink::execReadBinary( FFrame& Stack, RESULT_DECL )
{
	guard(AUdpLink::execReadBinary);
	P_GET_STRUCT_REF(FIpAddr, Addr);
	P_GET_INT(Count);
	P_GET_ARRAY_REF(BYTE,B);
	P_FINISH;
	if( GetSocket() )
	{
		sockaddr_in FromAddr;
		socklen_t FromSize = sizeof(FromAddr);
		INT BytesReceived = recvfrom( (SOCKET) Socket, (char*)B, Min<INT>(Count,255), 0, (sockaddr*)&FromAddr, &FromSize );
		if( BytesReceived != SOCKET_ERROR )
		{
			Addr->Addr = ntohl( FromAddr.sin_addr.s_addr );
			Addr->Port = ntohs( FromAddr.sin_port );
			*(DWORD*) Result = BytesReceived;
		}
		else
		{
			*(DWORD*)Result = 0;
			if( WSAGetLastError() != WSAEWOULDBLOCK )
				debugf( NAME_Log, TEXT("ReadBinary: Error reading text.") );
			return;
		}
		return;
	}
	*(DWORD*)Result = 0;

	unguardexec;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
