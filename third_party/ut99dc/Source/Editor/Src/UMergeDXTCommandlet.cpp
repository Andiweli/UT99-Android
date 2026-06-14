/*=============================================================================
	UMergeDXTCommandlet.cpp: Unreal DXT texture merger.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
=============================================================================*/

#include "EditorPrivate.h"

/*-----------------------------------------------------------------------------
	UMergeDXTCommandlet.
-----------------------------------------------------------------------------*/

class UMergeDXTCommandlet : public UCommandlet
{
	DECLARE_CLASS(UMergeDXTCommandlet,UCommandlet,CLASS_Transient);
	void StaticConstructor()
	{
		guard(StaticConstructor::StaticConstructor);

		LogToStdout     = 0;
		IsClient        = 1;
		IsEditor        = 1;
		IsServer        = 1;
		LazyLoad        = 0;
		ShowErrorCount  = 1;

		unguard;
	}
	void Merge( FString Src, FString Old, FString Dest )
	{
		// Src = normal .utx file to conform to.
		// Old = may contain s3tc compressed textures to merge into the destination package.
		guard(UMergeDXTCommandlet::Merge);
		
		// Skip if destination exists.
		if( GFileManager->FileSize(*Dest)<0 )
		{
			GWarn->Logf( TEXT("Merging %s..."), *Src );
			
			// Make a temp Old package with underscore so the Old and Src packages will be loaded with 
			// different names (hackish)
			FString UnderscoredOld = Old.LeftChop(4) + FString(TEXT("_.utx"));
			if( GFileManager->FileSize(*UnderscoredOld)<=0 && GFileManager->FileSize(*Old)>0 )
				GFileManager->Copy(*UnderscoredOld,*Old,1,0,0,NULL);

			// Load old file.
			UObject* OldPackage = LoadPackage( NULL, *UnderscoredOld, LOAD_NoWarn ); 
			if( !OldPackage )
			{
				GWarn->Logf(TEXT("Standard OLD utx file '%s' load failed"), *UnderscoredOld );
				OldPackage = NULL;
			}
			else
			{
				GWarn->Logf(TEXT("Loaded OLD utx file '%s' "), *UnderscoredOld );
			}

			// Load new.
			UObject* NewPackage = LoadPackage( NULL, *Src, LOAD_NoFail );
			if( !NewPackage )
				appErrorf( TEXT("Standard utx file '%s' load failed"), *Src );
			else
				GWarn->Logf(TEXT("Loaded SRC utx file '%s' "), *Src );

			// Process all textures.
			UObject::ResetLoaders(NULL,0,1);

			// Get the linker, for immediate conforming at SavePackage time ?
			ULinkerLoad* ConformLinker=NULL;

			for( TObjectIterator<UTexture> i; i; ++i )
			{
				if( i->IsIn(NewPackage) )
				{
					GWarn->Logf( TEXT(" Texture in new package %s %ix%i"),i->GetName(),i->USize,i->VSize);

					// Get the new package's linker from an actual object in the package - didn't work right.
					// ConformLinker = i->GetLinker();					

					// Merge all textures.
					UTexture* j=NULL;
					if( OldPackage )
					{
						guard(IterateOldPackage);

						for( TObjectIterator<UTexture> q; q; ++q )
						{
							if( q->IsIn(OldPackage) )
							{					
								if( q->IsIn(OldPackage) && q->IsA(UTexture::StaticClass()) && (appStricmp(q->GetName(),i->GetName())==0) )
								{
									if( !q->bParametric ) GWarn->Logf( TEXT(" Found it, apparently "));							

									j=*q; 
									break;								
								}							
							}

						}
						unguard;
					}

					// 
					if( (j != NULL) &&  j->bHasComp && !j->bParametric ) //(j->CompMips != NULL) ) //&& j->bHasComp ) //Format==TEXF_DXT1 
					{
						GWarn->Logf( TEXT("   Copying existing compressed %s %ix%i"),j->GetName(),j->USize,j->VSize);
						i->bHasComp   = 1;
						i->CompMips   = j->CompMips; 
						i->CompFormat = TEXF_DXT1; 
					}
					else if( !i->bParametric && !(i->PolyFlags&PF_Masked) && !(i->Palette && i->Palette->Colors(128).A!=255) && i->CompFormat!=TEXF_DXT1 )
					{
						GWarn->Logf( TEXT(" Compressing %s %ix%i"), i->GetName(),i->USize,i->VSize );
						i->Compress( TEXF_DXT1, i->Mips.Num()>1 );
						i->bHasComp   = 1;
						check(i->CompFormat==TEXF_DXT1);
					}
				}
			}
			SavePackage( NewPackage, NULL, RF_Standalone, *Dest, GError, ConformLinker );

			// Using ConformLinker didn't work, hence the separate conforming step below.

			// clean up underscored one.
			if (GFileManager->FileSize(*UnderscoredOld)>0) GFileManager->Delete(*UnderscoredOld);

			// Conform explicitly with the Dest package.
			guard(ConformAfterMerge);
			FString ConfSrc, ConfOld;
			ConfSrc = Dest;
			ConfOld = Src;
			GWarn->Log( TEXT("Loading...") );
			BeginLoad();
			ULinkerLoad* ConfOldLinker = UObject::GetPackageLinker( CreatePackage(NULL,*(ConfOld+FString(TEXT("_ConfOld")))), *ConfOld, LOAD_NoWarn|LOAD_NoVerify, NULL, NULL );
			EndLoad();
			UObject* NewPackage = LoadPackage( NULL, *ConfSrc, LOAD_NoFail );
			if( !ConfOldLinker )
				appErrorf( TEXT("Old file '%s' load failed"), *ConfOld );
			if( !NewPackage )
				appErrorf( TEXT("New file '%s' load failed"), *ConfSrc );
			GWarn->Log( TEXT("Saving...") );
			SavePackage( NewPackage, NULL, RF_Standalone, *ConfSrc, GError, ConfOldLinker );
			GWarn->Logf( TEXT("File %s successfully conformed to %s..."), *ConfSrc, *ConfOld );
			unguard;
		}
		unguard;
	}
	INT Main( const TCHAR* Parms )
	{
		guard(UMergeDXTCommandlet::Main);
		FString Src, Old, Dest;
		if( !ParseToken(Parms,Src,0) )
			appErrorf(TEXT("Standard utx path not specified"));
		if( !ParseToken(Parms,Old,0) )
			appErrorf(TEXT("DXT utx path not specified"));
		if( !ParseToken(Parms,Dest,0) )
			appErrorf(TEXT("Dest utx path not specified"));
		TArray<FString> A=GFileManager->FindFiles(*(Src*TEXT("*.utx")),1,0);
		for( TArray<FString>::TIterator i(A); i; ++i )
			Merge( Src*(*i), Old*(*i), Dest*(*i) );

		GIsRequestingExit=1;
		return 0;
		unguard;
	}
};
IMPLEMENT_CLASS(UMergeDXTCommandlet)

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
