#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "Core.h"

extern "C" FILE* __real_fopen( const char* Path, const char* Mode );
extern "C" int __real_fclose( FILE* Handle );
extern "C" size_t __real_fread( void* Ptr, size_t Size, size_t Num, FILE* Handle );
extern "C" size_t __real_fwrite( const void* Ptr, size_t Size, size_t Num, FILE* Handle );
extern "C" int __real_fseek( FILE*, long, int );
extern "C" long __real_ftell( FILE* );
extern "C" int __real_setvbuf( FILE *, char *, int, size_t );

#define FPOOL_MAX_FILES 7
#define FPOOL_MAX_FNAME 64
#define FPOOL_SIZE 64

struct FPoolHandle
{
	char Name[FPOOL_MAX_FNAME];
	char Mode[4];
	FILE* Handle;
	INT Pos;
};

static FPoolHandle GFilePool[FPOOL_SIZE];
static INT GFilePoolSize = 0;
static INT GFilesOpen = 0;

static FILE* TryOpen( const char* Name, const char* Mode )
{
	// try opening the file first in case there's another error
	FILE* Handle = __real_fopen( Name, Mode );
	if( Handle )
	{
		++GFilesOpen;
		return Handle;
	}
	if( errno != ENFILE )
		return nullptr;

	check( GFilesOpen >= FPOOL_MAX_FILES );

	// at the limit; grab a used file handle and close it
	// @TODO: LRU or some shit
	for( INT i = 0; i < GFilePoolSize && GFilesOpen >= FPOOL_MAX_FILES; ++i )
	{
		FPoolHandle* Iter = &GFilePool[i];
		if( Iter->Handle )
		{
			Iter->Pos = __real_ftell( Iter->Handle );
			__real_fclose( Iter->Handle );
			Iter->Handle = nullptr;
			--GFilesOpen;
		}
	}

	// try again
	Handle = __real_fopen( Name, Mode );
	if( Handle )
		++GFilesOpen;

	return Handle;
}

static inline FILE* GetStream( FPoolHandle* PHandle )
{
	if( !PHandle )
		return nullptr;

	if( (uintptr_t)PHandle < (uintptr_t)GFilePool || (uintptr_t)PHandle >= (uintptr_t)( GFilePool + FPOOL_SIZE ) )
		return (FILE*)PHandle;

	if( PHandle->Handle )
		return PHandle->Handle; // already open

	PHandle->Handle = TryOpen( PHandle->Name, PHandle->Mode );
	if( !PHandle->Handle )
		return nullptr;

	__real_fseek( PHandle->Handle, PHandle->Pos, SEEK_SET );

	return PHandle->Handle;
}

extern "C" FPoolHandle* __wrap_fopen( const char* Name, const char* Mode )
{
	FILE* Handle = TryOpen( Name, Mode );
	if( !Handle )
		return nullptr;

	FPoolHandle* PHandle = nullptr;
	for( INT i = 0; i < GFilePoolSize; ++i )
	{
		// pool handle is unoccupied if it has no name
		if( !GFilePool[i].Name[0] )
		{
			PHandle = &GFilePool[i];
			break;
		}
	}

	if( !PHandle )
	{
		// no unoccupied pool handles, make a new one
		check(GFilePoolSize < FPOOL_SIZE);
		PHandle = &GFilePool[GFilePoolSize++];
	}

	appStrncpy( PHandle->Name, Name, sizeof( PHandle->Name ) - 1 );
	appStrncpy( PHandle->Mode, Mode, sizeof( PHandle->Mode ) );
	PHandle->Pos = 0;
	PHandle->Handle = Handle;

	return PHandle;
}

extern "C" int __wrap_fclose( FPoolHandle* PHandle )
{
	if( !PHandle )
		return 0;

	if( (uintptr_t)PHandle < (uintptr_t)GFilePool || (uintptr_t)PHandle >= (uintptr_t)( GFilePool + FPOOL_SIZE ) )
		return __real_fclose( (FILE*)PHandle );

	int Ret = 0;
	if( PHandle->Handle )
	{
		Ret = __real_fclose( PHandle->Handle );
		PHandle->Handle = nullptr;
		--GFilesOpen;
	}

	PHandle->Name[0] = 0;

	return Ret;
}

extern "C" size_t __wrap_fread( void* Ptr, size_t Size, size_t Num, FPoolHandle* Handle )
{
	return __real_fread( Ptr, Size, Num, GetStream( Handle ) );
}

extern "C" size_t __wrap_fwrite( const void* Ptr, size_t Size, size_t Num, FPoolHandle* Handle )
{
	return __real_fwrite( Ptr, Size, Num, GetStream( Handle ) );
}

extern "C" int __wrap_fseek( FPoolHandle* Handle, long Ofs, int Mode )
{
	return __real_fseek( GetStream( Handle ), Ofs, Mode );
}

extern "C" long __wrap_ftell( FPoolHandle* Handle )
{
	return __real_ftell( GetStream( Handle ) );
}

extern "C" int __wrap_setvbuf( FPoolHandle* PHandle, char* Buffer, int Mode, size_t Size )
{
	if( (uintptr_t)PHandle < (uintptr_t)GFilePool || (uintptr_t)PHandle >= (uintptr_t)( GFilePool + FPOOL_SIZE ) )
		return __real_setvbuf( (FILE*)PHandle, Buffer, Mode, Size );
	return 0;
}
