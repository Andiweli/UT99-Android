/*=============================================================================
	UMasterCommandlet.cpp: Unreal command-line installer release builder.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
=============================================================================*/

#include "EditorPrivate.h"
#include "../../Setup/Inc/Setup.h"

/*-----------------------------------------------------------------------------
	Master distribution image generator.
-----------------------------------------------------------------------------*/

class UMasterCommandlet : public UCommandlet
{
	DECLARE_CLASS(UMasterCommandlet,UCommandlet,CLASS_Transient);

	// Variables.
	FString GConfigFile, GProduct, GRefPath, GMasterPath, GSrcPath, GArchive;
	FBufferArchive GArchiveData;
	FArchiveHeader GArc;

	// Archive management.
	void LocalCopyFile( const TCHAR* Dest, const TCHAR* Src, DWORD Flags )
	{
		guard(UMasterCommandlet::LocalCopyFile);
		if( !appIsPureAnsi(Dest) )
			appErrorf( TEXT("Non-ansi filename: %s"), Dest );
		if( GArchive!=TEXT("") )
		{
			TArray<BYTE> Data;
			if( !appLoadFileToArray( Data, Src ) )
				appErrorf( TEXT("Failed to load file %s"), Src );
			new(GArc._Items_)FArchiveItem(Dest,GArchiveData.Num(),Data.Num(),Flags);
			GArchiveData.Serialize( &Data(0), Data.Num() );
		}
		else
		{
			if( !GFileManager->Copy( *(GSrcPath * Dest), Src, 1, 1, 0, NULL ) )
				appErrorf( TEXT("Failed to copy %s to %s"), Src, *(GSrcPath * Dest) );
		}
		unguard;
	}

	// File diffing.
	struct FLink
	{
		INT Offset;
		FLink* Next;
		FLink( INT InOffset, FLink* InNext )
		: Offset( InOffset ), Next( InNext )
		{}
	};
	enum {ARRAY_SIZE=65536*64};
	enum {MIN_RUNLENGTH=10};
	INT ArrayCrc( const TArray<BYTE>& T, INT Offset )
	{
		return appMemCrc( &T(Offset), Min((INT)MIN_RUNLENGTH,T.Num()-Offset) ) & (ARRAY_SIZE-1);
	}
	void Decompress( TArray<BYTE>& New, TArray<BYTE>& Delta, TArray<BYTE> Old )
	{
		guard(UMasterCommandlet::Decompress);
		INT Magic=0, PrevSpot=0, OldSize=0, OldCRC=0, NewSize=0, NewCRC=0;
		FBufferReader Reader( Delta );
		Reader << Magic << OldSize << OldCRC << NewSize << NewCRC;
		check(Magic==0x92f92912);
		check(OldSize==Old.Num());
		check(OldCRC==(INT)appMemCrc(&Old(0),Old.Num()));
		while( !Reader.AtEnd() )
		{
			INT Index;
			Reader << AR_INDEX(Index);
			if( Index<0 )
			{
				INT Start = New.Add( -Index );
				Reader.Serialize( &New(Start), -Index );
			}
			else
			{
				INT CopyPos;
				Reader << AR_INDEX(CopyPos);
				CopyPos += PrevSpot;
				check(CopyPos>=0);
				check(CopyPos+Index<=Old.Num());
				INT Start = New.Add( Index );
				appMemcpy( &New(Start), &Old(CopyPos), Index );
				PrevSpot = CopyPos + Index;
			}
		}
		check(NewSize==New.Num());
		check(NewCRC==(INT)appMemCrc(&New(0),New.Num()));
		unguard;
	}
	UBOOL DeltaCode( const TCHAR* RefFilename, const TCHAR* MasterFilename, const TCHAR* SrcFilename )
	{
		guard(UMasterCommandlet::DeltaCode);

		// Load files, and delete the delta file.
		DOUBLE StartTime = appSeconds();
		FBufferArchive Data;
		TArray<BYTE> Old, New;
		FLink** Starts = new FLink*[ARRAY_SIZE];
		GWarn->Logf( TEXT("   Delta compressing %s to %s"), MasterFilename, *(GSrcPath*SrcFilename) );
		GWarn->Logf( TEXT("      Relative to %s"), RefFilename );
		if( !appLoadFileToArray(Old,RefFilename) )
			appErrorf( TEXT("Can't load ref file %s"), RefFilename );
		if( !appLoadFileToArray(New,MasterFilename) )
			appErrorf( TEXT("Can't load src file %s"), MasterFilename );

		// See if an exact match delta coded file exists.
		UBOOL Done=0;
		FString CachedDelta = BaseFilename(SrcFilename)+TEXT("_bak");
		if( appLoadFileToArray(Data,*CachedDelta) )
		{
			GWarn->Logf( TEXT("   Examining cached delta %s"), *(GSrcPath*SrcFilename) );
			TArray<BYTE> Test;
			Decompress( Test, Data, Old );
			if( Test.Num()==New.Num() )
			{
				for( INT i=0; i<Test.Num(); i++ )
					if( Test(i)!=New(i) )
						break;
				if( i==Test.Num() )
					Done=1;
			}
		}
		if( !Done )
		{
			// Save header.
			Data.Empty();
			INT	Magic=0x92f92912, OldSize=Old.Num(), OldCRC=appMemCrc(&Old(0),Old.Num()), NewSize=New.Num(), NewCRC=appMemCrc(&New(0),New.Num());
			Data << Magic << OldSize << OldCRC << NewSize << NewCRC;

			// Delta compress the files.
			GWarn->Logf( TEXT("Preprocessing...") );
			for( INT i=0; i<ARRAY_SIZE; i++ )
			{
				Starts[i] = NULL;
			}
			for( i=0; i<Old.Num(); i++ )
			{
				if( (i&1023)==0 )
					GWarn->Serialize( *FString::Printf( TEXT("Processed %i/%iK"), i/1024, Old.Num()/1024), NAME_Progress );
				INT Index = ArrayCrc(Old,i);
				Starts[Index] = new FLink(i,Starts[Index]);
			}
			GWarn->Logf( TEXT("\nCompressing...") );
			for( INT NewPos=0,LiteralStart=0,PrevSpot=0; NewPos<=New.Num(); )
			{
				INT BestPos=0, BestRunLength=0;
				for( FLink* Link=Starts[ArrayCrc(New,NewPos)]; Link; Link=Link->Next )
				{
					for( INT RunLength=0; Link->Offset+RunLength<Old.Num() && NewPos+RunLength<New.Num() && Old(Link->Offset+RunLength)==New(NewPos+RunLength); RunLength++ );
					if( RunLength > BestRunLength )
					{
						BestRunLength = RunLength;
						BestPos = Link->Offset;
					}
				}
				if( (BestRunLength>=MIN_RUNLENGTH || NewPos==New.Num()) && LiteralStart<NewPos )
				{
					INT NegativeLiteralCount = LiteralStart - NewPos;
					Data << AR_INDEX(NegativeLiteralCount);
					Data.Serialize( &New(LiteralStart), -NegativeLiteralCount );
				}
				if( BestRunLength>=MIN_RUNLENGTH )
				{
					INT DeltaPos = BestPos - PrevSpot;
					Data << AR_INDEX(BestRunLength) << AR_INDEX(DeltaPos);
					NewPos += BestRunLength;
					PrevSpot = BestPos + BestRunLength;
					LiteralStart = NewPos;
				}
				else NewPos++;

				if( (NewPos&1023) == 0 )
					GWarn->Serialize( *FString::Printf( TEXT("Processed %i/%iK"), NewPos/1024, Old.Num()/1024), NAME_Progress );
			}
			GWarn->Logf( TEXT("      Result size %i (%5.3f%%) Time = %5.2f Min"), Data.Num(), 100.0*Data.Num()/New.Num(), (appSeconds()-StartTime)/60.0 );

			// Reconstruct the new file.
			TArray<BYTE> Test;
			Decompress( Test, Data, Old );
			if( Test.Num()!=New.Num() )
				appErrorf( TEXT("%i %i"), Test.Num(), New.Num() );
			for( i=0; i<Test.Num(); i++ )
				if( Test(i)!=New(i) )
					appErrorf( TEXT("%i %i %i"), i, Test(i), New(i) );
		}

		// Save delta to disk.
		if( GArchive!=TEXT("") )
		{
			new(GArc._Items_)FArchiveItem(SrcFilename,GArchiveData.Num(),Data.Num(),0);
			GArchiveData.Serialize( &Data(0), Data.Num() );
		}
		else appSaveArrayToFile( Data, *(GSrcPath*SrcFilename) );
		appSaveArrayToFile( Data, *CachedDelta );

		// Cleanup.
		for( INT i=0; i<ARRAY_SIZE; i++ )
		{
			if( Starts[i]!=NULL )
			{
				delete Starts[i];
				Starts[i] = NULL;
			}
		}
		delete Starts;

		return 1;
		unguard;
	}

	// Process a group in advance.
	void UpdateGroup( FString MasterPath, const TCHAR* File, const TCHAR* Group, TMultiMap<FString,FString>& Map )
	{
		guard(UpdateGroup);
		GWarn->Logf( TEXT("   Processing group %s"), Group );
		TMultiMap<FString,FString> AllFiles;
		{for( TMultiMap<FString,FString>::TIterator It(Map); It; ++It )
		{
			if( It.Key()==TEXT("File") || It.Key()==TEXT("Copy") )
			{
				// Expand wildcard.
				FFileInfo Info( It.Value() );
				check(Info.Src!=TEXT(""));
				FString Master = Info.Master!=TEXT("") ? Info.Master : Info.Src;
				FString Src    = Info.Src;
				if( Master.InStr(TEXT("*"))>=0 )
				{
					GWarn->Logf( TEXT("   Expanding wildcard %s"), *Master );
					TArray<FString> Files
					=	Info.MasterRecurse
					?	FindFilesRecursive( MasterPath * BasePath(Master), BaseFilename(Master) )
					:	GFileManager->FindFiles( *(MasterPath * BasePath(Master) * BaseFilename(Master)), 1, 0 );
					for( INT i=0; i<Files.Num(); i++ )
					{
						FFileInfo NewInfo(Info);
						NewInfo.Src           = BasePath(*Src) * Files(i);
						NewInfo.Master        = BasePath(Master) * Files(i);
						NewInfo.MasterRecurse = 0;
						FStringOutputDevice Out;
						NewInfo.Write( Out, 1 );
						AllFiles.Add( *It.Key(), *Out );
					}
				}
				else AllFiles.Add( *It.Key(), *It.Value() );
			}
		}}
		Map.Remove( TEXT("File") );
		Map.Remove( TEXT("Copy") );
		{for( TMultiMap<FString,FString>::TIterator It(AllFiles); It; ++It )
		{
			// Compose filenames.
			FFileInfo Info( *It.Value() );
			check(Info.Src!=TEXT(""));
			FString Master = MasterPath * (Info.Master!=TEXT("") ? Info.Master : Info.Src);

			// Update size.
			Info.Size = GFileManager->FileSize( *Master );
			if( GRefPath==TEXT("") )
				Info.Ref = TEXT("");
			if( Info.Ref!=TEXT("") )
				Info.RefSize = GFileManager->FileSize( *(GRefPath*Info.Ref) );
			if( Info.Size<0 )
				appErrorf( TEXT("Missing file %s"), *Master );
			FStringOutputDevice Str;
			Info.Write( Str, 1 );

			// Add to list.
			Map.Add( *It.Key(), *Str );
		}}
		unguard;
	}

	// Copy a group.
	void CopyGroup( FString MasterPath, const TCHAR* File, const TCHAR* Group, TMultiMap<FString,FString>& Map )
	{
		guard(CopyGroup);
		GWarn->Logf( TEXT("   Copying group %s"), Group );
		for( TMultiMap<FString,FString>::TIterator It(Map); It; ++It )
		{
			if( It.Key()==TEXT("File") || It.Key()==TEXT("Copy") )
			{
				// Compose filenames.
				FFileInfo Info( It.Value() );
				check(Info.Src!=TEXT(""));
				FString Master = MasterPath * (Info.Master!=TEXT("") ? Info.Master : Info.Src);
				FString Src = GSrcPath * Info.Src;

				// Copy the file.
				FString Base = BasePath(Src);
				if( GArchive==TEXT("") && !GFileManager->MakeDirectory( *Base, 1 ) )
					appErrorf( TEXT("Failed to create directory %s"), *Base );
				if( GRefPath==TEXT("") || Info.Ref==TEXT("") )
				{
					GWarn->Logf( TEXT("   Copying %s to %s"), *Master, *Src );
					LocalCopyFile( *Info.Src, *Master, Info.Flags );
				}
				else
				{
					FString Ref = GRefPath * Info.Ref;
					DeltaCode( *Ref, *Master, *Info.Src );
				}
			}
		}
		unguard;
	}

	// Recursively process all groups.
	void ProcessGroup( FString MasterPath, const TCHAR* File, const TCHAR* Group, void(UMasterCommandlet::*Process)( FString MasterPath, const TCHAR* File, const TCHAR* Group, TMultiMap<FString,FString>& Map ) )
	{
		guard(ProcessGroup);
		TMultiMap<FString,FString>* Map = GConfig->GetSectionPrivate( Group, 0, 1, File );
		if( !Map )
			appErrorf( TEXT("Group '%s' not found in file '%s'"), Group, File );
		FString Str;
		if( GConfig->GetString( Group, TEXT("MasterPath"), Str, File ) )
			MasterPath = appFormat(Str,*GConfig->GetSectionPrivate(TEXT("Setup"),1,1,File));
		(this->*Process)( MasterPath, File, Group, *Map );
		for( TMultiMap<FString,FString>::TIterator It(*Map); It; ++It )
			if( It.Key()==TEXT("Group") )
				ProcessGroup( MasterPath, File, *It.Value(), Process );

		unguard;
	}

	// Static constructor.
	void StaticConstructor()
	{
		guard(UMasterCommandlet::StaticConstructor);
		LogToStdout = 0;
		IsClient    = 0;
		IsEditor    = 0;
		IsServer    = 0;
		LazyLoad    = 1;
		unguard;
	}

	// Main.
	INT Main( const TCHAR* Parms )
	{
		guard(UMasterCommandlet::Main);
		INT i;

		// Delete all manifest files.
		TArray<FString> Manifests = GFileManager->FindFiles( MANIFEST_FILE TEXT(".*"), 1, 0 );
		for( i=0; i<Manifests.Num(); i++ )
			if( !GFileManager->Delete( *Manifests(i), 1, 1 ) )
				appErrorf( TEXT("Failed to delete manifest file: %s"), *Manifests(i) );

		// Get configuration file.
		GConfigFile = MANIFEST_FILE MANIFEST_EXT;
		FString ConfigBase;
		if( !ParseToken( Parms, ConfigBase, 0 ) )
			appErrorf( TEXT("Config (%s) filename not specified"), MANIFEST_EXT );
		if( ConfigBase.Right(4)!=MANIFEST_EXT )
			ConfigBase += MANIFEST_EXT;
		if( GFileManager->FileSize(*ConfigBase)<0 )
			appErrorf( TEXT("Can't find config file %s"), *ConfigBase );
		if( !GFileManager->Copy( *GConfigFile, *ConfigBase, 1, 1, 0, NULL ) )
			appErrorf( TEXT("Error copying config file %s to %s"), *ConfigBase, *GConfigFile );
		GWarn->Logf( TEXT("Using config: %s"), *ConfigBase );

		// Copy all localized manifest files.
		UBOOL GotIntManifest = 0;
		TArray<FString> List = GFileManager->FindFiles( *(ConfigBase.LeftChop(4) + TEXT(".*") ), 1, 0 );
		for( i=0; i<List.Num(); i++ )
		{
			INT Pos = List(i).InStr(TEXT("."),1);
			if( Pos>=0 )
			{
				FString Ext = List(i).Mid(Pos);
				if( Ext!=MANIFEST_EXT )
				{
					FString Str;
					GConfig->Detach(*List(i));
					if( List(i)!=GConfigFile && GConfig->GetString(TEXT("Setup"),TEXT("LocalProduct"),Str,*List(i)) )
					{
						FString Dest = FString(MANIFEST_FILE)+Ext;
						GWarn->Logf( TEXT("   Copying manifest %s to %s"), *List(i), *Dest );
						if( !GFileManager->Copy( *Dest, *List(i), 0, 0, 0, NULL ) )
							appErrorf( TEXT("Failed to copy manifest file: %s"), *Dest );
						if( Ext==TEXT(".int") )
							GotIntManifest = 1;
					}
				}
			}
		}
		if( !GotIntManifest )
			appErrorf( TEXT("Failed to create Manifest.int") );

		// Copy command line parameters to [Build] section.
		FString Temp;
		while( ParseToken(Parms,Temp,0) )
		{
			INT Pos = Temp.InStr(TEXT("="));
			if( Pos<0 )
				appErrorf( TEXT("Option '%' unrecognized"), *Temp );
			GConfig->SetString( TEXT("Setup"), *Temp.Left(Pos), *Temp.Mid(Pos+1), *GConfigFile );
		}

		// Init.
		GConfig->GetString( TEXT("Setup"), TEXT("Archive"), GArchive, *GConfigFile );
		GArchive = appFormat(GArchive,*GConfig->GetSectionPrivate(TEXT("Setup"),1,1,*GConfigFile));
		GConfig->GetString( TEXT("Setup"), TEXT("RefPath"), GRefPath, *GConfigFile );
		if( !GConfig->GetString( TEXT("Setup"), TEXT("MasterPath"), GMasterPath, *GConfigFile ) )
			appErrorf( TEXT("Missing MasterPath=") );
		if( !GConfig->GetString( TEXT("Setup"), TEXT("SrcPath"), GSrcPath, *GConfigFile ) )
			appErrorf( TEXT("Missing SrcPath=") );
		GSrcPath = appFormat(GSrcPath,*GConfig->GetSectionPrivate(TEXT("Setup"),1,1,*GConfigFile));
		if( GArchive==TEXT("") )
		{
			// Make dest path.
			if( !GFileManager->DeleteDirectory( *GSrcPath, 0, 1 ) )
				appErrorf( TEXT("Failed to remove directory tree: %s"), *GSrcPath );
			if( !GFileManager->MakeDirectory( *GSrcPath, 1 ) )
				appErrorf( TEXT("Failed to create directory: %s"), *GSrcPath );
		}
		else if( GArchive.Right(4)==TEXT(".exe") )
		{
			// Write stub to self-extracting exe.
			GFileManager->Delete( *GArchive, 1, 0 );
			TArray<BYTE> Buffer;
			verify(appLoadFileToArray(Buffer,SFX_STUB));
			GArchiveData.Serialize( &Buffer(0), Buffer.Num() );
		}

		// Process and copy the groups.
		ProcessGroup( GMasterPath, *GConfigFile, TEXT("Setup"), UpdateGroup );
		GConfig->Flush( 0 );
		ProcessGroup( GMasterPath, *GConfigFile, TEXT("Setup"), CopyGroup );

		// Flush archive.
		if( GArchive!=TEXT("") )
		{
			FString Filename = GSrcPath * GArchive;
			GWarn->Logf( TEXT("   Saving archive: %s"), *Filename );
			GArc.TableOffset = GArchiveData.Num();
			GArchiveData << GArc._Items_;
			GArc.CRC      = appMemCrc( &GArchiveData(0), GArchiveData.Num(), 0 );
			GArc.FileSize = GArchiveData.Tell() + ARCHIVE_HEADER_SIZE;
			GArchiveData << GArc;
			if( !appSaveArrayToFile( GArchiveData, *Filename ) )
				appErrorf( TEXT("Failed saving archive: %s"), *Filename );
		}
		unguard;
		return 1;
	}
};
IMPLEMENT_CLASS(UMasterCommandlet)

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
