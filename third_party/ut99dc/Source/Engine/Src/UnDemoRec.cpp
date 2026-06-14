/*=============================================================================
	DemoRecDrv.cpp: Unreal demo recording network driver.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Jack Porter.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"

#define PACKETSIZE 512

/*-----------------------------------------------------------------------------
	UDemoRecConnection.
-----------------------------------------------------------------------------*/

void UDemoRecConnection::StaticConstructor()
{
	guard(UDemoRecConnection::StaticConstructor);
	unguard;
}
UDemoRecConnection::UDemoRecConnection( UNetDriver* InDriver, const FURL& InURL )
: UNetConnection( InDriver, InURL )
{
	guard(UDemoRecConnection::UDemoRecConnection);
	MaxPacket   = PACKETSIZE;
	InternalAck = 1;
	unguard;
}
UDemoRecDriver* UDemoRecConnection::GetDriver()
{
	return (UDemoRecDriver *)Driver;
}
FString UDemoRecConnection::LowLevelGetRemoteAddress()
{
	guard(UDemoRecConnection::LowLevelGetRemoteAddress);
	return TEXT("");
	unguard;
}
void UDemoRecConnection::LowLevelSend( void* Data, INT Count )
{
	guard(UDemoRecConnection::LowLevelSend);
	if( !GetDriver()->ServerConnection )
	{
		*GetDriver()->FileAr << GetDriver()->FrameNum << Driver->Time << Count;
		GetDriver()->FileAr->Serialize( Data, Count );
		//!!if GetDriver()->GetFileAr()->IsError(), print error, cancel demo recording
	}
	unguard;
}
FString UDemoRecConnection::LowLevelDescribe()
{
	guard(UDemoRecConnection::Describe);
	return TEXT("Demo recording driver connection");
	unguard;
}
INT UDemoRecConnection::IsNetReady( UBOOL Saturate )
{
	return 1;
}
void UDemoRecConnection::FlushNet()
{
	// in playback, there is no data to send except
	// channel closing if an error occurs.
	if( !GetDriver()->ServerConnection )
		Super::FlushNet();
}
void UDemoRecConnection::HandleClientPlayer( APlayerPawn* Pawn )
{
	guard(UDemoRecConnection::HandleClientPlayer);
	if( GetDriver()->ClientThirdPerson )
	{
		guard(SpawnSpectator);
		if(	GetDriver()->DemoSpectatorClass==TEXT("") )
			GetDriver()->DemoSpectatorClass = TEXT("Engine.Spectator");

		UClass* SpectatorClass = StaticLoadClass( APlayerPawn::StaticClass(), NULL, *GetDriver()->DemoSpectatorClass, NULL, LOAD_NoFail, NULL );
		check(SpectatorClass);

		FVector Location(0,0,0);
		FRotator Rotation(0,0,0);

		guard(FindPlayerStart);
		for( INT i=0; i<Pawn->XLevel->Actors.Num(); i++ )
		{
			if( Pawn->XLevel->Actors(i) && Pawn->XLevel->Actors(i)->IsA(APlayerStart::StaticClass()) )
			{
				Location = Pawn->XLevel->Actors(i)->Location;
				Rotation = Pawn->XLevel->Actors(i)->Rotation;
				break;
			}
		}
		unguard;

		guard(SpawnDemoSpectator);
		Pawn = CastChecked<APlayerPawn>(Pawn->XLevel->SpawnActor( SpectatorClass, NAME_None, NULL, NULL, Location, Rotation, NULL, 1, 0 ));
		check(Pawn);
		check(Pawn->XLevel->Engine->Client);
		check(Pawn->XLevel->Engine->Client->Viewports.Num());
		guard(AssignPlayer);
		UViewport* Viewport = Pawn->XLevel->Engine->Client->Viewports(0);
		Viewport->Actor->Player = NULL;
		Pawn->SetPlayer( Viewport );
		Pawn->Role	    = ROLE_Authority;
		Pawn->ShowFlags = SHOW_Backdrop | SHOW_Actors | SHOW_PlayerCtrl | SHOW_RealTime;
		Pawn->RendMap   = REN_DynLight;
		Pawn->bAdmin    = 1;
		Pawn->bNetOwner = 1;
		Pawn->Physics   = PHYS_Flying;
		Viewport->Input->ResetInput();
		unguard;
		unguard;
		unguard;

		// Mark this connection as open.
		State = USOCK_Open;
		Actor = Pawn;
	}
	else
		Super::HandleClientPlayer(Pawn);

	// Setup music for demo
	Pawn->Song        = Pawn->Level->Song;
	Pawn->SongSection = Pawn->Level->SongSection;
	Pawn->CdTrack     = Pawn->Level->CdTrack;
	Pawn->Transition  = MTRAN_Fade;

	check(Pawn->XLevel->Engine->Client);
	check(Pawn->XLevel->Engine->Client->Viewports.Num());

	if(		Pawn->XLevel->Engine->Client->Viewports(0)->Console
		&&	Pawn->XLevel->Engine->Client->Viewports(0)->Console->IsTimeDemo() )	
		GetDriver()->NoFrameCap = 1;	

	unguard;
}
IMPLEMENT_CLASS(UDemoRecConnection);

/*-----------------------------------------------------------------------------
	UDemoRecDriver.
-----------------------------------------------------------------------------*/

UDemoRecDriver::UDemoRecDriver()
{}
UBOOL UDemoRecDriver::InitBase( UBOOL Connect, FNetworkNotify* InNotify, FURL& ConnectURL, FString& Error )
{
	guard(UDemoRecDriver::Init);

	DemoFilename   = ConnectURL.Map;
	Time           = 0;
	FrameNum       = 0;

	return 1;
	unguard;
}
UBOOL UDemoRecDriver::InitConnect( FNetworkNotify* InNotify, FURL& ConnectURL, FString& Error )
{
	guard(UDemoRecDriver::InitConnect);
	if( !Super::InitConnect( InNotify, ConnectURL, Error ) )
		return 0;
	if( !InitBase( 1, InNotify, ConnectURL, Error ) )
		return 0;

	// Playback, local machine is a client, and the demo stream acts "as if" it's the server.
	ServerConnection = new UDemoRecConnection( this, ConnectURL );
	ServerConnection->CurrentNetSpeed = 1000000;
	ServerConnection->State        = USOCK_Pending;
	FileAr                         = GFileManager->CreateFileReader( *DemoFilename );
	if( !FileAr )
	{
		Error = FString::Printf( TEXT("Couldn't open demo file %s for reading"), *DemoFilename );//!!localize!!
		return 0;
	}
	ClientThirdPerson	= ConnectURL.HasOption(TEXT("3rdperson"));
	TimeBased			= ConnectURL.HasOption(TEXT("timebased"));
	NoFrameCap          = ConnectURL.HasOption(TEXT("noframecap"));

	return 1;
	unguard;
}
UBOOL UDemoRecDriver::InitListen( FNetworkNotify* InNotify, FURL& ConnectURL, FString& Error )
{
	guard(UDemoRecDriver::InitListen);
	if( !Super::InitListen( InNotify, ConnectURL, Error ) )
		return 0;
	if( !InitBase( 0, InNotify, ConnectURL, Error ) )
		return 0;

	// Recording, local machine is server, demo stream acts "as if" it's a client.
	UDemoRecConnection* Connection = new UDemoRecConnection( this, ConnectURL );
	Connection->CurrentNetSpeed   = 1000000;
	Connection->State             = USOCK_Open;
	Connection->InitOut();

	FileAr = GFileManager->CreateFileWriter( *DemoFilename );
	ClientConnections.AddItem( Connection );
	if( !FileAr )
	{
		Error = FString::Printf( TEXT("Couldn't open demo file %s for writing"), *DemoFilename );//localize!!
		return 0;
	}

	// Build package map.
	UGameEngine* GameEngine = CastChecked<UGameEngine>( GetLevel()->Engine );
	if( GetLevel()->GetLevelInfo()->NetMode == NM_Client )
		MasterMap->CopyLinkers( GetLevel()->NetDriver->ServerConnection->PackageMap );
	else
	{
		SpawnDemoRecSpectator(Connection);
		GameEngine->BuildServerMasterMap( this, GetLevel() );
	}

	// Create the control channel.
	Connection->CreateChannel( CHTYPE_Control, 1, 0 );

	// Welcome the player to the level.
	GetLevel()->WelcomePlayer( Connection, (TCHAR*) ((GetLevel()->GetLevelInfo()->NetMode == NM_Client || GetLevel()->GetLevelInfo()->NetMode == NM_Standalone) ? TEXT("CLIENTDEMO") : TEXT("SERVERDEMO")) );

	return 1;
	unguard;
}
void UDemoRecDriver::StaticConstructor()
{
	guard(UDemoRecDriver::StaticConstructor);
	new(GetClass(),TEXT("DemoSpectatorClass"), RF_Public)UStrProperty(CPP_PROPERTY(DemoSpectatorClass), TEXT("Client"), CPF_Config);
	unguard;
}
void UDemoRecDriver::LowLevelDestroy()
{
	guard(UDemoRecDriver::LowLevelDestroy);

	debugf( TEXT("Closing down demo driver.") );

	// Shut down file.
	guard(CloseFile);
	if( FileAr )
	{	
		delete FileAr;
		FileAr = NULL;
	}
	unguard;

	unguard;
}
void UDemoRecDriver::TickDispatch( FLOAT DeltaTime )
{
	guard(UDemoRecDriver::TickDispatch);
	Super::TickDispatch( DeltaTime );
	FrameNum++;

	BYTE Data[PACKETSIZE + 8];

	if(  ServerConnection && 
		(ServerConnection->State==USOCK_Pending || ServerConnection->State==USOCK_Open) )
	{	
		// Read data from the demo file
		DWORD PacketBytes;
		INT PlayedThisTick = 0;
		for( ; ; )
		{
			// At end of file?
			if( FileAr->AtEnd() || FileAr->IsError() )
			{
			AtEnd:
				ServerConnection->State = USOCK_Closed;
				return;
			}
	
			INT ServerFrameNum;
			DOUBLE ServerPacketTime;

			*FileAr << ServerFrameNum;
			*FileAr << ServerPacketTime;
			if((!TimeBased && ServerFrameNum > FrameNum) || (TimeBased && ServerPacketTime > Time))
			{
				FileAr->Seek(FileAr->Tell() - sizeof(ServerFrameNum) - sizeof(ServerPacketTime));
				break;
			}
			if(!NoFrameCap && !TimeBased && ServerPacketTime > Time)
			{
				// Busy-wait until it's time to play the frame.
				// WARNING: use appSleep() if appSeconds() isn't using CPU timestamp!
				// appSleep(ServerPacketTime - Time);
				DOUBLE t = appSeconds() + (ServerPacketTime - Time);
				while(appSeconds() < t);			
			}
			*FileAr << PacketBytes;

			// Read data from file.
			FileAr->Serialize( Data, PacketBytes );
			if( FileAr->IsError() )
			{
				debugf( NAME_DevNet, TEXT("Failed to read demo file packet") );
				goto AtEnd;
			}

			// Update stats.
			if( PacketBytes )
				PlayedThisTick++;

			// Process incoming packet.
			ServerConnection->ReceivedRawPacket( Data, PacketBytes );

			// Only play one packet per tick on demo playback, until we're 
			// fully connected.  This is like the handshake for net play.
			if(ServerConnection->State == USOCK_Pending)
				break;
		}
	}
	unguard;
}
FString UDemoRecDriver::LowLevelGetNetworkNumber()
{
	guard(UDemoRecDriver::LowLevelGetNetworkNumber);
	return TEXT("");
	unguard;
}
INT UDemoRecDriver::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(UDemoRecDriver::Exec);
	if( ParseCommand(&Cmd,TEXT("DEMOREC")) || ParseCommand(&Cmd,TEXT("DEMOPLAY")) )
	{
		if( ServerConnection )
			Ar.Logf( TEXT("Demo playback currently active: %s"), *DemoFilename );//!!localize!!
		else
			Ar.Logf( TEXT("Demo recording currently active: %s"), *DemoFilename );//!!localize!!
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("STOPDEMO")) )
	{
		Ar.Logf( TEXT("Demo %s stopped (%d frames)"), *DemoFilename, FrameNum );//!!localize!!
		if( !ServerConnection )
		{
			GetLevel()->DemoRecDriver=NULL;
			delete this;
		}
		else
			ServerConnection->State = USOCK_Closed;
		return 1;
	}
	else return 0;
	unguard;
}
ULevel* UDemoRecDriver::GetLevel()
{
	guard(UDemoRecDriver::GetLevel);
	check(Notify->NotifyGetLevel());
	return Notify->NotifyGetLevel();
	unguard;

}
void UDemoRecDriver::SpawnDemoRecSpectator( UNetConnection* Connection )
{
	guard(UDemoRecDriver::SpawnDemoRecSpectator);
	APlayerPawn *Spectator;
	// Spawn the recording actor
	UClass* C = StaticLoadClass( AActor::StaticClass(), NULL, TEXT("Engine.DemoRecSpectator"), NULL, LOAD_NoFail, NULL );
	Spectator = CastChecked<APlayerPawn>(GetLevel()->SpawnActor( C ));
	unguard;
}
IMPLEMENT_CLASS(UDemoRecDriver);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
