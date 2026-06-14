/*=============================================================================
	UnIX: Unix port of UnVcWin32.cpp.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Cloned by Mike Danylchuk
		* Severely amputated and mutilated by Brandon Reinhart
		* Surgically altered by Jack Porter
		* Mangled and rehabilitated by Brandon Reinhart
=============================================================================*/
#if __GNUG__


#if defined(PLATFORM_SDL)
#include <SDL2/SDL.h>
#elif defined(PLATFORM_DREAMCAST)
#include <kos.h>
#include <dirent.h>
#else
#error "Unsupported platform."
#endif

// Standard includes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <float.h>
#include <time.h>

// System includes.
#include <unistd.h>
#include <dirent.h>
#include <utime.h>
#include <sys/time.h>
#include <sys/stat.h>
#ifndef UNREAL_STATIC
#include <dlfcn.h>
#endif
#include <netdb.h>

// Core includes.
#include "CorePrivate.h"
#include "UnUnix.h"
/*-----------------------------------------------------------------------------
	Globals
-----------------------------------------------------------------------------*/

// Module
ANSICHAR GModule[32];

// Environment
extern char **environ;

/*-----------------------------------------------------------------------------
	USystem.
-----------------------------------------------------------------------------*/

//
// System manager.
//
static void Recurse()
{
	guard(Recurse);
	Recurse();
	unguard;
}
USystem::USystem()
:	SavePath	( E_NoInit )
,	CachePath	( E_NoInit )
,	CacheExt	( E_NoInit )
,	Paths		( E_NoInit )
,	Suppress	( E_NoInit )
{}
void USystem::StaticConstructor()
{
	guard(USystem::StaticConstructor);

	new(GetClass(),TEXT("PurgeCacheDays"),      RF_Public)UIntProperty   (CPP_PROPERTY(PurgeCacheDays    ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("SavePath"),            RF_Public)UStrProperty   (CPP_PROPERTY(SavePath          ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("CachePath"),           RF_Public)UStrProperty   (CPP_PROPERTY(CachePath         ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("CacheExt"),            RF_Public)UStrProperty   (CPP_PROPERTY(CacheExt          ), TEXT("Options"), CPF_Config );

	UArrayProperty* A = new(GetClass(),TEXT("Paths"),RF_Public)UArrayProperty( CPP_PROPERTY(Paths), TEXT("Options"), CPF_Config );
	A->Inner = new(A,TEXT("StrProperty0"),RF_Public)UStrProperty;

	UArrayProperty* B = new(GetClass(),TEXT("Suppress"),RF_Public)UArrayProperty( CPP_PROPERTY(Suppress), TEXT("Options"), CPF_Config );
	B->Inner = new(B,TEXT("NameProperty0"),RF_Public)UNameProperty;

	unguard;
}
UBOOL USystem::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if( ParseCommand(&Cmd,TEXT("MEMSTAT")) )
	{
		//!UNIX No MEMSTAT command.
		Ar.Logf( TEXT("MEMSTAT command not available.") );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("EXIT")) )
	{
		Ar.Log( TEXT("Closing by request") );
		appRequestExit( 0 );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("APP")) )
	{
		//!UNIX No APP command.
		Ar.Logf( TEXT("APP command not available.") );
		return 1;
	}
	else if( ParseCommand( &Cmd, TEXT("RELAUNCH") ) )
	{
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
		// UT99_ANDROID_V109_RELAUNCH_NO_UCC:
		// RELAUNCH is a desktop dedicated-server helper that forks ./ucc. Android
		// cannot launch that process from the app sandbox. UViewport::Exec converts
		// the UT99 Dedicated button to a listen-server travel first; keep this
		// fallback non-fatal in case RELAUNCH reaches USystem directly.
		debugf( TEXT("UT99_ANDROID_V109_RELAUNCH_NO_UCC ignored: %s"), Cmd );
		return 1;
#elif !defined(PLATFORM_DREAMCAST)
		debugf( TEXT("Relaunch: %s"), Cmd );
		GConfig->Flush( 0 );

		// Fork out a new process using Cmd as arguments.
		TCHAR* EndArg0 = appStrstr( Cmd, " " );
		INT CmdLen = appStrlen( Cmd );
		INT RestLen = appStrlen( EndArg0 );
		TCHAR Arg0[256];
		appStrncpy( Arg0, Cmd, (CmdLen - RestLen) + 1 );
		
		pid_t pid = fork();
		if (pid != 0)
		{
			appSleep(3);
			INT error = execl("./ucc", "ucc", "server", Arg0, (char*)NULL );
			if (error == -1)
				appErrorf( TEXT("Failed to launch process.") );			
		} else {
			appRequestExit( 0 );
		}
#endif
		return 1;
	}
	else if( ParseCommand( &Cmd, TEXT("DEBUG") ) )
	{
		if( ParseCommand(&Cmd,TEXT("CRASH")) )
		{
			appErrorf( TEXT("%s"), TEXT("Unreal crashed at your request") );
			return 1;
		}
		else if( ParseCommand( &Cmd, TEXT("GPF") ) )
		{
			Ar.Log( TEXT("Unreal crashing with voluntary GPF") );
			*(int *)NULL = 123;
			return 1;
		}
		else if( ParseCommand( &Cmd, TEXT("RECURSE") ) )
		{
			Ar.Logf( TEXT("Recursing") );
			Recurse();
			return 1;
		}
		else if( ParseCommand( &Cmd, TEXT("EATMEM") ) )
		{
			Ar.Log( TEXT("Eating up all available memory") );
			while( 1 )
			{
				void* Eat = appMalloc(65536,TEXT("EatMem"));
				memset( Eat, 0, 65536 );
			}
			return 1;
		}
		else return 0;
	}
	else return 0;
}
IMPLEMENT_CLASS(USystem);

/*-----------------------------------------------------------------------------
	Exit.
-----------------------------------------------------------------------------*/

//
// Immediate exit.
//
CORE_API void appRequestExit( UBOOL Force )
{
	guard(appForceExit);
	debugf( TEXT("appRequestExit(%i)"), Force );
	if( Force )
	{
		// Force immediate exit. Dangerous because config code isn't flushed, etc.
		exit( 1 );
	}
	else
	{
		// Tell the platform specific code we want to exit cleanly from the main loop.
		//!UNIX No quit message in UNIX.
		GIsRequestingExit = 1;
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	Clipboard.
-----------------------------------------------------------------------------*/

CORE_API void ClipboardCopy( const TCHAR* Str )
{
	guard(ClipboardCopy);
	//!UNIX Not supported in UNIX.
	unguard;
}

CORE_API void ClipboardPaste( FString& Result )
{
	guard(ClipboardPasteString);
	//!UNIX Not supported in UNIX.
	unguard;
}

/*-----------------------------------------------------------------------------
	Shared libraries.
-----------------------------------------------------------------------------*/
#ifdef UNREAL_STATIC

//
// Load a library.
//
CORE_API void* appGetDllHandle( const char* Filename )
{
	guard(appGetDllHandle);

	char Test[1024];
	const char* PackageName = Filename;
	char* Cur;

	check(Filename);

	// Get GLoadedPackage symbol name from full path.
	while( ( Cur = appStrchr( PackageName, '/' ) ) != NULL )
		PackageName = Cur + 1;
	while( ( Cur = appStrchr( PackageName, '\\' ) ) != NULL )
		PackageName = Cur + 1;
	appSprintf( Test, "GLoaded%s", PackageName );
	if( (Cur = appStrchr( Test, '.' )) != NULL )
		*Cur = '\0';

	return appGetStaticExport( Test );

	unguard;
}

#else
void* appGetDllHandle( const TCHAR* Filename )
{
	guard(appGetDllHandle);
	check(Filename);
	const TCHAR* PackageName = Filename;
	TCHAR* Cur;
	TCHAR* Test = appStaticString1024();
	void* Result;
	TCHAR* Error;

	// Get GLoadedPackage symbol name from full path.
 	while( (Cur = appStrchr( PackageName, '/' )) != NULL )
		PackageName = Cur + 1;
 	while( (Cur = appStrchr( PackageName, '\\' )) != NULL )
		PackageName = Cur + 1;
	appSprintf( Test, "GLoaded%s", PackageName );
	if( (Cur = appStrchr( Test, '.' )) != NULL )
		*Cur = '\0';

	dlerror();	// Clear any error condition.

	// Check if the library was linked to the executable.
	Result = (void*)dlopen( NULL, RTLD_NOW );
	Error = dlerror();
	if( Error != NULL )
		debugf( "dlerror(): %s", Error );
	else
	{
		(void*)dlsym( Result, appToAnsi(Test) );
		Error = dlerror();
		if( Error == NULL )
			return Result;
	}

	// Load the new library.
	Result = (void*)dlopen( Filename, RTLD_NOW );
	if( Result == NULL )
	{
		TCHAR Temp[256];
		appStrcpy( Temp, Filename );
		appStrcat( Temp, DLLEXT );
		Result = (void*)dlopen( Temp, RTLD_NOW );
	}

	return Result;
	unguard;
}
#endif
//
// Free a library.
//
void appFreeDllHandle( void* DllHandle )
{
	guard(appFreeDllHandle);
	check(DllHandle);

#if defined(UNREAL_STATIC)
	// nothing
#elif defined(PLATFORM_WIN32)
	FreeLibrary( (HMODULE)DllHandle );
#else
	dlclose( DllHandle );
#endif
	unguard;
}

//
// Lookup the address of a shared library function.
//
void* appGetDllExport( void* DllHandle, const TCHAR* ProcName )
{
	guard(appGetDllExport);
	check(DllHandle);
	check(ProcName);

	void* Result;
	TCHAR* Error;
#if defined(UNREAL_STATIC)
	return appGetStaticExport( ProcName );
#else
	dlerror();	// Clear any error condition.
	Result = (void*)dlsym( DllHandle, appToAnsi(ProcName) );
	Error = dlerror();
	if( Error != NULL )
		debugf( "dlerror: %s", Error );
	return Result;
#endif
	unguard;
}

//
// Break the debugger.
//
#ifndef DEFINED_appDebugBreak
void appDebugBreak()
{
	guard(appDebugBreak);
	//!UNIX Not implemented in UNIX.
	unguard;
}
#endif

/*-----------------------------------------------------------------------------
	External processes.
-----------------------------------------------------------------------------*/

void appCreateProc( const TCHAR* URL, const TCHAR* Parms )
{
#ifndef PLATFORM_DREAMCAST
	guard(appCreateProc);

	debugf( TEXT("Create Proc: %s %s"), URL, Parms );

	TCHAR LocalParms[PATH_MAX] = TEXT("");
	appStrcpy(LocalParms, Parms);

	char* argv[128];
	argv[0] = (char*) URL;
	argv[1] = (char*) LocalParms;
	INT j = 2;
	INT k = appStrlen(LocalParms) - 1;
	for (INT i=0; i<k; i++)
	{
		if (LocalParms[i] == ' ')
		{
			LocalParms[i] = 0;
			argv[j++] = LocalParms + i + 1;
		}
	}
	argv[j] = 0;

	pid_t pid = fork(); 
	if (pid != 0)
		INT error = execv( URL, argv );

	unguard;
#endif
}

/*-----------------------------------------------------------------------------
	Timing.
-----------------------------------------------------------------------------*/

//
// String timestamp.
//
CORE_API const TCHAR* appTimestamp()
{
	guard(appTimestamp);

	TCHAR* 		Result = appStaticString1024();
	time_t		CurTime;
	struct tm*	SysTime;

	CurTime = time( NULL );
	SysTime = localtime( &CurTime );
	appSprintf( Result, asctime( SysTime ) );
	// Strip newline.
	Result[appStrlen( Result )-1] = '\0';
	return Result;

	unguard;
}

//
// Get file time.
//
CORE_API DWORD appGetTime( const TCHAR* Filename )
{
	guard(appGetTime);

	struct utimbuf FileTime;
	if( utime(Filename,&FileTime)!=0 )
		return 0;
	//FIXME Is this the right "time" to return?
	return (DWORD)FileTime.modtime;

	unguard;
}

//
// Get time in seconds.
//
CORE_API DOUBLE appSecondsSlow()
{
	static DOUBLE InitialTime = 0.0;
	static DOUBLE TimeCounter = 0.0;
	DOUBLE NewTime;
	struct timeval TimeOfDay;

	gettimeofday( &TimeOfDay, NULL );

	// Initialize.
	if( InitialTime == 0.0 )
		 InitialTime = TimeOfDay.tv_sec + TimeOfDay.tv_usec / 1000000.0;

	// Accumulate difference to prevent wraparound.
	NewTime = TimeOfDay.tv_sec + TimeOfDay.tv_usec / 1000000.0;
	TimeCounter += NewTime - InitialTime;
	InitialTime = NewTime;

	return TimeCounter;
}

#if !DEFINED_appCycles
CORE_API DWORD appCycles()
{
	return (DWORD)appSeconds();
}
#endif

//
// Return the system time.
//
CORE_API void appSystemTime( INT& Year, INT& Month, INT& DayOfWeek, INT& Day, INT& Hour, INT& Min, INT& Sec, INT& MSec )
{
	guard(appSystemTime);

	time_t			CurTime;
	struct tm		*St;		// System time.
	struct timeval	Tv;			// Use timeval to get milliseconds.

	gettimeofday( &Tv, NULL );
	CurTime = time( NULL );
	St = localtime( &CurTime );
	
	Year		= St->tm_year + 1900;
	Month		= St->tm_mon + 1;
	DayOfWeek	= St->tm_wday;
	Day			= St->tm_mday;
	Hour		= St->tm_hour;
	Min			= St->tm_min;
	Sec			= St->tm_sec;
	MSec		= (INT) (Tv.tv_usec / 1000);

	unguard;
}

CORE_API void appSleep( FLOAT Seconds )
{
	guard(appSleep);

	INT SleepTime = (INT) (Seconds * 1000000);
	usleep( SleepTime );

	unguard;
}

/*-----------------------------------------------------------------------------
	Link functions.
-----------------------------------------------------------------------------*/

//
// Launch a uniform resource locator (i.e. http://www.epicgames.com/unreal).
// This is expected to return immediately as the URL is launched by another
// task.
//
void appLaunchURL( const TCHAR* URL, const TCHAR* Parms, FString* Error )
{
	guard(appLaunchURL);
	//!UNIX Server doesn't need to launch URLs.
	unguard;
}

/*-----------------------------------------------------------------------------
	File finding.
-----------------------------------------------------------------------------*/

//
// Clean out the file cache.
//
static INT GetFileAgeDays( const TCHAR* Filename )
{
	guard(GetFileAgeDays);
	struct stat Buf;
	INT Result = 0;

	Result = stat(TCHAR_TO_ANSI(Filename),&Buf);
	if( Result==0 )
	{
		time_t CurrentTime, FileTime;
		FileTime = Buf.st_mtime;
		time( &CurrentTime );
		DOUBLE DiffSeconds = difftime( CurrentTime, FileTime );
		return (INT)(DiffSeconds / 60.0 / 60.0 / 24.0);
	}
	return 0;
	unguard;
}

CORE_API void appCleanFileCache()
{
	guard(appCleanFileCache);

	// Delete all temporary files.
	guard(DeleteTemps);
	FString Temp = FString::Printf( TEXT("%s") PATH_SEPARATOR TEXT("*.tmp"), *GSys->CachePath );
	TArray<FString> Found = GFileManager->FindFiles( *Temp, 1, 0 );
	for( INT i=0; i<Found.Num(); i++ )
	{
		Temp = FString::Printf( TEXT("%s") PATH_SEPARATOR TEXT("%s"), *GSys->CachePath, *Found(i) );
		debugf( TEXT("Deleting temporary file: %s"), *Temp );
		GFileManager->Delete( *Temp );
	}
	unguard;

	// Delete cache files that are no longer wanted.
	guard(DeleteExpired);
	TArray<FString> Found = GFileManager->FindFiles( *(GSys->CachePath * TEXT("*") + GSys->CacheExt), 1, 0 );
	if( GSys->PurgeCacheDays )
	{
		for( INT i=0; i<Found.Num(); i++ )
		{
			FString Temp = FString::Printf( TEXT("%s") PATH_SEPARATOR TEXT("%s"), *GSys->CachePath, *Found(i) );
			INT DiffDays = GetFileAgeDays( *Temp );
			if( DiffDays > GSys->PurgeCacheDays )
			{
				debugf( TEXT("Purging outdated file from cache: %s (%i days old)"), *Temp, DiffDays );
				GFileManager->Delete( *Temp );
			}
		}
	}
	unguard;

	unguard;
}

/*-----------------------------------------------------------------------------
	Guids.
-----------------------------------------------------------------------------*/

//
// Create a new globally unique identifier.
//
CORE_API FGuid appCreateGuid()
{
	guard(appCreateGuid);

	FGuid Result;
	appGetGUID( (void*)&Result );
	return Result;

	unguard;
}

/*-----------------------------------------------------------------------------
	Clipboard
-----------------------------------------------------------------------------*/
static FString ClipboardText;
CORE_API void appClipboardCopy( const TCHAR* Str )
{
	guard(appClipboardCopy);
	ClipboardText = FString( Str );
	unguard;
}

CORE_API FString appClipboardPaste()
{
	guard(appClipboardPaste);
	return ClipboardText;
	unguard;
}
/*-----------------------------------------------------------------------------
	Command line.
-----------------------------------------------------------------------------*/

// Get startup directory.
CORE_API const char* appBaseDir()
{
	static char BaseDir[1024]="";

	if( !BaseDir[0] )
	{
#if defined(PLATFORM_ANDROID)
		const char* AndroidBaseDir = getenv("UT99_ANDROID_BASEDIR");
		if( AndroidBaseDir && AndroidBaseDir[0] )
		{
			appStrncpy( BaseDir, AndroidBaseDir, sizeof(BaseDir) );
			INT Len = appStrlen( BaseDir );
			if( Len > 0 && BaseDir[Len-1] != '/' )
				appStrcat( BaseDir, "/" );
		}
#endif
		// Get directory this executable was launched from.
#if defined(PLATFORM_SDL)
		if( !BaseDir[0] )
		{
			char* BasePath = SDL_GetBasePath();
			if( BasePath && BasePath[0] )
				appStrncpy( BaseDir, BasePath, sizeof(BaseDir) );
			if( BasePath )
				SDL_free( BasePath );
		}
#elif defined(PLATFORM_DREAMCAST)
		// try PC/SD/HDD first, then CD
		static const char* Paths[] =
		{
			"/pc/Unreal/System/",
#ifdef DREAMCAST_USE_FATFS
			"/sd/Unreal/System/",
			"/ide/Unreal/System/",
#endif
			"/cd/System/"
		};
		for( INT i = 0; i < ARRAY_COUNT( Paths ); ++i )
		{
			if( DIR* Dirp = opendir( Paths[i] ) )
			{
				strcpy( BaseDir, Paths[i] );
				closedir( Dirp );
				break;
			}
		}
#endif
		// Fallback to CWD.
		if ( !BaseDir[0] )
			strcpy( BaseDir, "./" );
	}

	return BaseDir;
}
// Get computer name.
CORE_API const TCHAR* appComputerName()
{
	guard(appComputerName);
	static TCHAR Result[256]="";
	if( !Result[0] )
	{
#ifdef PLATFORM_DREAMCAST
	net_ipv4_get_stats();
	if (!net_default_dev)
	{
		// TODO: Modem
		return 0;
	}
		appStrcpy( Result, "localhost" );
#else
		gethostname( Result, sizeof(Result) );
#endif
	}
	return Result;
	unguard;
}

// Get user name.
CORE_API const TCHAR* appUserName()
{
	guard(appUserName);
	static TCHAR Result[256]="";
	if( !Result[0] )
	{
#if defined(PLATFORM_ANDROID)
		const char* Login = getenv("USER");
		if( !Login || !Login[0] )
			Login = getenv("USERNAME");
		if( !Login || !Login[0] )
			Login = "android";
		appStrncpy( Result, Login, sizeof(Result) );
#else
		const char* Login = getlogin();
		if( !Login || !Login[0] )
			Login = "unknown";
		appStrncpy( Result, Login, sizeof(Result) );
#endif
	}
	return Result;
	unguard;
}

// Get launch package base name.
CORE_API const TCHAR* appPackage()
{
	guard(appPackage);
	return GModule;
	unguard;
}

/*-----------------------------------------------------------------------------
	App init/exit.
-----------------------------------------------------------------------------*/

//
// Platform specific initialization.
//
void appPlatformPreInit()
{
}

void appPlatformInit()
{
	guard(appPlatformInit);

	// System initialization.
	GSys = new USystem;
	GSys->AddToRoot();
	for( INT i=0; i<GSys->Suppress.Num(); i++ )
		GSys->Suppress(i).SetFlags( RF_Suppress );

	// Randomize.
	srand( (unsigned)time( NULL ) );

	// CPU speed.
	GTimestamp = 1;
#if defined(PLATFORM_DREAMCAST)
	debugf( NAME_Init, "Detected: Dreamcast / KOS" );

	// CPU speed.
	DOUBLE Frequency = 1000000.0; // we're using a microsecond timer
	GSecondsPerCycle = 1.0 / Frequency;
	debugf( NAME_Init, "CPU Timer Freq=%f Hz", (FLOAT)Frequency );

	// Get CPU info.
	GPageSize = 4096;
	GProcessorCount = 1;
#elif defined(PLATFORM_SDL)
	debugf( NAME_Init, "Detected: %s", SDL_GetPlatform() );

	// CPU speed.
	DOUBLE Frequency = SDL_GetPerformanceFrequency();
	check(Frequency!=0.0);
	GSecondsPerCycle = 1.0 / Frequency;
	debugf( NAME_Init, "CPU Timer Freq=%f Hz", (FLOAT)Frequency );

	// Get CPU info.
	GPageSize = 4096; // TODO: sysconf?
	GProcessorCount = SDL_GetCPUCount();
#endif // PLATFORM_
	unguard;
}

void appPlatformPreExit()
{
}

void appPlatformExit()
{
}

void appEnableFastMath( UBOOL Enable )
{
	guard(appEnableFastMath);

	unguard;
}

/*-----------------------------------------------------------------------------
	Pathnames.
-----------------------------------------------------------------------------*/

// Convert pathname to Unix format.
char* appUnixPath( const TCHAR* Path )
{
	guard(appUnixPath);
	TCHAR* UnixPath = appStaticString1024();
	TCHAR* Cur = UnixPath;
	appStrncpy( UnixPath, Path, 1024 );
	while( Cur = strchr( Cur, '\\' ) )
		*Cur = '/';
	return UnixPath;
	unguard;
}

/*-----------------------------------------------------------------------------
	Networking.
-----------------------------------------------------------------------------*/

unsigned long appGetLocalIP( void )
{
	static unsigned long LocalIP = 0;

	if( LocalIP==0 )
	{
#ifdef PLATFORM_DREAMCAST
		// Initialize network stats to ensure net_default_dev is set
		net_ipv4_get_stats();
		if( net_default_dev )
		{
			// Build IP from net_default_dev->ip_addr[0..3]
			LocalIP = (net_default_dev->ip_addr[0] << 0) |
			          (net_default_dev->ip_addr[1] << 8) |
			          (net_default_dev->ip_addr[2] << 16) |
			          (net_default_dev->ip_addr[3] << 24);
		}
		else
		{
			// No network device available, use loopback
			LocalIP = 0x0100007F; // 127.0.0.1 in network byte order
		}
#else
		struct hostent *Hostinfo;
		char Hostname[256];
		gethostname( Hostname, sizeof(Hostname) );
		Hostinfo = gethostbyname( Hostname );
		if( Hostinfo && Hostinfo->h_addr_list[0] )
			LocalIP = *(unsigned long*)Hostinfo->h_addr_list[0];
#endif
	}
	
	return LocalIP;
}

//
// Get time in seconds.
//
CORE_API DOUBLE appSeconds()
{
#ifdef PLATFORM_MSVC
	static LARGE_INTEGER ret;
	QueryPerformanceCounter(&ret);
	return (DOUBLE)ret.QuadPart * GSecondsPerCycle;
#elif defined(PLATFORM_SDL)
	return (DOUBLE)SDL_GetPerformanceCounter() * GSecondsPerCycle;
#elif defined(PLATFORM_DREAMCAST)
	return (DOUBLE)timer_us_gettime64() * GSecondsPerCycle;
#else
	return 0;
#endif
}

CORE_API DWORD appCycles()
{
#ifdef PLATFORM_MSVC
	static LARGE_INTEGER ret;
	QueryPerformanceCounter(&ret);
	return ret.LowPart;
#elif defined(PLATFORM_SDL)
	return SDL_GetPerformanceCounter();
#elif defined(PLATFORM_DREAMCAST)
	return (DOUBLE)timer_us_gettime64();
#else
	return 0;
#endif
}

#endif
/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
