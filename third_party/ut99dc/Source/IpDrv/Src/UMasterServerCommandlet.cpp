/*=============================================================================
	MasterServer.cpp: Unreal Engine Master Server
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* (4/14/99) Created by Brandon Reinhart
	* (4/15/99) Ported to UCC Commandlet interface by Brandon Reinhart

Todo:
	* Gspy style interface could be expanded to complete compliance
	  (allows for custom queries)
=============================================================================*/

#include "IpDrvPrivate.h"

/*-----------------------------------------------------------------------------
	UMasterServerCommandlet.
-----------------------------------------------------------------------------*/

extern void GenerateSecretKey( BYTE* key, const TCHAR *GameName );
extern void gs_encrypt(BYTE *buffer_ptr, INT buffer_len, BYTE *key);
extern void gs_encode(BYTE *ins, INT size, BYTE *result);

class UMasterServerCommandlet : public UCommandlet
{
	DECLARE_CLASS(UMasterServerCommandlet, UCommandlet, CLASS_Transient);

	FSocketData ListenSocket;
	FOperationStats OpStats;

	INT MASTER_TIMEOUT;

	// Output
	FString GameName;
	FString OpMode;

	// TextFile Mode
	FString OutputFileName;

	// TCPLink Mode
	INT TCPPort;
	FSocketData TCPSocket;
	SOCKET Connections[100];
	INT ConnectionTimer[100];
	INT ConnectionCount;

	// Server Map
	TMap<FString, FString> ValidationMap;		// Servers awaiting validation.
	TMap<FString, FString> MasterMap;			// Servers in the master list.
	INT NumServers;
	DOUBLE Last10Seconds, Last300Seconds;
	INT IgnoredValidations;
	INT RejectedValidations;

	void InitSockets( const TCHAR* ConfigFileName )
	{
		guard(InitSockets);

		GWarn->Logf( TEXT("!! Initializing sockets.") );

		IgnoredValidations = 0;
		RejectedValidations = 0;

		ConnectionCount = 0;
		for (INT i=0; i<100; i++)
			ConnectionTimer[i] = 0;

		GConfig->GetInt( TEXT("MasterServer"), TEXT("ListenPort"), ListenSocket.Port, ConfigFileName );
		ListenSocket.Socket = INVALID_SOCKET;

		// Initialize sockets.
		FString Error;
		::InitSockets( Error );

		// Create a UDP socket.
		ListenSocket.Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (ListenSocket.Socket == INVALID_SOCKET) {
			GWarn->Logf( TEXT("   Failed to create UDP socket.") );
			return;
		}

		// Bind the socket.
		ListenSocket.Addr.sin_family = PF_INET;
		ListenSocket.Addr.sin_port = htons(ListenSocket.Port);
		IpSetInt(ListenSocket.Addr.sin_addr, INADDR_ANY);
		if (bind(ListenSocket.Socket, (LPSOCKADDR)&ListenSocket.Addr, sizeof(struct sockaddr)) == SOCKET_ERROR) {
			GWarn->Logf( TEXT("   Failed to bind UDP socket.") );
			ListenSocket.Socket = INVALID_SOCKET;
			return;
		}

		GWarn->Logf( TEXT("   UDP socket bound at port %i"), ListenSocket.Port );

		if ( OpMode == TEXT("TCPLink") )
		{
			// Init the socket.
			TCPSocket.Socket = INVALID_SOCKET;
			TCPSocket.Port = TCPPort;

			// Create a TCP socket.
			TCPSocket.Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (TCPSocket.Socket == INVALID_SOCKET) {
				GWarn->Logf( TEXT("   Failed to create TCP socket.") );
				return;
			}

			// Bind the socket.
			TCPSocket.Addr.sin_family = PF_INET;
			TCPSocket.Addr.sin_port = htons(TCPSocket.Port);
			IpSetInt(TCPSocket.Addr.sin_addr, INADDR_ANY);
			if (bind(TCPSocket.Socket, (LPSOCKADDR)&TCPSocket.Addr, sizeof(struct sockaddr)) == SOCKET_ERROR) {
				GWarn->Logf( TEXT("   Failed to bind TCP socket.") );
				TCPSocket.Socket = INVALID_SOCKET;
				return;
			}
			GWarn->Logf( TEXT("   TCP socket bound at port %i"), TCPSocket.Port );

			if (listen( TCPSocket.Socket, SOMAXCONN ) == SOCKET_ERROR) {
				GWarn->Logf( TEXT("  Failed to listen on TCP socket.") );
				TCPSocket.Socket = INVALID_SOCKET;
				return;
			}
			GWarn->Logf( TEXT("   Listening on TCP socket.") );
		}

		unguard;
	}

	UBOOL ConsoleReadInput() 
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
				default:
					return 1;
			}
		}
		return 1;
#endif

		return 1;
		unguard;
	}

	void ListenSockets()
	{
		guard(ListenSockets);

		// Check for heartbeats...
		INT SocketStatus = 1;
		while ( SocketStatus == 1 )
		{
			ANSICHAR Buffer[1024];
			sockaddr_in FromAddr;
			socklen_t FromSize = sizeof(FromAddr);
			fd_set SocketSet;
			TIMEVAL SelectTime = {0, 0};
			FD_ZERO( &SocketSet );
			FD_SET( ListenSocket.Socket, &SocketSet );
			SocketStatus = select(0, &SocketSet, 0, 0, &SelectTime );
			if (SocketStatus == 1)
			{
				INT BytesReceived = recvfrom( ListenSocket.Socket, Buffer, sizeof(Buffer), 0, (LPSOCKADDR)&FromAddr, &FromSize );
				if( BytesReceived != SOCKET_ERROR ) {
					// Received data.
					OpStats.BytesReceived += BytesReceived;
					Buffer[BytesReceived] = 0;
					FString Message = FString(appFromAnsi((ANSICHAR*)Buffer));
					ServiceMessage( Message, &FromAddr );
				} else {
					GWarn->Logf( TEXT("!! Error while polling socket: %i"), WSAGetLastError() );
				}
			}
		}

		// Check for queued connections...
		if ( OpMode == TEXT("TCPLink") )
		{
			TIMEVAL SelectTime = {1, 0};
			fd_set SocketSet;
			FD_ZERO( &SocketSet );
			FD_SET( TCPSocket.Socket, &SocketSet );
			SocketStatus = select( 0, &SocketSet, 0, 0, &SelectTime );
			if ( SocketStatus == SOCKET_ERROR )
			{
				GWarn->Logf( TEXT("!! Error checking socket status: %i"), WSAGetLastError());
				return;
			} else if ( SocketStatus == 0 ) {
				// Nothing waiting.
				return;
			}
			socklen_t i = sizeof(SOCKADDR);
			SOCKADDR_IN ForeignHost;
			INT IncomingSocket = accept( TCPSocket.Socket, (LPSOCKADDR) &ForeignHost, &i );
			if ( IncomingSocket == INVALID_SOCKET )
			{
				GWarn->Logf( TEXT("!! Failed to accept queued connection: %i"), WSAGetLastError() );
				return;
			}
			INT MapCount = 0;
			for ( TMap<FString, FString>::TIterator It(MasterMap); It; ++It )
				MapCount++;
			INT SendSize = 30*MapCount + 1024, SizeSize=sizeof(SendSize);
			setsockopt( IncomingSocket, SOL_SOCKET, SO_SNDBUF, (char*)&SendSize, SizeSize );
			Connections[ConnectionCount++] = IncomingSocket;
			if (ConnectionCount > 100)
				ConnectionCount = 0;

			// We have a new connection, so lets send it an initial message.
			FString HelloMessage = FString::Printf( TEXT("\\basic\\\\secure\\wookie") );
			send( IncomingSocket, appToAnsi(*HelloMessage), HelloMessage.Len(), 0 );
		}

		unguard;
	}

	void GSValidate( FString* ValidationString, FString* const ValidationResult, FString* ValidateGameName )
	{
		guard(GSValidate);

		const INT ValidateSize = 6;
		BYTE SecretKey[7];
		GenerateSecretKey( SecretKey, **ValidateGameName );
		BYTE EncryptedString[ValidateSize];
		BYTE EncodedString[(ValidateSize * 4) / 3 + 1];

		BYTE* Pos = EncryptedString;
		const TCHAR* Tmp = **ValidationString;
		while( *Tmp )
			*Pos++ = *Tmp++;

		gs_encrypt( EncryptedString, ValidateSize, SecretKey );
		gs_encode( EncryptedString, ValidateSize, EncodedString );
		*(FString*)ValidationResult = appFromAnsi((ANSICHAR*)EncodedString);

		unguard;
	}

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

	void ServiceMessage( FString& Message, sockaddr_in* FromAddr )
	{
		guard(ServiceMessage);

		OpStats.MessagesServiced++;

		FString Key;
		while ( GetNextKey( &Message, &Key ) )
		{
			if (Key.Len() > 0)
			{
				if (Key == TEXT("heartbeat"))
				{
					FString PortID;
					GetNextKey( &Message, &PortID );
					DoHeartbeat( FromAddr, PortID );
				} else if (Key == TEXT("gamename")) {
					// Fixme: Reject none gamename servers?
				} else if (Key == TEXT("validate")) {
					FString ChallengeResponse;
					GetNextKey( &Message, &ChallengeResponse );
					DoValidate( FromAddr, ChallengeResponse );
				}
			}

			Key.Empty();
		}

		unguard;
	}

	void DoHeartbeat( sockaddr_in* FromAddr, FString& PortID )
	{
		guard(DoHeartbeat);

		// Derive the FromAddr string.
		INT PortNum = FromAddr->sin_port;
		FString FromAddrString = IpString(FromAddr->sin_addr) + TEXT(":") + FString::Printf( TEXT("%i"), PortNum );

		// Find out if this guy is already in the MasterMap.
		if ( MasterMap.Find( FromAddrString ) != NULL )
		{
			// Yeah, he's there.  Let's reset his timer.
			FString StoreString = FString::Printf( TEXT("%s\\%i"), *PortID, MASTER_TIMEOUT );
			MasterMap.Set( *FromAddrString, *StoreString );
			return;
		}

		INT Port = appAtoi(*PortID);
		if (Port == 0) {
			GWarn->Logf( TEXT("< Unknown Error Processing Port >") );
			return;
		}

		// Need to send this guy a validation query.
		FString ValidationChallenge = FString::Printf( TEXT("\\basic\\\\secure\\") );
		FString ValidationKey;
		for ( INT i=0; i<6; i++ )
		{
			FLOAT RandIndex = appFrand();
			INT CharVal = 0;
			if (RandIndex < 0.18)
			{
				RandIndex = appFrand();
				CharVal = 57 - appFloor(9 * RandIndex);
			} else if (RandIndex < 0.43) {
				RandIndex = appFrand();
				CharVal = 90 - appFloor(25 * RandIndex);
			} else {
				RandIndex = appFrand();
				CharVal = 123 - appFloor(25 * RandIndex);
			}
			TCHAR C[2];
			C[0] = CharVal;
			C[1] = 0;
			ValidationKey = ValidationKey + (const TCHAR*)C;
		}
		sockaddr_in ToAddr;
		appMemcpy(&ToAddr, FromAddr, sizeof(sockaddr_in));
		ToAddr.sin_port = PortNum;
		ValidationChallenge = ValidationChallenge + ValidationKey;
		INT BytesSent = sendto( ListenSocket.Socket, appToAnsi(*ValidationChallenge), ValidationChallenge.Len(), 0, (sockaddr*)&ToAddr, sizeof(ToAddr) );
		if (BytesSent == SOCKET_ERROR)
			GWarn->Logf( TEXT("ServiceMessage: Failed to send ValidationChallenge.") );
		OpStats.BytesSent += BytesSent;

		// Store this dude in the validation map.
		FString StoreString = FString::Printf( TEXT("%s\\%s"), *ValidationKey, *PortID );
		ValidationMap.Set( *FromAddrString, *StoreString );

		unguard;
	}

	void DoValidate( sockaddr_in* FromAddr, FString& ChallengeResponse )
	{
		guard(DoValidate);

		if ( ChallengeResponse.Len() != 8 )
			return;

		// Derive the FromAddr string.
		INT PortNum = FromAddr->sin_port;
		FString FromAddrString = IpString(FromAddr->sin_addr) + TEXT(":") + FString::Printf( TEXT("%i"), PortNum );

		FString* ValidationKey = ValidationMap.Find( FromAddrString );
		if (( ValidationKey == NULL ) || ( ValidationKey->Len() == 0 ))
		{
			// This guy is asking for validation, but never sent a heartbeat.  Just ignore.
			IgnoredValidations++;
			return;
		}

		// Split the key.
		FString ValidationChallenge = ValidationKey->Left(6);
		FString PortID = ValidationKey->Right(ValidationKey->Len() - 7);

		FString ValidationResult;
		GSValidate( &ValidationChallenge, &ValidationResult, &GameName );
		if (  ValidationResult != ChallengeResponse )
		{
			FString OldVer(TEXT("oldver"));
			GSValidate( &ValidationChallenge, &ValidationResult, &OldVer );
		}
		if (  ValidationResult == ChallengeResponse )
		{
			// This guy is legit.  Add him to the MasterMap.
			FString StoreString = FString::Printf( TEXT("%s\\%i"), *PortID, MASTER_TIMEOUT );
			if( MasterMap.Find( FromAddrString ) == NULL )
				NumServers++;
			MasterMap.Set( *FromAddrString, *StoreString );
		} else
			RejectedValidations++;
		ValidationMap.Remove( *FromAddrString );

		unguard;
	}

	void PollConnections()
	{
		guard(PollConnections);

		INT DeadSockets[100];
		INT SocketCount = 0;

		INT i;
		for ( i=0; i<100; i++ )
			DeadSockets[i] = 0;

		for ( i=0; i<100; i++ )
		{
			if (Connections[i] != 0)
			{
				char Buf[240];

				appMemset(Buf, 0, sizeof(Buf));

				// Check for a message...
				fd_set SocketSet;
				TIMEVAL SelectTime = {0, 0};
				FD_ZERO( &SocketSet );
				FD_SET( Connections[i], &SocketSet );
				INT SocketStatus = select( 0, &SocketSet, 0, 0, &SelectTime );
				if (SocketStatus == 1)
				{
					INT BytesReceived = recv( Connections[i], Buf, sizeof(Buf), 0 );
					if ( BytesReceived == 0 )
					{
						// Disconnect condition met.
						closesocket(Connections[i]);
						DeadSockets[SocketCount++] = i;
					} else if ( BytesReceived != SOCKET_ERROR )	{
						// Real message.  Service.
						OpStats.BytesReceived += BytesReceived;
						Buf[BytesReceived] = 0;
						TCHAR TBuf[240];
						for ( INT j=0; j<240; j++ )
							TBuf[j] = FromAnsi(Buf[j]);
						FString Message = FString::Printf( TEXT("%s"), TBuf );
						ServiceTCPMessage( Message, i );
					} else {
						// Socket error, sever the socket.
						closesocket(Connections[i]);
						DeadSockets[SocketCount++] = i;
					}
				}			
			}
		}

		// Remove all dead sockets.
		for ( i=0; i<100; i++ )
			if (DeadSockets[i] > 0)
				Connections[DeadSockets[i]] = 0;

		unguard;
	}

	void ServiceTCPMessage( FString& Message, INT Index )
	{
		guard(ServiceTCPMessage);

		OpStats.MessagesServiced++;

		FString Key;
		while ( GetNextKey( &Message, &Key ) )
		{
			if (Key.Len() > 0)
			{
				if (Key == TEXT("list"))
				{
					// Ah, they want a list of servers.
					for ( TMap<FString, FString>::TIterator It(MasterMap); It; ++It )
					{
						FString ListEntry = FString::Printf( TEXT("\\ip\\") );

						// Get the port.
						INT Pos = It.Value().InStr( TEXT("\\") );
						FString PortID = It.Value().Left(Pos);

						// Get the IP address.
						Pos = It.Key().InStr( TEXT(":") );
						FString IPAddr = It.Key().Left(Pos);

						ListEntry = ListEntry + IPAddr + FString::Printf(TEXT(":")) + PortID;

						INT BytesSent = send( Connections[Index], appToAnsi(*ListEntry), ListEntry.Len(), 0 );
						if ( BytesSent != SOCKET_ERROR )
							OpStats.BytesSent += BytesSent;
					}
					// Terminate the list.
					FString Final = FString::Printf( TEXT("\\final\\") );
					send( Connections[Index], appToAnsi(*Final), Final.Len(), 0 );
					closesocket( Connections[Index] );
					Connections[Index] = 0;
				}
			}

			Key.Empty();
		}

		unguard;
	}

	void PurgeValidationMap()
	{
		guard(PurgeValidationMap);

		ValidationMap.Empty();

		unguard;
	}

	void PurgeMasterMap()
	{
		guard(PurgeMasterMap);

		TArray<FString> PurgeKeys;

		// First pass, update times and note elements that need to be removed.
		for ( TMap<FString, FString>::TIterator It(MasterMap); It; ++It )
		{
			// Split the value.
			INT     Pos    = It.Value().InStr( TEXT("\\") );
			FString PortID = It.Value().Left(Pos);
			FString Time   = It.Value().Right( It.Value().Len() - (PortID.Len() + 1) );

			// Decrement the time.
			INT DecayTime = appAtoi(*Time);
			DecayTime -= 10;
			
			// Now update the entry.
			It.Value() = FString::Printf( TEXT("%s\\%i"), *PortID, DecayTime );

			// Check to see if this one needs to be removed.
			if (DecayTime <= 0)
				new(PurgeKeys)FString( It.Key() );
		}

		// Second pass, outside of the TIterator, remove all marked keys.
		for ( INT i=0; i<PurgeKeys.Num(); i++ )
		{
			MasterMap.Remove( *PurgeKeys(i) );
			NumServers--;
		}

		unguard;
	}

	void WriteMasterMap()
	{
		guard(WriteMasterMap);

		GFileManager->Delete( *OutputFileName );
		FString OutputString = FString::Printf( TEXT("") );
		for ( TMap<FString, FString>::TIterator It(MasterMap); It; ++It )
		{
			INT     Pos    = It.Value().InStr( TEXT("\\") );
			FString PortID = It.Value().Left(Pos);
			INT     Port   = appAtoi(*PortID);

			OutputString = FString::Printf( TEXT("%s%s %i %i\r\n"), *OutputString, *It.Key(), Port, Port+1 );
		}
		appSaveStringToFile( OutputString, *OutputFileName );

		unguard;
	}

	void CleanUp()
	{
		guard(CleanUp);

		GWarn->Logf( TEXT("!! Cleaning up and exiting.") );
		closesocket(ListenSocket.Socket);
		GWarn->Logf( TEXT("!! Session statistics.") );
		GWarn->Logf( TEXT("   Messages Serviced: %i"), OpStats.MessagesServiced );
		GWarn->Logf( TEXT("   Bytes Received:    %i"), OpStats.BytesReceived );
		GWarn->Logf( TEXT("   Bytes Sent:        %i"), OpStats.BytesSent );

		unguard;
	}

	// Main.
	INT Main( const TCHAR* Parms )
	{
		guard(UMasterServerCommandlet::Main);

		// Banner
		GWarn->Logf( TEXT("UMasterServerCommandlet: Unreal Engine Master Server") );
		GWarn->Logf( TEXT("Copyright 1999 Epic Games, Inc.\n\r") );

		// Get config file.
		FString ConfigFileName = FString::Printf(TEXT("MasterServer.ini"));
		TCHAR Token[256];
		if( ParseToken( Parms, Token, ARRAY_COUNT(Token), 0 ) )
			ConfigFileName = Token;
		GWarn->Logf( TEXT("!! Loading Config: %s"), *ConfigFileName );

		/*
		 * Load config sections...
		 */

		// [MasterServer]
		GConfig->GetString( TEXT("MasterServer"), TEXT("GameName"), GameName, *ConfigFileName );
		GWarn->Logf( TEXT("   GameName: %s"), *GameName );
		GConfig->GetString( TEXT("MasterServer"), TEXT("OperationMode"), OpMode, *ConfigFileName );
		GWarn->Logf( TEXT("   Operation Mode: %s"), *OpMode );

		// [TextFile]
		GWarn->Logf( TEXT("!! Loading TextFile Operation Mode Defaults") );
		GConfig->GetString( TEXT("TextFile"), TEXT("OutputFile"), OutputFileName, *ConfigFileName );
		GWarn->Logf( TEXT("   TextFile Mode Output: %s"), *OutputFileName );

		// [TCPLink]
		GWarn->Logf( TEXT("!! Loading TCPLink Operation Mode Defaults") );
		GConfig->GetInt( TEXT("TCPLink"), TEXT("TCPPort"), TCPPort, *ConfigFileName );
		GWarn->Logf( TEXT("   TCPLink Mode Service Port: %i"), TCPPort );

		// Initialize sockets.
		InitSockets(*ConfigFileName);

		// Get the time.
		Last10Seconds  = appSeconds();
		Last300Seconds = appSeconds();

		MASTER_TIMEOUT = 600;

		// Listen and service.
		GWarn->Logf( TEXT("!! Listening for and servicing messages.") );
		NumServers = 0;
		while (ConsoleReadInput()) {
			ListenSockets();

			DOUBLE CurrentTime = appSeconds();
			if ( CurrentTime - Last300Seconds > 300 )
			{
				// Every 300 seconds, purge the ValdiationMap.
				// This ensures memory doesn't fill up with stale validation attempts in the long run.
				PurgeValidationMap();
				Last300Seconds = CurrentTime;
			}

			if ( CurrentTime - Last10Seconds > 10 )
			{
				// Every 10 seconds, purge the MasterMap.
				PurgeMasterMap();
				Last10Seconds = CurrentTime;

				// Print report.
				INT VMapCount = 0;
				for ( TMap<FString, FString>::TIterator It(ValidationMap); It; ++It )
					VMapCount++;
				GWarn->Serialize( *FString::Printf( TEXT("Approved: %i Pending: %i Ignored: %i Rejected: %i        "), NumServers, VMapCount, IgnoredValidations, RejectedValidations ), NAME_Progress );

				// Write out the server list.
				if ( OpMode == TEXT("TextFile") )
					WriteMasterMap();

				// Check a connection to see if its timed out.
				for (INT i=0; i<100; i++)
					if (Connections[i] != 0)
					{
						ConnectionTimer[i] += 10;
						if (ConnectionTimer[i] > 20)
						{
							closesocket(Connections[i]);
							Connections[i] = 0;
							ConnectionTimer[i] = 0;
						}
					}
			}

			if ( OpMode == TEXT("TCPLink") )
				PollConnections();
		}

		// Clean up.
		CleanUp();

		unguard;
		return 1;
	}
};
IMPLEMENT_CLASS(UMasterServerCommandlet)

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
