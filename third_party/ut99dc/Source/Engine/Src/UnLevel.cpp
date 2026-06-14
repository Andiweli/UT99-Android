/*=============================================================================
	UnLevel.cpp: Level-related functions
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"


#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
#include <android/log.h>
static void AndroidListenLog( const TCHAR* Msg )
{
	#if UNICODE
	__android_log_print( ANDROID_LOG_INFO, "UT99LAN", "%ls", Msg );
	#else
	__android_log_print( ANDROID_LOG_INFO, "UT99LAN", "%s", Msg );
	#endif
}

static UBOOL AndroidLevelHasActorClassName( ULevel* Level, const TCHAR* ShortClassName )
{
	if( !Level )
		return 0;
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* A = Level->Actors(i);
		if( A && A->GetClass() && appStricmp(A->GetClass()->GetName(), ShortClassName)==0 )
			return 1;
	}
	return 0;
}

static void AndroidForceListenServerActor( ULevel* Level, const TCHAR* ClassPath, const TCHAR* ShortClassName )
{
	if( !Level )
		return;
	if( AndroidLevelHasActorClassName(Level, ShortClassName) )
	{
		AndroidListenLog( *FString::Printf(TEXT("UT99_ANDROID_V112_LISTEN server actor already alive %s"), ShortClassName) );
		return;
	}
	UClass* HelperClass = UObject::StaticLoadClass( AActor::StaticClass(), NULL, ClassPath, NULL, LOAD_NoWarn, NULL );
	if( !HelperClass )
	{
		AndroidListenLog( *FString::Printf(TEXT("UT99_ANDROID_V111_LISTEN failed to load %s"), ClassPath) );
		return;
	}
	AActor* Actor = Level->SpawnActor( HelperClass );
	AndroidListenLog( *FString::Printf(TEXT("UT99_ANDROID_V111_LISTEN forced server actor %s actor=%s"), ClassPath, Actor ? Actor->GetName() : TEXT("None")) );
}
#endif

/*-----------------------------------------------------------------------------
	ULevelBase implementation.
-----------------------------------------------------------------------------*/

ULevelBase::ULevelBase( UEngine* InEngine, const FURL& InURL )
:	URL( InURL )
,	Engine( InEngine )
,	Actors( this )
,	DemoRecDriver( NULL )
{}
void ULevelBase::Serialize( FArchive& Ar )
{
	guard(ULevelBase::Serialize);
	Super::Serialize(Ar);
	if( Ar.IsTrans() )
	{
		Ar << Actors;
	}
	else
	{
		//oldver Old-format actor list.
		INT DbNum=Actors.Num(), DbMax=DbNum;
		Actors.CountBytes( Ar );
		Ar << DbNum << DbMax;
		if( Ar.IsLoading() )
		{
			Actors.Empty( DbNum );
			Actors.Add( DbNum );
		}
		for( INT i=0; i<Actors.Num(); i++ )
			Ar << Actors(i);
	}
	
	// Level variables.
	Ar << URL;

	if( !Ar.IsLoading() && !Ar.IsSaving() )
	{
		Ar << NetDriver;
		Ar << DemoRecDriver;
	}

	unguard;
}
void ULevelBase::Destroy()
{
	guard(ULevelBase::Destroy);
	if( NetDriver )
	{
		delete NetDriver;
		NetDriver = NULL;
	}
	if( DemoRecDriver)
	{
		delete DemoRecDriver;
		DemoRecDriver = NULL;
	}
	Super::Destroy();
	unguard;
}
void ULevelBase::NotifyProgress( const TCHAR* Str1, const TCHAR* Str2, FLOAT Seconds )
{
	guard(ULevelBase::NotifyProgress);
	Engine->SetProgress( Str1, Str2, Seconds );
	unguard;
}
IMPLEMENT_CLASS(ULevelBase);

/*-----------------------------------------------------------------------------
	Level creation & emptying.
-----------------------------------------------------------------------------*/

//
//	Create a new level and allocate all objects needed for it.
//	Call with Editor=1 to allocate editor structures for it, also.
//
ULevel::ULevel( UEngine* InEngine, UBOOL InRootOutside )
:	ULevelBase( InEngine )
{
	guard(ULevel::ULevel);

	// Allocate subobjects.
	SetFlags( RF_Transactional );
	Model = new( GetOuter() )UModel( NULL, InRootOutside );
	Model->SetFlags( RF_Transactional );

	// Spawn the level info.
	SpawnActor( ALevelInfo::StaticClass() );
	check(GetLevelInfo());

	// Spawn the default brush.
	ABrush* Temp = SpawnBrush();
	check(Temp==Actors(1));
	Temp->Brush = new( GetOuter(), TEXT("Brush") )UModel( Temp, 1 );
	Temp->SetFlags( RF_NotForClient | RF_NotForServer | RF_Transactional );
	Temp->Brush->SetFlags( RF_NotForClient | RF_NotForServer | RF_Transactional );

	unguard;
}
void ULevel::ShrinkLevel()
{
	guard(ULevel::Shrink);

	Model->ShrinkModel();
	ReachSpecs.Shrink();

	unguard;
}
void ULevel::DetailChange( UBOOL NewDetail )
{
	guard(ULevel::DetailChange);
	GetLevelInfo()->bHighDetailMode = NewDetail;
	if( GetLevelInfo()->Game )
		GetLevelInfo()->Game->eventDetailChange();
	unguard;
}

/*-----------------------------------------------------------------------------
	Level locking and unlocking.
-----------------------------------------------------------------------------*/

//
// Modify this level.
//
void ULevel::Modify( UBOOL DoTransArrays )
{
	guard(ULevel::Modify);
	UObject::Modify();
	Model->Modify();
	unguard;
}
void ULevel::PostLoad()
{
	guard(ULevel::PostLoad);
	Super::PostLoad();
#if ENGINE_VERSION>230
	for( TObjectIterator<AActor> It; It; ++It )
		if( It->GetOuter()==GetOuter() )
			It->XLevel = this;
#endif
	unguard;
}
void ULevel::SetActorCollision( UBOOL bCollision )
{
	guard(ULevel::SetActorCollision);

	// Init collision if first time through.
	if( bCollision && !Hash )
	{
		// Init hash.
		guard(StartCollision);
		Hash = GNewCollisionHash();
		for( INT i=0; i<Actors.Num(); i++ )
			if( Actors(i) && Actors(i)->bCollideActors )
				Hash->AddActor( Actors(i) );
		unguard;
	}
	else if( Hash && !bCollision )
	{
		// Destroy hash.
		guard(EndCollision);
		for( INT i=0; i<Actors.Num(); i++ )
			if( Actors(i) && Actors(i)->bCollideActors )
				Hash->RemoveActor( Actors(i) );
		delete Hash;
		Hash = NULL;
		unguard;
	}

	unguard;
}

/*-----------------------------------------------------------------------------
	Level object implementation.
-----------------------------------------------------------------------------*/

void ULevel::Serialize( FArchive& Ar )
{
	guard(ULevel::Serialize);
	Super::Serialize( Ar );

	FLOAT ApproxTime = (FLOAT)TimeSeconds;
	Ar << Model;
	Ar << ReachSpecs;
	Ar << ApproxTime;
	Ar << FirstDeleted;
	for( INT i=0; i<NUM_LEVEL_TEXT_BLOCKS; i++ )
		Ar << TextBlocks[i];
	if( Ar.Ver()>62 )//oldver
	{
		Ar << TravelInfo;
	}
	else if( Ar.Ver()>=61 )
	{
		TArray<FString> Names, Items;
		Ar << Names << Items;
		TravelInfo = TMap<FString,FString>();
		for( INT i=0; i<Names.Num(); i++ )
			TravelInfo.Set( *Names(i), *Items(i) );
	}
	if( Model && !Ar.IsTrans() )
		Ar.Preload( Model );
	if( BrushTracker )
		BrushTracker->CountBytes( Ar );

	unguard;
}
void ULevel::Destroy()
{
	guard(ULevel::Destroy);

	// Free allocated stuff.
	if( Hash )
	{
		delete Hash;
		Hash = NULL; /* Required because actors may try to unhash themselves. */
	}
	if( BrushTracker )
	{
		delete BrushTracker;
		BrushTracker = NULL; /* Required because brushes may clean themselves up. */
	}

	Super::Destroy();
	unguard;
}
IMPLEMENT_CLASS(ULevel);

/*-----------------------------------------------------------------------------
	Reconcile actors and Viewports after loading or creating a new level.

	These functions provide the basic mechanism by which UnrealEd associates
	Viewports and actors together, even when new maps are loaded which contain
	an entirely different set of actors which must be mapped onto the existing 
	Viewports.
-----------------------------------------------------------------------------*/

//
// Remember actors.
//
void ULevel::RememberActors()
{
	guard(ULevel::RememberActors);
	if( Engine->Client )
	{
		for( INT i=0; i<Engine->Client->Viewports.Num(); i++ )
		{
			UViewport* Viewport			= Engine->Client->Viewports(i);
			Viewport->SavedOrthoZoom	= Viewport->Actor->OrthoZoom;
			Viewport->SavedFovAngle		= Viewport->Actor->FovAngle;
			Viewport->SavedShowFlags	= Viewport->Actor->ShowFlags;
			Viewport->SavedRendMap		= Viewport->Actor->RendMap;
			Viewport->SavedMisc1		= Viewport->Actor->Misc1;
			Viewport->SavedMisc2		= Viewport->Actor->Misc2;
			Viewport->Actor				= NULL;
		}
	}
	unguard;
}

//
// Reconcile actors.  This is called after loading a level.
// It attempts to match each existing Viewport to an actor in the newly-loaded
// level.  If no decent match can be found, creates a new actor for the Viewport.
//
void ULevel::ReconcileActors()
{
	guard(ULevel::ReconcileActors);
	check(GIsEditor);

	// Dissociate all actor Viewports and remember their view properties.
	for( INT i=0; i<Actors.Num(); i++ )
		if( Actors(i) && Actors(i)->IsA(APlayerPawn::StaticClass()) )
			if( ((APlayerPawn*)Actors(i))->Player )
				((APlayerPawn*)Actors(i))->Player = NULL;

	// Match Viewports and Viewport-actors with identical names.
	guard(MatchIdentical);
	for( INT i=0; i<Engine->Client->Viewports.Num(); i++ )
	{
		UViewport* Viewport = Engine->Client->Viewports(i);
		check(Viewport->Actor==NULL);
		for( INT j=0; j<Actors.Num(); j++ )
		{
			AActor* Actor = Actors(j);
			if( Actor && Actor->IsA(ACamera::StaticClass()) && appStricmp(*Actor->Tag,Viewport->GetName())==0 )
			{
				debugf( NAME_Log, TEXT("Matched Viewport %s"), Viewport->GetName() );
				Viewport->Actor         = (APlayerPawn *)Actor;
				Viewport->Actor->Player = Viewport;
				break;
			}
		}
	}
	unguard;

	// Match up all remaining Viewports to actors.
	guard(MatchEditorOther);
	INT i;
	for( i=0; i<Engine->Client->Viewports.Num(); i++ )
	{
		// Hook Viewport up to an existing actor or createa a new one.
		UViewport* Viewport = Engine->Client->Viewports(i);
		if( !Viewport->Actor )
			SpawnViewActor( Viewport );
	}
	unguard;

	// Handle remaining unassociated view actors.
	guard(KillViews);
	for( INT i=0; i<Actors.Num(); i++ )
	{
		ACamera* View = Cast<ACamera>(Actors(i));
		if( View )
		{
			UViewport* Viewport = Cast<UViewport>(View->Player);
			if( Viewport )
			{
				UViewport* Viewport	= (UViewport*)View->Player;
				View->ClearFlags( RF_Transactional );
				View->OrthoZoom		= Viewport->SavedOrthoZoom;	
				View->FovAngle		= Viewport->SavedFovAngle;
				View->ShowFlags		= Viewport->SavedShowFlags;
				View->RendMap		= Viewport->SavedRendMap;
				View->Misc1			= Viewport->SavedMisc1;
				View->Misc2			= Viewport->SavedMisc2;
			}
			else DestroyActor( View );
		}
	}
	unguard;

	unguard;
}

/*-----------------------------------------------------------------------------
	ULevel command-line.
-----------------------------------------------------------------------------*/

UBOOL ULevel::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(ULevel::Exec);
	if( NetDriver && NetDriver->Exec( Cmd, Ar ) )
	{
		return 1;
	}
	else if( DemoRecDriver && DemoRecDriver->Exec( Cmd, Ar ) )
	{
		return 1;
	}
	else if( ParseCommand( &Cmd, TEXT("DEMOREC") ) )
	{
		FURL URL;
		if( ParseToken( Cmd, URL.Map, 0 ) )
		{
			if( URL.Map.Right(4)!=TEXT(".dem") )
				URL.Map += TEXT(".dem");
			debugf( TEXT("Attempting to record demo %s"), *URL.Map );
			UClass* DemoDriverClass = StaticLoadClass( UNetDriver::StaticClass(), NULL, TEXT("ini:Engine.Engine.DemoRecordingDevice"), NULL, LOAD_NoFail, NULL );
			DemoRecDriver           = ConstructObject<UNetDriver>( DemoDriverClass );
			FString Error;
			if( !DemoRecDriver->InitListen( this, URL, Error ) )
			{
				Ar.Logf( TEXT("Demo recording failed: %s"), *Error );//!!localize!!
				delete DemoRecDriver;
				DemoRecDriver = NULL;
			}
			else
				Ar.Logf( TEXT("Demo recording started to %s"), *URL.Map );
		}
		else
			Ar.Log( TEXT("You must specify a filename") );//!!localize!!
		return 1;
	}
	else if( ParseCommand( &Cmd, TEXT("DEMOPLAY") ) )
	{
		FString Temp;
		if( ParseToken( Cmd, Temp, 0 ) )
		{
			FURL URL(NULL, *Temp, TRAVEL_Absolute);
			if( URL.Map.Right(4)!=TEXT(".dem") )
				URL.Map += TEXT(".dem");
			debugf( TEXT("Attempting to play demo %s"), *URL.Map );
			UGameEngine* GameEngine = CastChecked<UGameEngine>( Engine );
			if( GameEngine->GPendingLevel )
				GameEngine->CancelPending();
			GameEngine->GPendingLevel = new UDemoPlayPendingLevel( GameEngine, URL );
			if( !GameEngine->GPendingLevel->DemoRecDriver )
			{
				Ar.Logf( TEXT("Demo playback failed: %s"), *GameEngine->GPendingLevel->Error );//!!localize!!
				delete GameEngine->GPendingLevel;
				GameEngine->GPendingLevel = NULL;
			}
		}
		else Ar.Log( TEXT("You must specify a filename") );//!!localize!!
		return 1;
	}
	else return 0;
	unguard;
}

/*-----------------------------------------------------------------------------
	ULevel networking related functions.
-----------------------------------------------------------------------------*/

//
// Start listening for connections.
//
UBOOL ULevel::Listen( FString& Error )
{
	guard(ULevel::Listen);
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
	AndroidListenLog( *FString::Printf(TEXT("UT99_ANDROID_V111_LISTEN entered url=%s"), *URL.String()) );
#endif
	if( NetDriver )
	{
		Error = LocalizeError("NetAlready");
		return 0;
	}
	if( !GetLinker() )
	{
		Error = LocalizeError("NetListen");
		return 0;
	}

	// Create net driver.
	UClass* NetDriverClass = StaticLoadClass( UNetDriver::StaticClass(), NULL, TEXT("ini:Engine.Engine.NetworkDevice"), NULL, LOAD_NoFail, NULL );
	NetDriver = (UNetDriver*)StaticConstructObject( NetDriverClass );
	if( !NetDriver->InitListen( this, URL, Error ) )
	{
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
		AndroidListenLog( *FString::Printf(TEXT("UT99_ANDROID_V112_LISTEN InitListen FAILED error=%s"), *Error) );
#endif
		debugf( TEXT("Failed to listen: %s"), *Error );
		delete NetDriver;
		NetDriver=NULL;
		return 0;
	}
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
	AndroidListenLog( *FString::Printf(TEXT("UT99_ANDROID_V112_LISTEN InitListen OK port=%i"), URL.Port) );
#endif

	// Load everything required for network server support.
	UGameEngine* GameEngine = CastChecked<UGameEngine>( Engine );
	GameEngine->BuildServerMasterMap( NetDriver, this );

	// Spawn network server support.
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
	AndroidListenLog( *FString::Printf(TEXT("UT99_ANDROID_V112_LISTEN server actor config count=%i"), GameEngine->ServerActors.Num()) );
#endif
	for( INT i=0; i<GameEngine->ServerActors.Num(); i++ )
	{
		TCHAR Str[240];
		const TCHAR* Ptr = *GameEngine->ServerActors(i);
		if( ParseToken( Ptr, Str, ARRAY_COUNT(Str), 1 ) )
		{
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
			AndroidListenLog( *FString::Printf(TEXT("UT99_ANDROID_V112_LISTEN spawning config actor %s"), Str) );
#endif
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
			// UT99_ANDROID_V104_UWEB_SERVERACTOR_SAFE:
			// The stock UT99 config starts UWeb.WebServer for WebAdmin when a listen
			// server is created. This Android port is a single-binary build and does
			// not ship/register the UWeb native DLL exports, so loading UWeb here
			// fatal-errors during native function binding. WebAdmin is not useful on
			// Android; skip only that helper and keep normal IpDrv server actors.
			if( appStricmp(Str,TEXT("UWeb.WebServer"))==0 )
			{
				debugf( NAME_Log, TEXT("Android: skipping unsupported server actor %s"), Str );
				continue;
			}
#endif
			debugf( TEXT("Spawning: %s"), Str );
			UClass* HelperClass = StaticLoadClass( AActor::StaticClass(), NULL, Str, NULL, LOAD_NoFail, NULL );
			AActor* Actor = SpawnActor( HelperClass );
			while( Actor && ParseToken(Ptr,Str,ARRAY_COUNT(Str),1) )
			{
				TCHAR* Value = appStrchr(Str,'=');
				if( Value )
				{
					*Value++ = 0;
					for( TFieldIterator<UProperty> It(Actor->GetClass()); It; ++It )
						if
						(	appStricmp(It->GetName(),Str)==0
						&&	(It->PropertyFlags & CPF_Config) )
							It->ImportText( Value, (BYTE*)Actor + It->Offset, 0 );
				}
			}
		}
	}

#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
	// UT99_ANDROID_V111_FORCE_QUERY_ACTORS:
	// Older preserved INIs and some menu paths reach Listen() without the stock
	// IpDrv/IpServer helpers actually alive. The LAN list and OPENLAN fallback
	// both need UdpServerQuery; UdpBeacon keeps stock REPORTQUERY working too.
	AndroidForceListenServerActor( this, TEXT("IpServer.UdpServerQuery"), TEXT("UdpServerQuery") );
	AndroidForceListenServerActor( this, TEXT("IpDrv.UdpBeacon"), TEXT("UdpBeacon") );
#endif

	// Set LevelInfo properties.
	GetLevelInfo()->NetMode = Engine->Client ? NM_ListenServer : NM_DedicatedServer;
	GetLevelInfo()->NextSwitchCountdown = NetDriver->ServerTravelPause;

	return 1;
	unguard;
}

//
// Return whether this level is a server.
//
UBOOL ULevel::IsServer()
{
	guardSlow(ULevel::IsServer);
	return (!NetDriver || !NetDriver->ServerConnection) && (!DemoRecDriver || !DemoRecDriver->ServerConnection);
	unguardSlow;
}

/*-----------------------------------------------------------------------------
	ULevel network notifys.
-----------------------------------------------------------------------------*/

//
// The network driver is about to accept a new connection attempt by a
// connectee, and we can accept it or refuse it.
//
EAcceptConnection ULevel::NotifyAcceptingConnection()
{
	guard(ULevel::NotifyAcceptingConnection);
	check(NetDriver);
	if( NetDriver->ServerConnection )
	{
		// We are a client and we don't welcome incoming connections.
		debugf( NAME_DevNet, TEXT("NotifyAcceptingConnection: Client refused") );
		return ACCEPTC_Reject;
	}
	else if( GetLevelInfo()->NextURL!=TEXT("") )
	{
		// Server is switching levels.
		debugf( NAME_DevNet, TEXT("NotifyAcceptingConnection: Server %s refused"), GetName() );
		return ACCEPTC_Ignore;
	}
	else
	{
		// Server is up and running.
		debugf( NAME_DevNet, TEXT("NotifyAcceptingConnection: Server %s accept"), GetName() );
		return ACCEPTC_Accept;
	}
	unguard;
}

//
// This server has accepted a connection.
//
void ULevel::NotifyAcceptedConnection( UNetConnection* Connection )
{
	guard(ULevel::NotifyAcceptedConnection);
	check(NetDriver!=NULL);
	check(NetDriver->ServerConnection==NULL);
	debugf( NAME_NetComeGo, TEXT("Open %s %s %s"), GetName(), appTimestamp(), *Connection->LowLevelGetRemoteAddress() );
	unguard;
}

//
// The network interface is notifying this level of a new channel-open
// attempt by a connectee, and we can accept or refuse it.
//
UBOOL ULevel::NotifyAcceptingChannel( UChannel* Channel )
{
	guard(ULevel::NotifyAcceptingChannel);
	
	check(Channel);
	check(Channel->Connection);
	check(Channel->Connection->Driver);
	UNetDriver* Driver = Channel->Connection->Driver;

	if( Driver->ServerConnection )
	{
		// We are a client and the server has just opened up a new channel.
		//debugf( "NotifyAcceptingChannel %i/%i client %s", Channel->ChIndex, Channel->ChType, GetName() );
		if( Channel->ChType==CHTYPE_Actor )
		{
			// Actor channel.
			//debugf( "Client accepting actor channel" );
			return 1;
		}
		else
		{
			// Unwanted channel type.
			debugf( NAME_DevNet, TEXT("Client refusing unwanted channel of type %i"), Channel->ChType );
			return 0;
		}
	}
	else
	{
		// We are the server.
		if( Channel->ChIndex==0 && Channel->ChType==CHTYPE_Control )
		{
			// The client has opened initial channel.
			debugf( NAME_DevNet, TEXT("NotifyAcceptingChannel Control %i server %s: Accepted"), Channel->ChIndex, GetFullName() );
			return 1;
		}
		else if( Channel->ChType==CHTYPE_File )
		{
			// The client is going to request a file.
			debugf( NAME_DevNet, TEXT("NotifyAcceptingChannel File %i server %s: Accepted"), Channel->ChIndex, GetFullName() );
			return 1;
		}
		else
		{
			// Client can't open any other kinds of channels.
			debugf( NAME_DevNet, TEXT("NotifyAcceptingChannel %i %i server %s: Refused"), Channel->ChType, Channel->ChIndex, GetFullName() );
			return 0;
		}
	}
	unguard;
}

//
// Welcome a new player joining this server.
//
void ULevel::WelcomePlayer( UNetConnection* Connection, TCHAR* Optional )
{
	guard(ULevel::WelcomePlayer);

	Connection->PackageMap->Copy( Connection->Driver->MasterMap );
	Connection->SendPackageMap();
	if( Optional[0] )
		Connection->Logf( TEXT("WELCOME LEVEL=%s LONE=%i %s"), GetOuter()->GetName(), GetLevelInfo()->bLonePlayer, Optional );
	else
		Connection->Logf( TEXT("WELCOME LEVEL=%s LONE=%i"), GetOuter()->GetName(), GetLevelInfo()->bLonePlayer );
	Connection->FlushNet();

	unguard;
}

//
// Received text on the control channel.
//
void ULevel::NotifyReceivedText( UNetConnection* Connection, const TCHAR* Text )
{
	guard(ULevel::NotifyReceivedText);
	if( ParseCommand(&Text,TEXT("USERFLAG")) )
	{
		Connection->UserFlags = appAtoi(Text);
	}
	else if( NetDriver->ServerConnection )
	{
		// We are the client.
		debugf( NAME_DevNet, TEXT("Level client received: %s"), Text );
		if( ParseCommand(&Text,TEXT("FAILURE")) )
		{
			// Return to entry.
			check(Engine->Client->Viewports.Num());
			Engine->SetClientTravel( Engine->Client->Viewports(0), TEXT("?failed"), 0, TRAVEL_Absolute );
		}
	}
	else
	{
		// We are the server.
		debugf( NAME_DevNet, TEXT("Level server received: %s"), Text );
		if( ParseCommand(&Text,TEXT("HELLO")) )
		{
			// Versions.
			INT RemoteMinVer=219, RemoteVer=219;
			Parse( Text, TEXT("MINVER="), RemoteMinVer );
			Parse( Text, TEXT("VER="),    RemoteVer    );
			if( RemoteVer<ENGINE_MIN_NET_VERSION || RemoteMinVer>ENGINE_VERSION )
			{
				Connection->Logf( TEXT("UPGRADE MINVER=%i VER=%i"), ENGINE_MIN_NET_VERSION, ENGINE_VERSION );
				Connection->FlushNet();
				Connection->State = USOCK_Closed;
				return;
			}
			Connection->NegotiatedVer = Min<INT>( RemoteVer, ENGINE_VERSION );

			// Get byte limit.
			INT Stats = GetLevelInfo()->Game->bWorldLog;
			Connection->Challenge = appCycles();
			Connection->Logf( TEXT("CHALLENGE VER=%i CHALLENGE=%i STATS=%i"), Connection->NegotiatedVer, Connection->Challenge, Stats );
			Connection->FlushNet();
		}
		else if( ParseCommand(&Text,TEXT("NETSPEED")) )
		{
			INT Rate = appAtoi(Text);
			if( Rate>=500 )
				Connection->CurrentNetSpeed = Clamp<INT>( Rate, 500, NetDriver->MaxClientRate );
			debugf( TEXT("Client netspeed is %i"), Connection->CurrentNetSpeed );
		}
		else if( ParseCommand(&Text,TEXT("HAVE")) )
		{
			// Client specifying his generation.
			FGuid Guid(0,0,0,0);
			Parse( Text, TEXT("GUID=" ), Guid );
			for( TArray<FPackageInfo>::TIterator It(Connection->PackageMap->List); It; ++It )
				if( It->Guid==Guid )
					Parse( Text, TEXT("GEN=" ), It->RemoteGeneration );
		}
		else if( ParseCommand(&Text,TEXT("LOGIN")) )
		{
			// Admit or deny the player here.
			INT Response=0;
			if
			(	!Parse(Text,TEXT("RESPONSE="),Response)
			||	!(Engine->ChallengeResponse(Connection->Challenge)==Response) )
			{
				Connection->Logf( TEXT("FAILURE CHALLENGE") );
				Connection->FlushNet();
				Connection->State = USOCK_Closed;
				return;
			}
			TCHAR Str[1024]=TEXT("");
			FString Error, FailCode;
			Parse( Text, TEXT("URL="), Str, ARRAY_COUNT(Str) );
			Connection->RequestURL = Str;
			debugf( NAME_DevNet, TEXT("Login request: %s"), *Connection->RequestURL );
			const TCHAR* Tmp;
			for( Tmp=Str; *Tmp && *Tmp!='?'; Tmp++ );
			GetLevelInfo()->Game->eventPreLogin( Tmp, Connection->LowLevelGetRemoteAddress(), Error, FailCode );
			if( Error!=TEXT("") )
			{
				debugf( NAME_DevNet, TEXT("PreLogin failure: %s (%s)"), *Error, *FailCode );
				Connection->Logf( TEXT("FAILURE %s"), *Error );
				if( (*FailCode)[0] )
					Connection->Logf( TEXT("FAILCODE %s"), *FailCode );
				Connection->FlushNet();
				Connection->State = USOCK_Closed;
				return;
			}
			WelcomePlayer( Connection );
		}
		else if( ParseCommand(&Text,TEXT("JOIN")) && !Connection->Actor )
		{
			// Finish computing the package map.
			Connection->PackageMap->Compute();

			// Spawn the player-actor for this network player.
			FString Error;
			debugf( NAME_DevNet, TEXT("Join request: %s"), *Connection->RequestURL );
			if( !SpawnPlayActor( Connection, ROLE_AutonomousProxy, FURL(NULL,*Connection->RequestURL,TRAVEL_Absolute), Error ) )
			{
				// Failed to connect.
				debugf( NAME_DevNet, TEXT("Join failure: %s"), *Error );
				Connection->Logf( TEXT("FAILURE %s"), *Error );
				Connection->FlushNet();
				Connection->State = USOCK_Closed;
			}
			else
			{
				// Successfully in game.
				debugf( NAME_DevNet, TEXT("Join succeeded: %s"), *Connection->Actor->PlayerReplicationInfo->PlayerName );
			}
		}
	}
	unguard;
}

//
// Called when a file receive is about to begin.
//
void ULevel::NotifyReceivedFile( UNetConnection* Connection, INT PackageIndex, const TCHAR* Error )
{
	guard(ULevel::NotifyReceivingFile);
	appErrorf( TEXT("Level received unexpected file") );
	unguard;
}

//
// Called when other side requests a file.
//
UBOOL ULevel::NotifySendingFile( UNetConnection* Connection, FGuid Guid )
{
	guard(ULevel::NotifySendingFile);
	if( NetDriver->ServerConnection )
	{
		// We are the client.
		debugf( NAME_DevNet, TEXT("Server requested file: Refused") );
		return 0;
	}
	else
	{
		// We are the server.
		debugf( NAME_DevNet, TEXT("Client requested file: Allowed") );
		return 1;
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	Stats.
-----------------------------------------------------------------------------*/

void ULevel::InitStats()
{
	guard(ULevel::InitStats);
	NetTickCycles = NetDiffCycles = ActorTickCycles = AudioTickCycles = FindPathCycles
	= MoveCycles = NumMoves = NumReps = NumPV = GetRelevantCycles = NumRPC = SeePlayer
	= Spawning = Unused = 0;
	GScriptEntryTag = GScriptCycles = 0;
	unguard;
}
void ULevel::GetStats( TCHAR* Result )
{
	guard(ULevel::GetStats);
	appSprintf
	(
		Result,
		TEXT("Script=%05.1f Actor=%04.1f Path=%04.1f See=%04.1f Spawn=%04.1f Audio=%04.1f Un=%04.1f Move=%04.1f (%i) Net=%04.1f"),
		GSecondsPerCycle*1000 * GScriptCycles,
		GSecondsPerCycle*1000 * ActorTickCycles,
		GSecondsPerCycle*1000 * FindPathCycles,
		GSecondsPerCycle*1000 * SeePlayer,
		GSecondsPerCycle*1000 * Spawning,
		GSecondsPerCycle*1000 * AudioTickCycles,
		GSecondsPerCycle*1000 * Unused,
		GSecondsPerCycle*1000 * MoveCycles,
		NumMoves,
		GSecondsPerCycle*1000 * NetTickCycles
	);
	unguard;
}

/*-----------------------------------------------------------------------------
	Clock.
-----------------------------------------------------------------------------*/

void ULevel::UpdateTime(ALevelInfo* Info)
{
	appSystemTime( Info->Year, Info->Month, Info->DayOfWeek, Info->Day, Info->Hour, Info->Minute, Info->Second, Info->Millisecond );
}

/*-----------------------------------------------------------------------------
	Actors relevant to a viewer.
-----------------------------------------------------------------------------*/

void ULevel::TraceVisible
(
	FVector&		vTraceDirection,
	FCheckResult&	Hit,			// Item hit.
	AActor*			SourceActor,	// Source actor, this or its parents is never hit.
	const FVector&	Start,			// Start location.
	DWORD           TraceFlags,		// Trace flags.
	int				iDistance
)
{
	guard(ULevel::TraceVisible);

	const FBspNode*	Node = NULL;
	FCheckResult	FirstHit;
	FCheckResult*	Check;
	FVector			StartTrace,
					End,
					Extent(0,0,0);
	APlayerPawn*	Player = SourceActor->IsA( APlayerPawn::StaticClass() ) ? (APlayerPawn*)SourceActor : NULL;

	// trace the entire distance looking for a "selected" zone
	StartTrace = Start;
	End = Start + iDistance * vTraceDirection;
	while( (int)FDist( Start, StartTrace ) < iDistance )
	{
		// Get list of hit actors.
		if( SingleLineCheck( FirstHit, SourceActor, End, StartTrace, TraceFlags, Extent ) )
			break;

		// skip owned actors, but return the one nearest actor or level data
		for( Check = &FirstHit; Check != NULL; Check = Check->GetNext() )
		{
			if( !SourceActor || !SourceActor->IsOwnedBy( Check->Actor ) )
			{
				if( Check->Actor->IsA( ALevelInfo::StaticClass() ) )
				{
					// if we're in the rock (node 0), skip the test, then try again
					if( Check->Item == 0 )
						break;

					// make sure node we hit is in a visible zone
					Node = &Check->Actor->XLevel->Model->Nodes( Check->Item );
					if( !Player || Player->IsZoneVisible( Node->iZone[1] ) )
					{
						goto HitSection;
					}

				}
				else if( Check->Actor ) 
				{
					// make sure the actor we hit is a visible zone
					if( !Player || Player->IsZoneVisible( Check->Actor->Region.ZoneNumber ) )
					{
						goto HitSection;
					}
				}
			}
		}

		// found a room, but it's the wrong one, move forward one foot, then try again
		StartTrace = FirstHit.Location + 16 * vTraceDirection;
	}

	// missed section
	Hit.Time = 1.0;
	Hit.Actor = NULL;
	return;

HitSection:
	Hit = *Check;
	return;

	unguard;
}

// Trace a line and return the first actor hit in the selected section that matches the ParentClass
void ULevel::TraceVisibleObjects
(
	UClass*			ParentClass,	
	FVector&		vTraceDirection,
	FCheckResult&	Hit,			// Item hit.
	AActor*			SourceActor,	// Source actor, this or its parents is never hit.
	const FVector&	Start,			// Start location.
	DWORD           TraceFlags,		// Trace flags.
	int				iDistance
)
{
	guard(ULevel::TraceVisibleObjects);

	FCheckResult	FirstHit;
	FCheckResult*	Check;
	FVector			StartTrace,
					End,
					Extent(0,0,0);
	APlayerPawn*	Player = SourceActor->IsA( APlayerPawn::StaticClass() ) ? (APlayerPawn*)SourceActor : NULL;

	// trace the entire distance looking for a matching object in the "selected" zone
	StartTrace = Start;
	End = Start + iDistance * vTraceDirection;
	while( (int)FDist( Start, StartTrace ) < iDistance )
	{
		// Get list of hit actors.
		if( SingleLineCheck( FirstHit, SourceActor, End, StartTrace, TraceFlags, Extent ) )
		{
			break;
		}

		// skip owned actors, but return the one nearest actor matching the ParentClass
		for( Check = &FirstHit; Check != NULL; Check = Check->GetNext() )
		{
			if( !SourceActor || !SourceActor->IsOwnedBy( Check->Actor ) )
			{
				if( Check->Actor->GetClass()->IsChildOf( ParentClass ) )
				{
					// make sure actor we hit is in a visible zone
					if( !Player || Player->IsZoneVisible( Check->Actor->Region.ZoneNumber ) )
					{
						Hit = *Check;
						return;
					}
				}
			}
		}

		// found a room, but it's the wrong one, move forward 1 foot, then try again
		StartTrace = FirstHit.Location + 16 * vTraceDirection;
	}

	// missed section
	Hit.Time = 1.0;
	Hit.Actor = NULL;

	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
