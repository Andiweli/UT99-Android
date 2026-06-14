/*=============================================================================
	FOutputDeviceAnsiError.h: Ansi stdout error output device.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#ifdef PLATFORM_DREAMCAST
// Forward declaration for Dreamcast fatal error handler
extern void FatalError( const char* Fmt, ... ) __attribute__((noreturn));
#endif

#ifdef PLATFORM_ANDROID
#include <android/log.h>
#define UT99_ANDROID_ERROR_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "UT99Error", __VA_ARGS__)
#else
#define UT99_ANDROID_ERROR_LOGE(...)
#endif
//
// ANSI stdout output device.
//
class FOutputDeviceAnsiError : public FOutputDeviceError
{
	INT ErrorPos;
	EName ErrorType;
	void LocalPrint( const TCHAR* Str )
	{
#if UNICODE
		wprintf(TEXT("%s"),Str);
#else
		printf(TEXT("%s"),Str);
#endif
	}
public:
	FOutputDeviceAnsiError()
	: ErrorPos(0)
	, ErrorType(NAME_None)
	{}
	void Serialize( const TCHAR* Msg, enum EName Event )
	{
		UT99_ANDROID_ERROR_LOGE("FOutputDeviceAnsiError::Serialize Event=%d Msg=%s", (int)Event, Msg ? Msg : "(null)");
#ifdef PLATFORM_DREAMCAST
		// Dreamcast: Don't use exceptions, just print and halt
		GIsCriticalError = 1;
		printf("[CRITICAL ERROR] %s\n", Msg);
		fflush(stdout);
		UObject::StaticShutdownAfterError();
		appStrncpy( GErrorHist, Msg, ARRAY_COUNT(GErrorHist) );
		FatalError( "FATAL ERROR:\n%s", GErrorHist );
#elif defined(_DEBUG)
		// Just display info and break the debugger.
  		debugf( NAME_Critical, TEXT("appError called while debugging:") );
		debugf( NAME_Critical, Msg );
		UObject::StaticShutdownAfterError();
  		debugf( NAME_Critical, TEXT("Breaking debugger") );
		*(BYTE*)NULL=0;
#else
		if( !GIsCriticalError )
		{
			// First appError.
			GIsCriticalError = 1;
			ErrorType        = Event;
			debugf( NAME_Critical, TEXT("appError called:") );
			debugf( NAME_Critical, Msg );

			// Shut down.
			UObject::StaticShutdownAfterError();
			appStrncpy( GErrorHist, Msg, ARRAY_COUNT(GErrorHist) );
			appStrncat( GErrorHist, TEXT("\r\n\r\n"), ARRAY_COUNT(GErrorHist) );
			ErrorPos = appStrlen(GErrorHist);
			if( GIsGuarded )
			{
				appStrncat( GErrorHist, LocalizeError("History",TEXT("Core")), ARRAY_COUNT(GErrorHist) );
				appStrncat( GErrorHist, TEXT(": "), ARRAY_COUNT(GErrorHist) );
			}
			else
			{
				HandleError();
			}
		}
		else debugf( NAME_Critical, TEXT("Error reentered: %s"), Msg );

		// Propagate the error or exit.
		if( GIsGuarded )
			throw( 1 );
		else
			appRequestExit( 1 );
#endif
	}
	void HandleError()
	{
		UT99_ANDROID_ERROR_LOGE("FOutputDeviceAnsiError::HandleError Hist=%s", GErrorHist);
#ifdef PLATFORM_DREAMCAST
		GIsGuarded       = 0;
		GIsRunning       = 0;
		GIsCriticalError = 1;
		GLogHook         = NULL;
		UObject::StaticShutdownAfterError();
		GErrorHist[ErrorType==NAME_FriendlyError ? ErrorPos : ARRAY_COUNT(GErrorHist)-1]=0;
		LocalPrint( GErrorHist );
		LocalPrint( TEXT("\n\nExiting due to error\n") );
		FatalError( "HandleError: %s", GErrorHist );
#else
		try
		{
			GIsGuarded       = 0;
			GIsRunning       = 0;
			GIsCriticalError = 1;
			GLogHook         = NULL;
			UObject::StaticShutdownAfterError();
			GErrorHist[ErrorType==NAME_FriendlyError ? ErrorPos : ARRAY_COUNT(GErrorHist)-1]=0;
			LocalPrint( GErrorHist );
			LocalPrint( TEXT("\n\nExiting due to error\n") );
		}
		catch( ... )
		{}
#endif
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
