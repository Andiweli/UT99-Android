/*=============================================================================
	Launch.cpp: Game launcher.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Brandon Reinhart.
=============================================================================*/

#include "LaunchPrivate.h"
#ifdef PLATFORM_SDL
#include <SDL2/SDL.h>
#endif
#ifdef PLATFORM_ANDROID
#include <android/log.h>
#define UT99_ANDROID_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "UT99Launch", __VA_ARGS__)
#define UT99_ANDROID_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "UT99Launch", __VA_ARGS__)
#else
#define UT99_ANDROID_LOGI(...)
#define UT99_ANDROID_LOGE(...)
#endif

#ifdef PLATFORM_ANDROID
// UT99_ANDROID_V65_AUDIO_LINK_FIX:
// Audio is now linked from CMake (whole-archive Audio target).  Do not reference
// package internals here; if the Audio target is not linked, hard externs make
// the Android build fail before we can fall back to silent startup.
static void ForceLinkAndroidAudioPackage()
{
	UT99_ANDROID_LOGI("UT99_ANDROID_V65_AUDIO_LINK_FIX CMake owns Audio package linkage");
}
#endif
/*-----------------------------------------------------------------------------
	Global variables.
-----------------------------------------------------------------------------*/

extern "C" {TCHAR THIS_PACKAGE[64]=TEXT("Launch");}

// Memory allocator.
#include "FMallocAnsi.h"
FMallocAnsi Malloc;

// Log file.
#include "FOutputDeviceFile.h"
FOutputDeviceFile Log;

// Error handler.
#include "FOutputDeviceAnsiError.h"
FOutputDeviceAnsiError Error;

// Feedback.
#include "FFeedbackContextAnsi.h"
FFeedbackContextAnsi Warn;

// File manager.
#include "FFileManagerLinux.h"
FFileManagerLinux FileManager;

// Config.
#include "FConfigCacheIni.h"

#ifdef PLATFORM_DREAMCAST
#include <kos.h>
#include <malloc.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <kos/thread.h>
#define MAIN_STACK_SIZE (32 * 1024)  
#ifdef DREAMCAST_USE_FATFS
extern "C" {
#include <fatfs.h>
}
#endif
KOS_INIT_FLAGS( INIT_DEFAULT | INIT_CDROM | INIT_CONTROLLER | INIT_KEYBOARD | INIT_MOUSE | INIT_VMU | INIT_NET );
#endif

#ifdef PLATFORM_DREAMCAST
// fix thread stack underrun
static void init_thread_stack(void) {
    kthread_t *current = thd_get_current();
    if (current) {
        void *new_stack = malloc(MAIN_STACK_SIZE);
        if (new_stack) {
            current->stack = new_stack;
            current->stack_size = MAIN_STACK_SIZE;
            current->flags |= THD_OWNS_STACK;
        }
    }
}

// What dbgio device was active at startup
DLL_EXPORT const char* GStartupDbgDev = nullptr;

//
// Display error and lock up.
//
void FatalError( const char* Fmt, ... ) __attribute__((noreturn));
void FatalError( const char* Fmt, ... )
{
	char Msg[2048];

	va_list Args;
	va_start( Args, Fmt );
	vsnprintf( Msg, sizeof( Msg ), Fmt, Args );
	va_end( Args );


	printf( "%s\n\n", Msg );

	arch_stk_trace( 2 );

	while (true)
		thd_sleep( 100 );
}

//
// Handle assertion failure.
//
void HandleAssertFail( const char* File, int Line, const char* Expr, const char* Msg, const char* Func )
{
	FatalError( "ASSERTION FAILED:\nLoc: %s:%d (%s)\nExpr: %s\n%s", File, Line, Func, Expr, Msg);
}

void HandleIrqException( irq_t Code, irq_context_t* Context, void* Data )
{
	bfont_draw_str_vram_fmt( 8, 8, true, "UNHANDLED EXCEPTION 0x%08x", Code );
	bfont_draw_str_vram_fmt( 8, 32, true, "PC: %p PR: %p", (void*)Context->pc, (void*)Context->pr );
	bfont_draw_str_vram_fmt( 8, 56, true, "SR: %p R0: %p", (void*)Context->sr, (void*)Context->r[0] );
	bfont_draw_str_vram_fmt( 8, 80, true, "PROBABLY OUT OF MEMORY");

	arch_stk_trace_at( Context->r[14], 0 );

	volatile INT Dummy = 1;
	while (Dummy);
}

#endif

/*-----------------------------------------------------------------------------
	Initialization
-----------------------------------------------------------------------------*/

//
// Creates a UEngine object.
//
static UEngine* InitEngine()
{
	guard(InitEngine);
	UT99_ANDROID_LOGI("InitEngine: entered");
	DOUBLE LoadTime = appSeconds();
	UT99_LOADPROF_LOG("INITENGINE_BEGIN");

	// Set exec hook.
	GExec = NULL;

	// Update first-run.
	INT FirstRun=0;
	if (FirstRun<ENGINE_VERSION)
		FirstRun = ENGINE_VERSION;
	GConfig->SetInt( TEXT("FirstRun"), TEXT("FirstRun"), FirstRun );

	// Create the global engine object.
	UClass* EngineClass;
	UT99_ANDROID_LOGI("InitEngine: before StaticLoadClass Engine.GameEngine");
	EngineClass = UObject::StaticLoadClass(
		UGameEngine::StaticClass(), NULL,
		TEXT("ini:Engine.Engine.GameEngine"),
		NULL, LOAD_NoFail, NULL
	);
	UT99_ANDROID_LOGI("InitEngine: after StaticLoadClass EngineClass=%p", EngineClass);
	if( !EngineClass )
	{
		UT99_ANDROID_LOGE("InitEngine: EngineClass is NULL");
	}
	UT99_ANDROID_LOGI("InitEngine: before ConstructObject");
	UEngine* Engine = ConstructObject<UEngine>( EngineClass );
	UT99_ANDROID_LOGI("InitEngine: after ConstructObject Engine=%p", Engine);
	UT99_ANDROID_LOGI("InitEngine: before Engine->Init");
	Engine->Init();
	UT99_ANDROID_LOGI("InitEngine: after Engine->Init");
	UT99_LOADPROF_LOG("INITENGINE_END ms=%.3f", (appSeconds()-LoadTime)*1000.0);
#ifdef PLATFORM_DREAMCAST
	malloc_stats();
#endif
	debugf( TEXT("Startup time: %f seconds."), appSeconds()-LoadTime );

	return Engine;
	unguard;
}

//
// Handle an error.
//
void HandleError( const char* Exception )
{
	UT99_ANDROID_LOGE("HandleError Exception=%s", Exception ? Exception : "<null>");
	UT99_ANDROID_LOGE("GErrorHist=%s", GErrorHist);
	GIsGuarded=0;
	GIsCriticalError=1;
	debugf( NAME_Exit, "Shutting down after catching exception" );
	debugf( NAME_Exit, "Exiting due to exception" );
	GErrorHist[ARRAY_COUNT(GErrorHist)-1]=0;
#ifdef PLATFORM_SDL
	SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, LocalizeError("Critical"), GErrorHist, SDL_GetKeyboardFocus() );
#elif defined(PLATFORM_DREAMCAST)
	if( Exception )
		FatalError( "FATAL ERROR:\n%s\n\n%s", Exception, GErrorHist );
	else
		FatalError( "FATAL ERROR:\n%s", GErrorHist );
#endif
}

/*-----------------------------------------------------------------------------
	Main Loop
-----------------------------------------------------------------------------*/

//
// X game message loop.
//
static void MainLoop( UEngine* Engine )
{
	guard(MainLoop);
	check(Engine);

	// Loop while running.
	GIsRunning = 1;
	DOUBLE OldTime = appSeconds();
	DOUBLE SecondStartTime = OldTime;
	DOUBLE UT99MainLoopStart = OldTime;
	UBOOL UT99FirstTickLogged = 0;
	INT TickCount = 0;
	while( GIsRunning && !GIsRequestingExit )
	{
		// Update the world.
		guard(UpdateWorld);
		DOUBLE NewTime   = appSeconds();
		FLOAT  DeltaTime = NewTime - OldTime;
		Engine->Tick( DeltaTime );
		if( !UT99FirstTickLogged )
		{
			UT99FirstTickLogged = 1;
			UT99_LOADPROF_LOG("FIRST_TICK delta_ms=%.3f since_mainloop_ms=%.3f", DeltaTime*1000.0, (appSeconds()-UT99MainLoopStart)*1000.0);
		}
		if( GWindowManager )
			GWindowManager->Tick( DeltaTime );
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

	unguard;
}

/*-----------------------------------------------------------------------------
	Main.
-----------------------------------------------------------------------------*/

//
// Simple copy.
// 

void SimpleCopy(TCHAR* fromfile, TCHAR* tofile)
{
	INT c;
	FILE* from;
	FILE* to;
	from = fopen(fromfile, "r");
	if (from == NULL)
		return;
	to = fopen(tofile, "w");
	if (to == NULL)
	{
		printf("Can't open or create %s", tofile);
		return;
	}
	while ((c = getc(from)) != EOF)
		putc(c, to);
	fclose(from);
	fclose(to);
}

//
// Exit wound.
//
int CleanUpOnExit(int ErrorLevel)
{
	UT99_ANDROID_LOGI("CleanUpOnExit(%d)", ErrorLevel);
	GFileManager->Delete(TEXT("Running.ini"),0,0);
	debugf( NAME_Title, LocalizeGeneral("Exit") );
	appPreExit();
	GIsGuarded = 0;

	// Shutdown.
	appExit();
	GIsStarted = 0;

	// Restore the user's configuration.
	TCHAR baseconfig[PATH_MAX] = TEXT("");
	if( getcwd(baseconfig, sizeof(baseconfig)) == NULL )
	{
		appStrcpy( baseconfig, TEXT("./User.ini") );
	}
	else
	{
		appStrcat(baseconfig, "/User.ini");
	}

	TCHAR userconfig[PATH_MAX] = TEXT("");
	sprintf(userconfig, "~/.utconf");

	TCHAR exec[PATH_MAX] = TEXT("");
	sprintf(exec, "cp -f %s %s", baseconfig, userconfig);
	//system( exec );

	return ErrorLevel;
}

//
// Entry point.
//
int main( int argc, char* argv[] )
{
	UT99_ANDROID_LOGI("main entered argc=%d argv0=%s", argc, (argc>0 && argv && argv[0]) ? argv[0] : "<null>");
#ifdef PLATFORM_DREAMCAST
	// fix thread stack underrun
	init_thread_stack();
	// Redirect dbgio to the framebuffer if we're not already using dcload.
	GStartupDbgDev = dbgio_dev_get();
	if( !GStartupDbgDev || !appStrstr( GStartupDbgDev, "dcl" ) )
		dbgio_dev_select( "fb" );
	assert_set_handler( HandleAssertFail );
	irq_set_handler( EXC_UNHANDLED_EXC, HandleIrqException, nullptr );
#ifdef DREAMCAST_USE_FATFS
	if( fs_fat_mount_sd() == 0 )
	{
		printf( "SD card found, will try to load data from there\n" );
	}
	else
	{
		// failed
		printf( "SD card not found, will default to CD\n" );
		sd_shutdown();
		fs_fat_shutdown();
	}
#endif
#endif
	try
	{
	guard(main);
	
	INT ErrorLevel = 0;
	GIsStarted	   = 1;
#ifndef PLATFORM_DREAMCAST
	// Set module name.
	appStrcpy( GModule, argv[0] );

	// Set the package name.
	appStrcpy( THIS_PACKAGE, appPackage() );	
#endif
	// Get the command line.
	UT99_ANDROID_LOGI("building command line");
	TCHAR CmdLine[1024], *CmdLinePtr=CmdLine;
	*CmdLinePtr = 0;
	for( INT i=1; i<argc; i++ )
	{
		if( i>1 )
			appStrcat( CmdLine, " " );
		appStrcat( CmdLine, argv[i] );
	}
#ifndef PLATFORM_DREAMCAST
	// Take care of .ini swapping.
	TCHAR userconfig[PATH_MAX] = TEXT("");
	sprintf(userconfig, "~/.utconf");

	TCHAR baseconfig[PATH_MAX] = TEXT("");
	if( getcwd(baseconfig, sizeof(baseconfig)) == NULL )
	{
		appStrcpy( baseconfig, TEXT("./User.ini") );
	}
	else
	{
		appStrcat(baseconfig, "/User.ini");
	}

	TCHAR exec[PATH_MAX] = TEXT("");
	sprintf(exec, "cp -f %s %s", userconfig, baseconfig);
	//system( exec );
#endif
	//SimpleCopy( userconfig, baseconfig );

	UT99_ANDROID_LOGI("CmdLine=%s", CmdLine);

	// Init core.
	UT99_ANDROID_LOGI("before appInit");
	GIsClient = 1; 
	GIsGuarded = 0;
	appInit( TEXT("UnrealTournament"), CmdLine, &Malloc, &Log, &Error, &Warn, &FileManager, FConfigCacheIni::Factory, 1 );
	UT99_ANDROID_LOGI("after appInit");
#ifdef PLATFORM_ANDROID
	ForceLinkAndroidAudioPackage();
#endif

	// Init mode.
	GIsServer		= 1;
	GIsClient		= !ParseParam(appCmdLine(), TEXT("SERVER"));
	GIsEditor		= 0;
	GIsScriptable	= 1;
#ifdef PLATFORM_DREAMCAST
	GLazyLoad  		= 1;
#else
	GLazyLoad		= !GIsClient || ParseParam(appCmdLine(), TEXT("LAZY"));
#endif
#ifdef PLATFORM_DREAMCAST
		Warn.AuxOut = GLog;
		GLog		= &Warn;
#else
		// Init console log.
	if (ParseParam(CmdLine, TEXT("LOG")))
	{
			Warn.AuxOut = GLog;
			GLog		= &Warn;
	}
#endif

	// Init engine.
	UT99_ANDROID_LOGI("before InitEngine");
	UEngine* Engine = InitEngine();
	UT99_ANDROID_LOGI("after InitEngine Engine=%p", Engine);
	if( Engine )
	{
		debugf( NAME_Title, LocalizeGeneral("Run") );

		// Optionally Exec and exec file.
		FString Temp;
		if( Parse(CmdLine, TEXT("EXEC="), Temp) )
		{
			Temp = FString(TEXT("exec ")) + Temp;
			if( Engine->Client && Engine->Client->Viewports.Num() && Engine->Client->Viewports(0) )
				Engine->Client->Viewports(0)->Exec( *Temp, *GLog );
		}

		// Start main engine loop.
		debugf( TEXT("Entering main loop.") );
		if ( !GIsRequestingExit )
			MainLoop( Engine );
			UT99_ANDROID_LOGI("MainLoop returned");
	}

	// Finish up.
	return CleanUpOnExit(ErrorLevel);

	unguard;
	}
	catch (...)
	{
		UT99_ANDROID_LOGE("caught exception in main, GErrorHist=%s", GErrorHist);
		// Chained abort.  Do cleanup.
		HandleError(NULL);
		return CleanUpOnExit(1);
	}
}