/*=============================================================================
	UnEngine.cpp: Unreal engine main.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnRender.h"

/*-----------------------------------------------------------------------------
	Object class implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UEngine);
IMPLEMENT_CLASS(URenderBase);
IMPLEMENT_CLASS(URenderDevice);
IMPLEMENT_CLASS(URenderIterator);

/*-----------------------------------------------------------------------------
	Engine init and exit.
-----------------------------------------------------------------------------*/

//
// Construct the engine.
//
UEngine::UEngine()
{
	guard(UEngine::UEngine);
	unguard;
}

//
// Init class.
//
void UEngine::StaticConstructor()
{
	guard(UEngine::StaticConstructor);

	new(GetClass(),TEXT("CacheSizeMegs"),      RF_Public)UIntProperty (CPP_PROPERTY(CacheSizeMegs      ), TEXT("Settings"), CPF_Config );
	new(GetClass(),TEXT("UseSound"),           RF_Public)UBoolProperty(CPP_PROPERTY(UseSound           ), TEXT("Settings"), CPF_Config );
	CurrentTickRate = 0.f;

	unguard;
}

// Register things.
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) ENGINE_API FName ENGINE_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)
#include "EngineClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY

//
// Init audio.
//
void UEngine::InitAudio()
{
	guard(UEngine::InitAudio);
	if
	(	UseSound
	&&	GIsClient
	&&	!ParseParam(appCmdLine(),TEXT("NOSOUND")) )
	{
#if defined(PLATFORM_ANDROID)
		// UT99_ANDROID_V64_AUDIO_LOAD_SAFE:
		// On Android the native Audio package is statically linked into
		// libUnrealTournament.so.  During bring-up, never let a missing or
		// stripped audio package abort the whole engine; fall back to silent
		// startup so CityIntro/menu can still run and log the real cause.
		UClass* AudioClass = StaticLoadClass( UAudioSubsystem::StaticClass(), NULL, TEXT("ini:Engine.Engine.AudioDevice"), NULL, LOAD_NoWarn, NULL );
		if( !AudioClass )
		{
			debugf( NAME_Init, TEXT("Android audio: AudioDevice class not found, continuing without sound.") );
			Audio = NULL;
			return;
		}
#else
		UClass* AudioClass = StaticLoadClass( UAudioSubsystem::StaticClass(), NULL, TEXT("ini:Engine.Engine.AudioDevice"), NULL, LOAD_NoFail, NULL );
#endif
		Audio = ConstructObject<UAudioSubsystem>( AudioClass );
		if( !Audio || !Audio->Init() )
		{
			debugf( NAME_Log, TEXT("Audio initialization failed.") );
			if( Audio )
				delete Audio;
			Audio = NULL;
		}
	}
	unguard;
}

//
// Initialize the engine.
//
static TCHAR YesKey=0, NoKey=0;//oldver
void UEngine::Init()
{
	guard(UEngine::Init);

	// Add the intrinsic names.
	#define NAMES_ONLY
	#define AUTOGENERATE_NAME(name) ENGINE_##name = FName(TEXT(#name),FNAME_Intrinsic);
	#define AUTOGENERATE_FUNCTION(cls,idx,name)
	#include "EngineClasses.h"
	#undef AUTOGENERATE_FUNCTION
	#undef AUTOGENERATE_NAME
	#undef NAMES_ONLY

	// Subsystems.
	FURL::StaticInit();
	GEngineMem.Init( 65536 );
#ifdef PLATFORM_DREAMCAST
	GCache.Init( 1024 * 256, 2048 );
#else
	GCache.Init( 1024 * 1024 * Clamp<INT>( GIsClient ? CacheSizeMegs : 1, 1, 1024 ), 4096 );
#endif
	// Translation.
	YesKey = appToUpper( *Localize( "General", "Yes", TEXT("Core") ) );
	NoKey  = appToUpper( *Localize( "General", "No",  TEXT("Core") ) );

	// Objects.
	Cylinder = new UPrimitive;

	// Add to root.
	AddToRoot();

	debugf( NAME_Init, TEXT("Unreal engine initialized") );
	unguard;
}

//
// Pre shutdown.
//
void UEngine::Exit()
{
	guard(UEngine::Exit);

	// Exit sound.
	guard(ExitSound);
	if( Audio )
	{
		delete Audio;
		Audio = NULL;
	}
	unguard;

	unguard;
}

//
// Exit the engine.
//
void UEngine::Destroy()
{
	guard(UEngine::Destroy);

	// Remove from root.
	RemoveFromRoot();

	// Shut down all subsystems.
	Audio  = NULL;
	Render = NULL;
	Client = NULL;
	FURL::StaticExit();
	GEngineMem.Exit();
	GCache.Exit( 1 );

	Super::Destroy();
	unguard;
}

//
// Flush all caches.
//
void UEngine::Flush( UBOOL AllowPrecache )
{
	guard(UEngine::Flush);

	GCache.Flush();
	if( Client )
		Client->Flush( AllowPrecache );

	unguard;
}

//
// Tick rate.
//
FLOAT UEngine::GetMaxTickRate()
{
	guard(UEngine::GetMaxTickRate);
	return 0;
	unguard;
}

//
// Progress indicator.
//
void UEngine::SetProgress( const TCHAR* Str1, const TCHAR* Str2, FLOAT Seconds )
{
	guard(UEngine::SetProgress);
	unguard;
}

//
// Serialize.
//
void UEngine::Serialize( FArchive& Ar )
{
	guard(UGameEngine::Serialize);

	Super::Serialize( Ar );
	Ar << Cylinder << Client << Render << Audio;

	unguardobj;
}

/*-----------------------------------------------------------------------------
	Input.
-----------------------------------------------------------------------------*/

//
// This always going to be the last exec handler in the chain. It
// handles passing the command to all other global handlers.
//
UBOOL UEngine::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(UEngine::Exec);

	// See if any other subsystems claim the command.
	if( GSys    && GSys->Exec		(Cmd,Ar) ) return 1;
	if( UObject::StaticExec			(Cmd,Ar) ) return 1;
	if( GCache.Exec					(Cmd,Ar) ) return 1;
	if( GExec   && GExec->Exec      (Cmd,Ar) ) return 1;
	if( Client  && Client->Exec		(Cmd,Ar) ) return 1;
	if( Render  && Render->Exec		(Cmd,Ar) ) return 1;
	if( Audio   && Audio->Exec		(Cmd,Ar) ) return 1;

	// Handle engine command line.
	if( ParseCommand(&Cmd,TEXT("FLUSH")) )
	{
		Flush(1);
		Ar.Log( TEXT("Flushed engine caches") );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("CRACKURL")) )
	{
		FURL URL(NULL,Cmd,TRAVEL_Absolute);
		if( URL.Valid )
		{
			Ar.Logf( TEXT("     Protocol: %s"), *URL.Protocol );
			Ar.Logf( TEXT("         Host: %s"), *URL.Host );
			Ar.Logf( TEXT("         Port: %i"), URL.Port );
			Ar.Logf( TEXT("          Map: %s"), *URL.Map );
			Ar.Logf( TEXT("   NumOptions: %i"), URL.Op.Num() );
			for( INT i=0; i<URL.Op.Num(); i++ )
				Ar.Logf( TEXT("     Option %i: %s"), i, *URL.Op(i) );
			Ar.Logf( TEXT("       Portal: %s"), *URL.Portal );
			Ar.Logf( TEXT("       String: '%s'"), **URL.String() );
		}
		else Ar.Logf( TEXT("BAD URL") );
		return 1;
	}
	else return 0;
	unguard;
}

//
// Key handler.
//
UBOOL UEngine::Key( UViewport* Viewport, EInputKey Key )
{
	guard(UEngine::Key);
	return Viewport->Console && Viewport->Console->eventKeyType( Key );
	unguard;
}

//
// Input event handler.
//
UBOOL UEngine::InputEvent( UViewport* Viewport, EInputKey iKey, EInputAction State, FLOAT Delta )
{
	guard(UEngine::InputEvent);

	//oldver: Translate yes/no if necessary.
	if
	(	Viewport->Console
	&&	Viewport->Actor
	&&	Viewport->Actor->myHUD
	&&	Viewport->Actor->myHUD->MainMenu )
	{
		UClass* C;
		for( C = Viewport->Actor->myHUD->MainMenu->GetClass(); C; C=C->GetSuperClass() )
			if( appStricmp(C->GetName(),TEXT("UnrealQuitMenu"))==0 || appStricmp(C->GetName(),TEXT("UnrealYesNoMenu"))==0 )
				break;
		if( C && appToUpper(iKey)==YesKey )
			iKey=IK_Y;
		else if( C && appToUpper(iKey)==NoKey )
			iKey=IK_N;
	}

	// Process it.
	if( Viewport->Console && Viewport->Console->eventKeyEvent( iKey, State, Delta ) )
	{
		//!! fix for continuous mouse-up events!
		if( State == IST_Release )
			Viewport->Input->PreProcess( iKey, State, Delta );
		// Player console handled it.
		return 1;
	}
	else if
	(	Viewport->Input->PreProcess( iKey, State, Delta )
	&&	Viewport->Input->Process( Viewport->Console ? (FOutputDevice&)*Viewport->Console : *GLog, iKey, State, Delta ) )
	{
		// Input system handled it.
		return 1;
	}
	else
	{
		// Nobody handled it.
		return 0;
	}
	unguard;
}

INT UEngine::ChallengeResponse( INT Challenge )
{
	guard(UEngine::ChallengeResponse);
	return 0;
	unguard;
}

/*-----------------------------------------------------------------------------
	UServerCommandlet.
-----------------------------------------------------------------------------*/

class UServerCommandlet : public UCommandlet
{
	DECLARE_CLASS(UServerCommandlet,UCommandlet,CLASS_Transient);
	void StaticConstructor()
	{
		guard(UServerCommandlet::StaticConstructor);

		LogToStdout = 1;
		IsClient    = 0;
		IsEditor    = 0;
		IsServer    = 1;
		LazyLoad    = 1;

		unguard;
	}
	INT Main( const TCHAR* Parms )
	{
		guard(UServerCommandlet::Main);

		// Create the editor class.
		UClass* EngineClass = UObject::StaticLoadClass( UEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.GameEngine"), NULL, LOAD_NoFail | LOAD_DisallowFiles, NULL );
		UEngine* Engine = ConstructObject<UEngine>( EngineClass );
		Engine->Init();

		// Main loop.
		GIsRunning = 1;
		DOUBLE OldTime = appSeconds();
		DOUBLE SecondStartTime = OldTime;
		INT TickCount = 0;
		while( GIsRunning && !GIsRequestingExit )
		{
			// Update the world.
			guard(UpdateWorld);
			DOUBLE NewTime = appSeconds();
			Engine->Tick( NewTime - OldTime );
			OldTime = NewTime;
			TickCount++;
			if( OldTime > SecondStartTime + 1 )
			{
				Engine->CurrentTickRate = (FLOAT)TickCount / (OldTime - SecondStartTime);
				SecondStartTime = OldTime;
				TickCount = 0;
			}
			unguard;

			// Enforce optional maximum tick rate.
			guard(EnforceTickRate);
			FLOAT MaxTickRate = Engine->GetMaxTickRate();
			if( MaxTickRate>0.0 )
			{
				FLOAT Delta = (1.0/MaxTickRate) - (appSeconds()-OldTime);
				appSleep( Max(0.f,Delta) );
			}
			unguard;
		}
		GIsRunning = 0;
		return 0;
		unguard;
	}
};
IMPLEMENT_CLASS(UServerCommandlet)

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
