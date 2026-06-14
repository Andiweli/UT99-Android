/*=============================================================================
	UBatchExportCommandlet.cpp: Unreal file exporting commandlet.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
=============================================================================*/

#include "EditorPrivate.h"

/*-----------------------------------------------------------------------------
	UConformCommandlet.
-----------------------------------------------------------------------------*/

class UBatchExportCommandlet : public UCommandlet
{
	DECLARE_CLASS(UBatchExportCommandlet,UCommandlet,CLASS_Transient);
	void StaticConstructor()
	{
		guard(UBatchExportCommandlet::StaticConstructor);

		LogToStdout     = 0;
		IsClient        = 1;
		IsEditor        = 1;
		IsServer        = 1;
		LazyLoad        = 1;
		ShowErrorCount  = 1;

		unguard;
	}
	INT Main( const TCHAR* Parms )
	{
		guard(UBatchExportCommandlet::Main);
		FString Pkg, Cls, Ext, Path;
		if( !ParseToken(Parms,Pkg,0) )
			appErrorf(TEXT("Package file not specified"));
		if( !ParseToken(Parms,Cls,0) )
			appErrorf(TEXT("Exporter not specified"));
		if( !ParseToken(Parms,Ext,0) )
			appErrorf(TEXT("File extension not specified"));
		if( !ParseToken(Parms,Path,0) )
			Path=TEXT(".");
		if( Ext.Left(1)==TEXT(".") )
			Ext = Ext.Mid(1);
		UClass* Class = FindObjectChecked<UClass>( ANY_PACKAGE, *Cls );
		GWarn->Logf( TEXT("Loading package %s..."), *Pkg );
		UObject* Package = LoadPackage(NULL,*Pkg,LOAD_NoFail);
		if( !GFileManager->MakeDirectory( *Path, 1 ) )
			appErrorf( TEXT("Failed to make directory %s"), *Path );
		for( TObjectIterator<UObject> It; It; ++It )
		{
			if( It->IsA(Class) && It->IsIn(Package) )
			{
				FString Filename = Path * It->GetName() + TEXT(".") + Ext;
				if( UExporter::ExportToFile(*It, NULL, *Filename, 1, 0) )
					GWarn->Logf( TEXT("Exported %s to %s"), It->GetFullName(), *Filename );
				else
					appErrorf(TEXT("Can't export %s to file %s"),It->GetFullName(),*Filename);
			}
		}
		GIsRequestingExit=1;
		return 0;
		unguard;
	}
};
IMPLEMENT_CLASS(UBatchExportCommandlet)

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
