/*=============================================================================
	UnLevTic.cpp: Level timer tick function
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"

#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
#include <android/log.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
static void AndroidNetLogV115( const TCHAR* Text )
{
	if( Text )
	{
		debugf( NAME_Log, TEXT("%s"), Text );
		__android_log_print( ANDROID_LOG_INFO, "UT99LAN", "%s", appToAnsi(Text) );
	}
}


// UT99_ANDROID_V128_NATIVE_LAN_BROWSER_BEACON:
// In the current Android/OUYA path the listen server itself is stable, but the
// stock LAN Servers tab can stay empty because the scripted IpDrv.UdpBeacon does
// not visibly bind/tick on the Android build.  Keep the gameplay replication path
// untouched and provide a tiny native LAN beacon responder for the stock
// UBrowserLocalLink protocol:
//   client sends  REPORTQUERY  to 8777..8786
//   server replies "ut <gameport>" to the client's source port
// This also sends periodic old-style beacons to 9777 for ClientBeaconReceiver.
static INT    GAndroidV128LanSock       = -1;
static INT    GAndroidV128LanPort       = 0;
static INT    GAndroidV128QuerySock     = -1;
static INT    GAndroidV128QueryPort     = 0;
static DOUBLE GAndroidV128LastBeacon    = 0.0;
static UBOOL  GAndroidV128BindLogged    = 0;
static UBOOL  GAndroidV128QueryLogged   = 0;

static void AndroidV128LanSendTo( INT Sock, DWORD AddrHost, INT Port, const char* Text )
{
	if( Sock < 0 || !Text || Port <= 0 )
		return;
	sockaddr_in To;
	appMemzero( &To, sizeof(To) );
	To.sin_family      = AF_INET;
	To.sin_port        = htons(Port);
	To.sin_addr.s_addr = htonl(AddrHost);
	sendto( Sock, Text, strlen(Text), 0, (sockaddr*)&To, sizeof(To) );
}

static INT AndroidV128LanEnsureSocket()
{
	if( GAndroidV128LanSock >= 0 )
		return GAndroidV128LanSock;

	INT Sock = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if( Sock < 0 )
		return -1;

	INT One = 1;
	setsockopt( Sock, SOL_SOCKET, SO_REUSEADDR, &One, sizeof(One) );
	setsockopt( Sock, SOL_SOCKET, SO_BROADCAST, &One, sizeof(One) );

	INT Flags = fcntl( Sock, F_GETFL, 0 );
	if( Flags >= 0 )
		fcntl( Sock, F_SETFL, Flags | O_NONBLOCK );

	for( INT Port=8777; Port<8797; Port++ )
	{
		sockaddr_in Addr;
		appMemzero( &Addr, sizeof(Addr) );
		Addr.sin_family      = AF_INET;
		Addr.sin_addr.s_addr = htonl(INADDR_ANY);
		Addr.sin_port        = htons(Port);
		if( bind( Sock, (sockaddr*)&Addr, sizeof(Addr) ) == 0 )
		{
			GAndroidV128LanSock = Sock;
			GAndroidV128LanPort = Port;
			if( !GAndroidV128BindLogged )
			{
				GAndroidV128BindLogged = 1;
				AndroidNetLogV115( *FString::Printf(TEXT("UT99_ANDROID_V128_LAN native beacon bound port=%i"), Port) );
			}
			return GAndroidV128LanSock;
		}
	}

	if( !GAndroidV128BindLogged )
	{
		GAndroidV128BindLogged = 1;
		AndroidNetLogV115( TEXT("UT99_ANDROID_V128_LAN native beacon bind failed ports=8777-8796") );
	}
	close( Sock );
	return -1;
}


static INT AndroidV128BindUdpPortRange( INT FirstPort, INT LastPort, INT& OutPort )
{
	INT Sock = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if( Sock < 0 )
		return -1;
	INT One = 1;
	setsockopt( Sock, SOL_SOCKET, SO_REUSEADDR, &One, sizeof(One) );
	setsockopt( Sock, SOL_SOCKET, SO_BROADCAST, &One, sizeof(One) );
	INT Flags = fcntl( Sock, F_GETFL, 0 );
	if( Flags >= 0 )
		fcntl( Sock, F_SETFL, Flags | O_NONBLOCK );
	for( INT Port=FirstPort; Port<=LastPort; Port++ )
	{
		sockaddr_in Addr;
		appMemzero( &Addr, sizeof(Addr) );
		Addr.sin_family      = AF_INET;
		Addr.sin_addr.s_addr = htonl(INADDR_ANY);
		Addr.sin_port        = htons(Port);
		if( bind( Sock, (sockaddr*)&Addr, sizeof(Addr) ) == 0 )
		{
			OutPort = Port;
			return Sock;
		}
	}
	close( Sock );
	return -1;
}

static INT AndroidV128LanEnsureQuerySocket( INT GamePort )
{
	if( GAndroidV128QuerySock >= 0 )
		return GAndroidV128QuerySock;
	INT Preferred = GamePort > 0 ? GamePort + 1 : 7778;
	GAndroidV128QuerySock = AndroidV128BindUdpPortRange( Preferred, Preferred, GAndroidV128QueryPort );
	if( GAndroidV128QuerySock < 0 )
		GAndroidV128QuerySock = AndroidV128BindUdpPortRange( 7778, 7787, GAndroidV128QueryPort );
	if( !GAndroidV128QueryLogged )
	{
		GAndroidV128QueryLogged = 1;
		if( GAndroidV128QuerySock >= 0 )
			AndroidNetLogV115( *FString::Printf(TEXT("UT99_ANDROID_V128_LAN native query bound port=%i"), GAndroidV128QueryPort) );
		else
			AndroidNetLogV115( TEXT("UT99_ANDROID_V128_LAN native query bind skipped ports=7778-7787") );
	}
	return GAndroidV128QuerySock;
}

static void AndroidV128LanTickQuerySocket( ULevel* Level, INT GamePort )
{
	INT Sock = AndroidV128LanEnsureQuerySocket( GamePort );
	if( Sock < 0 || !Level )
		return;

	char MapName[128];
	char Title[128];
	strncpy( MapName, appToAnsi(*Level->URL.Map), sizeof(MapName)-1 );
	MapName[sizeof(MapName)-1] = 0;
	strncpy( Title, "UT99 Android LAN", sizeof(Title)-1 );
	Title[sizeof(Title)-1] = 0;
	ALevelInfo* Info = Level->GetLevelInfo();
	if( Info && Info->Title.Len() > 0 )
	{
		strncpy( Title, appToAnsi(*Info->Title), sizeof(Title)-1 );
		Title[sizeof(Title)-1] = 0;
	}

	for( INT n=0; n<12; n++ )
	{
		char Buf[512];
		sockaddr_in From;
		socklen_t FromLen = sizeof(From);
		INT Count = recvfrom( Sock, Buf, sizeof(Buf)-1, 0, (sockaddr*)&From, &FromLen );
		if( Count <= 0 )
			break;
		Buf[Count] = 0;
		if( Buf[0] != '\\' )
			continue;

		char Reply[1024];
		snprintf( Reply, sizeof(Reply),
			"\\hostname\\UT99 Android LAN\\hostport\\%d\\mapname\\%s\\maptitle\\%s\\gametype\\DeathMatchPlus\\gamemode\\openplaying\\numplayers\\1\\maxplayers\\16\\gamever\\400\\minnetver\\400\\listenserver\\True\\gamename\\ut\\final\\",
			GamePort, MapName[0] ? MapName : "DM", Title[0] ? Title : "UT99 Android LAN" );
		sendto( Sock, Reply, strlen(Reply), 0, (sockaddr*)&From, FromLen );

		DWORD FromHost = ntohl(From.sin_addr.s_addr);
		INT FromPort = ntohs(From.sin_port);
		AndroidNetLogV115( *FString::Printf(TEXT("UT99_ANDROID_V128_LAN native query reply from=%u.%u.%u.%u:%i queryport=%i game=%i"),
			(FromHost>>24)&255, (FromHost>>16)&255, (FromHost>>8)&255, FromHost&255, FromPort, GAndroidV128QueryPort, GamePort) );
	}
}

static void AndroidV128LanTick( ULevel* Level )
{
	if( !Level || !Level->NetDriver )
		return;

	INT Sock = AndroidV128LanEnsureSocket();
	if( Sock < 0 )
		return;

	INT GamePort = Level->URL.Port > 0 ? Level->URL.Port : 7777;
	AndroidV128LanTickQuerySocket( Level, GamePort );
	INT QueryPort = GAndroidV128QueryPort > 0 ? GAndroidV128QueryPort : GamePort + 1;
	char ReplyUt[96];
	char ReplyUnreal[96];
	snprintf( ReplyUt, sizeof(ReplyUt), "ut %d", QueryPort );
	snprintf( ReplyUnreal, sizeof(ReplyUnreal), "unreal %d", QueryPort );

	for( INT n=0; n<12; n++ )
	{
		char Buf[256];
		sockaddr_in From;
		socklen_t FromLen = sizeof(From);
		INT Count = recvfrom( Sock, Buf, sizeof(Buf)-1, 0, (sockaddr*)&From, &FromLen );
		if( Count <= 0 )
			break;
		Buf[Count] = 0;

		INT FromPort = ntohs(From.sin_port);
		DWORD FromHost = ntohl(From.sin_addr.s_addr);
		FString Incoming = appFromAnsi((ANSICHAR*)Buf);
		if( appStricmp(*Incoming, TEXT("REPORTQUERY")) == 0 )
		{
			AndroidV128LanSendTo( Sock, FromHost, FromPort, ReplyUt );
			AndroidNetLogV115( *FString::Printf(TEXT("UT99_ANDROID_V128_LAN replied REPORTQUERY from=%u.%u.%u.%u:%i game=%i query=%i listen=%i"),
				(FromHost>>24)&255, (FromHost>>16)&255, (FromHost>>8)&255, FromHost&255, FromPort, GamePort, QueryPort, GAndroidV128LanPort) );
		}
		else if( appStricmp(*Incoming, TEXT("REPORT")) == 0 )
		{
			AndroidV128LanSendTo( Sock, FromHost, FromPort, ReplyUt );
			AndroidV128LanSendTo( Sock, FromHost, FromPort, ReplyUnreal );
			AndroidNetLogV115( *FString::Printf(TEXT("UT99_ANDROID_V128_LAN replied REPORT from=%u.%u.%u.%u:%i game=%i"),
				(FromHost>>24)&255, (FromHost>>16)&255, (FromHost>>8)&255, FromHost&255, FromPort, GamePort) );
		}
	}

	DOUBLE Now = appSeconds();
	if( Now - GAndroidV128LastBeacon >= 1.0 )
	{
		GAndroidV128LastBeacon = Now;
		AndroidV128LanSendTo( Sock, 0xffffffff, 9777, ReplyUt );
		AndroidV128LanSendTo( Sock, 0xffffffff, 9777, ReplyUnreal );
		if( ((INT)Now & 7) == 0 )
			AndroidNetLogV115( *FString::Printf(TEXT("UT99_ANDROID_V128_LAN broadcast beacon game=%i query=%i listen=%i"), GamePort, QueryPort, GAndroidV128LanPort) );
	}
}

#endif

/*-----------------------------------------------------------------------------
	Helper classes.
-----------------------------------------------------------------------------*/

//
// Priority sortable list.
//
struct FActorPriority
{
	INT             Priority;  // Update priority, higher = more important.
	AActor*         Actor;     // Actor.
	UActorChannel*  Channel;   // Actor channel.
	FActorPriority()
	: Priority(-2147483647), Actor(NULL), Channel(NULL)
	{}
	FActorPriority( FVector& ViewPos, FVector& ViewDir, UNetConnection* InConnection, AActor* InActor )
	: Priority(-2147483647), Actor(InActor), Channel(NULL)
	{
		guard(FActorPriority::FActorPriority);
		if( InConnection && InConnection->Driver && InActor && !InActor->bDeleteMe )
		{
			Channel = InConnection->ActorChannels.FindRef(Actor);
			if( Channel && (Channel->Connection!=InConnection || Channel->Closing) )
				Channel = NULL;
			FLOAT Time = Channel
				? InConnection->Driver->Time - Channel->LastUpdateTime
				: InConnection->Driver->SpawnPrioritySeconds;
			FLOAT Dot = ViewDir | (Actor->Location - ViewPos).SafeNormal();
			Priority = appRound(65536.0 * (3.0+Dot) * Actor->GetNetPriority(
				(Channel && Channel->Recent.Num()) ? (AActor*)&Channel->Recent(0) : NULL,
				Time,
				InConnection->BestLag ));
			if( InActor->bNetOptional )
				Priority -= 100000;
		}
		unguard;
	}
	friend INT Compare( const FActorPriority* A, const FActorPriority* B )
	{
		if( A==B )
			return 0;
		if( !A )
			return 1;
		if( !B )
			return -1;
		return A->Priority < B->Priority ? 1 : (A->Priority > B->Priority ? -1 : 0);
	}
};

/*-----------------------------------------------------------------------------
	Tick a single actor.
-----------------------------------------------------------------------------*/

UBOOL AActor::Tick( FLOAT DeltaSeconds, ELevelTick TickType )
{
	guard(AActor::Tick);

	// Ignore actors in stasis
	if
	(	bStasis 
	&&	(bForceStasis || (Physics==PHYS_None) || (Physics == PHYS_Rotating))
	&&	(GetLevel()->TimeSeconds - GetLevel()->Model->Zones[Region.ZoneNumber].LastRenderTime > 5)
	&&	(Level->NetMode == NM_Standalone) )
		return 1;

	// Handle owner-first updating.
	if( Owner && (INT)Owner->bTicked!=GetLevel()->Ticked )
	{
		GetLevel()->NewlySpawned = new(GEngineMem)FActorLink(this,GetLevel()->NewlySpawned);
		return 0;
	}
	bTicked = GetLevel()->Ticked;
	APawn* Pawn = NULL;
	if( bIsPawn )
		Pawn = Cast<APawn>(this);

	INT bSimulatedPawn = ( Pawn && (Role == ROLE_SimulatedProxy) );

	// Update all animation, including multiple passes if necessary.
	INT Iterations = 0;
	FLOAT Seconds = DeltaSeconds;
	//if ( bSimulatedPawn )
	//	debugf("Animation %s frame %f rate %f tween %f",*AnimSequence,AnimFrame, AnimRate, TweenRate);
	while
	(	IsAnimating()
	&&	(Seconds>0.0)
	&&	(++Iterations <= 4) )
	{
		// Remember the old frame.
		FLOAT OldAnimFrame = AnimFrame;

		// Update animation, and possibly overflow it.
		if( AnimFrame >= 0.0 )
		{
			// Update regular or velocity-scaled animation.
			if( AnimRate >= 0.0 )
				AnimFrame += AnimRate * Seconds;
			else
				AnimFrame += ::Max( AnimMinRate, Velocity.Size() * -AnimRate ) * Seconds;

			// Handle all animation sequence notifys.
			if( bAnimNotify && Mesh )
			{
				const FMeshAnimSeq* Seq = Mesh->GetAnimSeq( AnimSequence );
				if( Seq )
				{
					FLOAT BestElapsedFrames = 100000.0;
					const FMeshAnimNotify* BestNotify = NULL;
					for( INT i=0; i<Seq->Notifys.Num(); i++ )
					{
						const FMeshAnimNotify& Notify = Seq->Notifys(i);
						if( OldAnimFrame<Notify.Time && AnimFrame>=Notify.Time )
						{
							FLOAT ElapsedFrames = Notify.Time - OldAnimFrame;
							if( BestNotify==NULL || ElapsedFrames<BestElapsedFrames )
							{
								BestElapsedFrames = ElapsedFrames;
								BestNotify        = &Notify;
							}
						}
					}
					if( BestNotify )
					{
						Seconds   = Seconds * (AnimFrame - BestNotify->Time) / (AnimFrame - OldAnimFrame);
						AnimFrame = BestNotify->Time;
						UFunction* Function = FindFunction( BestNotify->Function );
						if( Function )
							ProcessEvent( Function, NULL );
						continue;
					}
				}
			}

			// Handle end of animation sequence.
			if( AnimFrame<AnimLast )
			{
				// We have finished the animation updating for this tick.
				break;
			}
			else if( bAnimLoop )
			{
				if( AnimFrame < 1.0 )
				{
					// Still looping.
					Seconds = 0.0;
				}
				else
				{
					// Just passed end, so loop it.
					Seconds = Seconds * (AnimFrame - 1.0) / (AnimFrame - OldAnimFrame);
					AnimFrame = 0.0;
				}
				if( OldAnimFrame < AnimLast )
				{
					if( GetStateFrame()->LatentAction == EPOLL_FinishAnim )
						bAnimFinished = 1;
					if( !bSimulatedPawn )
						eventAnimEnd();
				}
			}
			else 
			{
				// Just passed end-minus-one frame.
				Seconds = Seconds * (AnimFrame - AnimLast) / (AnimFrame - OldAnimFrame);
				AnimFrame	 = AnimLast;
				bAnimFinished = 1;
				AnimRate      = 0.0;
				if ( !bSimulatedPawn )
					eventAnimEnd();
				
				if ( (RemoteRole < ROLE_SimulatedProxy) && !IsA(AWeapon::StaticClass()) )
				{
					SimAnim.X = 10000 * AnimFrame;
					SimAnim.Y = 5000 * AnimRate;
					if ( SimAnim.Y > 32767 )
						SimAnim.Y = 32767;
				}
			}
		}
		else
		{
			// Update tweening.
			AnimFrame += TweenRate * Seconds;
			if( AnimFrame >= 0.0 )
			{
				// Finished tweening.
				Seconds          = Seconds * (AnimFrame-0) / (AnimFrame - OldAnimFrame);
				AnimFrame = 0.0;
				if( AnimRate == 0.0 )
				{
					bAnimFinished = 1;
					if ( !bSimulatedPawn )
						eventAnimEnd();
				}
			}
			else
			{
				// Finished tweening.
				break;
			}
		}
	}

	// This actor is tickable.
	if( bSimulatedPawn )
	{
		// FIXME - predict fall for all pawns (COOP) - but need
		// new replicated bool for pawns which don't fly but don't fall
		// (i.e. stuck on wall, PHYS_Spider, etc.)
		if ( Pawn->bIsPlayer && !Pawn->bCanFly && !Region.Zone->bWaterZone )
		{
			// only add gravity if pawn is not resting on valid floor
			FCheckResult Hit(1.0);
			GetLevel()->SingleLineCheck(Hit, this, Location - FVector(0,0,8), Location, TRACE_VisBlocking, GetCylinderExtent());
			if ( (Hit.Time == 1.0) || (Hit.Normal.Z < 0.7) )
				Velocity += 0.5 * Region.Zone->ZoneGravity * DeltaSeconds;
		}
		//simulated pawns just predict location, no script execution
		moveSmooth(Velocity * DeltaSeconds);

		// Tick the nonplayer.
		if ( IsProbing(NAME_Tick) )
			eventTick(DeltaSeconds);
	}
	else if( RemoteRole == ROLE_AutonomousProxy ) 
	{
		if( Role == ROLE_Authority )
		{
			// update viewtarget replicated info
			APlayerPawn* PlayerPawn = NULL;
			if( Pawn )
			{
				PlayerPawn = Cast<APlayerPawn>(this);
			}
			if( PlayerPawn && PlayerPawn->ViewTarget )
			{
				APawn* TargetPawn = Cast<APawn>(PlayerPawn->ViewTarget);
				if ( TargetPawn )
				{
					PlayerPawn->TargetViewRotation = TargetPawn->ViewRotation;
					PlayerPawn->TargetEyeHeight = TargetPawn->EyeHeight;
					if ( TargetPawn->Weapon )
						PlayerPawn->TargetWeaponViewOffset = TargetPawn->Weapon->PlayerViewOffset;
				}
			}

			// Server handles timers for autonomous proxy.
			if( (TimerRate>0.0) && (TimerCounter+=DeltaSeconds)>=TimerRate )
			{
				// Normalize the timer count.
				INT TimerTicksPassed = 1;
				if( TimerRate > 0.0 )
				{
					TimerTicksPassed     = (int)(TimerCounter/TimerRate);
					TimerCounter -= TimerRate * TimerTicksPassed;
					if( TimerTicksPassed && !bTimerLoop )
					{
						// Only want a one-shot timer message.
						TimerTicksPassed = 1;
						TimerRate = 0.0;
					}
				}

				// Call timer routine with count of timer events that have passed.
				eventTimer();
			}
		}
	}
	else if( Role>=ROLE_SimulatedProxy )
	{
		APlayerPawn* PlayerPawn = NULL;
		if ( Pawn )
			PlayerPawn = Cast<APlayerPawn>(this);
		if( !PlayerPawn || !PlayerPawn->Player )
		{
			// Non-player update.
			if( TickType==LEVELTICK_ViewportsOnly )
				return 1;

			// Tick the nonplayer.
			if ( IsProbing(NAME_Tick) )
				eventTick(DeltaSeconds);
		}
		else
		{
			// Player update.
			if( PlayerPawn->IsA(ACamera::StaticClass()) && !(PlayerPawn->ShowFlags & SHOW_PlayerCtrl) )
				return 1;

			// Process PlayerTick with input.
			PlayerPawn->Player->ReadInput( DeltaSeconds );
			PlayerPawn->eventPlayerInput( DeltaSeconds );
			PlayerPawn->eventPlayerTick( DeltaSeconds );
			PlayerPawn->Player->ReadInput( -1.0 );

			if( GetLevel()->DemoRecDriver && !GetLevel()->DemoRecDriver->ServerConnection )
			{
				PlayerPawn->DemoViewPitch = PlayerPawn->ViewRotation.Pitch;
				PlayerPawn->DemoViewYaw = PlayerPawn->ViewRotation.Yaw;
			}
		}

		// Update the actor's script state code.
		ProcessState( DeltaSeconds );

		// Update timers.
		if( TimerRate>0.0 && (TimerCounter+=DeltaSeconds)>=TimerRate )
		{
			// Normalize the timer count.
			INT TimerTicksPassed = 1;
			if( TimerRate > 0.0 )
			{
				TimerTicksPassed     = (int)(TimerCounter/TimerRate);
				TimerCounter -= TimerRate * TimerTicksPassed;
				if( TimerTicksPassed && !bTimerLoop )
				{
					// Only want a one-shot timer message.
					TimerTicksPassed = 1;
					TimerRate = 0.0;
				}
			}

			// Call timer routine with count of timer events that have passed.
			eventTimer();
		}

		// Update LifeSpan.
		if( LifeSpan!=0.f )
		{
			LifeSpan -= DeltaSeconds;
			if( LifeSpan <= 0.0001 )
			{
				// Actor's LifeSpan expired.
				eventExpired();
				GetLevel()->DestroyActor( this );
				return 1;
			}
		}

		// Perform physics.
		if( Physics!=PHYS_None && Role!=ROLE_AutonomousProxy )
			performPhysics( DeltaSeconds );
	}
	else if ( Physics == PHYS_Falling ) // dumbproxies simulate falling if client side physics set
		performPhysics( DeltaSeconds );

	// During demo playback, setup view offsets for viewtarget
	if( GetLevel()->DemoRecDriver && GetLevel()->DemoRecDriver->ServerConnection )
	{
		if( Role == ROLE_Authority )
		{
			// update viewtarget replicated info
			APlayerPawn* PlayerPawn = NULL;
			if( Pawn )
			{
				PlayerPawn = Cast<APlayerPawn>(this);
			}
			if( PlayerPawn && PlayerPawn->ViewTarget && !PlayerPawn->bBehindView )
			{
				APawn* TargetPawn = Cast<APawn>(PlayerPawn->ViewTarget);
				if ( TargetPawn )
				{
					PlayerPawn->TargetViewRotation = TargetPawn->ViewRotation;
					PlayerPawn->TargetEyeHeight = TargetPawn->EyeHeight;
					if ( TargetPawn->Weapon )
						PlayerPawn->TargetWeaponViewOffset = TargetPawn->Weapon->PlayerViewOffset;
				}
			}
		}
	}
	
	// Update eyeheight and send visibility updates
	// with PVS, monsters look for other monsters, rather than sending msgs
	// Also sends PainTimer messages if PainTime
	if( Pawn )
	{
		if( Pawn->bIsPlayer && Role>=ROLE_AutonomousProxy )
		{
			if ( Pawn->bViewTarget )
				Pawn->eventUpdateEyeHeight( DeltaSeconds );
			else
				Pawn->ViewRotation = Rotation;
		}

		// update weapon location (in case its playing sounds, etc.)
		if ( Pawn->Weapon )
		{
			GetLevel()->FarMoveActor( Pawn->Weapon, Location );
		}
		if( Role==ROLE_Authority && TickType==LEVELTICK_All )
		{
			if( Pawn->SightCounter < 0.0 )
			{
				Pawn->SightCounter += 0.2;
			}
			Pawn->SightCounter = Pawn->SightCounter - DeltaSeconds; 
			if( Pawn->bIsPlayer && !Pawn->bHidden )
			{
				Pawn->ShowSelf();
			}
			if( Pawn->SightCounter<0.0 && Pawn->IsProbing(NAME_EnemyNotVisible) )
			{
				Pawn->CheckEnemyVisible();
				Pawn->SightCounter = 0.1;
			}
			if( Pawn->PainTime > 0.0 )
			{
				Pawn->PainTime -= DeltaSeconds;
				if (Pawn->PainTime < 0.001)
				{
					Pawn->PainTime = 0.0;
					Pawn->eventPainTimer();
				}
			}
			if( Pawn->SpeechTime > 0.0 )
			{
				Pawn->SpeechTime -= DeltaSeconds;
				if (Pawn->SpeechTime < 0.001)
				{
					Pawn->SpeechTime = 0.0;
					Pawn->eventSpeechTimer();
				}
			}
			if ( Pawn->bAdvancedTactics )
				Pawn->eventUpdateTactics(DeltaSeconds);
		}
	}

	return 1;
	unguard;
}

/*-----------------------------------------------------------------------------
	Network client tick.
-----------------------------------------------------------------------------*/

void ULevel::TickNetClient( FLOAT DeltaSeconds )
{
	guard(ULevel::TickNetClient);
	clock(NetTickCycles);
	if( NetDriver->ServerConnection->State==USOCK_Open )
	{
		for( TMap<AActor*,UActorChannel*>::TIterator ItC(NetDriver->ServerConnection->ActorChannels); ItC; ++ItC )
		{
			guard(UpdateLocalActors);
			UActorChannel* It = ItC.Value();
			APlayerPawn* PlayerPawn = Cast<APlayerPawn>(It->GetActor());
			if( PlayerPawn && PlayerPawn->Player )
				It->ReplicateActor();
			unguard;
		}
	}
	else if( NetDriver->ServerConnection->State==USOCK_Closed )
	{
		// Server disconnected.
		check(Engine->Client->Viewports.Num());
		Engine->SetClientTravel( Engine->Client->Viewports(0), TEXT("?failed"), 0, TRAVEL_Absolute );
	}
	unclock(NetTickCycles);
	unguard;
}

/*-----------------------------------------------------------------------------
	Network server ticking individual client.
-----------------------------------------------------------------------------*/

UBOOL ActorCanSee( AActor* Actor, APlayerPawn* RealViewer, AActor* Viewer, FVector SrcLocation )
{
	guardSlow(ActorCanSee);
	if( !Actor || !RealViewer || !Viewer || Actor->bDeleteMe || RealViewer->bDeleteMe || Viewer->bDeleteMe )
		return 0;
	if( Actor->bAlwaysRelevant || Actor->IsOwnedBy(Viewer) || Actor->IsOwnedBy(RealViewer) || Actor==Viewer || Actor==RealViewer )
		return 1;
	else if( Actor->AmbientSound
			&& ((Actor->Location-Viewer->Location).SizeSquared() < 0.3*Actor->WorldSoundRadius()*Actor->WorldSoundRadius()) )
		return 1;
	else if( Actor->Owner && !Actor->Owner->bDeleteMe && Actor->Owner->bIsPawn && Actor==((APawn*)Actor->Owner)->Weapon )
		return ActorCanSee( Actor->Owner, RealViewer, Viewer, SrcLocation );
	else if( (Actor->bHidden || Actor->bOnlyOwnerSee) && !Actor->bBlockPlayers && !Actor->AmbientSound )
		return 0;
	else
	{
		ULevel* ActorLevel = Actor->GetLevel();
		return ActorLevel && ActorLevel->Model
			? ActorLevel->Model->FastLineCheck(Actor->Location,SrcLocation)
			: 0;
	}
	unguardSlow;
}

INT ULevel::ServerTickClient( UNetConnection* Connection, FLOAT DeltaSeconds )
{
	guard(ULevel::ServerTickClient);
	check(Connection);
	check(Connection->State==USOCK_Pending || Connection->State==USOCK_Open || Connection->State==USOCK_Closed);
	DOUBLE CullTime=0.0, TraceTime=0.0, RepTime=0.0; INT CullCount=0, RepCount=0;

	INT Updated=0;
	if
	(   Connection->Actor
	&&  !Connection->Actor->bDeleteMe
	&&  NetDriver
	&&  Connection->Driver
	&&  Connection->PackageMap
	&&  Connection->IsNetReady(0)
	&&  Connection->State==USOCK_Open )
	{
		FMemMark Mark(GMem);
		NetTag++;
		Connection->TickCount++;

		// Do not reconsider temporary actors already sent on this connection.
		guard(SkipSentTemporaries);
		for( INT i=0; i<Connection->SentTemporaries.Num(); i++ )
		{
			AActor* SentTemp = Connection->SentTemporaries(i);
			if( SentTemp && !SentTemp->bDeleteMe )
				SentTemp->NetTag = NetTag;
		}
		unguard;

		// Get viewer coordinates.
		AActor*      Viewer    = Connection->Actor;
		APlayerPawn* InViewer  = Connection->Actor;
		FVector      Location  = InViewer->Location;
		FRotator     Rotation  = InViewer->ViewRotation;
		InViewer->eventPlayerCalcView( Viewer, Location, Rotation );
		if( !Viewer || Viewer->bDeleteMe )
		{
			Mark.Pop();
			return 0;
		}

		// Compute ahead-vectors for prediction.
		FVector Ahead = FVector(0,0,0);
		if( Connection->TickCount & 1 )
		{
			FLOAT PredictSeconds = (Connection->TickCount&2) ? 0.4 : 0.9;
			Ahead = PredictSeconds * Viewer->Velocity;
			if( Viewer->Base && !Viewer->Base->bDeleteMe )
				Ahead += PredictSeconds * Viewer->Base->Velocity;
			FCheckResult Hit(1.0);
			Hit.Location = Location + Ahead;
			ULevel* ViewerLevel = Viewer->GetLevel();
			if( ViewerLevel && ViewerLevel->Model )
				ViewerLevel->Model->LineCheck(Hit,NULL,Hit.Location,Location,FVector(0,0,0),NF_NotVisBlocking);
			Location = Hit.Location;
		}

		// Make list of every due actor. No class whitelist or join-time bypass:
		// movers, triggers, pawns, inventory and effects all use the same path.
		CullTime-=appSeconds();
		INT              ConsiderCount  = 0;
		FActorPriority*  PriorityList   = new(GMem,Actors.Num())FActorPriority;
		FActorPriority** PriorityActors = new(GMem,Actors.Num())FActorPriority*;
		FVector          ViewPos        = Viewer->Location;
		FVector          ViewDir        = InViewer->ViewRotation.Vector();
		DOUBLE           LastTime       = Connection->LastRepTime;
		DOUBLE           ThisTime       = Connection->Driver->Time;
		guard(MakeConsiderList);
		for( INT i=0; i<Actors.Num(); i++ )
		{
			AActor* Actor = Actors(i);
			if( Actor && !Actor->bDeleteMe && Actor->GetClass() && Actor->GetLevel() )
			{
				if
				(   (i>=iFirstDynamicActor || Actor->bAlwaysRelevant)
				&&  (Actor->NetTag!=NetTag)
				&&  (Actor->RemoteRole!=ROLE_None)
				&&  (appRound(LastTime*Actor->NetUpdateFrequency)!=appRound(ThisTime*Actor->NetUpdateFrequency)) )
				{
					CullCount++;
					Actor->NetTag                 = NetTag;
					PriorityList  [ConsiderCount] = FActorPriority( ViewPos, ViewDir, Connection, Actor );
					PriorityActors[ConsiderCount] = PriorityList + ConsiderCount++;
				}
				LastTime += 0.023;
				ThisTime += 0.023;
			}
		}
		Connection->LastRepTime = Connection->Driver->Time;
		CullTime+=appSeconds();
		unguard;

		guard(SortConsiderList);
		Sort( PriorityActors, ConsiderCount );
		unguard;

		guard(UpdateRelevant);
		for( INT j=0; j<ConsiderCount && Connection->State==USOCK_Open && Connection->IsNetReady(0); j++ )
		{
			FActorPriority* Item = PriorityActors[j];
			if( !Item )
				continue;
			AActor* Actor = Item->Actor;
			UActorChannel* Channel = Item->Channel;
			if( !Actor || Actor->bDeleteMe || !Actor->GetClass() || !Actor->GetLevel() )
				continue;
			if( Channel && (Channel->Connection!=Connection || Channel->Closing) )
				continue;

			TraceTime-=appSeconds();
			UBOOL CanSee = ActorCanSee( Actor, InViewer, Viewer, Location );
			TraceTime+=appSeconds();
			UBOOL RecentlyRelevant = Channel
				&& NetDriver->Time-Channel->RelevantTime<NetDriver->RelevantTimeout;
			if( CanSee || RecentlyRelevant )
			{
				Actor->GetLevel()->NumPV++;
				if( !Channel && Connection->PackageMap->ObjectToIndex(Actor->GetClass())!=INDEX_NONE )
				{
					Channel = (UActorChannel*)Connection->CreateChannel( CHTYPE_Actor, 1 );
					if( Channel )
						Channel->SetChannelActor( Actor );
				}
				if( Channel && Channel->Connection==Connection && !Channel->Closing )
				{
					if( CanSee )
						Channel->RelevantTime = NetDriver->Time;
					if( Channel->IsNetReady(0) )
					{
						RepTime-=appSeconds();
						RepCount++;
						Channel->ReplicateActor();
						RepTime+=appSeconds();
						Updated++;
					}
				}
			}
			else if( Channel )
				Channel->Close();
		}
		unguard;
		Mark.Pop();
	}
	if( NetDriver && NetDriver->ProfileStats )
		debugf(TEXT("Cull=%01.4f (%03i) Trace=%01.4f Rep=%01.4f (%03i)"),CullTime*1000,CullCount,TraceTime*1000,RepTime*1000,RepCount);
	return Updated;
	unguard;
}

/*-----------------------------------------------------------------------------
	Network server tick.
-----------------------------------------------------------------------------*/

void ULevel::TickNetServer( FLOAT DeltaSeconds )
{
	guard(ULevel::TickNetServer);

#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
	AndroidV128LanTick( this );
#endif

	// Update all clients.
	clock(NetTickCycles);
	INT Updated=0;
	for( INT i=NetDriver->ClientConnections.Num()-1; i>=0; i-- )
		Updated += ServerTickClient( NetDriver->ClientConnections(i), DeltaSeconds );
	unclock(NetTickCycles);

	// Log message.
	if( (INT)(TimeSeconds-DeltaSeconds)!=(INT)(TimeSeconds) )
		debugf( NAME_Title, LocalizeProgress("RunningNet"), *GetLevelInfo()->Title, *URL.Map, NetDriver->ClientConnections.Num() );

	// Stats.
	if( Updated )
	{
		for( INT i=0; i<NetDriver->ClientConnections.Num(); i++ )
		{
			UNetConnection* Connection = NetDriver->ClientConnections(i);
			if( Connection->Actor && Connection->State==USOCK_Open )
			{
				if( Connection->UserFlags&1 )
				{
					// Send stats.
					INT NumActors=0;
					for( INT i=0; i<Actors.Num(); i++ )
						NumActors += Actors(i)!=NULL;
					FString Stats = FString::Printf
					(
						TEXT("r=%i cli=%i act=%03.1f (%i) net=%03.1f pv/c=%i rep/c=%i rpc/c=%i"),
						appRound(Engine->GetMaxTickRate()),
						NetDriver->ClientConnections.Num(),
						GSecondsPerCycle*1000*ActorTickCycles,
						NumActors,
						GSecondsPerCycle*1000*NetTickCycles,
						NumPV  /NetDriver->ClientConnections.Num(),
						NumReps/NetDriver->ClientConnections.Num(),
						NumRPC /NetDriver->ClientConnections.Num()
					);
					Connection->Actor->eventClientMessage( *Stats, NAME_None, 0 );
				}
				if( Connection->UserFlags&2 )
				{
					FString Stats = FString::Printf
					(
						TEXT("snd=%02.1f recv=%02.1f"),
						GSecondsPerCycle*1000*Connection->Driver->SendCycles,
						GSecondsPerCycle*1000*Connection->Driver->RecvCycles
					);
					Connection->Actor->eventClientMessage( *Stats, NAME_None, 0 );
				}
			}
		}
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	Demo Recording tick.
-----------------------------------------------------------------------------*/

INT ULevel::TickDemoRecord( FLOAT DeltaSeconds )
{
	guard(ULevel::TickDemo);

	// All replicatable actors are assumed to be relevant for demo recording.
	UNetConnection* Connection = DemoRecDriver->ClientConnections(0);
	for( INT i=0; i<Actors.Num(); i++ )
	{
		AActor* Actor = Actors(i);
		UBOOL IsNetClient = (GetLevelInfo()->NetMode == NM_Client);
		if
		(	Actor
		&&	(Actor->RemoteRole!=ROLE_None || (IsNetClient && Actor->Role!=ROLE_None && Actor->Role != ROLE_Authority))
		&&  (i>=iFirstDynamicActor || Actor->IsA(AZoneInfo::StaticClass()))
		&&  (!Actor->bNetTemporary || Connection->SentTemporaries.FindItemIndex(Actor)==INDEX_NONE)
		&&  (Actor->bStatic || !Actor->GetClass()->GetDefaultActor()->bStatic))
		{
			// Create a new channel for this actor.
			UActorChannel* Channel = Connection->ActorChannels.FindRef( Actor );
			if( !Channel && Connection->PackageMap->ObjectToIndex(Actor->GetClass())!=INDEX_NONE )
			{
				// Check we haven't run out of actor channels.
				Channel = (UActorChannel*)Connection->CreateChannel( CHTYPE_Actor, 1 );
				check(Channel);
				Channel->SetChannelActor( Actor );
			}
			if( Channel )
			{
				// Send it out!
				check(!Channel->Closing);
				if( Channel->IsNetReady(0) )
				{
					Actor->bDemoRecording = 1;
					Actor->bClientDemoRecording = IsNetClient;
					if(IsNetClient)
						Exchange(Actor->RemoteRole, Actor->Role);
					Channel->ReplicateActor();
					if(IsNetClient)
						Exchange(Actor->RemoteRole, Actor->Role);
					Actor->bDemoRecording = 0;
					Actor->bClientDemoRecording = 0;
				}
			}
		}
	}
	return 1;
	unguard;
}
INT ULevel::TickDemoPlayback( FLOAT DeltaSeconds )
{
	guard(ULevel::TickDemoPlayback);
	if
	(	GetLevelInfo()->LevelAction==LEVACT_Connecting 
	&&	DemoRecDriver->ServerConnection->State!=USOCK_Pending )
	{
		GetLevelInfo()->LevelAction = LEVACT_None;
		Engine->SetProgress( TEXT(""), TEXT(""), 0.0 );
	} 
	if( DemoRecDriver->ServerConnection->State==USOCK_Closed )
	{
		// Demo stopped playing
		check(Engine->Client->Viewports.Num());
		Engine->SetClientTravel( Engine->Client->Viewports(0), TEXT("?entry"), 0, TRAVEL_Absolute );
	}
	return 1;
	unguard;
}

/*-----------------------------------------------------------------------------
	Main level timer tick handler.
-----------------------------------------------------------------------------*/

//
// Update the level after a variable amount of time, DeltaSeconds, has passed.
// All child actors are ticked after their owners have been ticked.
//
void ULevel::Tick( ELevelTick TickType, FLOAT DeltaSeconds )
{
	guard(ULevel::Tick);
	ALevelInfo* Info = GetLevelInfo();
	InitStats();
	FMemMark Mark(GMem);
	FMemMark EngineMark(GEngineMem);
	GInitRunaway();
	InTick=1;

	//Keep actor time profile FIXME TEMP!!!
	Info->AvgAITime = 0.95 * GetLevelInfo()->AvgAITime + 0.05 * 1000 * GSecondsPerCycle * ActorTickCycles;
	FLOAT ratio = GSecondsPerCycle * ActorTickCycles/DeltaSeconds;
	INT offset = (INT)(10 * ratio);
	if ( offset > 7 )
		offset = 7;
	else if ( offset < 0 )
		offset = 0;
	//debugf("ratio is %f, offset is %d",ratio,offset);
	Info->AIProfile[offset] += 1;

	// Update the net code and fetch all incoming packets.
	guard(UpdatePreNet);
	if( NetDriver )
	{
		NetDriver->TickDispatch( DeltaSeconds );
		if( NetDriver->ServerConnection )
			TickNetClient( DeltaSeconds );
	}
	unguard;

	// Fetch demo playback packets from demo file.
	guard(UpdatePreDemoRec);
	if( DemoRecDriver )
	{
		DemoRecDriver->TickDispatch( DeltaSeconds );
		if( DemoRecDriver->ServerConnection )
			TickDemoPlayback( DeltaSeconds );
	}
	unguard;

	// Update collision.
	guard(UpdateCollision);
	if( Hash )
		Hash->Tick();
	unguard;

	// Update time.
	guard(UpdateTime);
	DeltaSeconds *= Info->TimeDilation;
	TimeSeconds += DeltaSeconds;
	Info->TimeSeconds = TimeSeconds;
	UpdateTime(Info);
	if( Info->bPlayersOnly )
		TickType = LEVELTICK_ViewportsOnly;
	unguard;

	// Clamp time between 200 fps and 2.5 fps.
	DeltaSeconds = Clamp(DeltaSeconds,0.005f,0.40f);

	// If caller wants time update only, or we are paused, skip the rest.
	clock(ActorTickCycles);
	if
	(	(TickType!=LEVELTICK_TimeOnly)
	&&	Info->Pauser==TEXT("")
	&&	(!NetDriver || !NetDriver->ServerConnection || NetDriver->ServerConnection->State==USOCK_Open) )
	{
		// Tick all actors, owners before owned.
		guard(TickAllActors);
		NewlySpawned = NULL;
		INT Updated  = 0;
		for( INT iActor=iFirstDynamicActor; iActor<Actors.Num(); iActor++ )
			if( Actors( iActor ) )
				Updated += Actors( iActor )->Tick(DeltaSeconds,TickType);
		while( NewlySpawned && Updated )
		{
			FActorLink* Link = NewlySpawned;
			NewlySpawned     = NULL;
			Updated          = 0;
			for( Link; Link; Link=Link->Next )
				if( Link->Actor->bTicked!=(DWORD)Ticked )
					Updated += Link->Actor->Tick( DeltaSeconds, TickType );
		}
		unguard;
	}
	else if( Info->Pauser!=TEXT("") )
	{
		// Absorb input if paused.
		guard(AbsorbedPaused);
		for( INT iActor=iFirstDynamicActor; iActor<Actors.Num(); iActor++ )
		{
			APlayerPawn* PlayerPawn=Cast<APlayerPawn>(Actors(iActor));
			if( PlayerPawn && PlayerPawn->Player )
			{
				PlayerPawn->Player->ReadInput( DeltaSeconds );
				PlayerPawn->eventPlayerInput( DeltaSeconds );
				for( TFieldIterator<UFloatProperty> It(PlayerPawn->GetClass()); It; ++It )
					if( It->PropertyFlags & CPF_Input )
						*(FLOAT*)((BYTE*)PlayerPawn + It->Offset) = 0.f;
			}
			else if( Actors(iActor) && Actors(iActor)->bAlwaysTick )
				Actors(iActor)->Tick(DeltaSeconds,TickType);
		}
		unguard;
	}
	unclock(ActorTickCycles);

	// Update net server and flush networking.
	guard(UpdateNetServer);
	if( NetDriver )
	{
		if( !NetDriver->ServerConnection )
			TickNetServer( DeltaSeconds );
		NetDriver->TickFlush();
	}
	unguard;

	// Demo Recording.
	guard(UpdatePostDemoRec);
	if( DemoRecDriver )
	{
		if( !DemoRecDriver->ServerConnection )
			TickDemoRecord( DeltaSeconds );
		DemoRecDriver->TickFlush();
	}
	unguard;

	// Finish up.
	Ticked = !Ticked;
	InTick = 0;
	Mark.Pop();
	EngineMark.Pop();
	CleanupDestroyed( 0 );

	unguardf(( TEXT("(NetMode=%i)"), GetLevelInfo()->NetMode ));
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
