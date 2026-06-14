/*=============================================================================
	UUpdateServerCommandlet.cpp: Unreal Engine Update Server
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Brandon Reinhart
	* Ported to UCC Commandlet by Brandon Reinhart
	* Updated for portability by Brandon Reinhart
=============================================================================*/

#include "IpDrvPrivate.h"

/*-----------------------------------------------------------------------------
	Types.
-----------------------------------------------------------------------------*/

// VerData.
struct FVerData
{
	INT VersionNum;
	INT Day, Month, Year;
	INT Size;
};

// MirrorData
struct FMirrorData
{
	INT NumMirrors;
	FString MirrorName[32];
	FString MirrorURL[32];
};

/*-----------------------------------------------------------------------------
	UUpdateServerCommandlet.
-----------------------------------------------------------------------------*/

class UUpdateServerCommandlet : public UCommandlet
{
	DECLARE_CLASS(UUpdateServerCommandlet, UCommandlet, CLASS_Transient);

	// System
	FSocketData Socket;
	FOperationStats OpStats;

	// Key/Response
	TMap<FString, FString> Pairs;

	FArchive* Log;

	UBOOL GetNextKey( FString* Message, FString* Result )
	{
		guard(GetNextKey);

		UBOOL bFoundStart = 0;
		FString MessageRest;
		for (INT i=0; i<Message->Len(); i++)
		{
			if (bFoundStart)
			{
				if ( Message->Mid(i, 1) == TEXT("\\") )
				{
					MessageRest = Message->Right(Message->Len() - i);
					*Message = MessageRest;
					return 1;
				} else {
					*Result = *Result + Message->Mid(i, 1);
				}
			} else {
				if ( Message->Mid(i, 1) == TEXT("\\") )
				{
					bFoundStart = 1;
				}
			}
		}
		return 0;

		unguard;
	}

	INT SendResponse( FString Key, sockaddr_in* FromAddr )
	{
		guard(SendResponse);

		FString* Response = Pairs.Find( Key );
		if (Response != NULL)
		{
			INT SentBytes = sendto( Socket.Socket, (char*)appToAnsi(**Response), sizeof(ANSICHAR)*Response->Len(), 0, (sockaddr*)FromAddr, sizeof(*FromAddr) );
			if ( SentBytes == 0 )
				GWarn->Logf( TEXT("Error: Error sending response.") );
			return SentBytes;
		}
		return 0;

		unguard;
	}

	FString GetIpAddress( sockaddr_in* FromAddr )
	{
		guard(GetIpAddress);

		return IpString(FromAddr->sin_addr);

		unguard;
	}

	void ServiceMessage( FString Message, sockaddr_in* FromAddr )
	{
		guard(ServiceMessage);

		OpStats.MessagesServiced++;

		FString Result;
		while ( GetNextKey( &Message, &Result ) )
		{
			if (Result.Len() > 0)
			{
				if ( Result == TEXT("log") )
				{
					Result.Empty();
					GetNextKey( &Message, &Result );
					FString LogString;
					LogString = 
						FString::Printf(TEXT("%s"), appTimestamp()) + 
						FString::Printf(TEXT("|")) + 
						GetIpAddress(FromAddr) + 
						FString::Printf(TEXT("|")) +
						Result +
						FString::Printf(TEXT("\r\n"));
					Log->Serialize( (void*) appToAnsi(*LogString), LogString.Len()*sizeof(ANSICHAR) );
				} else
					OpStats.BytesSent += SendResponse(Result, FromAddr);
			}

			Result.Empty();
		}

		unguard;
	}

	void InitSockets( const TCHAR* ConfigFileName )
	{
		guard(InitSockets);

		GWarn->Logf( TEXT("Init: Initializing sockets.") );

		GConfig->GetInt( TEXT("UpdateServer"), TEXT("Port"), Socket.Port, ConfigFileName );
		Socket.Socket = INVALID_SOCKET;

		// Initialize sockets.
		FString Error;
		::InitSockets( Error );

		// Create a UDP socket.
		Socket.Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (Socket.Socket == INVALID_SOCKET) {
			GWarn->Logf( TEXT("  Failed to create UDP socket.") );
			return;
		}

		// Bind the socket.
		Socket.Addr.sin_family = PF_INET;
		Socket.Addr.sin_port = htons(Socket.Port);
		IpSetInt(Socket.Addr.sin_addr, INADDR_ANY);
		if (bind(Socket.Socket, (sockaddr*)&Socket.Addr, sizeof(struct sockaddr)) == SOCKET_ERROR) {
			GWarn->Logf( TEXT("  Failed to bind UDP socket.") );
			Socket.Socket = INVALID_SOCKET;
			return;
		}

		GWarn->Logf( TEXT("  UDP socket bound at port %i"), Socket.Port );

		unguard;
	}

	UBOOL ConsoleReadInput( const TCHAR* ConfigFileName ) 
	{
		guard(ConsoleReadInput);

#if _MSC_VER
		char c;

		if (_kbhit()) {
			c = _getch();
			switch (c) {
				case 'q':
				case 'Q':
					return 0;
				case 'r':
				case 'R':
					// Reload .ini settings.
					ReloadSettings( ConfigFileName );
					return 1;
				default:
					return 1;
			}
		}
		return 1;
#endif
		unguard;
	}

	void ListenSockets()
	{
		guard(ListenSockets);

		ANSICHAR Buffer[1024];
		sockaddr_in FromAddr;
		socklen_t FromSize = sizeof(FromAddr);
		fd_set SocketSet;
		TIMEVAL SelectTime = {1, 0};
		FD_ZERO( &SocketSet );
		FD_SET( Socket.Socket, &SocketSet );
		INT SocketStatus = select( 0, &SocketSet, 0, 0, &SelectTime );
		if (SocketStatus == 1)
		{
			INT BytesReceived = recvfrom( Socket.Socket, Buffer, sizeof(Buffer), 0, (LPSOCKADDR)&FromAddr, &FromSize );
			if( BytesReceived != SOCKET_ERROR ) {
				// Received data.
				OpStats.BytesReceived += BytesReceived;
				Buffer[BytesReceived] = 0;
				FString Message = FString(appFromAnsi((ANSICHAR*)Buffer));
				ServiceMessage( Message, &FromAddr );
			} else {
				GWarn->Logf( TEXT("!! Error while polling socket: %i"), SocketError() );
				return;
			}
		}
		unguard
	}

	void ReadKeyResponses( const TCHAR* ConfigFileName )
	{
		guard(ReadKeyResponses);

		GWarn->Logf( TEXT("Init: Reading key/response pairs.") );

		Pairs.Empty();
		INT NumPairs = 0;
		GConfig->GetInt( TEXT("KeyResponsePairs"), TEXT("NumPairs"), NumPairs, ConfigFileName );
		GWarn->Logf( TEXT("Init: Reading %i pairs."), NumPairs );
		for (INT i=0; i<NumPairs; i++)
		{
			TCHAR Buffer[256];
			FString Key, Response;

			// Get i'th key.
			appSprintf( Buffer, TEXT("Key(%i)"), i );
			GConfig->GetString( TEXT("KeyResponsePairs"), Buffer, Key, ConfigFileName );

			// Get i'th response.
			appSprintf( Buffer, TEXT("Response(%i)"), i );
			GConfig->GetString( TEXT("KeyResponsePairs"), Buffer, Response, ConfigFileName );

			Pairs.Set( *Key, *Response );
		}
		unguard;
	}

	void CleanUp()
	{
		guard(CleanUp);

		GWarn->Logf( TEXT("Status: Cleaning up and exiting.") );
		closesocket(Socket.Socket);
		GWarn->Logf( TEXT("===================") );
		GWarn->Logf( TEXT("Session statistics.") );
		GWarn->Logf( TEXT("  Messages Serviced: %i"), OpStats.MessagesServiced );
		GWarn->Logf( TEXT("  Bytes Received:    %i"), OpStats.BytesReceived );
		GWarn->Logf( TEXT("  Bytes Sent:        %i"), OpStats.BytesSent );

		unguard;
	}

	void ReloadSettings( const TCHAR* ConfigFileName )
	{
		guard(ReloadSettings);

		GWarn->Logf( TEXT("Status: Reloading settings.") );
		ReadKeyResponses( ConfigFileName );

		unguard;
	}

	// Main.
	INT Main( const TCHAR* Parms )
	{
		guard(UUpdateServerCommandlet::Main);

		// Get config file.
		FString ConfigFileName = FString::Printf(TEXT("UpdateServer.ini"));
		TCHAR Token[256];
		if( ParseToken( Parms, Token, ARRAY_COUNT(Token), 0 ) )
			ConfigFileName = Token;
		GWarn->Logf( TEXT("Init: Config File: %s"), *ConfigFileName );

		FString LogFileName;
		GConfig->GetString( TEXT("UpdateServer"), TEXT("LogFile"), LogFileName, *ConfigFileName );
		Log = GFileManager->CreateFileWriter( *LogFileName, FILEWRITE_Unbuffered|FILEWRITE_Append );

		// Read all keys and responses.
		ReadKeyResponses( *ConfigFileName );

		// Initialize sockets.
		InitSockets( *ConfigFileName );

		// Listen and service.
		GWarn->Logf( TEXT("Status: Listening for and servicing messages.") );
		while (ConsoleReadInput( *ConfigFileName )) {
			ListenSockets();
		}

		// Clean up.
		CleanUp();

		delete Log;

		unguard;
		return 1;
	}
};
IMPLEMENT_CLASS(UUpdateServerCommandlet)

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
