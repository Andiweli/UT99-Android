/*=============================================================================
	UnStaticExports.cpp: Dreamcast-specific routines.
=============================================================================*/

#ifndef UNREAL_STATIC
#error "This file is for static builds only."
#endif

#include "Core.h"

CORE_API FPackageExport* GExportsTable;

CORE_API void* appGetStaticExport( const char* Name )
{
	FPackageExport* Iter = GExportsTable;
	while( Iter )
	{
		if( !appStrcmp( Name, Iter->Name ) )
			return Iter->Address;
		Iter = Iter->Next;
	}
	return nullptr;
}
