/*=============================================================================
	UnEdSrv.cpp: UEditorEngine implementation, the Unreal editing server
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	What's happening: When the Visual Basic level editor is being used,
	this code exchanges messages with Visual Basic.  This lets Visual Basic
	affect the world, and it gives us a way of sending world information back
	to Visual Basic.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EditorPrivate.h"
#include "UnRender.h"
#include "../../Engine/Src/UnPath.h"

#pragma DISABLE_OPTIMIZATION /* Not performance-critical */

/*-----------------------------------------------------------------------------
	UnrealEd safe command line.
-----------------------------------------------------------------------------*/

//
// Execute a macro.
//
void UEditorEngine::ExecMacro( const TCHAR* Filename, FOutputDevice& Ar )
{
	guard(UEditorEngine::ExecMacro);

	// Create text buffer and prevent garbage collection.
	UTextBuffer* Text = ImportObject<UTextBuffer>( GetTransientPackage(), NAME_None, 0, Filename );
	if( Text )
	{
		Text->AddToRoot();
		debugf( TEXT("Execing %s"), Filename );
		TCHAR Temp[256];
		const TCHAR* Data = *Text->Text;
		while( ParseLine( &Data, Temp, ARRAY_COUNT(Temp) ) )
			Exec( Temp, Ar );
		Text->RemoveFromRoot();
		delete Text;
	}
	else Ar.Logf( NAME_ExecWarning, LocalizeError("FileNotFound",TEXT("UEditorEngine")), Filename );

	unguard;
}

//
// Execute a command that is safe for rebuilds.
//
UBOOL UEditorEngine::SafeExec( const TCHAR* InStr, FOutputDevice& Ar )
{
	guard(UEditorEngine::SafeExec);
	TCHAR TempFname[256], TempStr[256], TempName[NAME_SIZE];
	INT i;
	const TCHAR* Str=InStr;
	if( ParseCommand(&Str,TEXT("MACRO")) || ParseCommand(&Str,TEXT("EXEC")) )//oldver (exec)
	{
		TCHAR Filename[64];
		if( ParseToken( Str, Filename, ARRAY_COUNT(Filename), 0 ) )
			ExecMacro( Filename, Ar );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("NEW")) )
	{
		// Generalized object importing.
		DWORD   Flags         = RF_Public|RF_Standalone;
		if( ParseCommand(&Str,TEXT("STANDALONE")) )
			Flags = RF_Public|RF_Standalone;
		else if( ParseCommand(&Str,TEXT("PUBLIC")) )
			Flags = RF_Public;
		else if( ParseCommand(&Str,TEXT("PRIVATE")) )
			Flags = 0;
		FString ClassName     = ParseToken(Str,0);
		UClass* Class         = FindObject<UClass>( ANY_PACKAGE, *ClassName );
		if( !Class )
		{
			Ar.Logf( NAME_ExecWarning, TEXT("Unrecognized or missing factor class %s"), *ClassName );
			return 1;
		}
		FString  PackageName  = ParentContext ? ParentContext->GetName() : TEXT("");
		FString  FileName     = TEXT("");
		FString  ObjectName   = TEXT("");
		UClass*  ContextClass = NULL;
		UObject* Context      = NULL;
		Parse( Str, TEXT("Package="), PackageName );
		Parse( Str, TEXT("File="), FileName );
		ParseObject( Str, TEXT("ContextClass="), UClass::StaticClass(), *(UObject**)&ContextClass, NULL );
		ParseObject( Str, TEXT("Context="), ContextClass, Context, NULL );
		if
		(	!Parse( Str, TEXT("Name="), ObjectName )
		&&	FileName!=TEXT("") )
		{
			// Deduce object name from filename.
			ObjectName = FileName;
			for( ; ; )
			{
				INT i=ObjectName.InStr(PATH_SEPARATOR);
				if( i==-1 )
					i=ObjectName.InStr(TEXT("/"));
				if( i==-1 )
					break;
				ObjectName = ObjectName.Mid( i+1 );
			}
			if( ObjectName.InStr(TEXT("."))>=0 )
				ObjectName = ObjectName.Left( ObjectName.InStr(TEXT(".")) );
		}
		UFactory* Factory = NULL;
		if( Class->IsChildOf(UFactory::StaticClass()) )
			Factory = ConstructObject<UFactory>( Class );
		UObject* Object = UFactory::StaticImportObject
		(
			Factory ? Factory->SupportedClass : Class,
			CreatePackage(NULL,*PackageName),
			*ObjectName,
			Flags,
			*FileName,
			Context,
			Factory,
			Str,
			GWarn
		);
		if( !Object )
			Ar.Logf( NAME_ExecWarning, TEXT("Failed factoring: %s"), InStr );
		GCache.Flush( 0, ~0, 1 );
		return 1;
	}
	else if( ParseCommand( &Str, TEXT("LOAD") ) )
	{
		// Object file loading.
		if( Parse( Str, TEXT("FILE="), TempFname, 80 ) )
		{
			if( !ParentContext )
				Level->RememberActors();
			TCHAR PackageName[256]=TEXT("");
			UObject* Pkg=NULL;
			if( Parse( Str, TEXT("Package="), PackageName, ARRAY_COUNT(PackageName) ) )
			{
				TCHAR Temp[256], *End;
				appStrcpy( Temp, PackageName );
				End = appStrchr(Temp,'.');
				if( End )
					*End++ = 0;
				Pkg = CreatePackage( NULL, PackageName );
			}
			Pkg = LoadPackage( Pkg, TempFname, 0 );
			if( *PackageName )
				ResetLoaders( Pkg, 0, 1 );
			GCache.Flush();
			if( !ParentContext )
			{
				Level->ReconcileActors();
				RedrawLevel(Level);
			}
		}
		else Ar.Log( NAME_ExecWarning, TEXT("Missing filename") );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("Texture")) )
	{
		if( ParseCommand(&Str,TEXT("Import")) )
		{
			// Texture importing.
			//->FACTOR TEXTURE ...
			FName PkgName = ParentContext ? ParentContext->GetFName() : NAME_None;
			Parse( Str, TEXT("Package="), PkgName );
			if( PkgName!=NAME_None && Parse( Str, TEXT("File="), TempFname, ARRAY_COUNT(TempFname) ) )
			{
				UPackage* Pkg = CreatePackage(NULL,*PkgName);
				if( !Parse( Str, TEXT("Name="),  TempName,  NAME_SIZE ) )
				{
					// Deduce package name from filename.
					TCHAR* End = TempFname + appStrlen(TempFname);
					while( End>TempFname && End[-1]!=PATH_SEPARATOR[0] && End[-1]!='/' )
						End--;
					appStrncpy( TempName, End, NAME_SIZE );
					if( appStrchr(TempName,'.') )
						*appStrchr(TempName,'.') = 0;
				}
				GWarn->BeginSlowTask( TEXT("Importing texture"), 1, 0 );
				UBOOL DoMips=1;
				ParseUBOOL( Str, TEXT("Mips="), DoMips );
				extern TCHAR* GFile;
				GFile = TempFname;
				FName GroupName = NAME_None;
				if( Parse( Str, TEXT("GROUP="), GroupName ) && GroupName!=NAME_None )
					Pkg = CreatePackage(Pkg,*GroupName);
				UTexture* Texture = ImportObject<UTexture>( Pkg, TempName, RF_Public|RF_Standalone, TempFname );
				if( Texture )
				{
					DWORD TexFlags=0;
					Parse( Str, TEXT("LODSet="), Texture->LODSet );
					Parse( Str, TEXT("TexFlags="), TexFlags );
					Parse( Str, TEXT("FLAGS="),    Texture->PolyFlags );
					ParseObject<UTexture>( Str, TEXT("DETAIL="), Texture->DetailTexture, ANY_PACKAGE );
					ParseObject<UTexture>( Str, TEXT("MTEX="), Texture->MacroTexture, ANY_PACKAGE );
					ParseObject<UTexture>( Str, TEXT("NEXT="), Texture->AnimNext, ANY_PACKAGE );
					Texture->CreateMips( DoMips, 1 );
					Texture->CreateColorRange();
					UBOOL AlphaTrick=0;
					ParseUBOOL( Str, TEXT("ALPHATRICK="), AlphaTrick );
					if( AlphaTrick )
						for( INT i=0; i<256; i++ )
							Texture->Palette->Colors(i).A = Texture->Palette->Colors(i).B;
					debugf( NAME_Log, TEXT("Imported %s"), Texture->GetFullName() );
				}
				else Ar.Logf( NAME_ExecWarning, TEXT("Import texture %s from %s failed"), TempName, TempFname );
				GWarn->EndSlowTask();
				GCache.Flush( 0, ~0, 1 );
			}
			else Ar.Logf( NAME_ExecWarning, TEXT("Missing file or name") );
			return 1;
		}
	}
	else if( ParseCommand(&Str,TEXT("FONT")) )//oldver
	{
		if( ParseCommand(&Str,TEXT("IMPORT")) )//oldver
			return SafeExec( *(US+TEXT("NEW FONTFACTORY ")+Str), Ar ); 
	}
	else if( ParseCommand(&Str,TEXT("OBJ")) )//oldver
	{
		UClass* Type;
		if( ParseCommand( &Str, TEXT("LOAD") ) )//oldver
			return SafeExec( *(US+TEXT("LOAD ")+Str), Ar ); 
		else if( ParseCommand(&Str,TEXT("IMPORT")) )//oldver
			if( ParseObject<UClass>( Str, TEXT("TYPE="), Type, ANY_PACKAGE ) )
				return SafeExec( *(US+TEXT("NEW STANDALONE ")+Type->GetName()+TEXT(" ")+Str), Ar ); 
		return 0;
	}
	else if( ParseCommand( &Str, TEXT("MESHMAP")) )
	{
		if( ParseCommand( &Str, TEXT("SCALE") ) )
		{
			// Mesh scaling.
			UMesh* Mesh;
			if( ParseObject<UMesh>( Str, TEXT("MESHMAP="), Mesh, ANY_PACKAGE ) )
			{
				FVector Scale(0,0,0);
				GetFVECTOR( Str, Scale );
				Mesh->SetScale( Scale );
				FCoords Coords = GMath.UnitCoords * FVector(0,0,0) * Mesh->RotOrigin * FScale(Mesh->Scale,0.0,SHEER_None);
				TArray<FLOAT> RMS(Mesh->TextureLOD.Num()), Count(Mesh->TextureLOD.Num());
				{for( INT i=0; i<Mesh->TextureLOD.Num(); i++ )
					RMS(i)=Count(i)=0.0;}
				{for( INT n=0; n<Mesh->AnimFrames; n++ )
				{
					for( INT i=0; i<Mesh->Tris.Num(); i++ )
					{
						FMeshTri& Tri = Mesh->Tris(i);
						for( INT j=0,k=2; j<3; k=j++ )
						{
							FLOAT Space  = (Mesh->Verts(n*Mesh->FrameVerts+Tri.iVertex[j]).Vector()-Mesh->Verts(n*Mesh->FrameVerts+Tri.iVertex[k]).Vector()).TransformVectorBy(Coords).Size();
							FLOAT Texels = appSqrt(Square((INT)Tri.Tex[j].U-(INT)Tri.Tex[k].U) + Square((INT)Tri.Tex[j].V-(INT)Tri.Tex[k].V));
							RMS  (Tri.TextureIndex) += /*Square*/(Space/(Texels+1.0));
							Count(Tri.TextureIndex) += 1.0;
						}
					}
				}}
				{for( INT i=0; i<Mesh->TextureLOD.Num(); i++ )
				{
					Mesh->TextureLOD(i) = /*appSqrt*/(RMS(i)/(0.01+Count(i)));
					if( Count(i)>0.0 )
						debugf( TEXT("Texture LOD factor for %s %i = %f"), Mesh->GetName(), i, Mesh->TextureLOD(i) );
				}}
			}
			else Ar.Log( NAME_ExecWarning, TEXT("Missing meshmap") );
			return 1;
		}
		else if( ParseCommand( &Str, TEXT("SETTEXTURE") ) )
		{
			// Mesh texture mapping.
			UMesh* Mesh;
			UTexture* Texture;
			INT Num;
			if
			(	ParseObject<UMesh>( Str, TEXT("MESHMAP="), Mesh, ANY_PACKAGE )
			&&	ParseObject<UTexture>( Str, TEXT("TEXTURE="), Texture, ANY_PACKAGE )
			&&	Parse( Str, TEXT("NUM="), Num )
			&&	Num<Mesh->Textures.Num() )
			{
				Mesh->Textures( Num ) = Texture;
				FLOAT TextureLod=1.0;
				Parse( Str, TEXT("TLOD="), TextureLod );
				if( Num < Mesh->TextureLOD.Num() )
					Mesh->TextureLOD( Num ) *= TextureLod;
			}
			else Ar.Logf( NAME_ExecWarning, TEXT("Missing meshmap, texture, or num (%s)"), Str );
			return 1;
		}
	}
	else if( ParseCommand(&Str,TEXT("MESH")) )
	{
#if 0 /* Pre LOD */
		if( ParseCommand(&Str,TEXT("IMPORT")) )
		{
			// Mesh importing.
			TCHAR TempStr1[256];
			if
			(	Parse( Str, TEXT("MESH="), TempName, ARRAY_COUNT(TempName) )
			&&	Parse( Str, TEXT("ANIVFILE="), TempStr, ARRAY_COUNT(TempStr) )
			&&	Parse( Str, TEXT("DATAFILE="), TempStr1, ARRAY_COUNT(TempStr1) ) )
			{
				UBOOL Unmirror=0, ZeroTex=0; INT UnMirrorTex;
				ParseUBOOL( Str, TEXT("UNMIRROR="), Unmirror );
				ParseUBOOL( Str, TEXT("ZEROTEX="), ZeroTex );
				if( !Parse( Str, TEXT("UNMIRRORTEX="), UnMirrorTex ) )
					UnMirrorTex = -1;
				meshImport( TempName, ParentContext, TempStr, TempStr1, Unmirror, ZeroTex, UnMirrorTex );
			}
			else Ar.Log(NAME_ExecWarning,TEXT("Bad MESH IMPORT"));
			return 1;
		}
#else
		if( ParseCommand(&Str,TEXT("IMPORT")) )
		{
			// Mesh importing.
			TCHAR TempStr1[256];
			if
			(	Parse( Str, TEXT("MESH="), TempName, ARRAY_COUNT(TempName) )
			&&	Parse( Str, TEXT("ANIVFILE="), TempStr, ARRAY_COUNT(TempStr) )
			&&	Parse( Str, TEXT("DATAFILE="), TempStr1, ARRAY_COUNT(TempStr1) ) )
			{
				UBOOL Unmirror=0, ZeroTex=0; INT UnMirrorTex;

				ULODProcessInfo LODInfo;
				LODInfo.LevelOfDetail = true; 

#if ENGINE_VERSION>=230
				LODInfo.OldAnimFormat = 0;
				if( !Parse(Str,TEXT("REORDER="),LODInfo.OldAnimFormat) )
					LODInfo.OldAnimFormat = 0;
#else
				LODInfo.OldAnimFormat = 1; 
#endif

				LODInfo.Style = 0;				
				LODInfo.SampleFrame = 0;
				LODInfo.NoUVData = false;
				
				ParseUBOOL( Str, TEXT("UNMIRROR="), Unmirror );
				ParseUBOOL( Str, TEXT("ZEROTEX="), ZeroTex );

				ParseUBOOL( Str, TEXT("MLOD="),  LODInfo.LevelOfDetail ); 
				Parse(Str,TEXT("LODSTYLE="),	 LODInfo.Style );
				Parse(Str,TEXT("LODFRAME="),	 LODInfo.SampleFrame );
				ParseUBOOL(Str,TEXT("LODNOTEX="),LODInfo.NoUVData );
				ParseUBOOL(Str,TEXT("LODOLD="),  LODInfo.OldAnimFormat );

				if( !Parse( Str, TEXT("UNMIRRORTEX="), UnMirrorTex ) )
					UnMirrorTex = -1;
				meshImport( TempName, ParentContext, TempStr, TempStr1, Unmirror, ZeroTex, UnMirrorTex, &LODInfo );
			}
			else Ar.Log(NAME_ExecWarning,TEXT("Bad MESH IMPORT"));
			return 1;
		}
		else if( ParseCommand(&Str, TEXT("DROPFRAMES")) )
		{
			UMesh* Mesh;
			INT StartFrame;
			INT NumFrames;
			if
			(	ParseObject<UMesh>( Str, TEXT("MESH="), Mesh, ANY_PACKAGE )
			&&	Parse( Str, TEXT("STARTFRAME="), StartFrame )
			&&	Parse( Str, TEXT("NUMFRAMES="), NumFrames ) )
			{
				meshDropFrames(Mesh, StartFrame, NumFrames);
			}
		}
		// LodMeshes: parse LOD specific parameters.
		else if( ParseCommand(&Str,TEXT("LODPARAMS")) )
		{
			// Mesh origin.
			UMesh *Mesh;
			if( ParseObject<UMesh>(Str,TEXT("MESH="),Mesh,ANY_PACKAGE) )
			{
				// Ignore the LOD-specific parameters if Mesh is not a true ULodMesh.
				if( Mesh->IsA(ULodMesh::StaticClass()))
				{			
					// If not set, they keep their default values.
					ULodMesh* LodMesh = (ULodMesh*)Mesh;
					
					Parse(Str,TEXT("MINVERTS="),    LodMesh->LODMinVerts);
					Parse(Str,TEXT("STRENGTH="),    LodMesh->LODStrength);
					Parse(Str,TEXT("MORPH="),		LodMesh->LODMorph);
					Parse(Str,TEXT("HYSTERESIS="),	LodMesh->LODHysteresis);
					Parse(Str,TEXT("ZDISP="),       LodMesh->LODZDisplace);					

					// check validity
					if( (LodMesh->LODMorph < 0.0f) || (LodMesh->LODMorph >1.0f) )
					{
						LodMesh->LODMorph = 0.0f;
						Ar.Log( NAME_ExecWarning, TEXT("Bad LOD MORPH supplied."));	
					}
					if( (LodMesh->LODMinVerts < 0) || (LodMesh->LODMinVerts > LodMesh->FrameVerts) )
					{
						LodMesh->LODMinVerts = Max(10,LodMesh->FrameVerts);
						Ar.Log( NAME_ExecWarning, TEXT("Bad LOD MINVERTS supplied."));	
					}
					if( LodMesh->LODStrength < 0.00001f )
					{
						LodMesh->LODStrength = 0.0f;
					}
				}
				else Ar.Log( NAME_ExecWarning, TEXT("Need a LOD mesh (MLOD=1) for these LODPARAMS."));
			}
			else Ar.Log( NAME_ExecWarning, TEXT("Bad MESH LODPARAMS") );
			return 1;
		}
#endif
		else if( ParseCommand(&Str,TEXT("ORIGIN")) )
		{
			// Mesh origin.
			UMesh *Mesh;
			if( ParseObject<UMesh>(Str,TEXT("MESH="),Mesh,ANY_PACKAGE) )
			{
				GetFVECTOR ( Str, Mesh->Origin );
				GetFROTATOR( Str, Mesh->RotOrigin, 256 );
			}
			else Ar.Log( NAME_ExecWarning, TEXT("Bad MESH ORIGIN") );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("SEQUENCE")) )
		{
			// Mesh animation sequences.
			UMesh *Mesh;
			FMeshAnimSeq Seq;
			if
			(	ParseObject<UMesh>( Str, TEXT("MESH="), Mesh, ANY_PACKAGE )
			&&	Parse( Str, TEXT("SEQ="), Seq.Name )
			&&	Parse( Str, TEXT("STARTFRAME="), Seq.StartFrame )
			&&	Parse( Str, TEXT("NUMFRAMES="), Seq.NumFrames ) )
			{
				Parse( Str, TEXT("RATE="), Seq.Rate );
				Parse( Str, TEXT("GROUP="), Seq.Group );
				for( INT i=0; i<Mesh->AnimSeqs.Num(); i++ )
					if( Mesh->AnimSeqs(i).Name==Seq.Name )
						break;
				if( i<Mesh->AnimSeqs.Num() )
					Mesh->AnimSeqs(i)=Seq;
				else
					new( Mesh->AnimSeqs )FMeshAnimSeq( Seq );
				Mesh->AnimSeqs.Shrink();
			}
			else Ar.Log(NAME_ExecWarning,TEXT("Bad MESH SEQUENCE"));
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("NOTIFY")) )
		{
			// Mesh notifications.
			UMesh* Mesh;
			FName SeqName;
			FMeshAnimNotify Notify;
			if
			(	ParseObject<UMesh>( Str, TEXT("MESH="), Mesh, ANY_PACKAGE )
			&&	Parse( Str, TEXT("SEQ="), SeqName )
			&&	Parse( Str, TEXT("TIME="), Notify.Time )
			&&	Parse( Str, TEXT("FUNCTION="), Notify.Function ) )
			{
				FMeshAnimSeq* Seq = Mesh->GetAnimSeq( SeqName );
				if( Seq ) new( Seq->Notifys )FMeshAnimNotify( Notify );
				else Ar.Log( NAME_ExecWarning, TEXT("Unknown sequence in MESH NOTIFY") );
			}
			else Ar.Log( NAME_ExecWarning, TEXT("Bad MESH NOTIFY") );
			return 1;
		}
	}
	else if( ParseCommand( &Str, TEXT("AUDIO")) )//oldver
	{
		if( ParseCommand(&Str,TEXT("IMPORT")) )//oldver
		{
			FString File, Name, Group;
			Parse(Str,TEXT("FILE="),File);
			FString PkgName = ParentContext ? ParentContext->GetName() : Level->GetOuter()->GetName();
			Parse( Str, TEXT("PACKAGE="), PkgName );
			UPackage* Pkg = CreatePackage(NULL,*PkgName);
			if( Parse(Str,TEXT("GROUP="),Group) && Group!=NAME_None )
				Pkg = CreatePackage( Pkg, *Group );
			FString Cmd = US + TEXT("NEW SOUND FILE=") + File + TEXT(" PACKAGE=") + PkgName;
			if( Parse(Str,TEXT("GROUP="),Group) )
				Cmd = Cmd + TEXT(".") + Group;
			if( Parse(Str,TEXT("NAME="),Name) )
				Cmd = Cmd + TEXT(" NAME=") + Name;
			return SafeExec( *Cmd, Ar ); 
		}
	}
	return 0;
	unguardf(( TEXT("(%s)"), InStr ));
}

/*-----------------------------------------------------------------------------
	UnrealEd command line.
-----------------------------------------------------------------------------*/

//
// Process an incoming network message meant for the editor server
//
UBOOL UEditorEngine::Exec( const TCHAR* Stream, FOutputDevice& Ar )
{
	//debugf("GEditor Exec: %s",Stream);
	TCHAR ErrorTemp[256]=TEXT("Setup: ");
	guard(UEditorEngine::Exec);
	UBOOL Processed=0;
	INT i;

	_WORD	 		Word1,Word2,Word4;
	INT				Index1;
	TCHAR	 		TempStr[256],TempFname[256],TempName[256],Temp[256];

	if( appStrlen(Stream)<200 )
	{
		appStrcat( ErrorTemp, Stream );
		debugf( NAME_Cmd, Stream );
	}

	UModel* Brush = Level ? Level->Brush()->Brush : NULL;
	//if( Brush ) check(stricmp(Brush->GetName(),"BRUSH")==0);

	appStrncpy( Temp, Stream, 256 );
	const TCHAR* Str = &Temp[0];

	appStrncpy( ErrorTemp, Str, 79 );
	ErrorTemp[79]=0;

	//------------------------------------------------------------------------------------
	// BRUSH
	//
	if( SafeExec( Stream, Ar ) )
	{
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("BRUSH")) )
	{
		if( ParseCommand(&Str,TEXT("APPLYTRANSFORM")) )
		{
			goto ApplyXf;
		}
		else if( ParseCommand(&Str,TEXT("SET")) )
		{
			Trans->Begin( TEXT("Brush Set") );
			Brush->Modify();
			Constraints.Snap( NULL, Level->Brush()->Location, FVector(0,0,0), Level->Brush()->Rotation );
			FModelCoords TempCoords;
			Level->Brush()->BuildCoords( &TempCoords, NULL );
			Level->Brush()->Location -= Level->Brush()->PrePivot.TransformVectorBy( TempCoords.PointXform );
			Level->Brush()->PrePivot = FVector(0,0,0);
			Brush->Polys->Element.Empty();
			UPolysFactory* It = new UPolysFactory;
			It->FactoryCreateText( UPolys::StaticClass(), Brush->Polys->GetOuter(), Brush->Polys->GetName(), 0, Brush->Polys, TEXT("t3d"), Stream, Stream+appStrlen(Stream), GWarn );
			bspValidateBrush( Brush, 1, 1 );
			Brush->BuildBound();
			Trans->End();
			RedrawLevel( Level );
			NoteSelectionChange( Level );
			Processed = 1;
		}
		else if( ParseCommand(&Str,TEXT("MORE")) )
		{
			Trans->Continue();
			Brush->Modify();
			UPolysFactory* It = new UPolysFactory;
			It->FactoryCreateText( UPolys::StaticClass(), Brush->Polys->GetOuter(), Brush->Polys->GetName(), 0, Brush->Polys, TEXT("t3d"), Stream, Stream+appStrlen(Stream), GWarn );
			bspValidateBrush( Level->Brush()->Brush, 1, 1 );
			Brush->BuildBound();
			Trans->End();	
			RedrawLevel( Level );
			Processed = 1;
		}
		else if( ParseCommand(&Str,TEXT("RESET")) )
		{
			Trans->Begin( TEXT("Brush Reset") );
			Level->Brush()->Modify();
			Level->Brush()->InitPosRotScale();
			Trans->End();
			RedrawLevel(Level);
			Processed = 1;
		}
		else if( ParseCommand(&Str,TEXT("MIRROR")) )
		{
			Trans->Begin( TEXT("Brush Mirror") );
			Level->Brush()->Modify();
			if (ParseCommand(&Str,TEXT("X"))) Level->Brush()->MainScale.Scale.X *= -1.0;
			if (ParseCommand(&Str,TEXT("Y"))) Level->Brush()->MainScale.Scale.Y *= -1.0;
			if (ParseCommand(&Str,TEXT("Z"))) Level->Brush()->MainScale.Scale.Z *= -1.0;
			Trans->End();
			RedrawLevel(Level);
			Processed = 1;
		}
		else if( ParseCommand(&Str,TEXT("SCALE")) )
		{
			Trans->Begin( TEXT("Brush Scale") );
			Level->Brush()->Modify();
			if( ParseCommand(&Str,TEXT("RESET")) )
			{
				Level->Brush()->MainScale = GMath.UnitScale;
				Level->Brush()->PostScale = GMath.UnitScale;
			}
			else
			{
				GetFVECTOR( Str, Level->Brush()->MainScale.Scale );
				Parse( Str, TEXT("SHEER="), Level->Brush()->MainScale.SheerRate );
				if( Parse( Str, TEXT("SHEERAXIS="), TempStr, 255 ) )
				{
					if      (appStricmp(TempStr,TEXT("XY"))==0)	Level->Brush()->MainScale.SheerAxis = SHEER_XY;
					else if (appStricmp(TempStr,TEXT("XZ"))==0)	Level->Brush()->MainScale.SheerAxis = SHEER_XZ;
					else if (appStricmp(TempStr,TEXT("YX"))==0)	Level->Brush()->MainScale.SheerAxis = SHEER_YX;
					else if (appStricmp(TempStr,TEXT("YZ"))==0)	Level->Brush()->MainScale.SheerAxis = SHEER_YZ;
					else if (appStricmp(TempStr,TEXT("ZX"))==0)	Level->Brush()->MainScale.SheerAxis = SHEER_ZX;
					else if (appStricmp(TempStr,TEXT("ZY"))==0)	Level->Brush()->MainScale.SheerAxis = SHEER_ZY;
					else									Level->Brush()->MainScale.SheerAxis = SHEER_None;
				}
			}
			Trans->End();
			RedrawLevel(Level);
			Processed = 1;
		}
		else if( ParseCommand(&Str,TEXT("ROTATETO")) )
		{
			Trans->Begin( TEXT("Brush RotateTo") );
			Level->Brush()->Modify();
			GetFROTATOR( Str, Level->Brush()->Rotation, 256 );
			Trans->End();
			RedrawLevel(Level);
			Processed = 1;
		}
		else if( ParseCommand(&Str,TEXT("ROTATEREL")) )
		{
			Trans->Begin( TEXT("Brush RotateRel") );
			Level->Brush()->Modify();
			FRotator TempRotation(0,0,0);
			GetFROTATOR( Str, TempRotation, 256 );
			Level->Brush()->Rotation += TempRotation;
			Trans->End();
			RedrawLevel(Level);
			Processed = 1;
		}
		else if( ParseCommand(&Str,TEXT("MOVETO")) )
		{
			Trans->Begin( TEXT("Brush MoveTo") );
			Level->Brush()->Modify();
			GetFVECTOR( Str, Level->Brush()->Location );
			Trans->End();
			RedrawLevel(Level);
			Processed = 1;
		}
		else if( ParseCommand(&Str,TEXT("MOVEREL")) )
		{
			Trans->Begin( TEXT("Brush MoveRel") );
			Level->Brush()->Modify();
			FVector TempVector( 0, 0, 0 );
			GetFVECTOR( Str, TempVector );
			Level->Brush()->Location.AddBounded( TempVector );
			Trans->End();
			RedrawLevel(Level);
			Processed = 1;
		}
		else if (ParseCommand(&Str,TEXT("ADD")))
		{
			Trans->Begin( TEXT("Brush Add") );
			FinishAllSnaps(Level);
			INT DWord1=0;
			Parse( Str, TEXT("FLAGS="), DWord1 );
			Level->Modify();
			ABrush* NewBrush = csgAddOperation( Level->Brush(), Level, DWord1, CSG_Add );
			if( NewBrush )
				bspBrushCSG( NewBrush, Level->Model, DWord1, CSG_Add, 1 );
			Trans->End();
			RedrawLevel(Level);
			EdCallback(EDC_MapChange,0);
			Processed = 1;
		}
		else if (ParseCommand(&Str,TEXT("ADDMOVER"))) // BRUSH ADDMOVER
		{
			Trans->Begin( TEXT("Brush AddMover") );
			Level->Modify();
			FinishAllSnaps( Level );

			UClass* MoverClass = NULL;
			ParseObject<UClass>( Str, TEXT("CLASS="), MoverClass, ANY_PACKAGE );
			if( !MoverClass || !MoverClass->IsChildOf(AMover::StaticClass()) )
				MoverClass = AMover::StaticClass();

			Level->Modify();
			AMover* Actor = (AMover*)Level->SpawnActor(MoverClass,NAME_None,NULL,NULL,Level->Brush()->Location);
			if( Actor )
			{
				csgCopyBrush( Actor, Level->Brush(), 0, 0, 1 );
				Actor->PostEditChange();
			}
			Trans->End();
			RedrawLevel(Level);
			Processed = 1;
		}
		else if (ParseCommand(&Str,TEXT("SUBTRACT"))) // BRUSH SUBTRACT
			{
			Trans->Begin( TEXT("Brush Subtract") );
			FinishAllSnaps(Level);
			Level->Modify();
			ABrush* NewBrush = csgAddOperation(Level->Brush(),Level,0,CSG_Subtract); // Layer
			if( NewBrush )
				bspBrushCSG( NewBrush, Level->Model, 0, CSG_Subtract, 1 );
			Trans->End();
			RedrawLevel(Level);
			EdCallback(EDC_MapChange,0);
			Processed = 1;
			}
		else if (ParseCommand(&Str,TEXT("FROM"))) // BRUSH FROM ACTOR/INTERSECTION/DEINTERSECTION
		{
			if( ParseCommand(&Str,TEXT("INTERSECTION")) )
			{
				Ar.Log( TEXT("Brush from intersection") );
				Trans->Begin( TEXT("Brush From Intersection") );
				Brush->Modify();
				FinishAllSnaps( Level );
				bspBrushCSG( Level->Brush(), Level->Model, 0, CSG_Intersect, 0 );
				Trans->End();
				RedrawLevel( Level );
				Processed = 1;
			}
			else if( ParseCommand(&Str,TEXT("DEINTERSECTION")) )
			{
				Ar.Log( TEXT("Brush from deintersection") );
				Trans->Begin( TEXT("Brush From Deintersection") );
				Brush->Modify();
				FinishAllSnaps( Level );
				bspBrushCSG( Level->Brush(), Level->Model, 0, CSG_Deintersect, 0 );
				Trans->End();
				RedrawLevel( Level );
				Processed = 1;
			}
		}
		else if( ParseCommand (&Str,TEXT("NEW")) )
		{
			Trans->Begin( TEXT("Brush New") );
			Brush->Modify();
			Brush->Polys->Element.Empty();
			Trans->End();
			RedrawLevel( Level );
			Processed = 1;
		}
		else if( ParseCommand (&Str,TEXT("LOAD")) ) // BRUSH LOAD
		{
			if( Parse( Str, TEXT("FILE="), TempFname, 79 ) )
			{
				Trans->Reset( TEXT("loading brush") );
				FVector TempVector = Level->Brush()->Location;
				FRotator TempRotation = Level->Brush()->Rotation;
				LoadPackage( Level->GetOuter(), TempFname, 0 );
				Level->Brush()->Location = TempVector;
				Level->Brush()->Rotation = TempRotation;
				bspValidateBrush( Level->Brush()->Brush, 0, 1 );
				Cleanse( 1, TEXT("loading brush") );
				Processed = 1;
			}
		}
		else if( ParseCommand( &Str, TEXT("SAVE") ) )
		{
			if( Parse(Str,TEXT("FILE="),TempFname,79) )
			{
				Ar.Logf( TEXT("Saving %s"), TempFname );
				SavePackage( Level->GetOuter(), Brush, 0, TempFname, GWarn );
			}
			else Ar.Log( NAME_ExecWarning, TEXT("Missing filename") );
			Processed = 1;
		}
		else if( ParseCommand( &Str, TEXT("IMPORT")) )
		{
			if( Parse(Str,TEXT("FILE="),TempFname,79) )
			{
				GWarn->BeginSlowTask( TEXT("Importing brush"), 1, 0 );
				Trans->Begin( TEXT("Brush Import") );
				Brush->Polys->Modify();
				Brush->Polys->Element.Empty();
				DWORD Flags=0;
				UBOOL Merge=0;
				ParseUBOOL( Str, TEXT("MERGE="), Merge );
				Parse( Str, TEXT("FLAGS="), Flags );
				Brush->Linked = 0;
				ImportObject<UPolys>( Brush->Polys->GetOuter(), Brush->Polys->GetName(), 0, TempFname );
				if( Flags )
					for( Word2=0; Word2<TempModel->Polys->Element.Num(); Word2++ )
						Brush->Polys->Element(Word2).PolyFlags |= Flags;
				for( INT i=0; i<Brush->Polys->Element.Num(); i++ )
					Brush->Polys->Element(i).iLink = i;
				if( Merge )
				{
					bspMergeCoplanars( Brush, 0, 1 );
					bspValidateBrush( Brush, 0, 1 );
				}
				Trans->End();
				GWarn->EndSlowTask();
			}
			else Ar.Log( NAME_ExecWarning, TEXT("Missing filename") );
			Processed=1;
		}
		else if (ParseCommand(&Str,TEXT("EXPORT")))
		{
			if( Parse(Str,TEXT("FILE="),TempFname,79) )
			{
				GWarn->BeginSlowTask( TEXT("Exporting brush"), 1, 0 );
				UExporter::ExportToFile( Brush->Polys, NULL, TempFname );
				GWarn->EndSlowTask();
			}
			else Ar.Log(NAME_ExecWarning,TEXT("Missing filename"));
			Processed=1;
		}
	}
	//----------------------------------------------------------------------------------
	// EDIT
	//
	else if( ParseCommand(&Str,TEXT("EDIT")) )
	{
		if( ParseCommand(&Str,TEXT("CUT")) )
		{
			Trans->Begin( TEXT("Cut") );
			edactCopySelected( Level );
			edactDeleteSelected( Level );
			Trans->End();
			RedrawLevel( Level );
		}
		else if( ParseCommand(&Str,TEXT("COPY")) )
		{
			edactCopySelected( Level );
		}
		else if( ParseCommand(&Str,TEXT("PASTE")) )
		{
			Trans->Begin( TEXT("Cut") );
			SelectNone( Level, 1 );
			edactPasteSelected( Level );
			Trans->End();
			RedrawLevel( Level );
		}
	}
	//----------------------------------------------------------------------------------
	// PIVOT
	//
	else if( ParseCommand(&Str,TEXT("PIVOT")) )
	{
		if( ParseCommand(&Str,TEXT("HERE")) )
		{
			NoteActorMovement( Level );
			SetPivot( ClickLocation, 0, 0 );
			FinishAllSnaps( Level );
			RedrawLevel( Level );
		}
		else if( ParseCommand(&Str,TEXT("SNAPPED")) )
		{
			NoteActorMovement( Level );
			SetPivot( ClickLocation, 1, 0 );
			FinishAllSnaps( Level );
			RedrawLevel( Level );
		}
	}
	//----------------------------------------------------------------------------------
	// PATHS
	//
	else if( ParseCommand(&Str,TEXT("PATHS")) )
	{
		if (ParseCommand(&Str,TEXT("BUILD")))
		{
			int opt = 1; //assume medium
			if (ParseCommand(&Str,TEXT("LOWOPT")))
				opt = 0;
			else if (ParseCommand(&Str,TEXT("HIGHOPT")))
				opt = 2;
			FPathBuilder builder;
			Trans->Reset( TEXT("Paths") );
			Level->Modify();
			INT numpaths = builder.removePaths( Level );
			numpaths = builder.buildPaths( Level, opt );
			RedrawLevel( Level );
			Ar.Logf( TEXT("Built Paths: %d"), numpaths );
			Processed=1;
		}
		else if (ParseCommand(&Str,TEXT("SHOW")))
		{
			FPathBuilder builder;
			Trans->Reset( TEXT("Paths") );
			int numpaths = builder.showPaths(Level);
			RedrawLevel(Level);
			Ar.Logf( TEXT(" %d Paths are visible!"), numpaths );
			Processed=1;
		}
		else if (ParseCommand(&Str,TEXT("HIDE")))
		{
			FPathBuilder builder;
			Trans->Reset( TEXT("Paths") );
			int numpaths = builder.hidePaths(Level);
			RedrawLevel(Level);
			Ar.Logf( TEXT(" %d Paths are hidden!"), numpaths);
			Processed=1;
		}
		else if (ParseCommand(&Str,TEXT("REMOVE")))
		{
			FPathBuilder builder;
			Trans->Reset( TEXT("Paths") );
			int numpaths = builder.removePaths( Level );
			RedrawLevel( Level );
			Ar.Logf( TEXT("Removed %d Paths"), numpaths );
			Processed=1;
		}
		else if (ParseCommand(&Str,TEXT("UNDEFINE")))
		{
			FPathBuilder builder;
			Trans->Reset( TEXT("Paths") );
			builder.undefinePaths( Level );
			RedrawLevel(Level);
		}
		else if (ParseCommand(&Str,TEXT("DEFINE")))
		{
			FPathBuilder builder;
			Trans->Reset( TEXT("Paths") );
			builder.undefinePaths( Level );
			builder.definePaths( Level );
			RedrawLevel(Level);
			Processed=1;
		}
	}
	//------------------------------------------------------------------------------------
	// Bsp
	//
	else if( ParseCommand( &Str, TEXT("BSP") ) )
	{
		if( ParseCommand( &Str, TEXT("REBUILD")) ) // Bsp REBUILD [LAME/GOOD/OPTIMAL] [BALANCE=0-100] [LIGHTS] [MAPS] [REJECT]
		{
			Trans->Reset( TEXT("rebuilding Bsp") ); // Not tracked transactionally
			Ar.Log(TEXT("Bsp Rebuild"));
			EBspOptimization BspOpt;

			if      (ParseCommand(&Str,TEXT("LAME"))) 		BspOpt=BSP_Lame;
			else if (ParseCommand(&Str,TEXT("GOOD")))		BspOpt=BSP_Good;
			else if (ParseCommand(&Str,TEXT("OPTIMAL")))	BspOpt=BSP_Optimal;
			else											BspOpt=BSP_Good;

			if( !Parse( Str, TEXT("BALANCE="), Word2 ) )
				Word2=50;

			GWarn->BeginSlowTask( TEXT("Rebuilding Bsp"), 1, 0 );

			GWarn->StatusUpdatef( 0, 0, TEXT("%s"), TEXT("Building polygons") );
			bspBuildFPolys( Level->Model, 1, 0 );

			GWarn->StatusUpdatef( 0, 0, TEXT("%s"), TEXT("Merging planars") );
			bspMergeCoplanars( Level->Model, 0, 0 );

			GWarn->StatusUpdatef( 0, 0, TEXT("%s"), TEXT("Partitioning") );
			bspBuild( Level->Model, BspOpt, Word2, 0, 0 );

			if( Parse( Str, TEXT("ZONES"), TempStr, 1 ) )
			{
				GWarn->StatusUpdatef( 0, 0, TEXT("%s"), TEXT("Building visibility zones") );
				TestVisibility( Level, Level->Model, 0, 0 );
			}
			if( Parse( Str, TEXT("OPTGEOM"), TempStr, 1 ) )
			{
				GWarn->StatusUpdatef( 0, 0, TEXT("%s"), TEXT("Optimizing geometry") );
				bspOptGeom( Level->Model );
			}

			// Empty EdPolys.
			Level->Model->Polys->Element.Empty();

			GWarn->EndSlowTask();
			GCache.Flush();
			RedrawLevel(Level);
			EdCallback( EDC_MapChange, 0 );

			Processed=1;
		}
	}
	//------------------------------------------------------------------------------------
	// LIGHT
	//
	else if( ParseCommand( &Str, TEXT("LIGHT") ) )
	{
		if( ParseCommand( &Str, TEXT("APPLY") ) )
		{
			UBOOL Selected=0;
			ParseUBOOL( Str, TEXT("SELECTED="), Selected );
			shadowIlluminateBsp( Level, Selected );
			GCache.Flush();
			RedrawLevel( Level );
			Processed=1;
		}
	}
	//------------------------------------------------------------------------------------
	// MAP
	//
	else if (ParseCommand(&Str,TEXT("MAP")))
		{
		//
		// Commands:
		//
		if (ParseCommand(&Str,TEXT("GRID"))) // MAP GRID [SHOW3D=ON/OFF] [SHOW2D=ON/OFF] [X=..] [Y=..] [Z=..]
			{
			//
			// Before changing grid, force editor to current grid position to avoid jerking:
			//
			FinishAllSnaps (Level);
			GetFVECTOR( Str, Constraints.GridSize );
			RedrawLevel(Level);
			Processed=1;
			}
		else if (ParseCommand(&Str,TEXT("ROTGRID"))) // MAP ROTGRID [PITCH=..] [YAW=..] [ROLL=..]
			{
			FinishAllSnaps (Level);
			if( GetFROTATOR( Str, Constraints.RotGridSize, 256 ) )
				RedrawLevel(Level);
			Processed=1;
			}
		else if (ParseCommand(&Str,TEXT("SELECT")))
		{
			Trans->Begin( TEXT("Select") );
			if( ParseCommand(&Str,TEXT("ADDS")) )
				mapSelectOperation( Level, CSG_Add );
			else if( ParseCommand(&Str,TEXT("SUBTRACTS")) )
				mapSelectOperation( Level, CSG_Subtract );
			else if( ParseCommand(&Str,TEXT("SEMISOLIDS")) )
				mapSelectFlags( Level, PF_Semisolid );
			else if( ParseCommand(&Str,TEXT("NONSOLIDS")) )
				mapSelectFlags( Level, PF_NotSolid );
			else if( ParseCommand(&Str,TEXT("FIRST")) )
				mapSelectFirst( Level );
			else if( ParseCommand(&Str,TEXT("LAST")) )
				mapSelectLast( Level );
			Trans->End ();
			RedrawLevel( Level );
			Processed=1;
		}
		else if( ParseCommand(&Str,TEXT("DELETE")) )
		{
			Exec( TEXT("ACTOR DELETE"), Ar );
			Processed=1;
		}
		else if( ParseCommand(&Str,TEXT("BRUSH")) )
		{
			if( ParseCommand (&Str,TEXT("GET")) )
			{
				Trans->Begin( TEXT("Brush Get") );
				mapBrushGet( Level );
				Trans->End();
				RedrawLevel( Level );
				Processed=1;
			}
			else if( ParseCommand (&Str,TEXT("PUT")) )
			{
				Trans->Begin( TEXT("Brush Put") );
				mapBrushPut( Level );
				Trans->End();
				RedrawLevel( Level );
				Processed=1;
			}
		}
		else if (ParseCommand(&Str,TEXT("SENDTO")))
		{
			if( ParseCommand(&Str,TEXT("FIRST")) )
			{
				Trans->Begin( TEXT("Map SendTo Front") );
				mapSendToFirst( Level );
				Trans->End();
				RedrawLevel( Level );
				Processed=1;
			}
			else if( ParseCommand(&Str,TEXT("LAST")) )
			{
				Trans->Begin( TEXT("Map SendTo Back") );
				mapSendToLast( Level );
				Trans->End();
				RedrawLevel( Level );
				Processed=1;
			}
		}
		else if( ParseCommand(&Str,TEXT("REBUILD")) )
		{
			Trans->Reset( TEXT("rebuilding map") );
			csgRebuild( Level );
			GCache.Flush();
			RedrawLevel( Level );
			EdCallback( EDC_MapChange, 0 );
			Processed=1;
		}
		else if( ParseCommand (&Str,TEXT("NEW")) )
		{
			Trans->Reset( TEXT("clearing map") );
			Level->RememberActors();
			Level = new( Level->GetOuter(), TEXT("MyLevel") )ULevel( this, 0 );
			Level->ReconcileActors();
			ResetSound();
			RedrawLevel(Level);
			NoteSelectionChange( Level );
			EdCallback(EDC_MapChange,0);
			Cleanse( 1, TEXT("starting new map") );
			Processed=1;
		}
		else if( ParseCommand( &Str, TEXT("LOAD") ) )
		{
			if( Parse( Str, TEXT("FILE="), TempFname, 79 ) )
			{
				Trans->Reset( TEXT("loading map") );
				GWarn->BeginSlowTask( TEXT("Loading map"), 1, 0 );
				Level->RememberActors();
				ResetLoaders( Level->GetOuter(), 0, 0 );
				LoadPackage( Level->GetOuter(), TempFname, 0 );
				Level->Engine = this;
				Level->ReconcileActors();
				ResetSound();
				bspValidateBrush( Level->Brush()->Brush, 0, 1 );
				GWarn->EndSlowTask();
				RedrawLevel(Level);
				EdCallback( EDC_MapChange, 0 );
				NoteSelectionChange( Level );
				Level->SetFlags( RF_Transactional );
				Level->Model->SetFlags( RF_Transactional );
				if( Level->Model->Polys ) Level->Model->Polys->SetFlags( RF_Transactional );
				for( TObjectIterator<AActor> It; It; ++It )
				{
					for( INT i=0; i<Level->Actors.Num(); i++ )
						if( *It==Level->Actors(i) )
							break;
					if( i==Level->Actors.Num() )
					{
						It->bDeleteMe=1;
					}
					else
					{
						It->bDeleteMe=0;
						if( Cast<ACamera>(*It) )
							It->ClearFlags( RF_Transactional );
						else
							It->SetFlags( RF_Transactional );
					}
				}
				GCache.Flush();
				Cleanse( 0, TEXT("loading map") );
			}
			else Ar.Log( NAME_ExecWarning, TEXT("Missing filename") );
			Processed=1;
		}
		else if( ParseCommand (&Str,TEXT("SAVE")) )
		{
			if( Parse(Str,TEXT("FILE="),TempFname,79) )
			{
				Level->ShrinkLevel();
				Level->CleanupDestroyed( 1 );
				ALevelInfo* OldInfo = FindObject<ALevelInfo>(Level->GetOuter(),TEXT("LevelInfo0"));
				if( OldInfo && OldInfo!=Level->GetLevelInfo() )
					OldInfo->Rename();
				if( Level->GetLevelInfo()!=OldInfo )
					Level->GetLevelInfo()->Rename(TEXT("LevelInfo0"));
				ULevelSummary* Summary = Level->GetLevelInfo()->Summary = new(Level->GetOuter(),TEXT("LevelSummary"),RF_Public)ULevelSummary;
				Summary->Title					= Level->GetLevelInfo()->Title;
				Summary->Author					= Level->GetLevelInfo()->Author;
				Summary->IdealPlayerCount		= Level->GetLevelInfo()->IdealPlayerCount;
				Summary->RecommendedEnemies		= Level->GetLevelInfo()->RecommendedEnemies;
				Summary->RecommendedTeammates	= Level->GetLevelInfo()->RecommendedTeammates;
				Summary->LevelEnterText			= Level->GetLevelInfo()->LevelEnterText;
				GWarn->BeginSlowTask( TEXT("Saving map"), 1, 0 );
				SavePackage( Level->GetOuter(), Level, 0, TempFname, GWarn );
				GWarn->EndSlowTask();
			}
			else Ar.Log( NAME_ExecWarning, TEXT("Missing filename") );
			Processed=1;
		}
		else if( ParseCommand( &Str, TEXT("IMPORT") ) )
		{
			Word1=1;
			DoImportMap:
			if( Parse( Str, TEXT("FILE="), TempFname, 79 ) )
			{
				Trans->Reset( TEXT("importing map") );
				GWarn->BeginSlowTask( TEXT("Importing map"), 1, 0 );
				Level->RememberActors();
				if( Word1 )
					Level = new( Level->GetOuter(), TEXT("MyLevel") )ULevel( this, 0 );
				ImportObject<ULevel>( Level->GetOuter(), Level->GetFName(), RF_Transactional, TempFname );
				GCache.Flush();
				Level->ReconcileActors();
				ResetSound();
				if( Word1 )
					SelectNone( Level, 0 );
				GWarn->EndSlowTask();
				RedrawLevel(Level);
				EdCallback( EDC_MapChange, 0 );
				NoteSelectionChange( Level );
				Cleanse( 1, TEXT("importing map") );
			}
			else Ar.Log(NAME_ExecWarning,TEXT("Missing filename"));
			Processed=1;
		}
		else if( ParseCommand( &Str, TEXT("IMPORTADD") ) )
		{
			Word1=0;
			SelectNone( Level, 0 );
			goto DoImportMap;
		}
		else if (ParseCommand (&Str,TEXT("EXPORT")))
			{
			if (Parse(Str,TEXT("FILE="),TempFname,79))
				{
				GWarn->BeginSlowTask( TEXT("Exporting map"), 1, 0 );
				for( FObjectIterator It; It; ++It )
					It->ClearFlags( RF_TagImp | RF_TagExp );
				UExporter::ExportToFile( Level, NULL, TempFname );
				GWarn->EndSlowTask();
				}
			else Ar.Log(NAME_ExecWarning,TEXT("Missing filename"));
			Processed=1;
			}
		else if (ParseCommand (&Str,TEXT("SETBRUSH"))) // MAP SETBRUSH (set properties of all selected brushes)
			{
			Trans->Begin( TEXT("Set Brush Properties") );
			//
			Word1  = 0;  // Properties mask
			INT DWord1 = 0;  // Set flags
			INT DWord2 = 0;  // Clear flags
			//
			FName GroupName=NAME_None;
			if (Parse(Str,TEXT("COLOR="),Word2))			Word1 |= MSB_BrushColor;
			if (Parse(Str,TEXT("GROUP="),GroupName))		Word1 |= MSB_Group;
			if (Parse(Str,TEXT("SETFLAGS="),DWord1))		Word1 |= MSB_PolyFlags;
			if (Parse(Str,TEXT("CLEARFLAGS="),DWord2))		Word1 |= MSB_PolyFlags;
			//
			mapSetBrush(Level,(EMapSetBrushFlags)Word1,Word2,GroupName,DWord1,DWord2);
			//
			Trans->End			();
			RedrawLevel(Level);
			//
			Processed=1;
			}
		else if (ParseCommand (&Str,TEXT("SAVEPOLYS")))
			{
			if (Parse(Str,TEXT("FILE="),TempFname,79))
				{
				UBOOL DWord2=1;
				ParseUBOOL(Str, TEXT("MERGE="), DWord2 );
				//
				GWarn->BeginSlowTask( TEXT("Exporting map polys"), 1, 0 );
				GWarn->StatusUpdatef( 0, 0, TEXT("%s"), TEXT("Building polygons") );
				bspBuildFPolys( Level->Model, 0, 0 );
				//
				if (DWord2)
					{
					GWarn->StatusUpdatef( 0, 0, TEXT("%s"), TEXT("Merging planars") );
					bspMergeCoplanars	(Level->Model,0,1);
					};
				UExporter::ExportToFile( Level->Model->Polys, NULL, TempFname );
				Level->Model->Polys->Element.Empty();
				//
				GWarn->EndSlowTask 	();
				RedrawLevel(Level);
				}
			else Ar.Log( NAME_ExecWarning, TEXT("Missing filename") );
			Processed=1;
			};
		}
	//------------------------------------------------------------------------------------
	// SELECT: Rerouted to mode-specific command
	//
	else if( ParseCommand(&Str,TEXT("SELECT")) )
	{
		if( ParseCommand(&Str,TEXT("NONE")) )
		{
			Trans->Begin( TEXT("Select None") );
			SelectNone( Level, 1 );
			Trans->End();
			RedrawLevel( Level );
			Processed=1;
		}
		Processed=1;
	}
	//------------------------------------------------------------------------------------
	// DELETE: Rerouted to mode-specific command
	//
	else if (ParseCommand(&Str,TEXT("DELETE")))
	{
		return Exec( TEXT("ACTOR DELETE") );
	}
	//------------------------------------------------------------------------------------
	// DUPLICATE: Rerouted to mode-specific command
	//
	else if (ParseCommand(&Str,TEXT("DUPLICATE")))
	{
		return Exec( TEXT("ACTOR DUPLICATE") );
	}
	//------------------------------------------------------------------------------------
	// ACTOR: Actor-related functions
	//
	else if (ParseCommand(&Str,TEXT("ACTOR")))
	{
		if( ParseCommand(&Str,TEXT("ADD")) )
		{
			UClass* Class;
			if( ParseObject<UClass>( Str, TEXT("CLASS="), Class, ANY_PACKAGE ) )
			{
				AActor* Default   = Class->GetDefaultActor();
				FVector Collision = FVector(Default->CollisionRadius,Default->CollisionRadius,Default->CollisionHeight);
				FVector Location  = ClickLocation + ClickPlane * (FBoxPushOut(ClickPlane,Collision) + 0.1);
				AddActor( Level, Class, Location );
				RedrawLevel(Level);
				Processed = 1;
			}
		}
		else if( ParseCommand(&Str,TEXT("MIRROR")) )
		{
			Trans->Begin( TEXT("Mirroring Actors") );
			FVector V( 1, 1, 1 );
			GetFVECTOR( Str, V );
			for( INT i=0; i<Level->Actors.Num(); i++ )
			{
				ABrush* Actor=Cast<ABrush>(Level->Actors(i));
				if( Actor && Actor->bSelected )
				{
					Actor->Modify();
					Actor->MainScale.Scale *= V;
				}
			}
			Trans->End();
			RedrawLevel(Level);
			Processed = 1;
		}
		else if( ParseCommand(&Str,TEXT("HIDE")) )
		{
			if( ParseCommand(&Str,TEXT("SELECTED")) ) // ACTOR HIDE SELECTED
			{
				Trans->Begin( TEXT("Hide Selected") );
				Level->Modify();
				edactHideSelected( Level );
				Trans->End();
				RedrawLevel( Level );
				SelectNone( Level, 0 );
				NoteSelectionChange( Level );
				Processed=1;
			}
			else if( ParseCommand(&Str,TEXT("UNSELECTED")) ) // ACTOR HIDE UNSELECTEED
			{
				Trans->Begin( TEXT("Hide Unselected") );
				Level->Modify();
				edactHideUnselected( Level );
				Trans->End();
				RedrawLevel( Level );
				SelectNone( Level, 0 );
				NoteSelectionChange( Level );
				Processed=1;
			}
		}
		else if( ParseCommand(&Str,TEXT("UNHIDE")) ) // ACTOR UNHIDE ALL
		{
			// "ACTOR UNHIDE ALL" = "Drawing Region: Off": also disables the far (Z) clipping plane
			ResetZClipping();
			Trans->Begin( TEXT("UnHide All") );
			Level->Modify();
			edactUnHideAll( Level );
			Trans->End();
			RedrawLevel( Level );
			NoteSelectionChange( Level );
			Processed=1;
		}
		else if( ParseCommand(&Str, TEXT("APPLYTRANSFORM")) )
		{
		ApplyXf:
			Trans->Begin( TEXT("Apply brush transform") );
			Level->Modify();
			edactApplyTransform( Level );
			Trans->End();
			RedrawLevel( Level );
			Processed=1;
		}
		else if( ParseCommand(&Str,TEXT("CLIP")) ) // ACTOR CLIP Z/XY/XYZ
		{
			if( ParseCommand(&Str,TEXT("Z")) )
			{
				SetZClipping();
				RedrawLevel( Level );
				Processed=1;
			}
		}
		else if( ParseCommand(&Str, TEXT("REPLACE")) )
		{
			UClass* Class;
			if( ParseCommand(&Str, TEXT("BRUSH")) ) // ACTOR REPLACE BRUSH
			{
				Trans->Begin( TEXT("Replace selected brush actors") );
				Level->Modify();
				edactReplaceSelectedBrush( Level );
				Trans->End();
				RedrawLevel( Level );
				NoteSelectionChange( Level );
				Processed=1;
			}
			else if( ParseObject<UClass>( Str, TEXT("CLASS="), Class, ANY_PACKAGE ) ) // ACTOR REPLACE CLASS=<class>
			{
				Trans->Begin( TEXT("Replace selected non-brush actors") );
				Level->Modify();
				edactReplaceSelectedWithClass( Level, Class );
				Trans->End();
				RedrawLevel( Level );
				NoteSelectionChange( Level );
				Processed=1;
			}
		}
		else if( ParseCommand(&Str,TEXT("SELECT")) )
		{
			if( ParseCommand(&Str,TEXT("NONE")) ) // ACTOR SELECT NONE
			{
				return Exec( TEXT("SELECT NONE") );
			}
			else if( ParseCommand(&Str,TEXT("ALL")) ) // ACTOR SELECT ALL
			{
				Trans->Begin( TEXT("Select All") );
				Level->Modify();
				edactSelectAll( Level );
				Trans->End();
				RedrawLevel( Level );
				NoteSelectionChange( Level );
				Processed=1;
			}
			else if( ParseCommand(&Str,TEXT("INSIDE") ) ) // ACTOR SELECT INSIDE
			{
				Trans->Begin( TEXT("Select Inside") );
				Level->Modify();
				edactSelectInside( Level );
				Trans->End();
				RedrawLevel( Level );
				NoteSelectionChange( Level );
				Processed=1;
			}
			else if( ParseCommand(&Str,TEXT("INVERT") ) ) // ACTOR SELECT INVERT
			{
				Trans->Begin( TEXT("Select Invert") );
				Level->Modify();
				edactSelectInvert( Level );
				Trans->End();
				RedrawLevel( Level );
				NoteSelectionChange( Level );
				Processed=1;
			}
			else if( ParseCommand(&Str,TEXT("OFCLASS")) ) // ACTOR SELECT OFCLASS CLASS=<class>
			{
				UClass* Class;
				if( ParseObject<UClass>(Str,TEXT("CLASS="),Class,ANY_PACKAGE) )
				{
					Trans->Begin( TEXT("Select of class") );
					Level->Modify();
					edactSelectOfClass( Level, Class );
					Trans->End();
					RedrawLevel( Level );
					NoteSelectionChange( Level );
				}
				else Ar.Log( NAME_ExecWarning, TEXT("Missing class") );
				Processed=1;
			}
		}
		else if( ParseCommand(&Str,TEXT("DELETE")) )
		{
			Trans->Begin( TEXT("Delete Actors") );
			Level->Modify();
			edactDeleteSelected( Level );
			Trans->End();
			RedrawLevel( Level );
			NoteSelectionChange( Level );
			Processed=1;
		}
		else if( ParseCommand(&Str,TEXT("RESET")) )
		{
			Trans->Begin( TEXT("Reset Actors") );
			Level->Modify();
			UBOOL Location=0;
			UBOOL Pivot=0;
			UBOOL Rotation=0;
			UBOOL Scale=0;
			if( ParseCommand(&Str,TEXT("LOCATION")) )
			{
				Location=1;
				ResetPivot();
			}
			else if( ParseCommand(&Str, TEXT("PIVOT")) )
			{
				Pivot=1;
				ResetPivot();
			}
			else if( ParseCommand(&Str,TEXT("ROTATION")) )
			{
				Rotation=1;
			}
			else if( ParseCommand(&Str,TEXT("SCALE")) )
			{
				Scale=1;
			}
			else if( ParseCommand(&Str,TEXT("ALL")) )
			{
				Location=Rotation=Scale=1;
				ResetPivot();
			}
			for( INT i=0; i<Level->Actors.Num(); i++ )
			{
				AActor* Actor=Level->Actors(i);
				if( Actor && Actor->bSelected )
				{
					Actor->Modify();
					if( Location ) Actor->Location  = FVector(0,0,0);
					if( Location ) Actor->PrePivot  = FVector(0,0,0);
					if( Pivot && Cast<ABrush>(Actor) )
					{
						ABrush* Brush = Cast<ABrush>(Actor);
						FModelCoords Coords, Uncoords;
						Brush->BuildCoords( &Coords, &Uncoords );
						Brush->Location -= Brush->PrePivot.TransformVectorBy( Coords.PointXform );
						Brush->PrePivot = FVector(0,0,0);
						Brush->PostEditChange();
					}
					if( Rotation ) Actor->Rotation  = FRotator(0,0,0);
					if( Scale    ) Actor->DrawScale = 1.0;
					if( Scale && Cast<ABrush>(Actor) )
					{
						Cast<ABrush>(Actor)->MainScale=GMath.UnitScale;
						Cast<ABrush>(Actor)->PostScale=GMath.UnitScale;
					}
				}
			}
			Trans->End();
			RedrawLevel( Level );
			Processed=1;
		}
		else if( ParseCommand(&Str,TEXT("DUPLICATE")) )
		{
			Trans->Begin( TEXT("Duplicate Actors") );
			Level->Modify();
			edactDuplicateSelected( Level );
			Trans->End();
			RedrawLevel( Level );
			NoteSelectionChange( Level );
			Processed=1;
		}
#if 1 //LEGEND
		else if( ParseCommand(&Str, TEXT("ALIGN")) )
		{
			Trans->Begin( TEXT("Align brush vertices") );
			Level->Modify();
			edactAlignVertices( Level );
			Trans->End();
			RedrawLevel( Level );
			Processed=1;
		}
#endif
		else if( ParseCommand(&Str,TEXT("KEYFRAME")) )
		{
			INT Num=0;
			Parse(Str,TEXT("NUM="),Num);
			Trans->Begin( TEXT("Set mover keyframe") );
			Level->Modify();
			for( INT i=0; i<Level->Actors.Num(); i++ )
			{
				AMover* Mover=Cast<AMover>(Level->Actors(i));
				if( Mover && Mover->bSelected )
				{
					Mover->Modify();
					Mover->KeyNum = Num;
					Mover->PostEditChange();
					SetPivot( Mover->Location, 0, 0 );
				}
			}
			Trans->End();
			RedrawLevel( Level );
			Processed=1;
		}
	}
	//------------------------------------------------------------------------------------
	// POLY: Polygon adjustment and mapping
	//
	else if( ParseCommand(&Str,TEXT("POLY")) )
	{
		if( ParseCommand(&Str,TEXT("SELECT")) ) // POLY SELECT [ALL/NONE/INVERSE] FROM [LEVEL/SOLID/GROUP/ITEM/ADJACENT/MATCHING]
		{
			appSprintf( TempStr, TEXT("POLY SELECT %s"), Str );
			if( ParseCommand(&Str,TEXT("NONE")) )
			{
				return Exec( TEXT("SELECT NONE") );
				Processed=1;
			}
			else if( ParseCommand(&Str,TEXT("ALL")) )
			{
				Trans->Begin( TempStr );
				SelectNone( Level, 0 );
				polySelectAll( Level->Model );
				NoteSelectionChange( Level );
				Processed=1;
				Trans->End();
			}
			else if( ParseCommand(&Str,TEXT("REVERSE")) )
			{
				Trans->Begin( TempStr );
				polySelectReverse (Level->Model);
				EdCallback(EDC_SelPolyChange,0);
				Processed=1;
				Trans->End();
			}
			else if( ParseCommand(&Str,TEXT("MATCHING")) )
			{
				Trans->Begin( TempStr );
				if 		(ParseCommand(&Str,TEXT("GROUPS")))		polySelectMatchingGroups(Level->Model);
				else if (ParseCommand(&Str,TEXT("ITEMS")))		polySelectMatchingItems(Level->Model);
				else if (ParseCommand(&Str,TEXT("BRUSH")))		polySelectMatchingBrush(Level->Model);
				else if (ParseCommand(&Str,TEXT("TEXTURE")))	polySelectMatchingTexture(Level->Model);
				EdCallback(EDC_SelPolyChange,0);
				Processed=1;
				Trans->End();
			}
			else if( ParseCommand(&Str,TEXT("ADJACENT")) )
			{
				Trans->Begin( TempStr );
				if 	  (ParseCommand(&Str,TEXT("ALL")))			polySelectAdjacents( Level->Model );
				else if (ParseCommand(&Str,TEXT("COPLANARS")))	polySelectCoplanars( Level->Model );
				else if (ParseCommand(&Str,TEXT("WALLS")))		polySelectAdjacentWalls( Level->Model );
				else if (ParseCommand(&Str,TEXT("FLOORS")))		polySelectAdjacentFloors( Level->Model );
				else if (ParseCommand(&Str,TEXT("CEILINGS")))	polySelectAdjacentFloors( Level->Model );
				else if (ParseCommand(&Str,TEXT("SLANTS")))		polySelectAdjacentSlants( Level->Model );
				EdCallback(EDC_SelPolyChange,0);
				Processed=1;
				Trans->End();
			}
			else if( ParseCommand(&Str,TEXT("MEMORY")) )
			{
				Trans->Begin( TempStr );
				if 		(ParseCommand(&Str,TEXT("SET")))		polyMemorizeSet( Level->Model );
				else if (ParseCommand(&Str,TEXT("RECALL")))		polyRememberSet( Level->Model );
				else if (ParseCommand(&Str,TEXT("UNION")))		polyUnionSet( Level->Model );
				else if (ParseCommand(&Str,TEXT("INTERSECT")))	polyIntersectSet( Level->Model );
				else if (ParseCommand(&Str,TEXT("XOR")))		polyXorSet( Level->Model );
				EdCallback(EDC_SelPolyChange,0);
				Processed=1;
				Trans->End();
			}
#if 1 //LEGEND
			else if( ParseCommand(&Str,TEXT("ZONE")) )
			{
				Trans->Begin( TempStr );
				polySelectZone(Level->Model);
				EdCallback(EDC_SelPolyChange,0);
				Processed=1;
				Trans->End();
			}
#endif
			RedrawLevel(Level);
		}
		else if( ParseCommand(&Str,TEXT("DEFAULT")) ) // POLY DEFAULT <variable>=<value>...
		{
			CurrentTexture=NULL;
			ParseObject<UTexture>(Str,TEXT("TEXTURE="),CurrentTexture,ANY_PACKAGE);
			Processed=1;
		}
		else if( ParseCommand(&Str,TEXT("SETTEXTURE")) )
		{
			Trans->Begin( TEXT("Poly SetTexture") );
			Level->Model->ModifySelectedSurfs(1);
			for( Index1=0; Index1<Level->Model->Surfs.Num(); Index1++ )
			{
				if( Level->Model->Surfs(Index1).PolyFlags & PF_Selected )
				{
					Level->Model->Surfs(Index1).Texture = CurrentTexture;
					polyUpdateMaster( Level->Model, Index1, 0, 0 );
				}
			}
			Trans->End();
			RedrawLevel(Level);
			Processed=1;
		}
		else if( ParseCommand(&Str,TEXT("SET")) ) // POLY SET <variable>=<value>...
		{
			Trans->Begin( TEXT("Poly Set") );
			Level->Model->ModifySelectedSurfs( 1 );
			UTexture *Texture;
			if (ParseObject<UTexture>(Str,TEXT("TEXTURE="),Texture,ANY_PACKAGE))
				{
				for (Index1=0; Index1<Level->Model->Surfs.Num(); Index1++)
					{
					if (Level->Model->Surfs(Index1).PolyFlags & PF_Selected)
						{
						Level->Model->Surfs(Index1).Texture  = Texture;
						polyUpdateMaster( Level->Model, Index1, 0, 0 );
						};
					};
				};
			Word4  = 0;
			INT DWord1 = 0;
			INT DWord2 = 0;
			if (Parse(Str,TEXT("SETFLAGS="),DWord1))   Word4=1;
			if (Parse(Str,TEXT("CLEARFLAGS="),DWord2)) Word4=1;
			if (Word4)  polySetAndClearPolyFlags (Level->Model,DWord1,DWord2,1,1); // Update selected polys' flags
			//
			Trans->End();
			RedrawLevel(Level);
			Processed=1;
		}
		else if( ParseCommand(&Str,TEXT("TEXSCALE")) ) // POLY TEXSCALE [U=..] [V=..] [UV=..] [VU=..]
		{
			Trans->Begin( TEXT("Poly Texscale") );
			Level->Model->ModifySelectedSurfs( 1 );
			Word2 = 1; // Scale absolute
			if( ParseCommand(&Str,TEXT("RELATIVE")) )
				Word2=0;
			TexScale:

			FLOAT UU,UV,VU,VV;
			UU=1.0; Parse (Str,TEXT("UU="),UU);
			UV=0.0; Parse (Str,TEXT("UV="),UV);
			VU=0.0; Parse (Str,TEXT("VU="),VU);
			VV=1.0; Parse (Str,TEXT("VV="),VV);

			polyTexScale( Level->Model, UU, UV, VU, VV, Word2 );

			Trans->End();
			RedrawLevel( Level );
			Processed=1;
		}
		else if( ParseCommand(&Str,TEXT("TEXMULT")) ) // POLY TEXMULT [U=..] [V=..]
		{
			Trans->Begin( TEXT("Poly Texmult") );
			Level->Model->ModifySelectedSurfs( 1 );
			Word2 = 0; // Scale relative;
			goto TexScale;
		}
		else if( ParseCommand(&Str,TEXT("TEXPAN")) ) // POLY TEXPAN [RESET] [U=..] [V=..]
		{
			Trans->Begin( TEXT("Poly Texpan") );
			Level->Model->ModifySelectedSurfs( 1 );
			if( ParseCommand (&Str,TEXT("RESET")) )
				polyTexPan( Level->Model, 0, 0, 1 );
			Word1 = 0; Parse (Str,TEXT("U="),Word1);
			Word2 = 0; Parse (Str,TEXT("V="),Word2);
			polyTexPan( Level->Model, Word1, Word2, 0 );
			Trans->End();
			RedrawLevel( Level );
			Processed=1;
		}
		else if( ParseCommand(&Str,TEXT("TEXALIGN")) ) // POLY TEXALIGN [FLOOR/GRADE/WALL/NONE]
		{
			ETexAlign TexAlign;
			if		(ParseCommand (&Str,TEXT("DEFAULT")))	TexAlign = TEXALIGN_Default;
			else if (ParseCommand (&Str,TEXT("FLOOR")))		TexAlign = TEXALIGN_Floor;
			else if (ParseCommand (&Str,TEXT("WALLDIR")))	TexAlign = TEXALIGN_WallDir;
			else if (ParseCommand (&Str,TEXT("WALLPAN")))	TexAlign = TEXALIGN_WallPan;
			else if (ParseCommand (&Str,TEXT("WALLCOLUMN")))TexAlign = TEXALIGN_WallColumn;
			else if (ParseCommand (&Str,TEXT("ONETILE")))	TexAlign = TEXALIGN_OneTile;
			else								goto Skip;
			{
				INT DWord1=0;
				Parse( Str, TEXT("TEXELS="), DWord1 );
				Trans->Begin( TEXT("Poly Texalign") );
				Level->Model->ModifySelectedSurfs( 1 );
				polyTexAlign( Level->Model, TexAlign, DWord1 );
				Trans->End();
				RedrawLevel( Level );
				Processed=1;
			}
			Skip:;
		}
	}
	//------------------------------------------------------------------------------------
	// TEXTURE management:
	//
	else if( ParseCommand(&Str,TEXT("Texture")) )
	{
		if( ParseCommand(&Str,TEXT("Clear")) )
		{
			UTexture* Texture;
			if( ParseObject<UTexture>(Str,TEXT("NAME="),Texture,ANY_PACKAGE) )
				Texture->Clear( TCLEAR_Temporal );
		}
		else if( ParseCommand(&Str,TEXT("SCALE")) )
		{
			FLOAT DeltaScale;
			Parse( Str, TEXT("DELTA="), DeltaScale );
			if( DeltaScale <= 0 )
			{
				Ar.Logf( NAME_ExecWarning, TEXT("Invalid DeltaScale setting") );
				return 1;
			}

			// get the current viewport
			UViewport* CurrentViewport = NULL;
			for( int i = 0; i < Client->Viewports.Num(); i++ )
			{
				if( Client->Viewports(i)->Current )
					CurrentViewport = Client->Viewports(i);
			}
			if( CurrentViewport == NULL )
			{
				Ar.Logf( NAME_ExecWarning, TEXT("Current viewport not found") );
				return 1;
			}

			// get the selected texture package
			UObject* Pkg = CurrentViewport->MiscRes;
			if( Pkg && CurrentViewport->Group!=NAME_None )
				Pkg = FindObject<UPackage>( Pkg, *CurrentViewport->Group );

			// Make the list.
			FMemMark Mark(GMem);
			enum {MAX=16384};
			UTexture** List = new(GMem,MAX)UTexture*;
			INT n = 0;
			for( TObjectIterator<UTexture> It; It && n<MAX; ++It )
				if( It->IsIn(Pkg) )
					List[n++] = *It;

			// scale the textures in the list relative to their old values
			for( i=0; i<n; i++ )
			{
				UTexture* Texture = List[i];
				Texture->Scale *= DeltaScale;
			}
			Mark.Pop();
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("New")) )
		{
			FName GroupName=NAME_None;
			FName PackageName;
			UClass* TextureClass;
			INT USize, VSize;
			if
			(	Parse( Str, TEXT("NAME="),    TempName, NAME_SIZE )
			&&	ParseObject<UClass>( Str, TEXT("CLASS="), TextureClass, ANY_PACKAGE )
			&&	Parse( Str, TEXT("USIZE="),   USize )
			&&	Parse( Str, TEXT("VSIZE="),   VSize )
			&&	Parse( Str, TEXT("PACKAGE="), PackageName )
			&&	TextureClass->IsChildOf( UTexture::StaticClass() ) 
			&&	PackageName!=NAME_None )
			{
				UPackage* Pkg = CreatePackage(NULL,*PackageName);
				if( Parse( Str, TEXT("GROUP="), GroupName ) && GroupName!=NAME_None )
					Pkg = CreatePackage(Pkg,*GroupName);
				if( !StaticFindObject( TextureClass, Pkg, TempName ) )
				{
					// Create new texture object.
					UTexture* Result = (UTexture*)StaticConstructObject( TextureClass, Pkg, TempName, RF_Public|RF_Standalone );
					if( !Result->Palette )
					{
						Result->Palette = new( Result->GetOuter(), NAME_None, RF_Public )UPalette;
						Result->Palette->Colors.Add( 256 );
					}
					Result->Init( USize, VSize );
					Result->PostLoad();
					Result->Clear( TCLEAR_Temporal | TCLEAR_Bitmap );
				}
				else Ar.Logf( NAME_ExecWarning, TEXT("Texture exists") );
			}
			else Ar.Logf( NAME_ExecWarning, TEXT("Bad TEXTURE NEW") );
			Processed=1;
		}
	}
	//------------------------------------------------------------------------------------
	// MODE management (Global EDITOR mode):
	//
	else if( ParseCommand(&Str,TEXT("MODE")) )
		{
		Word1 = Mode;  // To see if we should redraw
		Word2 = Mode;  // Destination mode to set
		//
		UBOOL DWord1;
		if( ParseUBOOL(Str,TEXT("GRID="), DWord1) )
		{
			FinishAllSnaps (Level);
			Constraints.GridEnabled = DWord1;
			Word1=MAXWORD;
		}
		if( ParseUBOOL(Str,TEXT("ROTGRID="), DWord1) )
		{
			FinishAllSnaps (Level);
			Constraints.RotGridEnabled=DWord1;
			Word1=MAXWORD;
		}
		if( ParseUBOOL(Str,TEXT("SNAPVERTEX="), DWord1) )
		{
			FinishAllSnaps (Level);
			Constraints.SnapVertices=DWord1;
			Word1=MAXWORD;
		}
		Parse( Str, TEXT("SPEED="), MovementSpeed );
		Parse( Str, TEXT("SNAPDIST="), Constraints.SnapDistance );
		//
		// Major modes:
		//
		if 		(ParseCommand(&Str,TEXT("CAMERAMOVE")))		Word2 = EM_ViewportMove;
		else if	(ParseCommand(&Str,TEXT("CAMERAZOOM")))		Word2 = EM_ViewportZoom;
		else if	(ParseCommand(&Str,TEXT("BRUSHROTATE")))	Word2 = EM_BrushRotate;
		else if	(ParseCommand(&Str,TEXT("BRUSHSHEER")))		Word2 = EM_BrushSheer;
		else if	(ParseCommand(&Str,TEXT("BRUSHSCALE")))		Word2 = EM_BrushScale;
		else if	(ParseCommand(&Str,TEXT("BRUSHSTRETCH")))	Word2 = EM_BrushStretch;
		else if	(ParseCommand(&Str,TEXT("BRUSHSNAP"))) 		Word2 = EM_BrushSnap;
		else if	(ParseCommand(&Str,TEXT("TEXTUREPAN")))		Word2 = EM_TexturePan;
		else if	(ParseCommand(&Str,TEXT("TEXTUREROTATE")))	Word2 = EM_TextureRotate;
		else if	(ParseCommand(&Str,TEXT("TEXTURESCALE"))) 	Word2 = EM_TextureScale;
		//
		if( Word2 != Word1 )
		{
			edcamSetMode( Word2 );
			RedrawLevel( Level );
		}
		Processed=1;
		}
	//------------------------------------------------------------------------------------
	// Transaction tracking and control
	//
	else if( ParseCommand(&Str,TEXT("TRANSACTION")) )
	{
		if( ParseCommand(&Str,TEXT("UNDO")) )
		{
			if( Trans->Undo() )
				RedrawLevel( Level );
			Processed=1;
		}
		else if( ParseCommand(&Str,TEXT("REDO")) )
		{
			if( Trans->Redo() )
				RedrawLevel(Level);
			Processed=1;
		}
		NoteSelectionChange( Level );
		EdCallback( EDC_MapChange, 0 );
	}
	//------------------------------------------------------------------------------------
	// General objects
	//
	else if( ParseCommand(&Str,TEXT("OBJ")) )
	{
		if( ParseCommand(&Str,TEXT("EXPORT")) )//oldver
		{
			FName Package=NAME_None;
			UClass* Type;
			UObject* Res;
			Parse( Str, TEXT("PACKAGE="), Package );
			if
			(	ParseObject<UClass>( Str, TEXT("TYPE="), Type, ANY_PACKAGE )
			&&	Parse( Str, TEXT("FILE="), TempFname, 80 )
			&&	ParseObject( Str, TEXT("NAME="), Type, Res, ANY_PACKAGE ) )
			{
				for( FObjectIterator It; It; ++It )
					It->ClearFlags( RF_TagImp | RF_TagExp );
				UExporter* Exporter = UExporter::FindExporter( Res, appFExt(TempFname) );
				if( Exporter )
				{
					Exporter->ParseParms( Str );
					UExporter::ExportToFile( Res, Exporter, TempFname );
					delete Exporter;
				}
			}
			else Ar.Log( NAME_ExecWarning, TEXT("Missing file, name, or type") );
			Processed = 1;
		}
		else if( ParseCommand(&Str,TEXT("SavePackage")) )
		{
			UPackage* Pkg;
			if
			(	Parse( Str, TEXT("File="), TempFname, 79 ) 
			&&	ParseObject<UPackage>( Str, TEXT("Package="), Pkg, NULL ) )
			{
				GWarn->BeginSlowTask( TEXT("Saving package"), 1, 0 );
				SavePackage( Pkg, NULL, RF_Standalone, TempFname, GWarn );
				GWarn->EndSlowTask();
			}
			else Ar.Log( NAME_ExecWarning, TEXT("Missing filename") );
			Processed=1;
		}
	}
	//------------------------------------------------------------------------------------
	// CLASS functions
	//
	else if( ParseCommand(&Str,TEXT("CLASS")) )
	{
		if( ParseCommand(&Str,TEXT("SPEW")) )
		{
			UBOOL All = ParseCommand(&Str,TEXT("ALL"));
			for( TObjectIterator<UClass> It; It; ++It )
			{
				if( It->ScriptText && (All || (It->GetFlags() & RF_SourceModified)) )
				{
					// Make package directory.
					appStrcpy( TempFname, TEXT("..") PATH_SEPARATOR );
					appStrcat( TempFname, It->GetOuter()->GetName() );
					GFileManager->MakeDirectory( TempFname, 0 );

					// Make package\Classes directory.
					appStrcat( TempFname, PATH_SEPARATOR TEXT("Classes") );
					GFileManager->MakeDirectory( TempFname, 0 );

					// Save file.
					appStrcat( TempFname, PATH_SEPARATOR );
					appStrcat( TempFname, It->GetName() );
					appStrcat( TempFname, TEXT(".uc") );
					debugf( NAME_Log, TEXT("Spewing: %s"), TempFname );
					UExporter::ExportToFile( *It, NULL, TempFname );
				}
			}
			Processed=1;
		}
		else if( ParseCommand(&Str,TEXT("LOAD")) ) // CLASS LOAD FILE=..
		{
			if( Parse( Str, TEXT("FILE="), TempFname, 80 ) )
			{
				Ar.Logf( TEXT("Loading class from %s..."), TempFname );
				if( appStrfind(TempFname,TEXT("UC")) )
				{
					FName PkgName, ObjName;
					if
					(	Parse(Str,TEXT("PACKAGE="),PkgName)
					&&	Parse(Str,TEXT("NAME="),ObjName) )
					{
						// Import it.
						ImportObject<UClass>( CreatePackage(NULL,*PkgName), ObjName, RF_Public|RF_Standalone, TempFname );
					}
					else Ar.Log(TEXT("Missing package name"));
				}
				else if( appStrfind( TempFname, TEXT("U")) )
				{
					// Load from Unrealfile.
					UPackage* Pkg = Cast<UPackage>(LoadPackage( NULL, TempFname, LOAD_Forgiving ));
					if( Pkg && (Pkg->PackageFlags & PKG_BrokenLinks) )
					{
						debugf( TEXT("Some classes were broken; a recompile is required") );
						for( TObjectIterator<UClass> It; It; ++It )
						{
							if( It->IsIn(Pkg) )
							{
								It->Dependencies.Empty();
								It->Script.Empty();
							}
						}
					}
				}
				else Ar.Log( NAME_ExecWarning, TEXT("Unrecognized file type") );
			}
			else Ar.Log(NAME_ExecWarning,TEXT("Missing filename"));
			Processed=1;
		}
		else if( ParseCommand(&Str,TEXT("NEW")) ) // CLASS NEW
		{
			UClass *Parent;
			FName PackageName;
			if
			(	ParseObject<UClass>( Str, TEXT("PARENT="), Parent, ANY_PACKAGE )
			&&	Parse( Str, TEXT("PACKAGE="), PackageName )
			&&	Parse( Str, TEXT("NAME="), TempStr, NAME_SIZE ) )
			{
				UPackage* Pkg = CreatePackage(NULL,*PackageName);
				UClass* Class = new( Pkg, TempStr, RF_Public|RF_Standalone )UClass( Parent );
				if( Class )
					Class->ScriptText = new( Class->GetOuter(), TempStr, RF_NotForClient|RF_NotForServer )UTextBuffer;
				else
					Ar.Log( NAME_ExecWarning, TEXT("Class not found") );
			}
			Processed=1;
		}
	}
	//------------------------------------------------------------------------------------
	// SCRIPT: script compiler
	//
	else if( ParseCommand(&Str,TEXT("SCRIPT")) )
	{
		if( ParseCommand(&Str,TEXT("MAKE")) )
		{
			GWarn->BeginSlowTask( TEXT("Compiling scripts"), 0, 0 );
			UBOOL All  = ParseCommand(&Str,TEXT("ALL"));
			UBOOL Boot = ParseCommand(&Str,TEXT("BOOT"));
			MakeScripts( GWarn, All, Boot );
			GWarn->EndSlowTask();
			UpdatePropertiesWindows();
			Processed=1;
		}
	}
	//------------------------------------------------------------------------------------
	// CAMERA: cameras
	//
	else if( ParseCommand(&Str,TEXT("CAMERA")) )
	{
		UBOOL DoUpdate = ParseCommand(&Str,TEXT("UPDATE"));
		UBOOL DoOpen   = ParseCommand(&Str,TEXT("OPEN"));
		if( (DoUpdate || DoOpen) && Level )
		{
			UViewport* Viewport;
			UBOOL Temp=0;
			TCHAR TempStr[NAME_SIZE];
			if( Parse( Str, TEXT("NAME="), TempStr, NAME_SIZE ) )
			{
				Viewport = FindObject<UViewport>( Client, TempStr );
				if( !Viewport )
				{
					Viewport = Client->NewViewport( TempStr );
					Level->SpawnViewActor( Viewport );
					Viewport->Input->Init( Viewport );
					DoOpen = 1;
				}
				else Temp=1;
			}
			else
			{
				Viewport = Client->NewViewport( NAME_None );
				Level->SpawnViewActor( Viewport );
				Viewport->Input->Init( Viewport );
				DoOpen = 1;
			}
			check(Viewport->Actor);

			DWORD hWndParent=0;
			Parse( Str, TEXT("HWND="), hWndParent );

			INT NewX=Viewport->SizeX, NewY=Viewport->SizeY;
			Parse( Str, TEXT("XR="), NewX ); if( NewX<0 ) NewX=0;
			Parse( Str, TEXT("YR="), NewY ); if( NewY<0 ) NewY=0;
			Viewport->Actor->FovAngle = FovAngle;

			Viewport->Actor->Misc1=0;
			Viewport->Actor->Misc2=0;
			Viewport->MiscRes=NULL;
			Parse(Str,TEXT("FLAGS="),Viewport->Actor->ShowFlags);
			Parse(Str,TEXT("REN="),  Viewport->Actor->RendMap);
			Parse(Str,TEXT("MISC1="),Viewport->Actor->Misc1);
			Parse(Str,TEXT("MISC2="),Viewport->Actor->Misc2);
			FName GroupName=NAME_None;
			if( Parse(Str,TEXT("GROUP="),GroupName) )
				Viewport->Group = GroupName;
			if( appStricmp(*Viewport->Group,TEXT("(All)"))==0 )
				Viewport->Group = NAME_None;

			switch( Viewport->Actor->RendMap )
			{
				case REN_TexView:
					ParseObject<UTexture>(Str,TEXT("TEXTURE="),*(UTexture **)&Viewport->MiscRes,ANY_PACKAGE); 
					if( !Viewport->MiscRes )
						Viewport->MiscRes = Viewport->Actor->Level->DefaultTexture;
					break;
				case REN_MeshView:
					if( !Temp )
					{
						Viewport->Actor->Location = FVector(100.0,100.0,+60.0);
						Viewport->Actor->ViewRotation.Yaw=0x6000;
					}
					ParseObject<UMesh>( Str, TEXT("MESH="), *(UMesh**)&Viewport->MiscRes, ANY_PACKAGE ); 
					break;
				case REN_TexBrowser:
					ParseObject<UPackage>(Str,TEXT("PACKAGE="),*(UPackage**)&Viewport->MiscRes,NULL);
					break;
			}
			if( DoOpen )
			{
				INT OpenX = INDEX_NONE;
				INT OpenY = INDEX_NONE;
				Parse( Str, TEXT("X="), OpenX );
				Parse( Str, TEXT("Y="), OpenY );
				Viewport->OpenWindow( hWndParent, 0, NewX, NewY, OpenX, OpenY );
				if( appStricmp(Viewport->GetName(),TEXT("Standard3V"))==0 )
					ResetSound();
			}
			else Draw( Viewport, 1 );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("HIDESTANDARD")) )
		{
			Client->ShowViewportWindows( SHOW_StandardView, 0 );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("CLOSE")) )
		{
			UViewport* Viewport;
			if( ParseCommand(&Str,TEXT("ALL")) )
			{
				for( INT i=Client->Viewports.Num()-1; i>=0; i-- )
					delete Client->Viewports(i);
			}
			else if( ParseCommand(&Str,TEXT("FREE")) )
			{
				for( INT i=Client->Viewports.Num()-1; i>=0; i-- )
					if( appStrstr( Client->Viewports(i)->GetName(), TEXT("STANDARD") )==0 )
						delete Client->Viewports(i);
			}
			else if( ParseObject<UViewport>(Str,TEXT("NAME="),Viewport,GetTransientPackage()) )
			{
				delete Viewport;
			}
			else Ar.Log( TEXT("Missing name") );
			return 1;
		}
#if 1 //LEGEND
		else if( ParseCommand(&Str,TEXT("ALIGN") ) )
		{
			APlayerPawn* PlayerPawn = NULL;
			for( int i = 0; i < Client->Viewports.Num(); i++ )
			{
				if( Client->Viewports(i)->Current )
				{
					PlayerPawn = Client->Viewports(i)->Actor;
					break;
				}
			}
			if( PlayerPawn == NULL )
			{
				Ar.Log( TEXT("Can't find current camera") );
				return 1;
			}

			for( i = 0; i < Client->Viewports.Num(); i++ )
			{
				Client->Viewports(i)->Actor->Location = PlayerPawn->Location;
				Client->Viewports(i)->Actor->Rotation = PlayerPawn->Rotation;
			}
			Ar.Log( TEXT("Aligned camera on current viewport.") );
			RedrawLevel( Level );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("SELECT") ) )
		{
			if( Parse( Str, TEXT("NAME="), TempStr,NAME_SIZE ) )
			{
				AActor* Actor = NULL;
				for( INT i=0; i<Level->Actors.Num(); i++ )
				{
					Actor = Level->Actors(i);
					if( Actor && appStrcmp( Actor->GetName(), TempStr ) == 0 )
					{
						Actor->Modify();
						Actor->bSelected = 1;
						break;
					}
				}
				if( Actor == NULL )
				{
					Ar.Log( TEXT("Can't find the specified name.") );
					return 0;
				}

				for( i = 0; i < Client->Viewports.Num(); i++ )
				{
					AActor* Camera = Client->Viewports(i)->Actor;
					Camera->Location = Actor->Location - Camera->Rotation.Vector() * 48;
				}
				Ar.Log( TEXT("Aligned camera on named object.") );
				RedrawLevel( Level );
				return 1;
			}
			else return 0;
		}
#endif
		else return 0;
	}
	//------------------------------------------------------------------------------------
	// Level.
	//
	if( ParseCommand(&Str,TEXT("LEVEL")) )
	{
		if( ParseCommand(&Str,TEXT("REDRAW")) )
		{
			RedrawLevel(Level);
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("LINKS")) )
		{
			Results->Text.Empty();
			int Internal=0,External=0;
			Results->Logf( TEXT("Level links:\r\n") );
			for( int i=0; i<Level->Actors.Num(); i++ )
			{
				if( Cast<ATeleporter>(Level->Actors(i)) )
				{
					ATeleporter& Teleporter = *(ATeleporter *)Level->Actors(i);
					Results->Logf( TEXT("   %s\r\n"), Teleporter.URL );
					if( appStrchr(*Teleporter.URL,'//') )
						External++;
					else
						Internal++;
				}
			}
			Results->Logf( TEXT("End, %i internal link(s), %i external.\r\n"), Internal, External );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("VALIDATE")) )
		{
			// Validate the level.
			Results->Text.Empty();
			Results->Log( TEXT("Level validation:\r\n") );

			// Make sure it's not empty.
			if( Level->Model->Nodes.Num() == 0 )
			{
				Results->Log( TEXT("Error: Level is empty!\r\n") );
				return 1;
			}

			// Find playerstart.
			for( INT i=0; i<Level->Actors.Num(); i++ )
				if( Cast<APlayerStart>(Level->Actors(i)) )
					break;
			if( i == Level->Actors.Num() )
			{
				Results->Log( TEXT("Error: Missing PlayerStart actor!\r\n") );
				return 1;
			}

			// Make sure PlayerStarts are outside.
			for( i=0; i<Level->Actors.Num(); i++ )
			{
				if( Cast<APlayerStart>(Level->Actors(i)) )
				{
					FCheckResult Hit(0.0);
					if( !Level->Model->PointCheck( Hit, NULL, Level->Actors(i)->Location, FVector(0,0,0), 0 ) )
					{
						Results->Log( TEXT("Error: PlayerStart doesn't fit!\r\n") );
						return 1;
					}
				}
			}

			// Check scripts.
			if( GEditor && !GEditor->CheckScripts( GWarn, UObject::StaticClass(), *Results ) )
			{
				Results->Logf( TEXT("\r\nError: Scripts need to be rebuilt!\r\n") );
				return 1;
			}

			// Check level title.
			if( Level->GetLevelInfo()->Title==TEXT("") )
			{
				Results->Logf( TEXT("Error: Level is missing a title!") );
				return 1;
			}
			else if( Level->GetLevelInfo()->Title==TEXT("Untitled") )
			{
				Results->Logf( TEXT("Warning: Level is untitled\r\n") );
			}

			// Check actors.
			for( i=0; i<Level->Actors.Num(); i++ )
			{
				AActor* Actor = Level->Actors(i);
				if( Actor )
				{
					guard(CheckingActors);
					check(Actor->GetClass()!=NULL);
					check(Actor->GetStateFrame());
					check(Actor->GetStateFrame()->Object==Actor);
					check(Actor->Level!=NULL);
					check(Actor->GetLevel()!=NULL);
					check(Actor->GetLevel()==Level);
					check(Actor->GetLevel()->Actors(0)!=NULL);
					check(Actor->GetLevel()->Actors(0)==Actor->Level);
					unguardf(( TEXT("(%i %s)"), i, Actor->GetFullName() ));
				}
			}

			// Success.
			Results->Logf( TEXT("Success: Level validation succeeded!\r\n") );
			return 1;
		}
		else
		{
			return 0;
		}
	}
	//------------------------------------------------------------------------------------
	// Other handlers.
	//
	else if( ParseCommand(&Str,TEXT("FIX")) )
	{
		for( int i=0; i<Level->Actors.Num(); i++ )
			if( Level->Actors(i) )
				Level->Actors(i)->SoundRadius = Clamp(4*(INT)Level->Actors(i)->SoundRadius,0,255);
	}
	else if( ParseCommand(&Str,TEXT("MAYBEAUTOSAVE")) )
	{
		if( AutoSave && ++AutoSaveCount>=AutosaveTimeMinutes )
		{
			AutoSaveIndex = (AutoSaveIndex+1)%10;
			SaveConfig();
			TCHAR Cmd[256];
			appSprintf( Cmd, TEXT("MAP SAVE FILE=%s..") PATH_SEPARATOR TEXT("Maps") PATH_SEPARATOR TEXT("Auto%i.unr"), appBaseDir(), AutoSaveIndex );
			debugf( NAME_Log, TEXT("Autosaving '%s'"), Cmd );
			Exec( Cmd, Ar );
			AutoSaveCount=0;
		}
	}
	else if( ParseCommand(&Str,TEXT("HOOK")) )
	{
		return HookExec( Str, Ar );
	}
	else if( HookExec( Str, Ar ) )
	{
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("AUDIO")) )
	{
		if( ParseCommand(&Str,TEXT("PLAY")) )
		{
			UViewport* Viewport = FindObject<UViewport>( ANY_PACKAGE, TEXT("Standard3V") );
			if( Viewport && Audio )
			{
				USound* Sound;
				if( ParseObject<USound>( Str, TEXT("NAME="), Sound, ANY_PACKAGE ) )
					Audio->PlaySound( Viewport->Actor, 2*SLOT_Misc, Sound ? Sound : (USound*)-1, Viewport->Actor->Location, 1.0, 4096.0, 1.0 );
			}
			else Ar.Logf( TEXT("Can't find viewport for sound") );
			Processed = 1;
		}
	}
	else if( ParseCommand(&Str,TEXT("SETCURRENTCLASS")) )
	{
		ParseObject<UClass>( Str, TEXT("CLASS="), CurrentClass, ANY_PACKAGE );
	}
	else if( ParseCommand(&Str,TEXT("MUSIC")) )
	{
		UViewport* Viewport=NULL;
		for( int i=0; i<Client->Viewports.Num(); i++ )
			if( appStricmp(Client->Viewports(i)->GetName(), TEXT("Standard3V"))==0 )
				Viewport=Client->Viewports(i);
		if( !Viewport || !Audio )
		{
			Ar.Logf( TEXT("Can't find viewport for music") );
		}
		else if( ParseCommand(&Str,TEXT("PLAY")) )
		{
			UMusic* Music;
			if( ParseObject<UMusic>(Str,TEXT("NAME="),Music,ANY_PACKAGE) )
			{
				Viewport->Actor->Song        = Music;
				Viewport->Actor->SongSection = 0;
				Viewport->Actor->Transition  = MTRAN_Fade;
			}
		}
		Processed = 1;
	}
	else if( Level && Level->Exec(Stream,Ar) )
	{
		// The level handled it.
		Processed = 1;
	}
	else if( UEngine::Exec(Stream,Ar) )
	{
		// The engine handled it.
		Processed = 1;
	}
	else if( ParseCommand(&Str,TEXT("SELECTNAME")) )
	{
		FName FindName=NAME_None;
		Parse( Str, TEXT("NAME="), FindName );
		for( INT i=0; i<Level->Actors.Num(); i++ )
			if( Level->Actors(i) )
				Level->Actors(i)->bSelected = Level->Actors(i)->GetFName()==FindName;
	}
	else if( ParseCommand(&Str,TEXT("DUMPINT")) )
	{
		while( *Str==' ' )
			Str++;
		UObject* Pkg = LoadPackage( NULL, Str, LOAD_AllowDll );
		if( Pkg )
		{
			TCHAR Tmp[256],Loc[256];
			appStrcpy( Tmp, Str );
			if( appStrchr(Tmp,'.') )
				*appStrchr(Tmp,'.') = 0;
			appStrcat( Tmp, TEXT(".int") );
			appStrcpy( Loc, appBaseDir() );
			appStrcat( Loc, Tmp );
			for( FObjectIterator It; It; ++It )
			{
				if( It->IsIn(Pkg) )
				{
					TCHAR Temp[1024], TempKey[1024], TempValue[1024], *Value;
					UClass* Class = Cast<UClass>( *It );
					if( Class )
					{
						// Generate localizable class defaults.
						for( TFieldIterator<UProperty> ItP(Class); ItP; ++ItP )
							if( ItP->PropertyFlags & CPF_Localized )
								for( INT i=0; i<ItP->ArrayDim; i++ )
									if( ItP->ExportText( i, Value=Temp, &Class->Defaults(0), ItP->GetOuter()!=Class ? &Class->GetSuperClass()->Defaults(0) : NULL, 0 ) )
									{
										const TCHAR* Key = ItP->GetName();
										if( ItP->ArrayDim!=1 )
										{
											appSprintf( TempKey, TEXT("%s[%i]"), ItP->GetName(), i );
											Key = TempKey;
										}
										if( Value[0]==' ' || (*Value&&Value[appStrlen(TempValue)-1]==' ') )
										{
											appSprintf( TempValue, TEXT("\"%s\""), Value );
											Value = TempValue;
										}
										GConfig->SetString( Class->GetName(), Key, Value, Loc );
									}
					}
					else
					{
						// Generate localizable object properties.
						for( TFieldIterator<UProperty> ItP(It->GetClass()); ItP; ++ItP )
							if( ItP->PropertyFlags & CPF_Localized )
								for( INT i=0; i<ItP->ArrayDim; i++ )
									if( ItP->ExportText( i, Value=Temp, (BYTE*)*It, &It->GetClass()->Defaults(0), 0 ) )
									{
										const TCHAR* Key = ItP->GetName();
										if( ItP->ArrayDim!=1 )
										{
											appSprintf( TempKey, TEXT("%s[%i]"), ItP->GetName(), i );
											Key = TempKey;
										}
										if( Value[0]==' ' || (*Value&&Value[appStrlen(TempValue)-1]==' ') )
										{
											appSprintf( TempValue, TEXT("\"%s\""), Value );
											Value = TempValue;
										}
										GConfig->SetString( It->GetName(), Key, Value, Loc );
									}
					}
				}
			}
			GConfig->Flush( 0 );
			Ar.Logf( TEXT("Generated %s"), Loc );
		}
		else Ar.Logf( TEXT("LoadPackage failed") );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("JUMPTO")) )
	{
		TCHAR A[32], B[32], C[32];
		ParseToken( Str, A, ARRAY_COUNT(A), 0 );
		ParseToken( Str, B, ARRAY_COUNT(B), 0 );
		ParseToken( Str, C, ARRAY_COUNT(C), 0 );
		for( INT i=0; i<Client->Viewports.Num(); i++ )
			Client->Viewports(i)->Actor->Location = FVector(appAtoi(A),appAtoi(B),appAtoi(C));
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("LSTAT")) )
	{
		TArray<FVector> Sizes;
		for( INT i=0; i<Level->Model->LightMap.Num(); i++ )
			new(Sizes)FVector(Level->Model->LightMap(i).UClamp,Level->Model->LightMap(i).VClamp,0);
		/*for( i=0; i<Sizes.Num(); i++ )
			for( INT j=0; j<i; j++ )
				if
				(	(Sizes(j).X>Sizes(i).X)
				||	(Sizes(j).X==Sizes(i).X && Sizes(j).Y>Sizes(i).Y) )
					Exchange( Sizes(i), Sizes(j) );*/
		debugf( TEXT("LightMap Sizes: ") );
		INT DX[17], DY[17], Size=0, Under32=0, Under64=0;
		for( i=0; i<9; i++ )
			DX[i]=DY[i]=0;
		for( i=0; i<Sizes.Num(); i++ )
		{
			DX[appCeilLogTwo(Sizes(i).X)]++;
			DY[appCeilLogTwo(Sizes(i).Y)]++;
			Size += Sizes(i).X*Sizes(i).Y;
			if( Sizes(i).X<=32 && Sizes(i).Y<=32 )
				Under32++;
			if( Sizes(i).X<=64 && Sizes(i).Y<=64 )
				Under64++;
		}
		debugf( TEXT("Size=%iK elements"), Size/1024);
		debugf( TEXT("Under32=%f%% Under64=%f%%"), 100.0*Under32/Sizes.Num(), 100.0*Under64/Sizes.Num() );
		for( i=0; i<9; i++ )
		{
			debugf
			(
				TEXT("Distribution (%i..%i) X=%f%% Y=%f%%"),
				(1<<i)/2+1,
				(1<<i),
				100.0*DX[i]/Sizes.Num(),
				100.0*DY[i]/Sizes.Num()
			);
		}
		debugf( TEXT("Collision hulls=%i"), Level->Model->LeafHulls.Num() );
		return 1;
	}
	return Processed;
	unguardf(( TEXT("(%s)%s"), ErrorTemp, appStrlen(ErrorTemp)>=69 ? TEXT("..") : TEXT("") ));
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
