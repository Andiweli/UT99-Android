/*=============================================================================
	UMakeCommandlet.cpp: UnrealEd script recompiler.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
=============================================================================*/

#include "EditorPrivate.h"

/*-----------------------------------------------------------------------------
	UMakeCommandlet.
-----------------------------------------------------------------------------*/

class UMakeCommandlet : public UCommandlet
{
	DECLARE_CLASS(UMakeCommandlet,UCommandlet,CLASS_Transient);
	void StaticConstructor()
	{
		guard(StaticConstructor::StaticConstructor);

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
		guard(UMakeCommandlet::Main);

		// Create the editor class.
		UClass* EditorEngineClass = UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail | LOAD_DisallowFiles, NULL );
		GEditor  = ConstructObject<UEditorEngine>( EditorEngineClass );
		GEditor->UseSound = 0;
		GEditor->InitEditor();
		GIsRequestingExit = 1; // Causes ctrl-c to immediately exit.

		// Load classes for editing.
		UClassFactoryUC* ClassFactory = new UClassFactoryUC;
		for( INT i=0; i<GEditor->EditPackages.Num(); i++ )
		{
			// Try to load class.
			const TCHAR* Pkg = *GEditor->EditPackages( i );
			FString Filename = FString(Pkg) + TEXT(".u");
			GWarn->Log( NAME_Heading, Pkg );
			if( !LoadPackage( NULL, *Filename, LOAD_NoWarn ) )
			{
				// Create package.
				GWarn->Log( TEXT("Analyzing...") );
				UPackage* PkgObject = CreatePackage( NULL, Pkg );

				// Try reading from package's .ini file.
				PkgObject->PackageFlags &= ~(PKG_AllowDownload|PKG_ClientOptional|PKG_ServerSideOnly);
				FString IniName = FString(TEXT("..")) * Pkg * TEXT("Classes") * Pkg + TEXT(".upkg");
				UBOOL B=0;
				if( GConfig->GetBool(TEXT("Flags"), TEXT("AllowDownload"), B, *IniName) && B )
					PkgObject->PackageFlags |= PKG_AllowDownload;
				if( GConfig->GetBool(TEXT("Flags"), TEXT("ClientOptional"), B, *IniName) && B )
					PkgObject->PackageFlags |= PKG_ClientOptional;
				if( GConfig->GetBool(TEXT("Flags"), TEXT("ServerSideOnly"), B, *IniName) && B )
					PkgObject->PackageFlags |= PKG_ServerSideOnly;
 
				// Rebuild the class from its directory.
				FString Spec = FString(TEXT("..")) * Pkg * TEXT("Classes") * TEXT("*.uc");
				TArray<FString> Files = GFileManager->FindFiles( *Spec, 1, 0 );
				if( Files.Num() == 0 )
					appErrorf( TEXT("Can't find files matching %s"), *Spec );
				for( INT i=0; i<Files.Num(); i++ )
				{
					// Import class.
					FString Filename  = FString(TEXT("..")) * Pkg * TEXT("Classes") * Files(i);
					FString ClassName = Files(i).LeftChop(3);
					ImportObject<UClass>( PkgObject, *ClassName, RF_Public|RF_Standalone, *Filename, NULL, ClassFactory );
				}

				// Verify that all script declared superclasses exist.
				for( TObjectIterator<UClass> ItC; ItC; ++ItC )
					if( ItC->ScriptText && ItC->GetSuperClass() )
						if( !ItC->GetSuperClass()->ScriptText )
							appErrorf( TEXT("Superclass %s of class %s not found"), ItC->GetSuperClass()->GetName(), ItC->GetName() );

				// Bootstrap-recompile changed scripts.
				GEditor->Bootstrapping = 1;
				GEditor->MakeScripts( GWarn, 0, 1 );
				GEditor->Bootstrapping = 0;

				// Tag native classes in this package for export.
				INT ClassCount=0;
				for( i=0; i<FName::GetMaxNames(); i++ )
					if( FName::GetEntry(i) )
						FName::GetEntry(i)->Flags &= ~RF_TagExp;
				for( TObjectIterator<UClass> It; It; ++It )
					It->ClearFlags( RF_TagImp | RF_TagExp );
				for( TObjectIterator<UClass> It; It; ++It )
					if( It->GetOuter()==PkgObject && It->ScriptText && (It->GetFlags()&RF_Native) && !(It->ClassFlags&CLASS_NoExport) )
						ClassCount++, It->SetFlags( RF_TagExp );

				// Export the C++ header.
				if( ClassCount )
				{
					Filename = FString(TEXT("..")) * Pkg * TEXT("Inc") * Pkg + TEXT("Classes.h");
					debugf( TEXT("Autogenerating C++ header: %s"), *Filename );
					if( !UExporter::ExportToFile( UObject::StaticClass(), NULL, *Filename, 1, 1 ) )
						appErrorf( TEXT("Failed to export: %s"), *Filename );
				}

				// Save package.
				ULinkerLoad* Conform = NULL;
				if( !ParseParam(appCmdLine(),TEXT("NOCONFORM")) )
				{
					BeginLoad();
					Conform = UObject::GetPackageLinker( CreatePackage(NULL,*(US+Pkg+TEXT("_OLD"))), *(FString(TEXT("..")) * TEXT("SystemConform") * Pkg + TEXT(".u")), LOAD_NoWarn|LOAD_NoVerify, NULL, NULL );
					EndLoad();
					if( Conform )
						debugf( TEXT("Conforming: %s"), Pkg );
				}
				SavePackage( PkgObject, NULL, RF_Standalone, *(FString(Pkg)+TEXT(".u")), GError, Conform );
			}
		}
		GIsRequestingExit=1;
		return 0;

		unguard;
	}
};
IMPLEMENT_CLASS(UMakeCommandlet)

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
