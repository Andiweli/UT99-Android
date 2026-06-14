/*=============================================================================
	UnGnuG.h: Unreal definitions for Gnu G++. Unfinished. Unsupported.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*----------------------------------------------------------------------------
	Platform compiler definitions.
----------------------------------------------------------------------------*/


#ifdef PLATFORM_WIN32
#define __WIN32__	1
#endif

#ifndef PLATFORM_BIG_ENDIAN
#define __INTEL__	1
#define __INTEL_BYTE_ORDER__ 1
#endif

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <alloca.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <malloc.h>
#include <dirent.h>
#ifdef PLATFORM_WIN32
#include <minwindef.h>
#endif

/*----------------------------------------------------------------------------
	Platform specifics types and defines.
----------------------------------------------------------------------------*/

// Undo any Windows defines.
#undef BYTE
#undef WORD
#undef DWORD
#undef INT
#undef FLOAT
#undef MAXBYTE
#undef MAXWORD
#undef MAXDWORD
#undef MAXINT
#undef VOID
#undef CDECL

#ifndef _WINDOWS_
	#define HANDLE uintptr_t
	#define HINSTANCE uintptr_t
#endif


// Sizes.
enum {DEFAULT_ALIGNMENT = 8 }; // Default boundary to align memory allocations on.
enum {CACHE_LINE_SIZE   = 32}; // Cache line size.
#define GCC_PACK(n) __attribute__((packed,aligned(n)))
#define GCC_ALIGN(n) __attribute__((aligned(n)))

// Optimization macros

#define DISABLE_OPTIMIZATION _Pragma("GCC push_options") \
	_Pragma("GCC optimize(\"O0\")")
#define ENABLE_OPTIMIZATION  _Pragma("GCC pop_options")


// Function type macros.
#define DLL_IMPORT
#define DLL_EXPORT	
#define DLL_EXPORT_CLASS
#define VARARGS
#define CDECL
#define STDCALL
#define FORCEINLINE /* Force code to be inline */
#define ZEROARRAY 0 /* Zero-length arrays in structs */
#define __cdecl


#ifdef UNREAL_STATIC
#undef DLL_IMPORT
#define DLL_IMPORT
#endif

#ifdef PLATFORM_WIN32
#define GCC_HIDDEN
#elif !defined(UNREAL_STATIC)
#define GCC_HIDDEN __attribute__((visibility("hidden")))
#else
#define GCC_HIDDEN
#endif

#define GCC_USED __attribute__((used))

// Variable arguments.
#define GET_VARARGS(msg,len,fmt)	\
{	\
	va_list ArgPtr;	\
	va_start( ArgPtr, fmt );	\
	vsprintf( msg, fmt, ArgPtr );	\
	va_end( ArgPtr );	\
}

#define GET_VARARGS_RESULT(msg,len,fmt,result)	\
{	\
	va_list ArgPtr;	\
	va_start( ArgPtr, fmt );	\
	result = vsprintf( msg, fmt, ArgPtr );	\
	va_end( ArgPtr );	\
}

// Compiler name.
#ifdef _DEBUG
	#define COMPILER "Compiled with GCC (Debug)"
#else
	#define COMPILER "Compiled with GCC"
#endif

// Unsigned base types.
typedef uint8_t				BYTE;		// 8-bit  unsigned.
typedef uint16_t			_WORD;		// 16-bit unsigned.
typedef uint32_t 			DWORD;		// 32-bit unsigned.
typedef uint64_t 			QWORD;		// 64-bit unsigned.

// Signed base types.
typedef	int8_t				SBYTE;		// 8-bit  signed.
typedef int16_t  			SWORD;		// 16-bit signed.
typedef int32_t    			INT;		// 32-bit signed.
typedef int64_t  			SQWORD;		// 64-bit signed.

// Character types.
typedef char			    ANSICHAR;	// An ANSI character.
typedef uint16_t    		 UNICHAR;	// A unicode character.
typedef uint8_t				ANSICHARU;	// An ANSI character.
typedef uint16_t     		UNICHARU;	// A unicode character.

// Other base types.
typedef int32_t				UBOOL;		// Boolean 0 (false) or 1 (true).
typedef float				FLOAT;		// 32-bit IEEE floating point.
typedef double				DOUBLE;		// 64-bit IEEE double.
typedef size_t        		SIZE_T;     // Corresponds to C SIZE_T.

// Bitfield type.
typedef uint32_t			BITFIELD;	// For bitfields.


// Make sure characters are unsigned.
static_assert((char)-1 < 0, "char must be signed.");

// Strings.
#define LINE_TERMINATOR TEXT("\n")
#define PATH_SEPARATOR TEXT("/")
#define DLLEXT TEXT(".so")

// No VC++ asm.
#undef ASM
#define ASM 0
#undef ASMLINUX
#define ASMLINUX 0
#undef ASM3DNOW
#define ASM3DNOW 0


// NULL.
#undef NULL
#define NULL 0

// Package implementation.
#ifdef UNREAL_STATIC
	#define IMPLEMENT_PACKAGE_PLATFORM(pkgname) \
		extern "C" {BYTE GCC_USED DLL_EXPORT GLoaded##pkgname;} \
		STATIC_EXPORT( GLoaded##pkgname, GLoaded##pkgname )
#else
	#define IMPLEMENT_PACKAGE_PLATFORM(pkgname) \
		extern "C" {HINSTANCE hInstance;} \
		BYTE GLoaded##pkgname;
#endif
// Platform support options.
#define PLATFORM_NEEDS_ARRAY_NEW 1
#define FORCE_ANSI_LOG           0

// OS unicode function calling.
#define TCHAR_CALL_OS(funcW,funcA) (funcA)
#define TCHAR_TO_ANSI(str) str
#define ANSI_TO_TCHAR(str) str

// !! Fixme: This is a workaround.
#define GCC_OPT_INLINE

// Memory
#define appAlloca(size) alloca((size+7)&~7)

extern CORE_API UBOOL GTimestamp;
extern CORE_API DOUBLE GSecondsPerCycle;
CORE_API DOUBLE appSecondsSlow();
//
// Round a floating point number to an integer.
// Note that (int+.5) is rounded to (int+1).
//
#define DEFINED_appRound 1
inline INT appRound( FLOAT F )
{
	return (INT)(F);
}

//
// Converts to integer equal to or less than.
//
#define DEFINED_appFloor 1
inline INT appFloor( FLOAT F )
{
	static FLOAT Half=0.5;
	return (INT)(F - Half);
}

//
// CPU cycles, related to GSecondsPerCycle.
//
#define DEFINED_appCycles 1
CORE_API DWORD appCycles();
//
// Seconds, arbitrarily based.
//
#define DEFINED_appSeconds 1
CORE_API DOUBLE appSeconds();

//
// Memory copy.
//
#define DEFINED_appMemcpy 1
inline void appMemcpy( void* Dest, const void* Src, INT Count )
{
	memcpy( Dest, Src, Count);
}

//
// Memory zero.
//
#define DEFINED_appMemzero 1
inline void appMemzero( void* Dest, INT Count )
{
	memset( Dest, 0, Count );
}

//
// POSIX equivalents for non-standard functions.
//
#ifndef strupr
inline char* strupr( char* str )
{
	if( str )
	{
		for( char* p = str; *p; ++p )
			*p = (char)toupper( (unsigned char)*p );
	}
	return str;
}
#endif
#ifndef strnicmp
#define strnicmp strncasecmp
#endif
#ifndef stricmp
#define stricmp strcasecmp
#endif

/*----------------------------------------------------------------------------
	Globals.
----------------------------------------------------------------------------*/

// System identification.
extern "C"
{
	extern HINSTANCE      hInstance;
	extern CORE_API UBOOL GIsMMX;
	extern CORE_API UBOOL GIsPentiumPro;
	extern CORE_API UBOOL GIsKatmai;
	extern CORE_API UBOOL GIsK6;
	extern CORE_API UBOOL GIs3DNow;
}

// Module name
extern ANSICHAR GModule[32];

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/
