/*=============================================================================
	UnEdFact.cpp: Editor class factories.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EditorPrivate.h"

/*------------------------------------------------------------------------------
	ULevelFactoryNew implementation.
------------------------------------------------------------------------------*/

void ULevelFactoryNew::StaticConstructor()
{
	guard(ULevelFactoryNew::StaticConstructor);

	SupportedClass			= ULevel::StaticClass();
	bCreateNew				= 1;
	bShowPropertySheet		= 1;
	bShowCategories			= 0;
	Description				= TEXT("Level");
	InContextCommand		= TEXT("Create New Level");
	OutOfContextCommand		= TEXT("Create New Level");
	CloseExistingWindows	= 1;
	LevelTitle				= TEXT("Untitled");
	Author					= TEXT("Unknown");

	new(GetClass(),TEXT("LevelTitle"),           RF_Public)UStrProperty(CPP_PROPERTY(LevelTitle          ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Author"),               RF_Public)UStrProperty(CPP_PROPERTY(Author              ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("CloseExistingWindows"), RF_Public)UBoolProperty(CPP_PROPERTY(CloseExistingWindows), TEXT(""), CPF_Edit );

	unguard;
}
ULevelFactoryNew::ULevelFactoryNew()
{
	guard(ULevelFactoryNew::ULevelFactoryNew);
	unguard;
}
void ULevelFactoryNew::Serialize( FArchive& Ar )
{
	guard(ULevelFactoryNew::Serialize);
	Super::Serialize( Ar );

	Ar << LevelTitle << Author << CloseExistingWindows;

	unguard;
}
UObject* ULevelFactoryNew::FactoryCreateNew( UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, FFeedbackContext* Warn )
{
	guard(ULevelFactoryNew::FactoryCreateNew);

	//!!needs updating: optionally close existing windows, create NEW level, create new wlevelframe
	GEditor->Trans->Reset( TEXT("clearing map") );
	GEditor->Level->RememberActors();
	GEditor->Level = new( GEditor->Level->GetOuter(), TEXT("MyLevel") )ULevel( GEditor, 0 );
	GEditor->Level->GetLevelInfo()->Title  = LevelTitle;
	GEditor->Level->GetLevelInfo()->Author = Author;

	GEditor->Level->ReconcileActors();
	GEditor->ResetSound();
	GEditor->RedrawLevel( GEditor->Level );
	GEditor->NoteSelectionChange( GEditor->Level );
	GEditor->EdCallback(EDC_MapChange,0);
	GEditor->Cleanse( 1, TEXT("starting new map") );

	return GEditor->Level;
	unguard;
}
IMPLEMENT_CLASS(ULevelFactoryNew);

/*------------------------------------------------------------------------------
	UClassFactoryNew implementation.
------------------------------------------------------------------------------*/

void UClassFactoryNew::StaticConstructor()
{
	guard(UClassFactoryNew::StaticConstructor);

	SupportedClass		= UClass::StaticClass();
	bCreateNew			= 1;
	bShowPropertySheet	= 1;
	bShowCategories		= 0;
	Description			= TEXT("UnrealScript Class");
	InContextCommand	= TEXT("Create New Subclass");
	OutOfContextCommand	= TEXT("Create New Class");
	ClassName			= TEXT("MyClass");
	ClassPackage		= CreatePackage( NULL, TEXT("MyPackage") );
	Superclass			= AActor::StaticClass();
	new(GetClass(),TEXT("ClassName"),    RF_Public)UNameProperty  (CPP_PROPERTY(ClassName   ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("ClassPackage"), RF_Public)UObjectProperty(CPP_PROPERTY(ClassPackage), TEXT(""), CPF_Edit, UPackage::StaticClass() );
	new(GetClass(),TEXT("Superclass"),   RF_Public)UClassProperty (CPP_PROPERTY(Superclass  ), TEXT(""), CPF_Edit, UObject::StaticClass() );

	unguard;
}
UClassFactoryNew::UClassFactoryNew()
{
	guard(UClassFactoryNew::UClassFactoryNew);
	unguard;
}
void UClassFactoryNew::Serialize( FArchive& Ar )
{
	guard(UClassFactoryNew::Serialize);
	Super::Serialize( Ar );

	Ar << ClassName << ClassPackage << Superclass;

	unguard;
}
UObject* UClassFactoryNew::FactoryCreateNew( UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, FFeedbackContext* Warn )
{
	guard(UClassFactoryNew::FactoryCreateNew);
	return NULL;
	unguard;
}
IMPLEMENT_CLASS(UClassFactoryNew);

/*------------------------------------------------------------------------------
	UTextureFactoryNew implementation.
------------------------------------------------------------------------------*/

void UTextureFactoryNew::StaticConstructor()
{
	guard(UTextureFactoryNew::StaticConstructor);

	SupportedClass		= UTexture::StaticClass();
	bCreateNew			= 1;
	bShowPropertySheet	= 1;
	bShowCategories		= 0;
	Description			= TEXT("Procedural Texture");
	InContextCommand	= TEXT("Create New Texture");
	OutOfContextCommand	= TEXT("Create New Texture");
	TextureName			= TEXT("MyTexture");
	TexturePackage		= CreatePackage( NULL, TEXT("MyPackage") );
	TextureClass		= UTexture::StaticClass();
	USize				= 256;
	VSize				= 256;

	new(GetClass(),TEXT("TextureName"),    RF_Public)UNameProperty  (CPP_PROPERTY(TextureName   ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("TexturePackage"), RF_Public)UObjectProperty(CPP_PROPERTY(TexturePackage), TEXT(""), CPF_Edit, UPackage::StaticClass() );
	new(GetClass(),TEXT("TextureClass"),   RF_Public)UClassProperty (CPP_PROPERTY(TextureClass  ), TEXT(""), CPF_Edit, UObject::StaticClass() );
	new(GetClass(),TEXT("USize"),          RF_Public)UIntProperty   (CPP_PROPERTY(USize         ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("VSize"),          RF_Public)UIntProperty   (CPP_PROPERTY(VSize         ), TEXT(""), CPF_Edit );

	unguard;
}
UTextureFactoryNew::UTextureFactoryNew()
{
	guard(UTextureFactoryNew::UTextureFactoryNew);
	unguard;
}
void UTextureFactoryNew::Serialize( FArchive& Ar )
{
	guard(UTextureFactoryNew::Serialize);
	Super::Serialize( Ar );

	Ar << TextureName << TexturePackage << TextureClass << USize << VSize;

	unguard;
}
UObject* UTextureFactoryNew::FactoryCreateNew( UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, FFeedbackContext* Warn )
{
	guard(UTextureFactoryNew::FactoryCreateNew);
	return NULL;
	unguard;
}
IMPLEMENT_CLASS(UTextureFactoryNew);

/*------------------------------------------------------------------------------
	UClassFactoryUC implementation.
------------------------------------------------------------------------------*/

void UClassFactoryUC::StaticConstructor()
{
	guard(UClassFactoryUC::StaticConstructor);

	SupportedClass = UClass::StaticClass();
	new(Formats)FString(TEXT("uc;Unreal class definitions"));
	bCreateNew = 0;
	bText  = 1;
	bMulti = 1;

	unguard;
}
UClassFactoryUC::UClassFactoryUC()
{
	guard(UClassFactoryUC::UClassFactoryUC);
	unguard;
}
UObject* UClassFactoryUC::FactoryCreateText
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	DWORD				Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const TCHAR*&		Buffer,
	const TCHAR*		BufferEnd,
	FFeedbackContext*	Warn
)
{
	guard(UClassFactoryUC::FactoryCreateText);
	const TCHAR* InBuffer=Buffer;
	FString StrLine, ClassName, BaseClassName;

	// Validate format.
	if( Class != UClass::StaticClass() )
	{
		Warn->Logf( TEXT("Can only import classes"), Type );
		return NULL;
	}
	if( appStricmp(Type,TEXT("UC"))!=0 )
	{
		Warn->Logf( TEXT("Can't import classes from files of type '%s'"), Type );
		return NULL;
	}

	// Import the script text.
	FStringOutputDevice ScriptText, DefaultPropText;
	while( ParseLine(&Buffer,StrLine,1) )
	{
		const TCHAR* Str=*StrLine, *Temp;
		if( ParseCommand(&Str,TEXT("defaultproperties")) )
		{
			// Get default properties text.
			while( ParseLine(&Buffer,StrLine,1) )
			{
				Str = *StrLine;
				ParseNext( &Str );
				if( *Str=='}' )
					break;
				DefaultPropText.Logf( TEXT("%s\r\n"), *StrLine );
			}
		}
		else
		{
			// Get script text.
			ScriptText.Logf( TEXT("%s\r\n"), *StrLine );

			// Stub out the comments.
			INT Pos = StrLine.InStr(TEXT("//"));
			if( Pos>=0 )
				StrLine = StrLine.Left( Pos );
			Str=*StrLine;

			// Get class name.
			if( ClassName==TEXT("") && (Temp=appStrfind(Str, TEXT("class")))!=0 )
			{
				Temp+=6;
				ParseToken( Temp, ClassName, 0 );
			}
			if
			(	BaseClassName==TEXT("")
			&&	((Temp=appStrfind(Str, TEXT("expands")))!=0 || (Temp=appStrfind(Str, TEXT("extends")))!=0) )
			{
				Temp+=7;
				ParseToken( Temp, BaseClassName, 0 );
				while( BaseClassName.Right(1)==TEXT(";") )
					BaseClassName = BaseClassName.LeftChop( 1 );
			}
		}
	}

	// Handle failure.
	if( ClassName==TEXT("") || (BaseClassName==TEXT("") && ClassName!=TEXT("Object")) )
	{
		Warn->Logf( TEXT("Bad class definition '%s'/'%s'/%i/%i"), ClassName, BaseClassName, BufferEnd-InBuffer, appStrlen(InBuffer) );
		return NULL;
	}
	else if( ClassName!=*Name )
	{
		Warn->Logf( TEXT("Script vs. class name mismatch (%s/%s)"), *Name, ClassName );		
	}

	UClass* ResultClass = FindObject<UClass>( InParent, *ClassName );
	if( ResultClass && (ResultClass->GetFlags() & RF_Native) )
	{
		// Gracefully update an existing hardcoded class.
		debugf( NAME_Log, TEXT("Updated native class '%s'"), ResultClass->GetFullName() );
		ResultClass->ClassFlags &= ~(CLASS_Parsed | CLASS_Compiled);
	}
	else
	{
		// Create new class.
		ResultClass = new( InParent, *ClassName, Flags )UClass( NULL );

		// Find or forward-declare base class.
		ResultClass->SuperField = FindObject<UClass>( InParent, *BaseClassName );
		if( !ResultClass->SuperField )
			ResultClass->SuperField = FindObject<UClass>( ANY_PACKAGE, *BaseClassName );
		if( !ResultClass->SuperField )
			ResultClass->SuperField = new( InParent, *BaseClassName )UClass( ResultClass );
		debugf( NAME_Log, TEXT("Imported: %s"), ResultClass->GetFullName() );
	}

	// Set class info.
	ResultClass->ScriptText      = new( ResultClass, TEXT("ScriptText"),   RF_NotForClient|RF_NotForServer )UTextBuffer( *ScriptText );
	ResultClass->DefaultPropText = DefaultPropText;

	return ResultClass;
	unguard;
}
IMPLEMENT_CLASS(UClassFactoryUC);

/*------------------------------------------------------------------------------
	ULevelFactory.
------------------------------------------------------------------------------*/

static void ForceValid( ULevel* Level, UStruct* Struct, BYTE* Data )
{
	guard(ForceValid);
	INT j;
	for( TFieldIterator<UProperty> It(Struct); It; ++It )
	{
		for( INT i=0; i<It->ArrayDim; i++ )
		{
			BYTE* Value = Data + It->Offset + i*It->ElementSize;
			if( Cast<UObjectProperty>(*It) )
			{
				UObject*& Obj = *(UObject**)Value;
				if( Cast<AActor>(Obj) )
				{
					for( INT j=0; j<Level->Actors.Num(); j++ )
						if( Level->Actors(j)==Obj )
							break;
					if( j==Level->Actors.Num() )
					{
						debugf( NAME_Log, TEXT("Usurped %s"), Obj->GetClass()->GetName() );
						Obj = NULL;
					}
				}
			}
			else if( Cast<UStructProperty>(*It) )
			{
				ForceValid( Level, ((UStructProperty*)*It)->Struct, Value );
			}
		}
	}
	unguard;
}

void ULevelFactory::StaticConstructor()
{
	guard(ULevelFactory::StaticConstructor);

	SupportedClass = ULevel::StaticClass();
	new(Formats)FString(TEXT("t3d;Unreal level text"));
	bCreateNew = 0;
	bText = 1;

	unguard;
}
ULevelFactory::ULevelFactory()
{
	guard(ULevelFactory::ULevelFactory);
	unguard;
}
UObject* ULevelFactory::FactoryCreateText
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	DWORD				Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const TCHAR*&		Buffer,
	const TCHAR*		BufferEnd,
	FFeedbackContext*	Warn
)
{
	guard(ULevelFactory::FactoryCreateText);
	TMap<AActor*,FString> Map;

	// Create (or replace) the level object.
	ULevel* Level = GEditor->Level;
	Level->CompactActors();
	check(Level->Actors.Num()>1);
	check(Cast<ALevelInfo>(Level->Actors(0)));
	check(Cast<ABrush>(Level->Actors(1)));

	// Init actors.
	guard(InitActors);
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		if( Level->Actors(i) )
		{
			Level->Actors(i)->bTempEditor = 1;
			Level->Actors(i)->bSelected   = 0;
		}
	}
	unguard;

	// Assumes data is being imported over top of a new, valid map.
	ParseNext( &Buffer );
	if( !GetBEGIN( &Buffer, TEXT("MAP")) )
		return Level;

	// Import everything.
	INT ImportedActive=appStricmp(Type,TEXT("paste"))==0;
	FString StrLine;
	while( ParseLine(&Buffer,StrLine) )
	{
		const TCHAR* Str = *StrLine;
		if( GetEND(&Str,TEXT("MAP")) )
		{
			// End of brush polys.
			break;
		}
		else if( GetBEGIN(&Str,TEXT("BRUSH")) )
		{
			guard(ParseBrush);
			Warn->StatusUpdatef( 0, 0, TEXT("%s"), TEXT("Importing Brushes") );
			TCHAR BrushName[NAME_SIZE];
			if( Parse(Str,TEXT("NAME="),BrushName,NAME_SIZE) )
			{
				ABrush* Actor;
				if( !ImportedActive )
				{
					// Parse the active brush, which has already been allocated.
					Actor          = Level->Brush();
					ImportedActive = 1;
				}
				else
				{
					// Parse a new brush which has not yet been allocated.
					Actor             = Level->SpawnBrush();
					Actor->bSelected  = 1;
					Actor->Brush      = new( InParent, NAME_None, RF_NotForClient|RF_NotForServer )UModel( NULL );			
				}

				// Import.
				Actor->SetFlags       ( RF_NotForClient | RF_NotForServer );
				Actor->Brush->SetFlags( RF_NotForClient | RF_NotForServer );
				UModelFactory* It = new UModelFactory;
				Actor->Brush = (UModel*)It->FactoryCreateText(UModel::StaticClass(),InParent,Actor->Brush->GetFName(),0,Actor,Type,Buffer,BufferEnd,Warn);
				check(Actor->Brush);
				if( (Actor->PolyFlags&PF_Portal) && !(Actor->PolyFlags&PF_Translucent) )
					Actor->PolyFlags |= PF_Invisible;
			}
			unguard;
		}
		else if( GetBEGIN(&Str,TEXT("ACTOR")) )
		{
			guard(ParseActor);
			UClass* TempClass;
			if( ParseObject<UClass>( Str, TEXT("CLASS="), TempClass, ANY_PACKAGE ) )
			{
				// Get actor name.
				FName ActorName(NAME_None);
				Parse( Str, TEXT("NAME="), ActorName );

				// Make sure this name is unique.
				AActor* Found=NULL;
				if( ActorName!=NAME_None )
					Found = FindObject<AActor>( InParent, *ActorName );
				if( Found )
					Found->Rename();

				// Import it.
				AActor* Actor = Level->SpawnActor( TempClass, ActorName, NULL, NULL, FVector(0,0,0), FRotator(0,0,0), NULL, 1, 0 );
				check(Actor);

				// Get property text.
				FString PropText, StrLine;
				while
				(	GetEND( &Buffer, TEXT("ACTOR") )==0
				&&	ParseLine( &Buffer, StrLine ) )
				{
					PropText += *StrLine;
					PropText += TEXT("\r\n");
				}
				Map.Set( Actor, *PropText );

				// Handle class.
				if( Cast<ALevelInfo>(Actor) )
				{
					// Copy the one LevelInfo the position #0.
					check(Level->Actors.Num()>0);
					INT iActor=0; Level->Actors.FindItem( Actor, iActor );
					Level->Actors(0)       = Actor;
					Level->Actors(iActor)  = NULL;
				}
				else if( Actor->GetClass()==ABrush::StaticClass() && !ImportedActive )
				{
					// Copy the active brush the position #0.
					INT iActor=0; Level->Actors.FindItem( Actor, iActor );
					Level->Actors(1)       = Actor;
					Level->Actors(iActor)  = NULL;
					ImportedActive = 1;
				}
			}
			unguard;
		}
	}

	// Import actor properties.
	// We do this after creating all actors so that actor references can be matched up.
	guard(Finishing);
	check(Cast<ALevelInfo>(Level->Actors(0)));
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( Actor )
		{
			Actor->bSelected = !Actor->bTempEditor;
			FString* PropText = Map.Find(Actor);
			if( PropText )
			{
				guard(ImportActorProperties);
				ImportProperties( Actor->GetClass(), (BYTE*)Actor, Level, **PropText, InParent, Warn );
				unguard;
			}
			guard(FixupActor);
			Actor->Level  = (ALevelInfo*)Level->Actors(0);
			Actor->Region = FPointRegion( (ALevelInfo*)Level->Actors(0) );
			ForceValid( Level, Actor->GetClass(), (BYTE*)Actor );
			unguard;
		}
	}
	unguard;

	return Level;
	unguard;
}
IMPLEMENT_CLASS(ULevelFactory);

/*-----------------------------------------------------------------------------
	UPolysFactory.
-----------------------------------------------------------------------------*/

void UPolysFactory::StaticConstructor()
{
	guard(UPolysFactory::StaticConstructor);

	SupportedClass = UPolys::StaticClass();
	new(Formats)FString(TEXT("t3d;Unreal brush text"));
	bCreateNew = 0;
	bText = 1;

	unguard;
}
UPolysFactory::UPolysFactory()
{
	guard(UPolysFactory::UPolysFactory);
	unguard;
}
UObject* UPolysFactory::FactoryCreateText
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	DWORD				Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const TCHAR*&		Buffer,
	const TCHAR*		BufferEnd,
	FFeedbackContext*	Warn
)
{
	guard(UPolysFactory::FactoryCreateText);

	// Create polys.
	UPolys* Polys = Context ? CastChecked<UPolys>(Context) : new(InParent,Name,Flags)UPolys;

	// Eat up if present.
	GetBEGIN( &Buffer, TEXT("POLYLIST") );

	// Parse all stuff.
	INT First=1, GotBase=0;
	FString StrLine, ExtraLine;
	FPoly Poly;
	while( ParseLine( &Buffer, StrLine ) )
	{
		const TCHAR* Str = *StrLine;
		if( GetEND(&Str,TEXT("POLYLIST")) )
		{
			// End of brush polys.
			break;
		}
		else if( appStrstr(Str,TEXT("ENTITIES")) && First )
		{
			// Autocad .DXF file.
			debugf(NAME_Log,TEXT("Reading Autocad DXF file"));
			INT Started=0, NumPts=0, IsFace=0;
			FVector PointPool[4096];
			FPoly NewPoly; NewPoly.Init();

			while
			(	ParseLine( &Buffer, StrLine, 1 )
			&&	ParseLine( &Buffer, ExtraLine, 1 ) )
			{
				// Handle the line.
				Str = *ExtraLine;
				INT Code = appAtoi(*StrLine);
				//debugf("DXF: %i: %s",Code,*ExtraLine);
				if( Code==0 )
				{
					// Finish up current poly.
					if( Started )
					{
						if( NewPoly.NumVertices == 0 )
						{
							// Got a vertex definition.
							NumPts++;
							//debugf("DXF: Added vertex %i",NewPoly.NumVertices);
						}
						else if( NewPoly.NumVertices>=3 && NewPoly.NumVertices<FPoly::MAX_VERTICES )
						{
							// Got a poly definition.
							if( IsFace ) NewPoly.Reverse();
							NewPoly.Base = NewPoly.Vertex[0];
							NewPoly.Finalize(0);
							new(Polys->Element)FPoly( NewPoly );
							//debugf("DXF: Added poly %i",Num);
						}
						else
						{
							// Bad.
							Warn->Logf( TEXT("DXF: Bad vertex count %i"), NewPoly.NumVertices );
						}
						
						// Prepare for next.
						NewPoly.Init();
					}
					Started=0;

					if( ParseCommand(&Str,TEXT("VERTEX")) )
					{
						// Start of new vertex.
						//debugf("DXF: Vertex");
						PointPool[NumPts] = FVector(0,0,0);
						Started = 1;
						IsFace  = 0;
					}
					else if( ParseCommand(&Str,TEXT("3DFACE")) )
					{
						// Start of 3d face definition.
						//debugf("DXF: 3DFace");
						Started = 1;
						IsFace  = 1;
					}
					else if( ParseCommand(&Str,TEXT("SEQEND")) )
					{
						// End of sequence.
						//debugf("DXF: SEQEND");
						NumPts=0;
					}
					else if( ParseCommand(&Str,TEXT("EOF")) )
					{
						// End of file.
						//debugf("DXF: End");
						break;
					}
				}
				else if( Started )
				{
					// Replace commas with periods to handle european dxf's.
					//for( TCHAR* Stupid = appStrchr(*ExtraLine,','); Stupid; Stupid=appStrchr(Stupid,',') )
					//	*Stupid = '.';

					// Handle codes.
					if( Code>=10 && Code<=19 )
					{
						// X coordinate.
						if( IsFace && Code-10==NewPoly.NumVertices )
						{
							//debugf("DXF: NewVertex %i",NewPoly.NumVertices);
							NewPoly.Vertex[NewPoly.NumVertices++] = FVector(0,0,0);
						}
						NewPoly.Vertex[Code-10].X = PointPool[NumPts].X = appAtof(*ExtraLine);
					}
					else if( Code>=20 && Code<=29 )
					{
						// Y coordinate.
						NewPoly.Vertex[Code-20].Y = PointPool[NumPts].Y = appAtof(*ExtraLine);
					}
					else if( Code>=30 && Code<=39 )
					{
						// Z coordinate.
						NewPoly.Vertex[Code-30].Z = PointPool[NumPts].Z = appAtof(*ExtraLine);
					}
					else if( Code>=71 && Code<=79 && (Code-71)==NewPoly.NumVertices )
					{
						INT iPoint = Abs(appAtoi(*ExtraLine));
						if( iPoint>0 && iPoint<=NumPts )
							NewPoly.Vertex[NewPoly.NumVertices++] = PointPool[iPoint-1];
						else debugf( NAME_Warning, TEXT("DXF: Invalid point index %i/%i"), iPoint, NumPts );
					}
				}
			}
		}
		else if( appStrstr(Str,TEXT("Tri-mesh,")) && First )
		{
			// 3DS .ASC file.
			debugf( NAME_Log, TEXT("Reading 3D Studio ASC file") );
			FVector PointPool[4096];

			AscReloop:
			int NumVerts = 0, TempNumPolys=0, TempVerts=0;
			while( ParseLine( &Buffer, StrLine ) )
			{
				Str = *StrLine;

				FString VertText = FString::Printf( TEXT("Vertex %i:"), NumVerts );
				FString FaceText = FString::Printf( TEXT("Face %i:"), TempNumPolys );
				if( appStrstr(Str,*VertText) )
				{
					PointPool[NumVerts].X = appAtof(appStrstr(Str,TEXT("X:"))+2);
					PointPool[NumVerts].Y = appAtof(appStrstr(Str,TEXT("Y:"))+2);
					PointPool[NumVerts].Z = appAtof(appStrstr(Str,TEXT("Z:"))+2);
					NumVerts++;
					TempVerts++;
				}
				else if( appStrstr(Str,*FaceText) )
				{
					Poly.Init();
					Poly.NumVertices=3;
					Poly.Vertex[0] = PointPool[appAtoi(appStrstr(Str,TEXT("A:"))+2)];
					Poly.Vertex[1] = PointPool[appAtoi(appStrstr(Str,TEXT("B:"))+2)];
					Poly.Vertex[2] = PointPool[appAtoi(appStrstr(Str,TEXT("C:"))+2)];
					Poly.Base = Poly.Vertex[0];
					Poly.Finalize(0);
					new(Polys->Element)FPoly(Poly);
					TempNumPolys++;
				}
				else if( appStrstr(Str,TEXT("Tri-mesh,")) )
					goto AscReloop;
			}
			debugf( NAME_Log, TEXT("Imported %i vertices, %i faces"), TempVerts, Polys->Element.Num() );
		}
		else if( GetBEGIN(&Str,TEXT("POLYGON")) ) // Unreal .t3d file.
		{
			// Init to defaults and get group/item and texture.
			Poly.Init();
			Parse( Str, TEXT("LINK="), Poly.iLink );
			Parse( Str, TEXT("ITEM="), Poly.ItemName );
			ParseObject<UTexture>( Str, TEXT("TEXTURE="), Poly.Texture, ANY_PACKAGE );
			Parse( Str, TEXT("FLAGS="), Poly.PolyFlags );
			Poly.PolyFlags &= ~PF_NoImport;
		}
		else if( ParseCommand(&Str,TEXT("PAN")) )
		{
			Parse( Str, TEXT("U="), Poly.PanU );
			Parse( Str, TEXT("V="), Poly.PanV );
		}
		else if( ParseCommand(&Str,TEXT("ORIGIN")) )
		{
			GotBase=1;
			GetFVECTOR( Str, Poly.Base );
		}
		else if( ParseCommand(&Str,TEXT("VERTEX")) )
		{
			if( Poly.NumVertices < FPoly::MAX_VERTICES )
			{
				GetFVECTOR( Str, Poly.Vertex[Poly.NumVertices] );
				Poly.NumVertices++;
			}
		}
		else if( ParseCommand(&Str,TEXT("TEXTUREU")) )
		{
			GetFVECTOR( Str, Poly.TextureU );
		}
		else if( ParseCommand(&Str,TEXT("TEXTUREV")) )
		{
			GetFVECTOR( Str, Poly.TextureV );
		}
		else if( GetEND(&Str,TEXT("POLYGON")) )
		{
			if( !GotBase )
				Poly.Base = Poly.Vertex[0];
			if( Poly.Finalize(1)==0 )
				new(Polys->Element)FPoly(Poly);
			GotBase=0;
		}
	}

	// Success.
	return Polys;
	unguard;
}
IMPLEMENT_CLASS(UPolysFactory);

/*-----------------------------------------------------------------------------
	UModelFactory.
-----------------------------------------------------------------------------*/

void UModelFactory::StaticConstructor()
{
	guard(UModelFactory::StaticConstructor);

	SupportedClass = UModel::StaticClass();
	new(Formats)FString(TEXT("t3d;Unreal model text"));
	bCreateNew = 0;
	bText = 1;

	unguard;
}
UModelFactory::UModelFactory()
{
	guard(UModelFactory::UModelFactory);
	unguard;
}
UObject* UModelFactory::FactoryCreateText
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	DWORD				Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const TCHAR*&		Buffer,
	const TCHAR*		BufferEnd,
	FFeedbackContext*	Warn
)
{
	guard(UModelFactory::FactoryCreateText);
	ABrush* TempOwner = (ABrush*)Context;
	UModel* Model = new( InParent, Name, Flags )UModel( TempOwner, 1 );

	const TCHAR* StrPtr;
	FString StrLine;
	if( TempOwner )
	{
		TempOwner->InitPosRotScale();
		TempOwner->bSelected   = 0;
		TempOwner->bTempEditor = 0;
	}
	while( ParseLine( &Buffer, StrLine ) )
	{
		StrPtr = *StrLine;
		if( GetEND(&StrPtr,TEXT("BRUSH")) )
		{
			break;
		}
		else if( GetBEGIN (&StrPtr,TEXT("POLYLIST")) )
		{
			UPolysFactory* PolysFactory = new UPolysFactory;
			Model->Polys = (UPolys*)PolysFactory->FactoryCreateText(UPolys::StaticClass(),InParent,NAME_None,0,NULL,Type,Buffer,BufferEnd,Warn);
			check(Model->Polys);
		}
		if( TempOwner )
		{
			if      (ParseCommand(&StrPtr,TEXT("PREPIVOT"	))) GetFVECTOR 	(StrPtr,TempOwner->PrePivot);
			else if (ParseCommand(&StrPtr,TEXT("SCALE"		))) GetFSCALE 	(StrPtr,TempOwner->MainScale);
			else if (ParseCommand(&StrPtr,TEXT("POSTSCALE"	))) GetFSCALE 	(StrPtr,TempOwner->PostScale);
			else if (ParseCommand(&StrPtr,TEXT("LOCATION"	))) GetFVECTOR	(StrPtr,TempOwner->Location);
			else if (ParseCommand(&StrPtr,TEXT("ROTATION"	))) GetFROTATOR  (StrPtr,TempOwner->Rotation,1);
			if( ParseCommand(&StrPtr,TEXT("SETTINGS")) )
			{
				Parse( StrPtr, TEXT("CSG="), TempOwner->CsgOper );
				Parse( StrPtr, TEXT("POLYFLAGS="), TempOwner->PolyFlags );
			}
		}
	}
	if( GEditor )
		GEditor->bspValidateBrush( Model, 1, 0 );

	return Model;
	unguard;
}
IMPLEMENT_CLASS(UModelFactory);

/*-----------------------------------------------------------------------------
	USoundFactory.
-----------------------------------------------------------------------------*/

void USoundFactory::StaticConstructor()
{
	guard(USoundFactory::StaticConstructor);

	SupportedClass = USound::StaticClass();
	new(Formats)FString(TEXT("wav;Wave audio files"));
	bCreateNew = 0;

	unguard;
}
USoundFactory::USoundFactory()
{
	guard(USoundFactory::USoundFactory);
	unguard;
}
UObject* USoundFactory::FactoryCreateBinary
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	DWORD				Flags,
	UObject*			Context,
	const TCHAR*		FileType,
	const BYTE*&		Buffer,
	const BYTE*			BufferEnd,
	FFeedbackContext*	Warn
)
{
	guard(USoundFactory::FactoryCreateBinary);
	if
	(	appStricmp(FileType, TEXT("WAV"))==0
	||	appStricmp(FileType, TEXT("MP3"))==0 )
	{
		// Wave file.
		USound* Sound = new(InParent,Name,Flags)USound;
		Sound->FileType = FName(FileType);
		Sound->Data.Add( BufferEnd-Buffer );
		appMemcpy( &Sound->Data(0), Buffer, Sound->Data.Num() );
		return Sound;
	}
	else if( appStricmp(FileType, TEXT("UFX"))==0 )//oldver
	{
		// Outdated ufx file.
		Warn->Logf( TEXT("Invalid old-format sound %s"), *Name );
		return NULL;
	}
	else
	{
		// Unrecognized.
		Warn->Logf( TEXT("Unrecognized sound format '%s' in %s"), FileType, *Name );
		return NULL;
	}
	unguard;
}
IMPLEMENT_CLASS(USoundFactory);

/*-----------------------------------------------------------------------------
	UMusicFactory.
-----------------------------------------------------------------------------*/

void UMusicFactory::StaticConstructor()
{
	guard(UMusicFactory::StaticConstructor);

	SupportedClass = UMusic::StaticClass();
	new(Formats)FString(TEXT("mod;Amiga modules;s3m;Scream Tracker 3"));
	bCreateNew = 0;

	unguard;
}
UMusicFactory::UMusicFactory()
{
	guard(UMusicFactory::UMusicFactory);
	unguard;
}
UObject* UMusicFactory::FactoryCreateBinary
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	DWORD				Flags,
	UObject*			Context,
	const TCHAR*		FileType,
	const BYTE*&		Buffer,
	const BYTE*			BufferEnd,
	FFeedbackContext*	Warn
)
{
	guard(UMusicFactory::FactoryCreateBinary);

	UMusic* Music = new(InParent,Name,Flags)UMusic;
	Music->FileType = FName(FileType);
	Music->Data.Add( BufferEnd - Buffer );
	appMemcpy( &Music->Data(0), Buffer, Music->Data.Num() );
	return Music;

	unguard;
}
IMPLEMENT_CLASS(UMusicFactory);

/*-----------------------------------------------------------------------------
	UTextureFactory.
-----------------------------------------------------------------------------*/

// .PCX file header.
#pragma pack(push,1)
class FPCXFileHeader
{
public:
	BYTE	Manufacturer;		// Always 10.
	BYTE	Version;			// PCX file version.
	BYTE	Encoding;			// 1=run-length, 0=none.
	BYTE	BitsPerPixel;		// 1,2,4, or 8.
	_WORD	XMin;				// Dimensions of the image.
	_WORD	YMin;				// Dimensions of the image.
	_WORD	XMax;				// Dimensions of the image.
	_WORD	YMax;				// Dimensions of the image.
	_WORD	XDotsPerInch;		// Horizontal printer resolution.
	_WORD	YDotsPerInch;		// Vertical printer resolution.
	BYTE	OldColorMap[48];	// Old colormap info data.
	BYTE	Reserved1;			// Must be 0.
	BYTE	NumPlanes;			// Number of color planes (1, 3, 4, etc).
	_WORD	BytesPerLine;		// Number of bytes per scanline.
	_WORD	PaletteType;		// How to interpret palette: 1=color, 2=gray.
	_WORD	HScreenSize;		// Horizontal monitor size.
	_WORD	VScreenSize;		// Vertical monitor size.
	BYTE	Reserved2[54];		// Must be 0.
	friend FArchive& operator<<( FArchive& Ar, FPCXFileHeader& H )
	{
		guard(FPCXFileHeader<<);
		INT i;
		Ar << H.Manufacturer << H.Version << H.Encoding << H.BitsPerPixel;
		Ar << H.XMin << H.YMin << H.XMax << H.YMax << H.XDotsPerInch << H.YDotsPerInch;
		for( INT i=0; i<ARRAY_COUNT(H.OldColorMap); i++ )
			Ar << H.OldColorMap[i];
		Ar << H.Reserved1 << H.NumPlanes;
		Ar << H.BytesPerLine << H.PaletteType << H.HScreenSize << H.VScreenSize;
		for( i=0; i<ARRAY_COUNT(H.Reserved2); i++ )
			Ar << H.Reserved2[i];
		return Ar;
		unguard;
	}
};
#pragma pack(pop)

// Bitmap compression types.
enum EBitmapCompression
{
	BCBI_RGB       = 0,
	BCBI_RLE8      = 1,
	BCBI_RLE4      = 2,
	BCBI_BITFIELDS = 3,
};

// .BMP file header.
#pragma pack(push,1)
struct FBitmapFileHeader
{
    _WORD bfType;
    DWORD bfSize;
    _WORD bfReserved1;
    _WORD bfReserved2;
    DWORD bfOffBits;
	friend FArchive& operator<<( FArchive& Ar, FBitmapFileHeader& H )
	{
		guard(FBitmapFileHeader<<);
		Ar << H.bfType << H.bfSize << H.bfReserved1 << H.bfReserved2 << H.bfOffBits;
		return Ar;
		unguard;
	}
};
#pragma pack(pop)

// .BMP subheader.
#pragma pack(push,1)
struct FBitmapInfoHeader
{
    DWORD biSize;
    DWORD biWidth;
    DWORD biHeight;
    _WORD biPlanes;
    _WORD biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    DWORD biXPelsPerMeter;
    DWORD biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
	friend FArchive& operator<<( FArchive& Ar, FBitmapInfoHeader& H )
	{
		guard(FBitmapInfoHeader<<);
		Ar << H.biSize << H.biWidth << H.biHeight;
		Ar << H.biPlanes << H.biBitCount;
		Ar << H.biCompression << H.biSizeImage;
		Ar << H.biXPelsPerMeter << H.biYPelsPerMeter;
		Ar << H.biClrUsed << H.biClrImportant;
		return Ar;
		unguard;
	}
};
#pragma pack(pop)

void UTextureFactory::StaticConstructor()
{
	guard(UTextureFactory::StaticConstructor);

	SupportedClass = UTexture::StaticClass();
	new(Formats)FString(TEXT("bmp;Bitmap files;pcx;PC Painbrush files"));
	bCreateNew = 0;

	unguard;
}
UTextureFactory::UTextureFactory()
{
	guard(UTextureFactory::UTextureFactory);
	unguard;
}
TCHAR* GFile=NULL;
UObject* UTextureFactory::FactoryCreateBinary
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	DWORD				Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const BYTE*&		Buffer,
	const BYTE*			BufferEnd,
	FFeedbackContext*	Warn
)
{
	guard(UTextureFactory::FactoryCreate);
	UTexture* Texture = NULL;

	const FPCXFileHeader*    PCX   = (FPCXFileHeader *)Buffer;
	const FBitmapFileHeader* bmf   = (FBitmapFileHeader *)(Buffer + 0);
    const FBitmapInfoHeader* bmhdr = (FBitmapInfoHeader *)(Buffer + sizeof(FBitmapFileHeader));

	// Validate it.
	INT Length = BufferEnd - Buffer;
    if( (Length>=sizeof(FBitmapFileHeader)+sizeof(FBitmapInfoHeader)) && Buffer[0]=='B' && Buffer[1]=='M' )
    {
        // This is a .bmp type data stream.
		if( (bmhdr->biWidth&(bmhdr->biWidth-1)) || (bmhdr->biHeight&(bmhdr->biHeight-1)) )
		{
			Warn->Logf( TEXT("Texture dimensions are not powers of two") );
			return NULL;
		}
		if( bmhdr->biCompression != BCBI_RGB )
		{
			Warn->Logf( TEXT("RLE compression of BMP images not supported") );
			return NULL;
		}
		if( bmhdr->biPlanes==1 && bmhdr->biBitCount==8 )
		{
			// Set texture properties.
			Texture = CastChecked<UTexture>(StaticConstructObject( Class, InParent, Name, Flags ) );
			Texture->Init( bmhdr->biWidth, bmhdr->biHeight );
			Texture->PostLoad();

			// Do palette.
			UPalette* Palette = new( InParent, NAME_None, RF_Public )UPalette;
			const BYTE* bmpal = (BYTE*)Buffer + sizeof(FBitmapFileHeader) + sizeof(FBitmapInfoHeader);
			Palette->Colors.Empty();
			for( INT i=0; i<Min<INT>(NUM_PAL_COLORS,bmhdr->biClrUsed?bmhdr->biClrUsed:NUM_PAL_COLORS); i++ )
				new( Palette->Colors )FColor( bmpal[i*4+2], bmpal[i*4+1], bmpal[i*4+0], 255 );
			while( Palette->Colors.Num()<NUM_PAL_COLORS )
				new(Palette->Colors)FColor(0,0,0,255);
			Texture->Palette = Palette->ReplaceWithExisting();

			// Copy upside-down scanlines.
			for( INT y=0; y<(INT)bmhdr->biHeight; y++ )
				appMemcpy
				(
					&Texture->Mips(0).DataArray((bmhdr->biHeight - 1 - y) * bmhdr->biWidth),
					(BYTE*)Buffer + bmf->bfOffBits + y * Align(bmhdr->biWidth,4),
					bmhdr->biWidth
				);
		}
		else if( bmhdr->biPlanes==1 && bmhdr->biBitCount==24 )
		{
			// Set texture properties.
			Texture = CastChecked<UTexture>(StaticConstructObject( Class, InParent, Name, Flags ) );
			Texture->Format = TEXF_RGBA8;
			Texture->Init( bmhdr->biWidth, bmhdr->biHeight );
			Texture->PostLoad();

			// Copy upside-down scanlines.
			const BYTE* Ptr = (BYTE*)Buffer + bmf->bfOffBits;
			for( INT y=0; y<(INT)bmhdr->biHeight; y++ )
			{
				BYTE* DestPtr = &Texture->Mips(0).DataArray((bmhdr->biHeight - 1 - y) * bmhdr->biWidth * 4);
				BYTE* SrcPtr = (BYTE*) &Ptr[y * bmhdr->biWidth * 3];
				for( INT x=0; x<(INT)bmhdr->biWidth; x++ )
				{
					*DestPtr++ = *SrcPtr++;
					*DestPtr++ = *SrcPtr++;
					*DestPtr++ = *SrcPtr++;
					*DestPtr++ = 0xFF;
				}
			}
		}
		else
		{
            Warn->Logf( TEXT("BMP uses an unsupported format (%i/%i)"), bmhdr->biPlanes, bmhdr->biBitCount );
            return NULL;
		}
    }
	else if( Length >= sizeof(FPCXFileHeader) && PCX->Manufacturer==10 )
	{
		// This is a .PCX.
		INT NewU = PCX->XMax + 1 - PCX->XMin;
		INT NewV = PCX->YMax + 1 - PCX->YMin;
		if( (NewU&(NewU-1)) || (NewV&(NewV-1)) )
		{
			Warn->Logf( TEXT("Texture dimensions are not powers of two") );
			return NULL;
		}
		else if( PCX->NumPlanes==1 && PCX->BitsPerPixel==8 )
		{
			// Set texture properties.
			Texture = CastChecked<UTexture>(StaticConstructObject( Class, InParent, Name, Flags ) );
			Texture->Init( NewU, NewV );
			Texture->PostLoad();

			// Import it.
			BYTE* DestPtr	= &Texture->Mips(0).DataArray(0);
			BYTE* DestEnd	= DestPtr + Texture->Mips(0).DataArray.Num();
			Buffer += 128;
			while( DestPtr < DestEnd )
			{
				BYTE Color = *Buffer++;
				if( (Color & 0xc0) == 0xc0 )
				{
					INT RunLength = Color & 0x3f;
					Color     = *Buffer++;
					appMemset( DestPtr, Color, Min(RunLength,(INT)(DestEnd - DestPtr)) );
					DestPtr  += RunLength;
				}
				else *DestPtr++ = Color;
			}

			// Do the palette.
			UPalette* Palette = new( InParent, NAME_None, RF_Public )UPalette;
			BYTE* PCXPalette = (BYTE *)(BufferEnd - NUM_PAL_COLORS * 3);
			Palette->Colors.Empty();
			for( INT i=0; i<NUM_PAL_COLORS; i++ )
				new(Palette->Colors)FColor(PCXPalette[i*3+0],PCXPalette[i*3+1],PCXPalette[i*3+2],255);
			Texture->Palette = Palette->ReplaceWithExisting();
		}
		else if( PCX->NumPlanes==3 && PCX->BitsPerPixel==8 )
		{
			// Set texture properties.
			Texture = CastChecked<UTexture>(StaticConstructObject( Class, InParent, Name, Flags ) );
			Texture->Format = TEXF_RGBA8;
			Texture->Init( NewU, NewV );
			Texture->PostLoad();

			// Copy upside-down scanlines.
			Buffer += 128;
			INT CountU = Min<INT>(PCX->BytesPerLine,NewU);
			TArray<BYTE>& Dest = Texture->Mips(0).DataArray;
			for( INT i=0; i<NewV; i++ )
			{
				// We need to decode image one line per time building RGB image color plane by color plane.
				for( INT ColorPlane=2; ColorPlane>=0; ColorPlane-- )
				{
					for( INT j=0; j<CountU; j++ )
					{
						INT RunLength = 1;
						if( (*Buffer & 0xc0)==0xc0 )
							RunLength = Min( *Buffer & 0x3f, CountU - j );
						BYTE Color = *Buffer++;
						for( INT k=j; k<j+RunLength; k++ )
							Dest( (i*NewU+k)*4 + ColorPlane ) = Color;
					}
				}
			}
		}
		else
		{
            Warn->Logf( TEXT("BMP uses an unsupported format (%i/%i)"), bmhdr->biPlanes, bmhdr->biBitCount );
            return NULL;
		}
	}
	else
	{
		// Unknown format.
        Warn->Logf( TEXT("Bad image format for texture import") );
        return NULL;
 	}

	// See if part of an animation sequence.
	check(Texture);
	if( appStrlen(Texture->GetName())>=4 )
	{
		TCHAR Temp[256];
		appStrcpy( Temp, Texture->GetName() );
		TCHAR* End = Temp + appStrlen(Temp) - 4;
		if( End[0]=='_' && appToUpper(End[1])=='A' && appIsDigit(End[2]) && appIsDigit(End[3]) )
		{
			INT i = appAtoi( End+2 );
			debugf( NAME_Log, TEXT("Texture animation frame %i: %s"), i, Texture->GetName() );
			if( i>0 )
			{
				appSprintf( End+2, TEXT("%02i"), i-1 );
				UTexture* Other = FindObject<UTexture>( Texture->GetOuter(), Temp );
				if( Other )
				{
					Other->AnimNext = Texture;
					debugf( NAME_Log, TEXT("   Linked to previous: %s"), Other->GetName() );
				}
			}
			if( i<99 )
			{
				appSprintf( End+2, TEXT("%02i"), i+1 );
				UTexture* Other = FindObject<UTexture>( Texture->GetOuter(), Temp );
				if( Other )
				{
					Texture->AnimNext = Other;
					debugf( NAME_Log, TEXT("   Linked to next: %s"), Other->GetName() );
				}
			}
		}
	}

	// See if we should compress.
	if( ParseParam(appCmdLine(),TEXT("COMPRESSDXT")) )
		Texture->Compress(TEXF_DXT1,1);

	return Texture;
	unguard;
}
IMPLEMENT_CLASS(UTextureFactory);

/*------------------------------------------------------------------------------
	UTextureExporterPCX implementation.
------------------------------------------------------------------------------*/

void UTextureExporterPCX::StaticConstructor()
{
	guard(UTextureExporterPCX::StaticConstructor);

	SupportedClass = UTexture::StaticClass();
	new(Formats)FString(TEXT("PCX"));

	unguard;
}
UBOOL UTextureExporterPCX::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn )
{
	guard(UTextureExporterPCX::ExportBinary);
	UTexture* Texture = CastChecked<UTexture>( Object );
	INT i;

	// Set all PCX file header properties.
	FPCXFileHeader PCX;
	appMemzero( &PCX, sizeof(PCX) );
	PCX.Manufacturer	= 10;
	PCX.Version			= 05;
	PCX.Encoding		= 1;
	PCX.BitsPerPixel	= 8;
	PCX.XMin			= 0;
	PCX.YMin			= 0;
	PCX.XMax			= Texture->USize-1;
	PCX.YMax			= Texture->VSize-1;
	PCX.XDotsPerInch	= Texture->USize;
	PCX.YDotsPerInch	= Texture->VSize;
	PCX.BytesPerLine	= Texture->USize;
	PCX.PaletteType		= 0;
	PCX.HScreenSize		= 0;
	PCX.VScreenSize		= 0;

	// Figure out format.
	ETextureFormat Format      = (ETextureFormat)Texture->Format;
	UBOOL MustDecompress       = 0;//Format==TEXF_DXT1;
	TArray<FMipmap>& TheseMips = MustDecompress ? Texture->CompMips : Texture->Mips;
	/*if( MustDecompress )
	{
		Format = TEXF_RGBA8;
		if( !Texture->Decompress( Format ) )
			return 0;
	}
	else*/ Texture->Mips(0).DataArray.Load();

	// Copy all RLE bytes.
	BYTE RleCode=0xc1;
	if( Format==TEXF_RGBA8 )
	{
		PCX.NumPlanes = 3;
		Ar << PCX;
		for( INT Line=0; Line<Texture->VSize; Line++ )
		{
			for( INT ColorPlane = 2; ColorPlane >= 0; ColorPlane-- )
			{
				BYTE* ScreenPtr = &TheseMips(0).DataArray(0) + (Line * Texture->USize * 4) + ColorPlane;
				for( INT Row=0; Row<Texture->USize; Row++ )
				{
					if( (*ScreenPtr&0xc0)==0xc0 )
						Ar << RleCode;
					Ar << *ScreenPtr;
					ScreenPtr += 4;
				}
			}
		}
		return 1;
	}
	else if( Format==TEXF_P8 )
	{
		PCX.NumPlanes = 1;
		Ar << PCX;
		BYTE* ScreenPtr = &TheseMips(0).DataArray(0);
		for( INT i=0; i<Texture->USize*Texture->VSize; i++ )
		{
			if( (*ScreenPtr&0xc0)==0xc0 )
				Ar << RleCode;
			Ar << *ScreenPtr++;
		}

		// Write PCX trailer then palette.
		BYTE Extra = 12;
		Ar << Extra;
		FColor* Colors = Texture->GetColors();
		for( i=0; i<NUM_PAL_COLORS; i++ )
			Ar << Colors[i].R << Colors[i].G << Colors[i].B;
		return 1;
	}
	else return 0;
	unguard;
}
IMPLEMENT_CLASS(UTextureExporterPCX);

/*------------------------------------------------------------------------------
	UTextureExporterBMP implementation.
------------------------------------------------------------------------------*/

void UTextureExporterBMP::StaticConstructor()
{
	guard(UTextureExporterBMP::StaticConstructor);

	SupportedClass = UTexture::StaticClass();
	new(Formats)FString(TEXT("BMP"));

	unguard;
}
UBOOL UTextureExporterBMP::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn )
{
	guard(UTextureExporterBMP::ExportBinary);
	UTexture* Texture = CastChecked<UTexture>( Object );
	INT i;

	// Figure out format.
	ETextureFormat Format      = (ETextureFormat)Texture->Format;
	UBOOL MustDecompress       = 0;//Format==TEXF_DXT1;
	TArray<FMipmap>& TheseMips = MustDecompress ? Texture->CompMips : Texture->Mips;
	/*if( MustDecompress )
	{
		Format = TEXF_RGBA8;
		if( !Texture->Decompress( Format ) )
			return 0;
	}
	else*/ Texture->Mips(0).DataArray.Load();

	// File header.
	FBitmapFileHeader bmf;
	bmf.bfType      = 'B' + (256*(INT)'M');
    bmf.bfReserved1 = 0;
    bmf.bfReserved2 = 0;
	INT biSizeImage;
	if( Format==TEXF_RGBA8 )
	{
		biSizeImage		= Texture->USize * Texture->VSize * 3;
		bmf.bfOffBits   = sizeof(FBitmapFileHeader) + sizeof(FBitmapInfoHeader);
	}
	else if( Format==TEXF_P8 )
	{
		biSizeImage		= Texture->USize * Texture->VSize * 1;
		bmf.bfOffBits   = sizeof(FBitmapFileHeader) + sizeof(FBitmapInfoHeader) + 256*4;
	}
	else return 0;
	bmf.bfSize		= bmf.bfOffBits + biSizeImage;
	Ar << bmf;

	// Info header.
	FBitmapInfoHeader bmhdr;
    bmhdr.biSize          = sizeof(FBitmapInfoHeader);
    bmhdr.biWidth         = Texture->USize;
    bmhdr.biHeight        = Texture->VSize;
    bmhdr.biPlanes        = 1;
	bmhdr.biBitCount      = Format==TEXF_RGBA8 ? 24 : 8;
    bmhdr.biCompression   = BCBI_RGB;
    bmhdr.biSizeImage     = biSizeImage;
    bmhdr.biXPelsPerMeter = 0;
    bmhdr.biYPelsPerMeter = 0;
    bmhdr.biClrUsed       = 0;
    bmhdr.biClrImportant  = 0;
	Ar << bmhdr;

	// Write data.
	if( Format==TEXF_RGBA8 )
	{
		// Upside-down scanlines.
		for( INT i=Texture->VSize-1; i>=0; i-- )
		{
			BYTE* ScreenPtr = &TheseMips(0).DataArray(i*Texture->USize*4);
			for( INT j=Texture->USize; j>0; j-- )
			{
				Ar << *ScreenPtr++;
				Ar << *ScreenPtr++;
				Ar << *ScreenPtr++;
				*ScreenPtr++;
			}
		}
		return 1;
	}
	else
	{
		// Palette.
		FColor* Colors = Texture->GetColors();
		for( INT i=0; i<256; i++ )
			Ar << Colors[i].B << Colors[i].G << Colors[i].R << Colors[i].A;

		// Upside-down scanlines.
		for( i=Texture->VSize-1; i>=0; i-- )
			Ar.Serialize( &TheseMips(0).DataArray(i*Texture->USize), Texture->USize );
		return 1;
	}
	return 0;
	unguard;
}
IMPLEMENT_CLASS(UTextureExporterBMP);

/*------------------------------------------------------------------------------
	UFontFactory.
------------------------------------------------------------------------------*/

//
//	Fast pixel-lookup.
//
static inline BYTE AT( BYTE* Screen, int SXL, int X, int Y )
{
	return Screen[X+Y*SXL];
}

//
// Codepage 850 -> Latin-1 mapping table:
//
BYTE FontRemap[256] = 
{
	  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
	 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
	 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
	 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,

	 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
	 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
	 96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
	112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,

	000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,
	032,173,184,156,207,190,124,245,034,184,166,174,170,196,169,238,
	248,241,253,252,239,230,244,250,247,251,248,175,172,171,243,168,

	183,181,182,199,142,143,146,128,212,144,210,211,222,214,215,216,
	209,165,227,224,226,229,153,158,157,235,233,234,154,237,231,225,
	133,160,131,196,132,134,145,135,138,130,136,137,141,161,140,139,
	208,164,149,162,147,228,148,246,155,151,163,150,129,236,232,152,
};

//
//	Find the border around a font glyph that starts at x,y (it's upper
//	left hand corner).  If it finds a glyph box, it returns 0 and the
//	glyph 's length (xl,yl).  Otherwise returns -1.
//
static UBOOL ScanFontBox( UTexture* Texture, INT X, INT Y, INT& XL, INT& YL )
{
	guard(UFont_ScanFontBox);
	BYTE* Data = &Texture->Mips(0).DataArray(0);
	INT FontXL = Texture->USize;

	// Find x-length.
	INT NewXL = 1;
	while ( AT(Data,FontXL,X+NewXL,Y)==255 && AT(Data,FontXL,X+NewXL,Y+1)!=255 )
		NewXL++;

	if( AT(Data,FontXL,X+NewXL,Y)!=255 )
		return 0;

	// Find y-length.
	INT NewYL = 1;
	while( AT(Data,FontXL,X,Y+NewYL)==255 && AT(Data,FontXL,X+1,Y+NewYL)!=255 )
		NewYL++;

	if( AT(Data,FontXL,X,Y+NewYL)!=255 )
		return 0;

	XL = NewXL - 1;
	YL = NewYL - 1;

	return 1;
	unguard;
}

void UFontFactory::StaticConstructor()
{
	guard(UFontFactory::StaticConstructor);

	SupportedClass = UFont::StaticClass();

	unguard;
}
UFontFactory::UFontFactory()
{
	guard(UFontFactory::UFontFactory);
	unguard;
}
UObject* UFontFactory::FactoryCreateBinary
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	DWORD				Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const BYTE*&		Buffer,
	const BYTE*			BufferEnd,
	FFeedbackContext*	Warn
)
{
	guard(UFontFactory::FactoryCreateBinary);
	check(Class==UFont::StaticClass());
	UFont* Font = new( InParent, Name, Flags )UFont;
	Font->CharactersPerPage = 256;
	FFontPage* Page = new(Font->Pages)FFontPage;
	Page->Texture = CastChecked<UTexture>( UTextureFactory::FactoryCreateBinary( UTexture::StaticClass(), Font, NAME_None, 0, Context, Type, Buffer, BufferEnd, Warn ) );
	if( Page->Texture )
	{
		// Init.
		Page->Texture->PolyFlags = PF_Masked;
		BYTE* TextureData = &Page->Texture->Mips(0).DataArray(0);
		Page->Characters.AddZeroed( NUM_FONT_CHARS );

		// Scan in all fonts, starting at glyph 32.
		INT i = 32;
		INT Y = 0;
		do
		{
			INT X = 0;
			while( AT(TextureData,Page->Texture->USize,X,Y)!=255 && Y<Page->Texture->VSize )
			{
				X++;
				if( X >= Page->Texture->USize )
				{
					X = 0;
					if( ++Y >= Page->Texture->VSize )
						break;
				}
			}

			// Scan all glyphs in this row.
			if( Y < Page->Texture->VSize )
			{
				INT XL=0, YL=0, MaxYL=0;
				while( i<Page->Characters.Num() && ScanFontBox(Page->Texture,X,Y,XL,YL) )
				{
					Page->Characters(i).StartU = X+1;
					Page->Characters(i).StartV = Y+1;
					Page->Characters(i).USize  = XL;
					Page->Characters(i).VSize  = YL;
					X += XL + 1;
					i++;
					if( YL > MaxYL )
						MaxYL = YL;
				}
				Y += MaxYL + 1;
			}
		} while( i<Page->Characters.Num() && Y<Page->Texture->VSize );

		// Cleanup font data.
		for( i=0; i<Page->Texture->Mips.Num(); i++ )
			for( INT j=0; j<Page->Texture->Mips(i).DataArray.Num(); j++ )
				if( Page->Texture->Mips(i).DataArray(j)==255 )
					Page->Texture->Mips(i).DataArray(j) = 0;

		// Remap old fonts.
		TArray<FFontCharacter> Old = Page->Characters;
		for( i=0; i<Page->Characters.Num(); i++ )
			Page->Characters(i) = Old(FontRemap[i]);
		return Font;
	}
	else return NULL;
	unguard;
}
IMPLEMENT_CLASS(UFontFactory);


/*------------------------------------------------------------------------------
	The end.
------------------------------------------------------------------------------*/
