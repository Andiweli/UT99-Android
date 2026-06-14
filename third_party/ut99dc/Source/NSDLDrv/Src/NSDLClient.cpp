#include <stdlib.h>
#include <ctype.h>

#ifdef PLATFORM_ANDROID
#include <android/log.h>
#endif

#include "NSDLDrv.h"
#include "UnRender.h"

IMPLEMENT_CLASS( UNSDLClient );

/*-----------------------------------------------------------------------------
	UNSDLClient implementation.
-----------------------------------------------------------------------------*/

//
// Static init.
//
void UNSDLClient::InternalClassInitializer( UClass* Class )
{
	guard(UNSDLClient::InternalClassInitializer);
	if( appStricmp( Class->GetName(), "NSDLClient" ) == 0 )
	{
		new(Class, "DefaultDisplay",    RF_Public)UIntProperty(CPP_PROPERTY(DefaultDisplay),     "Display",  CPF_Config );
		new(Class, "StartupFullscreen", RF_Public)UBoolProperty(CPP_PROPERTY(StartupFullscreen), "Display",  CPF_Config );
		new(Class, "UseJoystick",       RF_Public)UBoolProperty(CPP_PROPERTY(UseJoystick),       "Joystick", CPF_Config );
		new(Class, "DeadZoneXYZ",       RF_Public)UFloatProperty(CPP_PROPERTY(DeadZoneXYZ),      "Joystick", CPF_Config );
		new(Class, "DeadZoneRUV",       RF_Public)UFloatProperty(CPP_PROPERTY(DeadZoneRUV),      "Joystick", CPF_Config );
		new(Class, "ScaleXYZ",          RF_Public)UFloatProperty(CPP_PROPERTY(ScaleXYZ),         "Joystick", CPF_Config );
		new(Class, "ScaleRUV",          RF_Public)UFloatProperty(CPP_PROPERTY(ScaleRUV),         "Joystick", CPF_Config );
		new(Class, "InvertY",           RF_Public)UBoolProperty(CPP_PROPERTY(InvertY),           "Joystick", CPF_Config );
		new(Class, "InvertV",           RF_Public)UBoolProperty(CPP_PROPERTY(InvertV),           "Joystick", CPF_Config );
	}
	unguard;
}

//
// UNSDLClient constructor.
//
UNSDLClient::UNSDLClient()
{
	guard(UNSDLClient::UWindowsClient);
	Controller = NULL;
	DefaultDisplay = 0;
	UseJoystick = true;
	ScaleXYZ = 100.f;
	ScaleRUV = 100.f;
	DeadZoneXYZ = 0.1f;
	DeadZoneRUV = 0.1f;
	unguard;
}

//
// Initialize the platform-specific viewport manager subsystem.
// Must be called after the Unreal object manager has been initialized.
// Must be called before any viewports are created.
//
void UNSDLClient::Init( UEngine* InEngine )
{
	guard(UNSDLClient::Init);

	// Init base.
	UClient::Init( InEngine );

	Controller = NULL;

	if ( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK ) < 0 )
	{
		appErrorf( "SDL_Init failed: %s", SDL_GetError() );
		appExit();
	}

	atexit( SDL_Quit );

	if( SDL_NumJoysticks() > 0 )
		Controller = SDL_GameControllerOpen( 0 );

	SDL_GameControllerEventState( SDL_ENABLE );

	// Not calling SDL_StartTextInput because that pops up on-screen keyboards sometimes.
	SDL_EventState( SDL_TEXTINPUT, SDL_IGNORE );

	SDL_GetDesktopDisplayMode( DefaultDisplay, &DefaultDisplayMode );

	unguard;
}

//
// Shut down the platform-specific viewport manager subsystem.
//
void UNSDLClient::Destroy()
{
	guard(UNSDLClient::Destroy);

	Controller = NULL;

	SDL_QuitSubSystem( SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK );

	UClient::Destroy();

	unguard;
}

//
// Failsafe routine to shut down viewport manager subsystem
// after an error has occured. Not guarded.
//
void UNSDLClient::ShutdownAfterError()
{
	debugf( NAME_Exit, "Executing UNSDLClient::ShutdownAfterError" );

	if( Engine && Engine->Audio )
	{
		Engine->Audio->ConditionalShutdownAfterError();
	}

	for( INT i=Viewports.Num()-1; i>=0; i-- )
	{
		UNSDLViewport* Viewport = (UNSDLViewport*)Viewports(i);
	}

	Super::ShutdownAfterError();
}

//
// Enable or disable all viewport windows that have ShowFlags set (or all if ShowFlags=0).
//
void UNSDLClient::EnableViewportWindows( DWORD ShowFlags, int DoEnable )
{
	guard(UNSDLClient::EnableViewportWindows);
	unguard;
}

//
// Show or hide all viewport windows that have ShowFlags set (or all if ShowFlags=0).
//
void UNSDLClient::ShowViewportWindows( DWORD ShowFlags, int DoShow )
{
	guard(UNSDLClient::ShowViewportWindows);
	unguard;
}

//
// Configuration change.
//
void UNSDLClient::PostEditChange()
{
	guard(UNSDLClient::PostEditChange);
	Super::PostEditChange();
	unguard;
}

// Perform background processing.  Should be called 100 times
// per second or more for best results.
//
void UNSDLClient::Poll()
{
	guard(UNSDLClient::Poll);
	unguard;
}

//
// Perform timer-tick processing on all visible viewports.  This causes
// all realtime viewports, and all non-realtime viewports which have been
// updated, to be blitted.
//
void UNSDLClient::Tick()
{
	guard(UNSDLClient::Tick);

	// Process input and blit any viewports that need blitting.
	UNSDLViewport* BestViewport = NULL;
	for( INT i=0; i<Viewports.Num(); i++ )
	{
		UNSDLViewport* Viewport = CastChecked<UNSDLViewport>(Viewports(i));
		if( !Viewport->GetWindow() )
		{
			// Window was closed via close button.
			delete Viewport;
			return;
		}
		else if (	Viewport->IsRealtime()
			&&	Viewport->SizeX && Viewport->SizeY && Viewport->HoldCount == 0
			&&	(!BestViewport || Viewport->LastUpdateTime<BestViewport->LastUpdateTime) )
		{
			BestViewport = Viewport;
		}
		// Tick input for this viewport and see if it wants to die.
		if( Viewport->TickInput() )
		{
			delete Viewport;
			return;
		}
	}

	if( BestViewport )
		BestViewport->Repaint( 1 );

	unguard;
}

//
// Make this viewport the current one.
// If Viewport=0, makes no viewport the current one.
//
void UNSDLClient::MakeCurrent( UViewport* InViewport )
{
	guard(UNSDLClient::MakeCurrent);
	for( INT i=0; i<Viewports.Num(); i++ )
	{
		UViewport* OldViewport = Viewports(i);
		if( OldViewport->Current && OldViewport!=InViewport )
		{
			OldViewport->Current = 0;
			OldViewport->UpdateWindowFrame();
		}
	}
	if( InViewport )
	{
		InViewport->Current = 1;
		InViewport->UpdateWindowFrame();
	}
	unguard;
}

//
// Create a new viewport.
//
UViewport* UNSDLClient::NewViewport( const FName Name )
{
	guard(UNSDLClient::NewViewport);
	return new( this, Name )UNSDLViewport( NULL, this );
	unguard;
}

//
// Return the current viewport.  Returns NULL if no viewport has focus.
//
UViewport* UNSDLClient::CurrentViewport()
{
	guard(UNSDLClient::CurrentViewport);

	// Note: FullscreenViewport tracking removed - Unreal 1 specific
	SDL_Window *MouseWin = SDL_GetMouseFocus();
	UNSDLViewport* TestViewport = NULL;
	for( int i=0; i<Viewports.Num(); i++ )
	{
		TestViewport = (UNSDLViewport*)Viewports(i);
		if( TestViewport->Current || (SDL_Window*)TestViewport->GetWindow() == MouseWin )
			return TestViewport;
	}

	return NULL;

	unguard;
}

//
// Assemble display mode array.
//
static INT Compare( const SDL_Rect& A, const SDL_Rect& B )
{
	if( A.w == B.w )
		return A.h - B.h;
	return A.w - B.w;
}
static UBOOL operator==( const SDL_Rect& A, const SDL_Rect& B )
{
	return A.w == B.w && A.h == B.h;
}
const TArray<SDL_Rect>& UNSDLClient::GetDisplayResolutions()
{
	guard(UNSDLClient::GetDisplayResolutions)

	DisplayResolutions.Empty();

	SDL_DisplayMode Mode;
	SDL_Rect Rect = { 0, 0, 0, 0 };
	if( SDL_GetDesktopDisplayMode( DefaultDisplay, &Mode ) == 0 )
	{
		// we're only interested in size
		Rect.w = Mode.w;
		Rect.h = Mode.h;
		DisplayResolutions.AddItem( Rect );
	}

	const INT NumModes = SDL_GetNumDisplayModes( DefaultDisplay );
	
	for( INT i=0; i<NumModes; ++i )
	{
		if( SDL_GetDisplayMode( DefaultDisplay, i, &Mode ) == 0 )
		{
			// we're only interested in size
			Rect.w = Mode.w;
			Rect.h = Mode.h;
			DisplayResolutions.AddUniqueItem( Rect );
		}
	}

	// Sort display resolutions
	if( DisplayResolutions.Num() > 0 )
		appQsort( &DisplayResolutions(0), DisplayResolutions.Num(), sizeof(SDL_Rect), (int(CDECL *)(const void*, const void*))Compare );

	return DisplayResolutions;

	unguard;
}

//
// Command line.
//
UBOOL UNSDLClient::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(UNSDLClient::Exec);

	if( ParseCommand( &Cmd, TEXT("EndFullscreen") ) )
	{
		EndFullscreen();
		return 1;
	}
	else if( ParseCommand( &Cmd, TEXT("GetRes") ) )
	{
#ifdef PLATFORM_ANDROID
		// UT99_ANDROID_V166_REAL_RENDER_RESOLUTIONS:
		// Offer actual SurfaceHolder backbuffer modes. The Android View remains
		// fullscreen and scales the selected render buffer to the display. Keep labels
		// space-free because UMenu splits GetRes on ASCII spaces.
		INT NativeW = 1920;
		INT NativeH = 1080;
		SDL_DisplayMode AndroidMode;
		if( SDL_GetDesktopDisplayMode( 0, &AndroidMode ) == 0 && AndroidMode.w > 0 && AndroidMode.h > 0 )
		{
			NativeW = Max( AndroidMode.w, AndroidMode.h );
			NativeH = Min( AndroidMode.w, AndroidMode.h );
		}
		auto Round16V166 = []( INT V ) -> INT
		{
			if( V < 320 ) V = 320;
			return ((V + 8) / 16) * 16;
		};
		INT W1080 = Round16V166( (INT)((DOUBLE)NativeW * 1080.0 / (DOUBLE)Max( 1, NativeH ) + 0.5) );
		INT W720  = Round16V166( (INT)((DOUBLE)NativeW *  720.0 / (DOUBLE)Max( 1, NativeH ) + 0.5) );
		FString Result = TEXT("Native");
		auto AddModeV166 = [&Result]( INT W, INT H )
		{
			if( W < 320 || H < 240 )
				return;
			W &= ~1;
			H &= ~1;
			FString Item = FString::Printf( TEXT("%dx%d"), W, H );
			if( Result.InStr( *Item ) < 0 )
			{
				Result += TEXT(" " );
				Result += Item;
			}
		};
		AddModeV166( W1080, 1080 );
		AddModeV166( W720, 720 );
		AddModeV166( 1440, 1080 );
		AddModeV166( 1024, 768 );
		Ar.Log( *Result );
		return 1;
#else
		FString Result;
		GetDisplayResolutions();
		for( INT i=0; i<DisplayResolutions.Num(); ++i )
		{
			const SDL_Rect& Mode = DisplayResolutions(i);
			if( i > 0 )
				Result += TEXT(" ");
			Result += FString::Printf( TEXT("%ix%i"), Mode.w, Mode.h );
		}
		Ar.Log( *Result );
		return 1;
#endif
	}
	else if( UClient::Exec( Cmd, Ar ) )
	{
		return 1;
	}

	return 0;

	unguard;
}

void UNSDLClient::EndFullscreen()
{
	// End fullscreen for all viewports
	for( INT i=0; i<Viewports.Num(); i++ )
	{
		UNSDLViewport* Viewport = Cast<UNSDLViewport>(Viewports(i));
		if( Viewport && Viewport->IsFullscreen() )
		{
			Viewport->EndFullscreen();
		}
	}
}

//
// Try switching to a new rendering device.
//
void UNSDLClient::TryRenderDevice( UViewport* Viewport, const char* ClassName, UBOOL Fullscreen )
{
	guard(UNSDLClient::TryRenderDevice);

	UNSDLViewport* SDLViewport = Cast<UNSDLViewport>(Viewport);
	if( !SDLViewport )
		return;

	// Shut down current rendering device.
	if( Viewport->RenDev )
	{
		Viewport->RenDev->Exit();
		delete Viewport->RenDev;
		Viewport->RenDev = NULL;
	}

	// Find device driver.
	FString ClassNameStr = appFromAnsi(ClassName);
	UClass* RenderClass = UObject::StaticLoadClass( URenderDevice::StaticClass(), NULL, *ClassNameStr, NULL, LOAD_NoWarn, NULL );
	if( RenderClass )
	{
		Viewport->RenDev = ConstructObject<URenderDevice>( RenderClass );
		if( Engine->Audio && !GIsEditor )
			Engine->Audio->SetViewport( NULL );
		if( Viewport->RenDev->Init( Viewport, Viewport->SizeX, Viewport->SizeY, Viewport->ColorBytes, Fullscreen ) )
		{
			Viewport->Actor->GetLevel()->DetailChange( Viewport->RenDev->HighDetailActors );
			if( Fullscreen )
				SDLViewport->MakeFullscreen( FullscreenViewportX, FullscreenViewportY, 1 );
		}
		else
		{
			debugf( NAME_Log, LocalizeError("Failed3D") );
			delete Viewport->RenDev;
			Viewport->RenDev = NULL;
		}
		if( Engine->Audio && !GIsEditor )
			Engine->Audio->SetViewport( Viewport );
	}

	unguard;
}
