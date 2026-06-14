/*=============================================================================
	UExporter.cpp: Exporter class implementation.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

// Core includes.
#include "CorePrivate.h"

/*----------------------------------------------------------------------------
	UExporter.
----------------------------------------------------------------------------*/

void UExporter::StaticConstructor()
{
	guard(UExporter::StaticConstructor);
	UArrayProperty* A = new(GetClass(),TEXT("Formats"),RF_Public)UArrayProperty(CPP_PROPERTY(Formats),TEXT(""),0);
	A->Inner = new(A,TEXT("StrProperty0"),RF_Public)UStrProperty;
	unguard;
}
UExporter::UExporter()
: Formats( E_NoInit )
{}
void UExporter::Serialize( FArchive& Ar )
{
	guard(UExporter::Serialize);
	Super::Serialize( Ar );
	Ar << SupportedClass << Formats;
	unguard;
}
IMPLEMENT_CLASS(UExporter);

/*----------------------------------------------------------------------------
	Object exporting.
----------------------------------------------------------------------------*/

//
// Find an exporter.
//
UExporter* UExporter::FindExporter( UObject* Object, const TCHAR* FileType )
{
	guard(UExporter::FindExporter);
	check(Object);
	for( TObjectIterator<UClass> It; It; ++It )
	{
		if( It->IsChildOf(UExporter::StaticClass()) )
		{
			UExporter* Default = (UExporter*)It->GetDefaultObject();
			if( Default->SupportedClass && Object->IsA(Default->SupportedClass) )
				for( INT i=0; i<Default->Formats.Num(); i++ )
					if
					(	appStricmp( *Default->Formats(i), FileType  )==0
					||	appStricmp( *Default->Formats(i), TEXT("*") )==0 )
						return ConstructObject<UExporter>( *It );
		}
	}
	return NULL;
	unguard;
}

//
// Export an object to an archive.
//
void UExporter::ExportToArchive( UObject* Object, UExporter* InExporter, FArchive& Ar, const TCHAR* FileType )
{
	guard(UExporter::ExportToArchive);
	check(Object);
	UExporter* Exporter = InExporter;
	if( !Exporter )
	{
		Exporter = FindExporter( Object, FileType );
	}
	if( !Exporter )
	{
		GWarn->Logf( TEXT("No %s exporter found for %s"), FileType, Object, Object->GetFullName() );
		return;
	}
	check(Object->IsA(Exporter->SupportedClass));
	Exporter->ExportBinary( Object, FileType, Ar, GWarn );
	if( !InExporter )
		delete Exporter;
	unguard;
}

//
// Export an object to an output device.
//
void UExporter::ExportToOutputDevice( UObject* Object, UExporter* InExporter, FOutputDevice& Out, const TCHAR* FileType, INT Indent )
{
	guard(UExporter::ExportToOutputDevice);
	check(Object);
	UExporter* Exporter = InExporter;
	if( !Exporter )
	{
		Exporter = FindExporter( Object, FileType );
	}
	if( !Exporter )
	{
		GWarn->Logf( TEXT("No %s exporter found for %s"), FileType, Object->GetFullName() );
		return;
	}
	check(Object->IsA(Exporter->SupportedClass));
	INT SavedIndent = Exporter->TextIndent;
	Exporter->TextIndent = Indent;
	Exporter->ExportText( Object, FileType, Out, GWarn );
	Exporter->TextIndent = SavedIndent;
	if( !InExporter )
		delete Exporter;
	unguard;
}

//
// Export this object to a file.  Child classes do not
// override this, but they do provide an Export() function
// to do the resoource-specific export work.  Returns 1
// if success, 0 if failed.
//
UBOOL UExporter::ExportToFile( UObject* Object, UExporter* InExporter, const TCHAR* Filename, UBOOL NoReplaceIdentical, UBOOL Prompt )
{
	guard(UExporter::ExportToFile);
	check(Object);
	UExporter* Exporter = InExporter;
	const TCHAR* FileType = appFExt(Filename);
	UBOOL Result = 0;
	if( !Exporter )
	{
		Exporter = FindExporter( Object, FileType );
	}
	if( !Exporter )
	{
		GWarn->Logf( TEXT("No %s exporter found for %s"), FileType, Object->GetFullName() );
		return 0;
	}
	if( Exporter->bText )
	{
		FStringOutputDevice Buffer;
		ExportToOutputDevice( Object, Exporter, Buffer, FileType, 0 );
		if( NoReplaceIdentical )
		{
			FString FileBytes;
			if
			(	appLoadFileToString(FileBytes,Filename)
			&&	appStrcmp(*Buffer,*FileBytes)==0 )
			{
				debugf( TEXT("Not replacing %s because identical"), Filename );
				Result = 1;
				goto Done;
			}
			if( Prompt )
			{
				if( !GWarn->YesNof( LocalizeQuery("Overwrite"), Filename ) )
				{
					Result = 1;
					goto Done;
				}
			}
		}
		if( !appSaveStringToFile( Buffer, Filename ) )
		{
			GWarn->Logf( LocalizeError("ExportOpen"), Object->GetFullName(), Filename );
			goto Done;
		}
		Result = 1;
	}
	else
	{
		FBufferArchive Buffer;
		ExportToArchive( Object, Exporter, Buffer, FileType );
		if( NoReplaceIdentical )
		{
			TArray<BYTE> FileBytes;
			if
			(	appLoadFileToArray(FileBytes,Filename)
			&&	FileBytes.Num()==Buffer.Num()
			&&	appMemcmp(&FileBytes(0),&Buffer(0),Buffer.Num())==0 )
			{
				debugf( TEXT("Not replacing %s because identical"), Filename );
				Result = 1;
				goto Done;
			}
			if( Prompt )
			{
				if( !GWarn->YesNof( LocalizeQuery("Overwrite"), Filename ) )
				{
					Result = 1;
					goto Done;
				}
			}
		}
		if( !appSaveArrayToFile( Buffer, Filename ) )
		{
			GWarn->Logf( LocalizeError("ExportOpen"), Object->GetFullName(), Filename );
			goto Done;
		}
		Result = 1;
	}
Done:
	if( !InExporter )
		delete Exporter;
	return Result;
	unguard;
}

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/
