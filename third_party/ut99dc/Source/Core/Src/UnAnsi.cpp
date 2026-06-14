/*=============================================================================
	UnFile.cpp: ANSI C core.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

// Core includes.
#include "CorePrivate.h"

// To help ANSI out.
#undef clock
#undef unclock

// ANSI C++ includes.
#include <math.h>
#include <float.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#endif

/*-----------------------------------------------------------------------------
	Time.
-----------------------------------------------------------------------------*/

//
// String timestamp.
// !! Note to self: Move to UnVcWin32.cpp
// !! Make Linux version.
//
#if _MSC_VER
CORE_API const TCHAR* appTimestamp()
{
	guard(appTimestamp);
	static TCHAR Result[1024];
	*Result = 0;
#if UNICODE
	if( GUnicodeOS )
	{
		_wstrdate( Result );
		appStrcat( Result, TEXT(" ") );
		_wstrtime( Result + appStrlen(Result) );
	}
	else
#endif
	{
		ANSICHAR Temp[1024]="";
		_strdate( Temp );
		appStrcpy( Result, appFromAnsi(Temp) );
		appStrcat( Result, TEXT(" ") );
		_strtime( Temp );
		appStrcat( Result, appFromAnsi(Temp) );
	}
	return Result;
	unguard;
}
#endif

//
// Get a GMT Ref
//
CORE_API FString appGetGMTRef()
{
	guard(appGetGMTRef);
	struct tm *newtime;
	TCHAR* GMTRef = appStaticString1024();
	time_t ltime, gtime;
	FLOAT diff;
	time( &ltime );
	newtime = gmtime( &ltime );
	gtime = mktime(newtime);
	diff = (ltime - gtime) / 3600;
	appSprintf( GMTRef, (diff>0)?TEXT("+%1.1f"):TEXT("%1.1f"), diff );
	return GMTRef;
	unguard;
}

/*-----------------------------------------------------------------------------
	Math functions.
-----------------------------------------------------------------------------*/

CORE_API DOUBLE appExp( DOUBLE Value )
{
	return exp(Value);
}
CORE_API DOUBLE appLoge( DOUBLE Value )
{
	return log(Value);
}
CORE_API DOUBLE appFmod( DOUBLE Y, DOUBLE X )
{
	return fmod(Y,X);
}
CORE_API DOUBLE appSin( DOUBLE Value )
{
	return sin(Value);
}
CORE_API DOUBLE appCos( DOUBLE Value )
{
	return cos(Value);
}
CORE_API DOUBLE appTan( DOUBLE Value )
{
	return tan(Value);
}
CORE_API DOUBLE appAtan( DOUBLE Value )
{
	return atan(Value);
}
CORE_API DOUBLE appAtan2( DOUBLE Y, DOUBLE X )
{
	return atan2(Y,X);
}
CORE_API DOUBLE appSqrt( DOUBLE Value )
{
	return sqrt(Value);
}
CORE_API DOUBLE appPow( DOUBLE A, DOUBLE B )
{
	return pow(A,B);
}
CORE_API UBOOL appIsNan( DOUBLE A )
{
#if _MSC_VER
	return _isnan(A)==1;
#else
	return isnan(A)==1;
#endif
}
CORE_API INT appRand()
{
	return rand();
}
CORE_API FLOAT appFrand()
{
	return rand() / (FLOAT)RAND_MAX;
}

#if !DEFINED_appFloor
CORE_API INT appFloor( FLOAT Value )
{
	return (INT)floor(Value);
}
#endif

#if !DEFINED_appCeil
CORE_API INT appCeil( FLOAT Value )
{
	return (INT)ceil(Value);
}
#endif

#if !DEFINED_appRound
CORE_API INT appRound( FLOAT Value )
{
	return (INT)floor(Value + 0.5);
}
#endif

/*-----------------------------------------------------------------------------
	Memory functions.
-----------------------------------------------------------------------------*/

CORE_API INT appMemcmp( const void* Buf1, const void* Buf2, INT Count )
{
	return memcmp( Buf1, Buf2, Count );
}

CORE_API UBOOL appMemIsZero( const void* V, int Count )
{
	guardSlow(appMemIsZero);
	BYTE* B = (BYTE*)V;
	while( Count-- > 0 )
		if( *B++ != 0 )
			return 0;
	return 1;
	unguardSlow;
}
#ifndef PLATFORM_DREAMCAST
CORE_API void* appMemmove( void* Dest, const void* Src, INT Count )
{
	return memmove( Dest, Src, Count );
}

CORE_API void appMemset( void* Dest, INT C, INT Count )
{
	memset( Dest, C, Count );
}
#endif
#ifndef DEFINED_appMemzero
CORE_API void appMemzero( void* Dest, INT Count )
{
	memset( Dest, 0, Count );
}
#endif

#ifndef DEFINED_appMemcpy
CORE_API void appMemcpy( void* Dest, const void* Src, INT Count )
{
	memcpy( Dest, Src, Count );
}
#endif

//
// Find files in a directory.
// Example Dir = "c:\\place\\*.ext".
// Example Result = "test.ext"
//
CORE_API TArray<FString> appFindFiles( const TCHAR* Spec )
{
	guard(appFindFiles)
	TArray<FString> Result;

#ifdef PLATFORM_WIN32
	_finddata_t Found;
	long hFind = _findfirst( Spec, &Found );
	if( hFind != -1 )
	{
		do new(Result)FString(Found.name);
		while( _findnext( hFind, &Found )!=-1 );
	}
#else
	DIR *Dirp;
	struct dirent* Direntp;
	char Path[256];
	char File[256];
	char *Filestart;
	char *Cur;
	UBOOL Match;

	// Initialize Path to Filename.
	appStrcpy( Path, Spec );

	// Convert MS "\" to Unix "/".
	for( Cur = Path; *Cur != '\0'; Cur++ )
		if( *Cur == '\\' )
			*Cur = '/';

	// Separate path and filename.
	Filestart = Path;
	for( Cur = Path; *Cur != '\0'; Cur++ )
		if( *Cur == '/' )
			Filestart = Cur + 1;

	// Store filename and remove it from Path.
	appStrcpy( File, Filestart );
	*Filestart = '\0';

	// Check for empty path.
	if (appStrlen( Path ) == 0)
		appSprintf( Path, "./" );

	// Open directory, get first entry.
	Dirp = opendir( Path );
	if (Dirp == NULL)
			return Result;
	Direntp = readdir( Dirp );

	// Check each entry.
	while( Direntp != NULL )
	{
		Match = false;

		if( appStrcmp( File, "*" ) == 0 )
		{
			// Any filename.
			Match = true;
		}
		else if( appStrcmp( File, "*.*" ) == 0 )
		{
			// Any filename with a '.'.
			if( appStrchr( Direntp->d_name, '.' ) != NULL )
				Match = true;
		}
		else if( File[0] == '*' )
		{
			// "*.ext" filename.
			if( appStrstrFs( Direntp->d_name, (File + 1) ) != NULL )
				Match = true;
		}
		else if( File[appStrlen( File ) - 1] == '*' )
		{
			// "name.*" filename.
			if( appStrnicmp( Direntp->d_name, File, appStrlen( File ) - 1 ) == 0 )
				Match = true;
		}
		else if( appStrstr( File, "*" ) != NULL )
		{
			// single str.*.str match.
			char* star = appStrstr( File, "*" );
			INT filelen = appStrlen( File );
			INT starlen = appStrlen( star );
			INT starpos = filelen - (starlen - 1);
			char prefix[256];
			appStrncpy( prefix, File, starpos );
			star++;
			if( appStrnicmp( Direntp->d_name, prefix, starpos - 1 ) == 0 )
			{
				// part before * matches
				char* postfix = Direntp->d_name + (appStrlen(Direntp->d_name) - starlen) + 1;
				if ( appStricmp( postfix, star ) == 0 )
					Match = true;
			}
		}
		else
		{
			// Literal filename.
			if( appStricmp( Direntp->d_name, File ) == 0 )
				Match = true;
		}

		// Does this entry match the Filename?
		if( Match )
		{
			// Yes, add the file name to Result.
			new(Result)FString(Direntp->d_name);
		}
	
		// Get next entry.
		Direntp = readdir( Dirp );
	}

	// Close directory.
	closedir( Dirp );
#endif

	return Result;
	unguard;
}

/*-----------------------------------------------------------------------------
	String functions.
-----------------------------------------------------------------------------*/

//
// Copy a string with length checking.
//warning: Behavior differs from strncpy; last character is zeroed.
//
TCHAR* appStrncpy( TCHAR* Dest, const TCHAR* Src, INT MaxLen )
{
	guard(appStrncpy);

#if UNICODE
	wcsncpy( Dest, Src, MaxLen );
#else
	strncpy( Dest, Src, MaxLen );
#endif
	Dest[MaxLen-1]=0;
	return Dest;

	unguard;
}

//
// Concatenate a string with length checking
//
TCHAR* appStrncat( TCHAR* Dest, const TCHAR* Src, INT MaxLen )
{
	guard(appStrncat);
	INT Len = appStrlen(Dest);
	TCHAR* NewDest = Dest + Len;
	if( (MaxLen-=Len) > 0 )
	{
		appStrncpy( NewDest, Src, MaxLen );
		NewDest[MaxLen-1] = 0;
	}
	return Dest;
	unguard;
}

//
// Standard string functions.
//
CORE_API INT appSprintf( TCHAR* Dest, const TCHAR* Fmt, ... )
{
#if _MSC_VER
	return GET_VARARGS(Dest,1024/*!!*/,Fmt);
#else
	int Result;
	GET_VARARGS_RESULT(Dest,1024,Fmt,Result);
	return Result;
#endif
}

#if _MSC_VER
CORE_API INT appGetVarArgs( TCHAR* Dest, INT Count, const TCHAR*& Fmt )
{
	va_list ArgPtr;
	va_start( ArgPtr, Fmt );
#if UNICODE
	INT Result = _vsnwprintf( Dest, Count, Fmt, ArgPtr );
#else
	INT Result = _vsnprintf( Dest, Count, Fmt, ArgPtr );
#endif
	va_end( ArgPtr );
	return Result;
}
#endif

CORE_API INT appStrlen( const TCHAR* String )
{
#if UNICODE
	return wcslen( String );
#else
	return strlen( String );
#endif
}

CORE_API TCHAR* appStrstr( const TCHAR* String, const TCHAR* Find )
{
#if UNICODE
	return (TCHAR*)wcsstr( String, Find );
#else
	return const_cast<TCHAR*>(strstr( String, Find ));
#endif
}
CORE_API char* appStrstrFs( const TCHAR* String, const TCHAR* Find )
{
#ifndef PLATFORM_CASE_SENSITIVE_FS
	return appStrstr( String, Find );
#else
	return const_cast<char*>( strcasestr(String,Find) );
#endif
}

CORE_API TCHAR* appStrchr( const TCHAR* String, INT c )
{
#if UNICODE
	return (TCHAR*)wcschr( String, c );
#else
	return const_cast<TCHAR*>(strchr( String, c ));
#endif
}

CORE_API TCHAR* appStrcat( TCHAR* Dest, const TCHAR* Src )
{
#if UNICODE
	return wcscat( Dest, Src );
#else
	return strcat( Dest, Src );
#endif
}

CORE_API INT appStrcmp( const TCHAR* String1, const TCHAR* String2 )
{
#if UNICODE
	return wcscmp( String1, String2 );
#else
	return strcmp( String1, String2 );
#endif
}

CORE_API INT appStricmp( const TCHAR* String1, const TCHAR* String2 )
{
#if UNICODE
	return _wcsicmp( String1, String2 );
#else
	return stricmp( String1, String2 );
#endif
}

CORE_API TCHAR* appStrcpy( TCHAR* Dest, const TCHAR* Src )
{
#if UNICODE
	return wcscpy( Dest, Src );
#else
	return strcpy( Dest, Src );
#endif
}

CORE_API TCHAR* appStrupr( TCHAR* String )
{
#if UNICODE
	return _wcsupr( String );
#else
	return strupr( String );
#endif
}

CORE_API INT appAtoi( const TCHAR* Str )
{
#if UNICODE
	return _wtoi( Str );
#else
	return atoi( Str );
#endif
}

CORE_API FLOAT appAtof( const TCHAR* Str )
{
	return atof( appToAnsi(Str) );
}

CORE_API INT appStrtoi( const TCHAR* Start, TCHAR** End, INT Base )
{
#if UNICODE
	return wcstoul( Start, End, Base );
#else
	return strtoul( Start, End, Base );
#endif
}

CORE_API INT appStrncmp( const TCHAR* A, const TCHAR* B, INT Count )
{
#if UNICODE
	return wcsncmp( A, B, Count );
#else
	return strncmp( A, B, Count );
#endif
}

CORE_API INT appStrnicmp( const TCHAR* A, const TCHAR* B, INT Count )
{
#if UNICODE
	return _wcsnicmp( A, B, Count );
#else
	return strnicmp( A, B, Count );
#endif
}

/*-----------------------------------------------------------------------------
	Sorting.
-----------------------------------------------------------------------------*/

#ifdef PLATFORM_DREAMCAST
static QSORT_COMPARE GQsortCompare = nullptr;
static int QsortWrapper( const void* A, const void* B )
{
	return (int)GQsortCompare(A, B);
}
#endif

CORE_API void appQsort( void* Base, INT Num, INT Width, QSORT_COMPARE Compare )
{
#ifdef PLATFORM_DREAMCAST
	GQsortCompare = Compare;
	qsort( Base, Num, Width, QsortWrapper );
#else
	qsort( Base, Num, Width, Compare );
#endif
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
