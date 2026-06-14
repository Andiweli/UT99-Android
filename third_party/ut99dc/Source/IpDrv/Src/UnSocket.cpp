/*============================================================================
	UnSocket.cpp: Common interface for WinSock and BSD sockets.

	Revision history:
		* Created by Mike Danylchuk
============================================================================*/

#include "IpDrvPrivate.h"

/*----------------------------------------------------------------------------
	Resolve.
----------------------------------------------------------------------------*/

// Resolution thread entrypoint.
#if __GNUG__
void* ResolveThreadEntry( void* Arg )
#else
DWORD STDCALL ResolveThreadEntry( void* Arg )
#endif
{
	FResolveInfo* Info = (FResolveInfo*)Arg;
	IpSetInt( Info->Addr, 0 );
#ifdef PLATFORM_DREAMCAST
	// On Dreamcast, get IP from network device instead of DNS lookup
	if( net_default_dev != NULL )
	{
		IpSetBytes( Info->Addr,
			net_default_dev->ip_addr[0],
			net_default_dev->ip_addr[1],
			net_default_dev->ip_addr[2],
			net_default_dev->ip_addr[3] );
	}
	else
	{
		appSprintf( Info->Error, TEXT("Network device not initialized") );
	}
#else
	HOSTENT* HostEnt = NULL;
	INT e = 0;
	for( INT i=0; i<3; i++)
	{
		HostEnt = gethostbyname( appToAnsi(Info->HostName) );
		if( HostEnt )
			break;
		e = WSAGetLastError();
		if( e == WSAHOST_NOT_FOUND || e == WSANO_DATA)
			break;
		appSleep(1);
	}

	if( HostEnt==NULL || HostEnt->h_addrtype!=PF_INET )
		appSprintf( Info->Error, TEXT("Can't find host %s (%s)"), Info->HostName, SocketError(e) );
	else
		Info->Addr = *(in_addr*)( *HostEnt->h_addr_list );
#endif
	Info->ThreadId = 0;
	return 0;
}

/*----------------------------------------------------------------------------
	Initialization.
----------------------------------------------------------------------------*/

UBOOL InitSockets( FString& Error )
{
	guard(InitSockets);

	// Init names.
	#define NAMES_ONLY
	#define AUTOGENERATE_NAME(name) extern IPDRV_API FName IPDRV_##name; IPDRV_##name=FName(TEXT(#name),FNAME_Intrinsic);
	#define AUTOGENERATE_FUNCTION(cls,idx,name)
	#include "IpDrvClasses.h"
	#undef DECLARE_NAME
	#undef NAMES_ONLY

#if __WINSOCK__
	// Init WSA.
	static UBOOL Tried = 0;
	if( !Tried )
	{
		Tried = 1;
		WSADATA WSAData;
		INT Code = WSAStartup( 0x0101, &WSAData );
		if( Code==0 )
		{
			GInitialized = 1;
			debugf
			(
				NAME_Init,
				TEXT("WinSock: version %i.%i (%i.%i), MaxSocks=%i, MaxUdp=%i"),
				WSAData.wVersion>>8,WSAData.wVersion&255,
				WSAData.wHighVersion>>8,WSAData.wHighVersion&255,
				WSAData.iMaxSockets,WSAData.iMaxUdpDg
			);
			//debugf( NAME_Init, TEXT("WinSock: %s"), WSAData.szDescription );
		} else {
			TCHAR Error256[256];
			appSprintf( Error256, TEXT("WSAStartup failed (%s)"), SocketError(Code) );
			Error = FString::Printf( TEXT("%s"), Error256 );
		}
	}
#elif __BSD_SOCKETS__
	GInitialized = 1;
#endif

	return GInitialized;
	unguard;
}

/*----------------------------------------------------------------------------
	Errors.
----------------------------------------------------------------------------*/

//
// Convert error code to text.
//
TCHAR* SocketError( INT Code )
{
#if __WINSOCK__
	if( Code == -1 )
		Code = WSAGetLastError();
	switch( Code )
	{
		case WSAEINTR:				return TEXT("WSAEINTR");
		case WSAEBADF:				return TEXT("WSAEBADF");
		case WSAEACCES:				return TEXT("WSAEACCES");
		case WSAEFAULT:				return TEXT("WSAEFAULT");
		case WSAEINVAL:				return TEXT("WSAEINVAL");
		case WSAEMFILE:				return TEXT("WSAEMFILE");
		case WSAEWOULDBLOCK:		return TEXT("WSAEWOULDBLOCK");
		case WSAEINPROGRESS:		return TEXT("WSAEINPROGRESS");
		case WSAEALREADY:			return TEXT("WSAEALREADY");
		case WSAENOTSOCK:			return TEXT("WSAENOTSOCK");
		case WSAEDESTADDRREQ:		return TEXT("WSAEDESTADDRREQ");
		case WSAEMSGSIZE:			return TEXT("WSAEMSGSIZE");
		case WSAEPROTOTYPE:			return TEXT("WSAEPROTOTYPE");
		case WSAENOPROTOOPT:		return TEXT("WSAENOPROTOOPT");
		case WSAEPROTONOSUPPORT:	return TEXT("WSAEPROTONOSUPPORT");
		case WSAESOCKTNOSUPPORT:	return TEXT("WSAESOCKTNOSUPPORT");
		case WSAEOPNOTSUPP:			return TEXT("WSAEOPNOTSUPP");
		case WSAEPFNOSUPPORT:		return TEXT("WSAEPFNOSUPPORT");
		case WSAEAFNOSUPPORT:		return TEXT("WSAEAFNOSUPPORT");
		case WSAEADDRINUSE:			return TEXT("WSAEADDRINUSE");
		case WSAEADDRNOTAVAIL:		return TEXT("WSAEADDRNOTAVAIL");
		case WSAENETDOWN:			return TEXT("WSAENETDOWN");
		case WSAENETUNREACH:		return TEXT("WSAENETUNREACH");
		case WSAENETRESET:			return TEXT("WSAENETRESET");
		case WSAECONNABORTED:		return TEXT("WSAECONNABORTED");
		case WSAECONNRESET:			return TEXT("WSAECONNRESET");
		case WSAENOBUFS:			return TEXT("WSAENOBUFS");
		case WSAEISCONN:			return TEXT("WSAEISCONN");
		case WSAENOTCONN:			return TEXT("WSAENOTCONN");
		case WSAESHUTDOWN:			return TEXT("WSAESHUTDOWN");
		case WSAETOOMANYREFS:		return TEXT("WSAETOOMANYREFS");
		case WSAETIMEDOUT:			return TEXT("WSAETIMEDOUT");
		case WSAECONNREFUSED:		return TEXT("WSAECONNREFUSED");
		case WSAELOOP:				return TEXT("WSAELOOP");
		case WSAENAMETOOLONG:		return TEXT("WSAENAMETOOLONG");
		case WSAEHOSTDOWN:			return TEXT("WSAEHOSTDOWN");
		case WSAEHOSTUNREACH:		return TEXT("WSAEHOSTUNREACH");
		case WSAENOTEMPTY:			return TEXT("WSAENOTEMPTY");
		case WSAEPROCLIM:			return TEXT("WSAEPROCLIM");
		case WSAEUSERS:				return TEXT("WSAEUSERS");
		case WSAEDQUOT:				return TEXT("WSAEDQUOT");
		case WSAESTALE:				return TEXT("WSAESTALE");
		case WSAEREMOTE:			return TEXT("WSAEREMOTE");
		case WSAEDISCON:			return TEXT("WSAEDISCON");
		case WSASYSNOTREADY:		return TEXT("WSASYSNOTREADY");
		case WSAVERNOTSUPPORTED:	return TEXT("WSAVERNOTSUPPORTED");
		case WSANOTINITIALISED:		return TEXT("WSANOTINITIALISED");
		case WSAHOST_NOT_FOUND:		return TEXT("WSAHOST_NOT_FOUND");
		case WSATRY_AGAIN:			return TEXT("WSATRY_AGAIN");
		case WSANO_RECOVERY:		return TEXT("WSANO_RECOVERY");
		case WSANO_DATA:			return TEXT("WSANO_DATA");
		case 0:						return TEXT("WSANO_ERROR");
		default:					return TEXT("WSA_Unknown");
	}
#elif __BSD_SOCKETS__
	if( Code == -1 )
		Code = errno;
	switch( Code )
	{
		case EINTR:					return TEXT("EINTR");
		case EBADF:					return TEXT("EBADF");
		case EACCES:				return TEXT("EACCES");
		case EFAULT:				return TEXT("EFAULT");
		case EINVAL:				return TEXT("EINVAL");
		case EMFILE:				return TEXT("EMFILE");
		case EWOULDBLOCK:			return TEXT("EWOULDBLOCK");
		case EINPROGRESS:			return TEXT("EINPROGRESS");
		case EALREADY:				return TEXT("EALREADY");
		case ENOTSOCK:				return TEXT("ENOTSOCK");
		case EDESTADDRREQ:			return TEXT("EDESTADDRREQ");
		case EMSGSIZE:				return TEXT("EMSGSIZE");
		case EPROTOTYPE:			return TEXT("EPROTOTYPE");
		case ENOPROTOOPT:			return TEXT("ENOPROTOOPT");
		case EPROTONOSUPPORT:		return TEXT("EPROTONOSUPPORT");
		case ESOCKTNOSUPPORT:		return TEXT("ESOCKTNOSUPPORT");
		case EOPNOTSUPP:			return TEXT("EOPNOTSUPP");
		case EPFNOSUPPORT:			return TEXT("EPFNOSUPPORT");
		case EAFNOSUPPORT:			return TEXT("EAFNOSUPPORT");
		case EADDRINUSE:			return TEXT("EADDRINUSE");
		case EADDRNOTAVAIL:			return TEXT("EADDRNOTAVAIL");
		case ENETDOWN:				return TEXT("ENETDOWN");
		case ENETUNREACH:			return TEXT("ENETUNREACH");
		case ENETRESET:				return TEXT("ENETRESET");
		case ECONNABORTED:			return TEXT("ECONNABORTED");
		case ECONNRESET:			return TEXT("ECONNRESET");
		case ENOBUFS:				return TEXT("ENOBUFS");
		case EISCONN:				return TEXT("EISCONN");
		case ENOTCONN:				return TEXT("ENOTCONN");
		case ESHUTDOWN:				return TEXT("ESHUTDOWN");
		case ETOOMANYREFS:			return TEXT("ETOOMANYREFS");
		case ETIMEDOUT:				return TEXT("ETIMEDOUT");
		case ECONNREFUSED:			return TEXT("ECONNREFUSED");
		case ELOOP:					return TEXT("ELOOP");
		case ENAMETOOLONG:			return TEXT("ENAMETOOLONG");
		case EHOSTDOWN:				return TEXT("EHOSTDOWN");
		case EHOSTUNREACH:			return TEXT("EHOSTUNREACH");
		case ENOTEMPTY:				return TEXT("ENOTEMPTY");
		case EUSERS:				return TEXT("EUSERS");
		case EDQUOT:				return TEXT("EDQUOT");
		case ESTALE:				return TEXT("ESTALE");
		case EREMOTE:				return TEXT("EREMOTE");
		case HOST_NOT_FOUND:		return TEXT("HOST_NOT_FOUND");
		case TRY_AGAIN:				return TEXT("TRY_AGAIN");
		case NO_RECOVERY:			return TEXT("NO_RECOVERY");
		case 0:						return TEXT("NO_ERROR");
		default:					return TEXT("Unknown");
	}
#endif
}

/*----------------------------------------------------------------------------
	Helper functions.
----------------------------------------------------------------------------*/

UBOOL IpMatches( sockaddr_in& A, sockaddr_in& B )
{
#if __WINSOCK__
	return	A.sin_addr.S_un.S_addr == B.sin_addr.S_un.S_addr
	&&		A.sin_port             == B.sin_port
	&&		A.sin_family           == B.sin_family;
#elif __BSD_SOCKETS__
	return	A.sin_addr.s_addr      == B.sin_addr.s_addr
	&&		A.sin_port             == B.sin_port
	&&		A.sin_family           == B.sin_family;
#endif
}

void IpGetBytes( in_addr Addr, BYTE& Ip1, BYTE& Ip2, BYTE& Ip3, BYTE& Ip4 )
{
	Ip1 = IP(Addr,1);
	Ip2 = IP(Addr,2);
	Ip3 = IP(Addr,3);
	Ip4 = IP(Addr,4);
}

void IpSetBytes( in_addr& Addr, BYTE Ip1, BYTE Ip2, BYTE Ip3, BYTE Ip4 )
{
	IP(Addr,1) = Ip1;
	IP(Addr,2) = Ip2;
	IP(Addr,3) = Ip3;
	IP(Addr,4) = Ip4;
}

void IpGetInt( in_addr Addr, DWORD& Ip )
{
#if __WINSOCK__
	Ip = Addr.S_un.S_addr;
#elif __BSD_SOCKETS__
	Ip = Addr.s_addr;
#endif
}

void IpSetInt( in_addr& Addr, DWORD Ip )
{
#if __WINSOCK__
	Addr.S_un.S_addr = Ip;
#elif __BSD_SOCKETS__
	Addr.s_addr = Ip;
#endif
}

FString IpString( in_addr Addr, INT Port )
{
	guard(IpString);
	FString Result = FString::Printf( TEXT("%i.%i.%i.%i"), IP(Addr,1), IP(Addr,2), IP(Addr,3), IP(Addr,4) );
	if( Port )
		Result += FString::Printf( TEXT(":%i"), Port );
	return Result;
	unguard;
}

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/
