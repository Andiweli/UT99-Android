#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

#include "DCUtilPrivate.h"

// Define missing globals.
UBOOL GCheckConflicts = 0;

#include "UnLinker.h"

// Memory stacks.
FMemStack GDynMem, GSceneMem;

// Memory allocator.
#include "FMallocAnsi.h"
FMallocAnsi Malloc;

// Log file.
#include "FOutputDeviceFile.h"
FOutputDeviceFile Log;

// Error handler.
#include "FOutputDeviceAnsiError.h"
FOutputDeviceAnsiError Error;

// Feedback.
#include "FFeedbackContextAnsi.h"
FFeedbackContextAnsi Warn;

// File manager.
#include "FFileManagerLinux.h"
FFileManagerLinux FileManager;

// Config.
#include "FConfigCacheIni.h"

extern DLL_IMPORT UBOOL GTickDue;
extern "C" {HINSTANCE hInstance;}
extern "C" {char GCC_HIDDEN THIS_PACKAGE[64]="DCUtil";}

// FObjListItem.
struct FObjListItem
{
	UObject* Obj;
	INT Size;
};

static INT Compare( const FObjListItem& A, const FObjListItem& B )
{
	return A.Size - B.Size;
}

// FExecHook.
class FExecHook : public FExec
{
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar )
	{
		return 0;
	}
};

FExecHook GLocalHook;
DLL_EXPORT FExec* GThisExecHook = &GLocalHook;

//
// Handle an error.
//
void FDCUtil::HandleError( const char* Exception )
{
	GIsGuarded=0;
	GIsCriticalError=1;

	debugf( NAME_Exit, "Shutting down after catching exception" );
	UObject::StaticShutdownAfterError();

	debugf( NAME_Exit, "Exiting due to exception" );
	GErrorHist[ARRAY_COUNT(GErrorHist)-1]=0;

	if( Exception )
		fprintf( stderr, "Fatal error: %s\n", Exception );

	abort();
}

//
// Initialize.
//
void FDCUtil::InitEngine()
{
	guard(InitEngine);
	
	GDynMem.Init( 65536 );
	// Init subsystems.
	GSceneMem.Init( 32768 );

	// Create editor engine.
	UClass* EngineClass;
	EngineClass = UObject::StaticLoadClass( UEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail, NULL );

	// Init engine.
	Engine = ConstructObject<UEngine>( EngineClass );
	Engine->Init();

	unguard;
}

//
// Exit the engine.
//
void FDCUtil::ExitEngine()
{
	guard(ExitEngine);

	UObject::StaticExit();
	GMem.Exit();
	GDynMem.Exit();
	GSceneMem.Exit();
	GCache.Exit(1);

	unguard;
}

void FDCUtil::LoadPackages( const char *Dir )
{
	guard(LoadPackages);

	char Path[2048];
	appStrcpy( Path, Dir );

	if( char* Glob = appStrchr( Path, '*' ) )
	{
		char Temp[2048];
		TArray<FString> Files = appFindFiles( Path );
		*Glob = 0;
		for( INT i = 0; i < Files.Num(); ++i )
		{
			snprintf( Temp, sizeof(Temp), "%s%s", Path, *Files(i) );
			UPackage* Pkg = Cast<UPackage>( UObject::LoadPackage( nullptr, Temp, 0 ) );
			if( !Pkg )
				appErrorf(  "Package '%s' does not exist", Temp );

			FString PackageName(Temp);
			LoadedPackageNames.Add( PackageName );
			LoadedPackagePtrs.Add( Pkg );
		}
	}
	else
	{
		UPackage* Pkg = Cast<UPackage>( UObject::LoadPackage( nullptr, Path, 0 ) );
		if( !Pkg )
			appErrorf(  "Package '%s' does not exist", Path );
		LoadedPackageNames.Add( FString(Path) );
		LoadedPackagePtrs.Add( Pkg );
	}

	unguard;
}

void FDCUtil::ParsePackageArg( const char* Arg, const char* Glob )
{
	if( !appStrcmp( Arg, "*" ) )
	{
		// Go through ALL .u and specific packages
		if( Glob )
			LoadPackages( Glob );
		//LoadPackages( "../System/*.u" );
	}
	else
	{
		if( !appStrchr( Arg, '.' ) )
			appErrorf( "Specify filename with extension." );
		LoadPackages( Arg );
	}
}

UBOOL FDCUtil::ConvertTexturePkg( const FString& PkgPath, UPackage* Pkg )
{
	guard(ConvertTexturePkg);

	printf( "Converting textures in '%s'\n", Pkg->GetName() );

	// Process textures directly without intermediate collection to avoid lazy loader issues
	UBOOL Changed = false;

	// First pass: collect palettes
	for( TObjectIterator<UTexture> It; It; ++It )
	{
		if( It->IsIn( Pkg ) )
		{
			if( It->Palette && It->Palette->IsIn( Pkg ) )
			{
				if( !( It->bRealtime || It->bParametric ) )
				{
					// Check if already in the array
					UBOOL AlreadyIn = false;
					for( INT k = 0; k < UnrefPalettes.Num(); k++ )
					{
						if( UnrefPalettes(k) == It->Palette )
						{
							AlreadyIn = true;
							break;
						}
					}
					if( !AlreadyIn )
						UnrefPalettes.Add( It->Palette );
				}
			}
		}
	}

	// Second pass: convert textures
	for( TObjectIterator<UTexture> It; It; ++It )
	{
		if( It->IsIn( Pkg ) )
		{
			UTexture* Tex = *It;
			const BYTE OldFmt = Tex->Format;
			INT OldSize = 0;
			for( INT j=0; j<Tex->Mips.Num(); j++ ) OldSize += Tex->Mips(j).DataArray.Num();
			UBOOL Modified = 0;

			const UBOOL bFlatten = FTextureConverter::ShouldFlattenTexture( Tex );
			if( bFlatten )
			{
				FTextureConverter::FlattenToSolidWhite( Tex );
				DWORD NewSize = 0;
				for( INT j=0; j<Tex->Mips.Num(); j++ ) NewSize += Tex->Mips(j).DataArray.Num();
				printf( "- Flattened '%s' to solid white (%d -> %d bytes)\n", Tex->GetName(), OldSize, NewSize );
				Modified = 1;
			}
			else if( FTextureConverter::AutoConvertTexture( Tex ) )
			{
				DWORD NewSize = 0;
				for( INT j=0; j<Tex->Mips.Num(); j++ ) NewSize += Tex->Mips(j).DataArray.Num();
				if( OldFmt != Tex->Format )
					printf( "- Converted '%s' from %d to %d (%d -> %d bytes)\n", Tex->GetName(), OldFmt, Tex->Format, OldSize, NewSize );
				else
					printf( "- Converted '%s' (%d -> %d bytes)\n", Tex->GetName(), OldSize, NewSize );
				Modified = 1;
			}

			if( Modified )
			{
				DWORD NewSize = 0;
				for( INT j=0; j<Tex->Mips.Num(); j++ ) NewSize += Tex->Mips(j).DataArray.Num();
				Changed = true;
				TotalPrevSize += OldSize;
				TotalNewSize += NewSize;
			}

			if( Tex->Palette )
				UnrefPalettes.RemoveItem( Tex->Palette );
		}
	}

	return Changed;
	unguard;

}

UBOOL FDCUtil::ConvertSoundPkg( const FString& PkgPath, UPackage* Pkg )
{
	guard(ConvertSoundPkg);

	printf( "Compressing sounds in '%s'\n", Pkg->GetName() );

	UBOOL Changed = false;
	for( TObjectIterator<USound> It; It; ++It )
	{
		if( It->IsIn( Pkg ) && It->Data.Num() )
		{
            const DWORD OldSize = It->Data.Num();
			if( FSoundCompressor::CompressUSound( *It ) )
			{
				DWORD NewSize = It->Data.Num();
				Changed = true;
				printf( "- Compressed '%s' (%u -> %u bytes)\n", It->GetName(), OldSize, NewSize );
				TotalPrevSize += OldSize;
				TotalNewSize += NewSize;
			}
		}
	}

	return Changed;
	unguard;
}

UBOOL FDCUtil::ConvertMusicPkg(const FString &PkgPath, UPackage *Pkg)
{
	guard(ConvertMusicPkg);

	printf( "Nuking music in '%s'\n", Pkg->GetName() );

	UBOOL Changed = false;
	for( TObjectIterator<UMusic> It; It; ++It )
	{
		// just nuke for now
		if( It->IsIn( Pkg ) && It->Data.Num() )
		{
			const DWORD Size = It->Data.Num();
			printf( "- Nuking '%s' (%u bytes)\n", It->GetName(), Size );
			TotalPrevSize += Size;
			TotalNewSize += Size - It->Data.Num();
			It->Data.Empty();
			Changed = true;
		}
	}

	return Changed;
	unguard;
}

UBOOL FDCUtil::ConvertMeshPkg( const FString& PkgPath, UPackage* Pkg )
{
	guard(ConvertMeshPkg);

	printf( "Analyzing meshes in '%s'\n", Pkg->GetName() );

	// Get all meshes in this package first to avoid lazy loader issues with TObjectIterator
	TArray<UMesh*> PackageMeshes;
	for( TObjectIterator<UMesh> It; It; ++It )
	{
		if( It->IsIn( Pkg ) )
			PackageMeshes.AddItem( *It );
	}

	UBOOL Changed = false;
	for( INT i = 0; i < PackageMeshes.Num(); i++ )
	{
		UMesh* Mesh = PackageMeshes(i);


		Mesh->Verts.Num();  // Load vertices
		Mesh->Tris.Num();   // Load triangles
		Mesh->Connects.Num(); // Load connectivity

		// Check if this is one of biggest meshes to nuke
		FString MeshName = Mesh->GetName();
		UBOOL ShouldNuke = (MeshName == TEXT("FCommando") ||
						   MeshName == TEXT("SGirl") ||
						   MeshName == TEXT("Commando") ||
						   MeshName == TEXT("Boss") ||
						   MeshName == TEXT("TrophyMale1") ||
						   MeshName == TEXT("TrophyBoss") ||
						   MeshName == TEXT("TrophyFemale1") ||
						   MeshName == TEXT("WHHand") ||
						   MeshName == TEXT("WHPick") ||
						   MeshName == TEXT("WHHand") ||
						   MeshName == TEXT("Trans3loc") ||
						   MeshName == TEXT("TCowMesh") ||
						   MeshName == TEXT("tnalimesh") ||
						   MeshName == TEXT("tnalimesh") ||
						   MeshName == TEXT("TSkM"));

		if (ShouldNuke)
		{
			// Calculate old size before nuking
			INT OldSize = 0;
			for( INT j=0; j<Mesh->Tris.Num(); j++ ) OldSize += sizeof(FMeshTri);
			for( INT j=0; j<Mesh->FrameVerts * Mesh->AnimFrames; j++ ) OldSize += sizeof(FMeshVert);


			Mesh->Verts.Empty();
			Mesh->Tris.Empty();
			Mesh->Connects.Empty();
			Mesh->FrameVerts = 0;
			Mesh->AnimFrames = 0;
			Mesh->AnimSeqs.Empty();

			printf( "- %s: NUKED for Dreamcast (%d bytes saved)\n",
				Mesh->GetName(), OldSize );

			TotalPrevSize += OldSize;
			TotalNewSize += 0; // Completely removed
			Changed = true; // Mark that we made changes
		}
		else
		{
			// Calculate mesh size before reduction
			INT OldSize = 0;
			for( INT j=0; j<Mesh->Tris.Num(); j++ ) OldSize += sizeof(FMeshTri);
			for( INT j=0; j<Mesh->FrameVerts * Mesh->AnimFrames; j++ ) OldSize += sizeof(FMeshVert);

			// Apply mesh reduction for Dreamcast optimization (frames only)
			FMeshReducer::FOptions ReduceOptions;
			ReduceOptions.PositionTolerance = 0.01f;     // Conservative vertex reduction
			ReduceOptions.NormalTolerance = 0.01f;       // Conservative vertex reduction
			ReduceOptions.UVTolerance = 0.01f;           // Conservative vertex reduction
			ReduceOptions.FrameErrorTolerance = 1.0f;     // Allow more frame error
			ReduceOptions.MotionErrorScale = 1.0f;       // Allow more frame error
			ReduceOptions.UVSnapGrid = 1.0f / 64.0f;
			ReduceOptions.SeamPositionTolerance = 0.25f; // Allow frame error
			ReduceOptions.SeamNormalAngleDeg = 15.0f;    // Allow frame error
			ReduceOptions.UVToleranceBytes = 15.0f;      // Allow frame error
			ReduceOptions.NormalAngleToleranceDeg = 15.0f; // Allow frame error
			ReduceOptions.MaxMeshletVertices = 15.0f;    // Allow frame error


			FMeshReductionStats Stats;
			const UBOOL MeshReduced = FMeshReducer::Reduce( Mesh, ReduceOptions, &Stats );

			// Calculate new size after reduction
			INT NewSize = 0;
			for( INT j=0; j<Mesh->Tris.Num(); j++ ) NewSize += sizeof(FMeshTri);
			for( INT j=0; j<Mesh->FrameVerts * Mesh->AnimFrames; j++ ) NewSize += sizeof(FMeshVert);

			if( MeshReduced )
			{
				printf( "- %s: REDUCED %d -> %d verts, %d -> %d tris, %d -> %d frames (%d -> %d bytes)\n",
					Mesh->GetName(),
					Stats.OriginalVerts, Stats.ReducedVerts,
					Stats.OriginalTriangles, Stats.ReducedTriangles,
					Stats.OriginalFrames, Stats.ReducedFrames,
					OldSize, NewSize );

				TotalPrevSize += OldSize;
				TotalNewSize += NewSize;
				Changed = true;
			}
			else
			{
				printf( "- %s: %d verts, %d tris, %d frames (%d bytes) - no reduction needed\n",
					Mesh->GetName(), Mesh->FrameVerts, Mesh->Tris.Num(), Mesh->AnimFrames, OldSize );

				TotalPrevSize += OldSize;
				TotalNewSize += OldSize;
			}
		}
	}

	return Changed;
	unguard;

}

void FDCUtil::CommitChanges( const FSimpleArray<FString>& ChangedNames, const FSimpleArray<UPackage*>& ChangedPtrs )
{
	printf("Committing %d changed packages\n", ChangedNames.Num());
	if( UnrefPalettes.Num() )
	{
		// double check if any other packages are referencing these
		for( TObjectIterator<UTexture> It; It; ++It )
			if( It->Palette )
				UnrefPalettes.RemoveItem( It->Palette );
		// then delete remaining
		printf( "Cleaning up %d orphaned palettes\n", UnrefPalettes.Num() );
		for( INT i = 0; i < UnrefPalettes.Num(); ++i )
		{
			UnrefPalettes(i)->Colors.Empty();
			UnrefPalettes(i)->ConditionalDestroy();
			UnrefPalettes(i) = nullptr;
		}
		UnrefPalettes.Empty();
	}


	for( INT i = 0; i < ChangedNames.Num(); ++i )
	{
		UPackage* Pkg = ChangedPtrs(i);
		// Force load all lazy data in this package
		for( TObjectIterator<UObject> It; It; ++It )
		{
			if( It->IsIn( Pkg ) )
			{
				// Touch the object to ensure it's fully loaded from any lazy arrays
				It->GetClass();
				It->GetName();
				// For textures, ensure mip data is loaded
				if( UTexture* Tex = Cast<UTexture>(*It) )
				{
					for( INT j = 0; j < Tex->Mips.Num(); j++ )
					{
						Tex->Mips(j).DataArray.Num(); // Force load mip data
					}
				}
			}
		}
	}

	// Before garbage collection, ensure all level objects are marked to prevent collection
	// This is critical for level packages where objects might not have RF_Standalone
	for( INT i = 0; i < ChangedNames.Num(); ++i )
	{
		FString PkgName = ChangedNames(i);
		UPackage* Pkg = ChangedPtrs(i);

		// Check if this is a level package
		ULevel* Level = FindObject<ULevel>( Pkg, "MyLevel" );
		if( Level )
		{
			// Mark Level and all its objects to prevent garbage collection
			Level->SetFlags( RF_Standalone );
			if( Level->Model )
				Level->Model->SetFlags( RF_Standalone );
			// Mark all actors in the level
			for( INT j = 0; j < Level->Actors.Num(); j++ )
			{
				if( Level->Actors(j) && Level->Actors(j)->IsIn( Pkg ) )
					Level->Actors(j)->SetFlags( RF_Standalone );
			}
		}
	}

	// Skip garbage collection for texture packages to avoid lazy loader detachment issues
	// GC can cause problems with lazy loaders when saving packages immediately after conversion
	// UObject::CollectGarbage( RF_Standalone );

	if( ChangedNames.Num() )
	{
		printf( "Saving %d changed packages\n", ChangedNames.Num() );
		for( INT i = 0; i < ChangedNames.Num(); ++i )
		{
			FString PkgName = ChangedNames(i);
			UPackage* Pkg = ChangedPtrs(i);
			// Try to keep the previous version in heritage list to maintain backwards compatibility
			// OldGuid = PackageGuids.Find( Pkg );

			// if( QWORD* OldSizePtr = PackageSizeBefore.Find( Pkg ) )
			// {
			//	TotalPrevSize += *OldSizePtr;
			// }

			// For level packages (.unr), we need to save the Level object as Base
			UObject* SaveBase = nullptr;
			DWORD SaveFlags = RF_Standalone;

			// Check if this is a level package by looking for MyLevel
			ULevel* Level = FindObject<ULevel>( Pkg, "MyLevel" );
			if( Level )
			{
				// Ensure Level is properly set up and marked
				if( !Level->Model )
				{
					printf( "  WARNING: Level '%s' has no Model, trying to reload...\n", Level->GetName() );
					// Try to reload the level
					Level = LoadObject<ULevel>( Pkg, "MyLevel", nullptr, LOAD_NoFail, nullptr );
				}

				if( Level && Level->Model )
				{
					Level->Modify();
					if( !(Level->Model->GetFlags() & RF_Public) )
						Level->Model->SetFlags( RF_Public );
					Level->Model->Modify();

					for( INT i = 0; i < Level->Actors.Num(); i++ )
					{
						if( Level->Actors(i) && Level->Actors(i)->IsIn( Pkg ) )
						{
							if( !(Level->Actors(i)->GetFlags() & RF_Public) )
								Level->Actors(i)->SetFlags( RF_Public );
						}
					}

					SaveBase = Level;
					SaveFlags = 0;
				}
				else
				{
					printf( "  ERROR: Level '%s' could not be properly loaded for saving\n", Pkg->GetName() );
					continue;
				}
			}

			UBOOL SaveSuccess = UObject::SavePackage( Pkg, SaveBase, SaveFlags, *PkgName );

			if( !SaveSuccess )
			{
				printf( "  ERROR: Failed to save %s\n", *PkgName );
				continue;
			}

			// Get new file size after saving
			// Use absolute path to avoid path resolution issues
			char AbsPath[512];
			if( getcwd( AbsPath, sizeof(AbsPath) ) )
			{
				appStrcat( AbsPath, "/" );
				appStrcat( AbsPath, *PkgName );
			}
			else
			{
				appStrcpy( AbsPath, *PkgName );
			}

			struct stat st2;
			INT NewSize = stat( AbsPath, &st2 ) == 0 ? st2.st_size : 0;
			if( NewSize >= 0 )
			{
				TotalNewSize += NewSize;
				if( QWORD* OldSizePtr = PackageSizeBefore.Find( Pkg ) )
				{
					INT SizeDiff = NewSize - (INT)*OldSizePtr;
					printf( "  %s: %d -> %d bytes (%s%d bytes)\n",
						*PkgName, (INT)*OldSizePtr, NewSize,
						SizeDiff < 0 ? "-" : "+", SizeDiff < 0 ? -SizeDiff : SizeDiff );

					if( NewSize < 1000 && (INT)*OldSizePtr > 1000 )
					{
						printf( "    WARNING: File size is suspiciously small! Check if save actually completed.\n" );
					}
				}
			}
			else
			{
				printf( "  WARNING: Could not get file size for %s (tried: %s)\n", *PkgName, AbsPath );
			}
		}
	}

	LoadedPackageNames.Empty();
	LoadedPackagePtrs.Empty();
	PackageGuids.Empty();

	printf( "Total size change: %u -> %u\n", TotalPrevSize, TotalNewSize );
}

void FDCUtil::CommitChanges()
{
	CommitChanges( ChangedPackageNames, ChangedPackagePtrs );
}

//
// Handle an error.
//
void HandleError( const char* Exception )
{
	GIsGuarded=0;
	GIsCriticalError=1;
	debugf( NAME_Exit, "Shutting down after catching exception" );
	debugf( NAME_Exit, "Exiting due to exception" );
	GErrorHist[ARRAY_COUNT(GErrorHist)-1]=0;
}


//
// Exit wound.
//
int CleanUpOnExit(int ErrorLevel)
{
	GFileManager->Delete(TEXT("Running.ini"),0,0);
	debugf( NAME_Title, LocalizeGeneral("Exit") );
	appPreExit();
	GIsGuarded = 0;

	// Shutdown.
	appExit();
	GIsStarted = 0;

	// Restore the user's configuration.
	TCHAR baseconfig[PATH_MAX] = TEXT("");
	if( getcwd(baseconfig, sizeof(baseconfig)) == NULL )
	{
		appStrcpy( baseconfig, TEXT("./User.ini") );
	}
	else
	{
		appStrcat(baseconfig, "/User.ini");
	}

	TCHAR userconfig[PATH_MAX] = TEXT("");
	sprintf(userconfig, "~/.utconf");

	TCHAR exec[PATH_MAX] = TEXT("");
	sprintf(exec, "cp -f %s %s", baseconfig, userconfig);
	//system( exec );

	return ErrorLevel;
}

//
// Actual main function.
//
void FDCUtil::Main( )
{
	guard(Main);

	GIsRunning = 1;

	char Temp[2048] = { 0 };
	const char* Cmd = appCmdLine();
	FString PkgPath;
	UPackage* Pkg = nullptr;
	if( Parse( Cmd, "CVTUTX=", Temp, sizeof( Temp ) - 1 ) )
	{

		// Process texture packages - collect all changed packages then save them together
		char Path[2048];
		appStrcpy( Path, Temp );

		// Arrays to collect all changed packages
		FSimpleArray<FString> ChangedNames;
		FSimpleArray<UPackage*> ChangedPtrs;

		if( char* Glob = appStrchr( Path, '*' ) )
		{
			FString BasePath = FString(Path).Left(Glob - Path);
			TArray<FString> Files = appFindFiles( Path );

			// Use iterator to process files
			for( TArray<FString>::TIterator It(Files); It; ++It )
			{
				FString FullPath = BasePath + **It;
				UPackage* Pkg = Cast<UPackage>( UObject::LoadPackage( nullptr, *FullPath, 0 ) );
				if( Pkg )
				{
					printf("Converting texture package: %s\n", *FullPath);
					if( ConvertTexturePkg( FullPath, Pkg ) )
					{
						// Collect changed packages instead of saving immediately
						ChangedNames.Add(FullPath);
						ChangedPtrs.Add(Pkg);
					}
				}
			}
		}
		else
		{
			// Single file
			FString FullPath = FString(Path);
			UPackage* Pkg = Cast<UPackage>( UObject::LoadPackage( nullptr, *FullPath, 0 ) );
			if( Pkg )
			{
				printf("Converting texture package: %s\n", *FullPath);
				if( ConvertTexturePkg( FullPath, Pkg ) )
				{
					// Collect changed packages instead of saving immediately
					ChangedNames.Add(FullPath);
					ChangedPtrs.Add(Pkg);
				}
			}
		}

		// Save all changed packages together
		if( ChangedNames.Num() > 0 )
		{
			CommitChanges( ChangedNames, ChangedPtrs );
		}
	}
	else if( Parse( Cmd, "CVTUAX=", Temp, sizeof( Temp ) - 1 ) )
	{
		ParsePackageArg( Temp, "../Sounds/*.uax" );
		FSimpleArray<FString> LocalChangedNames;
		FSimpleArray<UPackage*> LocalChangedPtrs;
		for( INT i = 0; i < LoadedPackageNames.Num(); ++i )
		{
			PkgPath = LoadedPackageNames(i);
			Pkg = LoadedPackagePtrs(i);
			if( ConvertSoundPkg( PkgPath, Pkg ) )
			{
				LocalChangedNames.Add(PkgPath);
				LocalChangedPtrs.Add(Pkg);
			}
		}
		CommitChanges( LocalChangedNames, LocalChangedPtrs );
	}
	else if( Parse( Cmd, "CVTUMX=", Temp, sizeof( Temp ) - 1 ) )
	{
		ParsePackageArg( Temp, "../Music/*.umx" );
		FSimpleArray<FString> LocalChangedNames;
		FSimpleArray<UPackage*> LocalChangedPtrs;
		for( INT i = 0; i < LoadedPackageNames.Num(); ++i )
		{
			PkgPath = LoadedPackageNames(i);
			Pkg = LoadedPackagePtrs(i);
			if( ConvertMusicPkg( PkgPath, Pkg ) )
			{
				LocalChangedNames.Add(PkgPath);
				LocalChangedPtrs.Add(Pkg);
			}
		}
		CommitChanges( LocalChangedNames, LocalChangedPtrs );
	}
	else if( Parse( Cmd, "CVTUMH=", Temp, sizeof( Temp ) - 1 ) )
	{
		ParsePackageArg( Temp, "../System/*.u" );
		FSimpleArray<FString> LocalChangedNames;
		FSimpleArray<UPackage*> LocalChangedPtrs;
		for( INT i = 0; i < LoadedPackageNames.Num(); ++i )
		{
			PkgPath = LoadedPackageNames(i);
			Pkg = LoadedPackagePtrs(i);
			if( ConvertMeshPkg( PkgPath, Pkg ) )
			{
				LocalChangedNames.Add(PkgPath);
				LocalChangedPtrs.Add(Pkg);
			}
		}
		if( LocalChangedNames.Num() > 0 )
		{
			CommitChanges( LocalChangedNames, LocalChangedPtrs );
		}
	}
#if 0
	else if( Parse( Cmd, "CVTALL=", Temp, sizeof( Temp ) - 1 ) )
	{
		if( Temp[0] == '*' && Temp[0] == 0 )
		{
			LoadPackages( "../Textures/*.utx" );
			LoadPackages( "../Sounds/*.uax" );
			LoadPackages( "../Music/*.umx" );
			LoadPackages( "../System/*.u" );
		}
		else
		{
			if( !appStrchr( Temp, '.' ) )
				appErrorf( "Specify filename with extension." );
			LoadPackages( Temp );
		}
		for( INT i = 0; i < LoadedPackageNames.Num(); ++i )
		{
			PkgPath = LoadedPackageNames(i);
			Pkg = LoadedPackagePtrs(i);
			ConvertTexturePkg( PkgPath, Pkg );
			ConvertSoundPkg( PkgPath, Pkg );
			ConvertMusicPkg( PkgPath, Pkg );
		}
		CommitChanges();
	}
#endif
	else if( Parse( Cmd, "CVTUNR=", Temp, sizeof( Temp ) - 1 ) )
	{
		ParsePackageArg( Temp, "../Maps/*.unr" );
		FSimpleArray<FString> LocalChangedNames;
		FSimpleArray<UPackage*> LocalChangedPtrs;
		for( INT i = 0; i < LoadedPackageNames.Num(); ++i )
		{
			FString PkgPath = LoadedPackageNames(i);
			UPackage* Pkg = LoadedPackagePtrs(i);
			if( ConvertMapPkg( PkgPath, Pkg ) )
			{
				LocalChangedNames.Add(PkgPath);
				LocalChangedPtrs.Add(Pkg);
			}
		}
		if( LocalChangedNames.Num() > 0 )
		{
			CommitChanges( LocalChangedNames, LocalChangedPtrs );
		}
	}
	else
	{
		printf( "Usage: dctool CVTUTX=<TEXPKG> | CVTUAX=<SOUNDPKG> | CVTUMX=<MUSPKG> | CVTUMH=<UMESHPKG> | CVTUNR=<MAPPKG>\n" );
	}

	GIsRunning = 0;

	unguard;
}

int main( int argc, const char** argv )
{
	try
	{
	
	INT ErrorLevel = 0;
	GIsStarted	   = 1;

	// Set module name.
	appStrcpy( GModule, argv[0] );

	// Set the package name.
	appStrcpy( THIS_PACKAGE, appPackage() );	

	// Get the command line.
	TCHAR CmdLine[1024], *CmdLinePtr=CmdLine;
	*CmdLinePtr = 0;
	for( INT i=1; i<argc; i++ )
	{
		if( i>1 )
			appStrcat( CmdLine, " " );
		appStrcat( CmdLine, argv[i] );
	}

	// Init core.
	GIsClient = 1; 
	GIsGuarded = 0;
	appInit( TEXT("DCUtil"), CmdLine, &Malloc, &Log, &Error, &Warn, &FileManager, FConfigCacheIni::Factory, 0 );

	// Init console log.
	if (ParseParam(CmdLine, TEXT("LOG")))
	{
		Warn.AuxOut = GLog;
		GLog = &Warn;
	}

	// Init mode.
	GIsServer		= 1;
	GIsClient		= !ParseParam(appCmdLine(), TEXT("SERVER"));
	GIsEditor		= 1;
	GIsScriptable	= 1;

	// Begin.
	FDCUtil DCUtil;
	// Start main loop.
	GIsGuarded=1;
	DCUtil.InitEngine();
	DCUtil.Main();
	DCUtil.ExitEngine();
	GIsGuarded=0;

	// Finish up.
	return CleanUpOnExit(ErrorLevel);
	}
	catch (...)
	{
		// Chained abort.  Do cleanup.
		HandleError(NULL);
		return CleanUpOnExit(1);
	}
}

