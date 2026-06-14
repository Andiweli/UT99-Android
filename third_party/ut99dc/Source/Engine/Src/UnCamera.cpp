/*=============================================================================
	UnViewport.cpp: Generic Unreal viewport code
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnRender.h"
#include "UnNet.h"


#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
static UBOOL UT99AndroidV109ContainsOption( const FString& URL, const TCHAR* OptionWithQuestion )
{
	return URL.Caps().InStr( FString(OptionWithQuestion).Caps() ) >= 0;
}

static FString UT99AndroidV109BuildListenURLFromRelaunch( const TCHAR* RelaunchArgs )
{
	TCHAR URLToken[1024];
	URLToken[0] = 0;
	const TCHAR* Scan = RelaunchArgs;
	if( !ParseToken( Scan, URLToken, ARRAY_COUNT(URLToken), 0 ) || !URLToken[0] )
		return TEXT("");

	FString URL = URLToken;
	if( !UT99AndroidV109ContainsOption( URL, TEXT("?LISTEN") ) )
		URL += TEXT("?Listen");
	if( ParseParam( RelaunchArgs, TEXT("lanplay") ) && !UT99AndroidV109ContainsOption( URL, TEXT("?LAN") ) )
		URL += TEXT("?LAN");

	return URL;
}

static FLOAT UT99AndroidCalcSceneWidescreenFOVV161( FLOAT BaseFOV, FLOAT FrameX, FLOAT FrameY )
{
	if( FrameX <= 0.0f || FrameY <= 0.0f || BaseFOV <= 0.0f || BaseFOV >= 170.0f )
		return BaseFOV;

	const FLOAT CurrentAspect = FrameX / FrameY;
	const FLOAT ClassicAspect = 4.0f / 3.0f;
	if( CurrentAspect <= ClassicAspect + 0.01f )
		return BaseFOV;

	// UT99_ANDROID_V161_WIDESCREEN_FOV_SCENENODE:
	// Apply Hor+ before the engine builds FSceneNode projection/culling values.
	// The GLES device must then consume Frame->Proj instead of widening a second time.
	FLOAT WideFOV = (FLOAT)( appAtan( appTan( BaseFOV * PI / 360.0 ) * ( CurrentAspect / ClassicAspect ) ) * 360.0 / PI );
	WideFOV = Max<FLOAT>( BaseFOV, WideFOV );
	WideFOV = Min<FLOAT>( 120.0f, WideFOV );
	return WideFOV;
}

#endif

/*-----------------------------------------------------------------------------
	URenderDevice.
-----------------------------------------------------------------------------*/

#define LINE_NEAR_CLIP_Z 1.0

//
// Init URenderDevice class.
//
void URenderDevice::StaticConstructor()
{
	guard(URenderDevice::StaticConstructor);

	new(GetClass(),TEXT("VolumetricLighting"),RF_Public)UBoolProperty(CPP_PROPERTY(VolumetricLighting), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("ShinySurfaces"),     RF_Public)UBoolProperty(CPP_PROPERTY(ShinySurfaces     ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("Coronas"),           RF_Public)UBoolProperty(CPP_PROPERTY(Coronas           ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("HighDetailActors"),  RF_Public)UBoolProperty(CPP_PROPERTY(HighDetailActors  ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("Description"),       RF_Public)UStrProperty(CPP_PROPERTY(Description        ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("DescFlags"),         RF_Public)UIntProperty(CPP_PROPERTY(DescFlags          ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("DetailTextures"),    RF_Public)UBoolProperty(CPP_PROPERTY(DetailTextures     ), TEXT("Client"), CPF_Config );

	DecompFormat = TEXF_P8;

	unguard;
}

//
// Draw a clipped 2D line.
//
void URenderDevice::Draw2DClippedLine
(
	FSceneNode*		Frame,
	FPlane			Color,
	DWORD			LineFlags,
	FVector			P1,
	FVector			P2
)
{
	guard(URenderDevice::Draw2DClippedLine);

	// X clip it.
	if( P1.X > P2.X )
		Exchange( P1, P2 );
	if( P2.X<0 || P1.X>Frame->FX )
		return;
	if( P1.X<0 )
	{
		if( Abs(P2.X-P1.X)<0.001 )
			return;
		P1.Y += (0-P1.X)*(P2.Y-P1.Y)/(P2.X-P1.X);
		P1.X  = 0;
	}
	if( P2.X>=Frame->FX )
	{
		if( Abs(P2.X-P1.X)<0.001 )
			return;
		P2.Y += ((Frame->FX-1.0)-P2.X)*(P2.Y-P1.Y)/(P2.X-P1.X);
		P2.X  = Frame->FX-1.0;
	}

	// Y clip it.
	if( P1.Y > P2.Y )
		Exchange( P1, P2 );
	if( P2.Y < 0 || P1.Y > Frame->FY )
		return;
	if( P1.Y < 0 )
	{
		if( Abs(P2.Y-P1.Y)<0.001 )
			return;
		P1.X += (0-P1.Y)*(P2.X-P1.X)/(P2.Y-P1.Y);
		P1.Y  = 0;
	}
	if( P2.Y >= Frame->FY )
	{
		if( Abs(P2.Y-P1.Y)<0.001 )
			return;
		P2.X += ((Frame->FY-1.0)-P2.Y)*(P2.X-P1.X)/(P2.Y-P1.Y);
		P2.Y  = Frame->FY-1.0;
	}

	// Assure no coordinates are out of bounds. 
	ClipFloatFromZero(P1.X,Frame->FX);
	ClipFloatFromZero(P2.X,Frame->FX);
	ClipFloatFromZero(P1.Y,Frame->FY);
	ClipFloatFromZero(P2.Y,Frame->FY);

	// Draw it.
	Draw2DLine( Frame, Color, LineFlags, P1, P2 );

	unguard;
}

UBOOL URenderDevice::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(UVRenderDevice::Exec);

	if( ParseCommand(&Cmd,TEXT("GetDetailTextures")) )
	{
		Ar.Logf( TEXT("%i"), DetailTextures );
		return 1;
	} 
	else if( ParseCommand(&Cmd,TEXT("ToggleDetailTextures")) )
	{
		DetailTextures ^= 1;
		SaveConfig();
		return 1;
	}
	else
		return 0;

	unguard;
}

//
// Draw a clipped 3D line.
//
void URenderDevice::Draw3DLine
(
	FSceneNode*		Frame,
	FPlane			Color,
	DWORD			LineFlags,
	FVector			P1,
	FVector			P2
)
{
	guard(URenderDevice::Draw3DLine);
	FLOAT SX2 = Frame->FX2;
	FLOAT SY2 = Frame->FY2;

	// Transform.
	P1 = P1.TransformPointBy( Frame->Coords );
	P2 = P2.TransformPointBy( Frame->Coords );
	if( Frame->Viewport->IsOrtho() )
	{
		// Zoom.
		P1 = P1 / Frame->Zoom + FVector( SX2, SY2, 0 );
		P2 = P2 / Frame->Zoom + FVector( SX2, SY2, 0 );

		// See if points form a line parallel to our line of sight (i.e. line appears as a dot).
		if( Abs(P2.X-P1.X)+Abs(P1.Y-P2.Y)<0.2 )
		{
			// Line is visible as a point.
			if( Frame->Viewport->Actor->OrthoZoom < ORTHO_LOW_DETAIL )
				Draw2DPoint( Frame, Color, LINE_None, P1.X-1, P1.Y-1, P1.X+1, P1.Y+1, 1.0 );
			return;
		}
	}
	else
	{
		// Calculate delta, discard line if points are identical.
		FVector D = P2-P1;
		if( D.SizeSquared() < Square(0.01) )
			return;

		// Clip to near clipping plane.
		if( P1.Z <= LINE_NEAR_CLIP_Z )
		{
			// Clip P1 to NCP.
			if( P2.Z<(LINE_NEAR_CLIP_Z-0.01) )
				return;
			P1.X +=  (LINE_NEAR_CLIP_Z-P1.Z) * D.X/D.Z;
			P1.Y +=  (LINE_NEAR_CLIP_Z-P1.Z) * D.Y/D.Z;
			P1.Z  =  (LINE_NEAR_CLIP_Z);
		}
		else if( P2.Z<(LINE_NEAR_CLIP_Z-0.01) )
		{
			// Clip P2 to NCP.
			P2.X += (LINE_NEAR_CLIP_Z-P2.Z) * D.X/D.Z;
			P2.Y += (LINE_NEAR_CLIP_Z-P2.Z) * D.Y/D.Z;
			P2.Z =  (LINE_NEAR_CLIP_Z);
		}

		// Calculate perspective.
		P1.Z = 1.0/P1.Z; P1.X = P1.X * Frame->Proj.Z * P1.Z + SX2; P1.Y = P1.Y * Frame->Proj.Z * P1.Z + SY2;
		P2.Z = 1.0/P2.Z; P2.X = P2.X * Frame->Proj.Z * P2.Z + SX2; P2.Y = P2.Y * Frame->Proj.Z * P2.Z + SY2;
	}

	// Clip it and draw it.
	Draw2DClippedLine( Frame, Color, LineFlags, P1, P2 );

	unguard;
}

/*-----------------------------------------------------------------------------
	UViewport object implementation.
-----------------------------------------------------------------------------*/

void UViewport::Serialize( FArchive& Ar )
{
	guard(UViewport::Serialize);
	Super::Serialize( Ar );
	Ar << Console << MiscRes << Canvas << RenDev << Input;
	unguard;
}
void* UViewport::GetServer()
{
	guard(UViewport::GetServer);
	return NULL;
	unguard;
}
IMPLEMENT_CLASS(UViewport);

/*-----------------------------------------------------------------------------
	Dragging.
-----------------------------------------------------------------------------*/

//
// Set mouse dragging.
// The mouse is dragging when and only when one or more button is held down.
//
UBOOL UViewport::SetDrag( UBOOL NewDrag )
{
	guard(UViewport::SetDrag);
	UBOOL Result = Dragging;
	Dragging = NewDrag;
	if( GIsRunning )
	{
		if( Dragging && !Result )
		{
			// First hit.
			GetOuterUClient()->Engine->MouseDelta( this, MOUSE_FirstHit, 0, 0 );
		}
		else if( Result && !Dragging )
		{
			// Last release.
			GetOuterUClient()->Engine->MouseDelta( this, MOUSE_LastRelease, 0, 0 );
		}
	}
	return Result;
	unguard;
}

/*-----------------------------------------------------------------------------
	Scene frames.
-----------------------------------------------------------------------------*/

//
// Precompute rendering info.
//
void FSceneNode::ComputeRenderCoords( FVector& Location, FRotator& Rotation )
{
	guard(UViewport::ComputeRenderCoords);
	check(Viewport->Actor);
	Coords =
	(	Viewport->Actor->RendMap==REN_OrthXY ? FCoords(FVector(0,0,0),FVector(+1,0,0),FVector(0,+1,0),FVector(0,0,+1))
	:	Viewport->Actor->RendMap==REN_OrthXZ ? FCoords(FVector(0,0,0),FVector(+1,0,0),FVector(0,0,-1),FVector(0,+1,0))
	:	Viewport->Actor->RendMap==REN_OrthYZ ? FCoords(FVector(0,0,0),FVector(0,+1,0),FVector(0,0,-1),FVector(+1,0,0))
	:								           GMath.ViewCoords / Rotation) / Location;
	Uncoords = Coords.Transpose();
	ComputeRenderSize();
	unguard;
}

//
// Precompute sizing info.
//
void FSceneNode::ComputeRenderSize()
{
	guard(FSceneNode::ComputeRenderSize);

	// Precomputes.
	FX 			= (FLOAT)X;
	FY 			= (FLOAT)Y;
	FX2			= FX * 0.5;
	FY2			= FY * 0.5;	
	FX15		= (FX+1.0001) * 0.5;
	FY15		= (FY+1.0001) * 0.5;	
	FLOAT SceneFovAngle = Viewport->Actor->FovAngle;
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
	if( Viewport && Viewport->GetOuterUClient() && Viewport->GetOuterUClient()->AndroidWidescreenFOV )
		SceneFovAngle = UT99AndroidCalcSceneWidescreenFOVV161( SceneFovAngle, FX, FY );
#endif
	Proj		= FVector( 0.5-0.5*FX, 0.5-0.5*FY, 0.5*FX / appTan(SceneFovAngle * PI/360.0) );
	RProj		= FVector( 1/Proj.X, 1/Proj.Y, 1/Proj.Z );
	Zoom 		= Viewport->Actor->OrthoZoom / (FX * 15.0);
	PrjXM		= (0  - FX2)*(-RProj.Z);
	PrjXP		= (FX - FX2)*(+RProj.Z);
	PrjYM		= (0  - FY2)*(-RProj.Z);
	PrjYP		= (FY - FY2)*(+RProj.Z);

	// Precompute side info.
	FLOAT TempSigns[2]={-1.0,+1.0};
	for( INT i=0; i<2; i++ )
	{
		for( INT j=0; j<2; j++ )
		{
			ViewSides[i*2+j] = FVector(TempSigns[i] * FX2, TempSigns[j] * FY2, Proj.Z).UnsafeNormal().TransformVectorBy(Uncoords);
		}
		ViewPlanes[i] = FPlane
		(
			Coords.Origin,
			FVector(0,TempSigns[i] / FY2,1.0/Proj.Z).UnsafeNormal().TransformVectorBy(Uncoords)
		);
		ViewPlanes[i+2] = FPlane
		(
			Coords.Origin,
			FVector(TempSigns[i] / FX2,0,1.0/Proj.Z).UnsafeNormal().TransformVectorBy(Uncoords)
		);
	}

	// Tell rendering device.
	Viewport->RenDev->SetSceneNode( this );

	unguard;
}

/*-----------------------------------------------------------------------------
	Custom viewport creation and destruction.
-----------------------------------------------------------------------------*/

//
// UViewport constructor.  Creates the viewport object but doesn't open its window.
//
UViewport::UViewport()
{
	guard(UViewport::UViewport);

	// Update viewport array.
	GetOuterUClient()->Viewports.AddItem( this );

	// Create canvas.
	UClass* CanvasClass = StaticLoadClass( UCanvas::StaticClass(), NULL, TEXT("ini:Engine.Engine.Canvas"), NULL, LOAD_NoFail, NULL );
	Canvas = CastChecked<UCanvas>(StaticConstructObject( CanvasClass ));
	Canvas->Init( this );

	// Create input system.
	UClass* InputClass = StaticLoadClass( UInput::StaticClass(), NULL, TEXT("ini:Engine.Engine.Input"), NULL, LOAD_NoFail, NULL );
	Input = (UInput*)StaticConstructObject( InputClass );

	// Set initial time.
	LastUpdateTime = appSeconds();

	unguard;
}

//
// Close a viewport.
//warning: Lots of interrelated stuff is happening here between the viewport code,
// object code, platform-specific viewport manager code, and Windows.
//
void UViewport::Destroy()
{
	guard(UViewport::Destroy);

	// Temporary for editor!!
	if( GetOuterUClient()->Engine->Audio && GetOuterUClient()->Engine->Audio->GetViewport()==this )
		GetOuterUClient()->Engine->Audio->SetViewport( NULL );

	// Close the viewport window.
	guard(CloseWindow);
	CloseWindow();
	unguard;

	#if _MSC_VER

	// Delete the input subsystem.
	guard(ExitInput);
	delete Input;
	unguard;

	// Delete the console.
	guard(DeleteConsole);
	if( Console )
		delete Console;
	unguard;

	// Delete the canvas.
	guard(DeleteCanvas);
	delete Canvas;
	unguard;

	#endif

	// Shut down rendering.
	guard(DeleteRenDev);
	if( RenDev )
	{
		RenDev->Exit();
		delete RenDev;
	}
	unguard;

	// Remove from viewport list.
	GetOuterUClient()->Viewports.RemoveItem( this );

	Super::Destroy();
	unguardobj;
}

/*---------------------------------------------------------------------------------------
	Viewport information functions.
---------------------------------------------------------------------------------------*/

//
// Is this camrea a wireframe view?
//
UBOOL UViewport::IsWire()
{
	guard(UViewport::IsWire);
	return
	Actor &&
	(	Actor->GetLevel()->Model->Nodes.Num()==0
	||	Actor->RendMap==REN_OrthXY
	||	Actor->RendMap==REN_OrthXZ
	||	Actor->RendMap==REN_OrthYZ
	||	Actor->RendMap==REN_Wire );

	unguard;
}

/*-----------------------------------------------------------------------------
	Viewport locking & unlocking.
-----------------------------------------------------------------------------*/

//
// Lock the viewport for rendering.
//
UBOOL UViewport::Lock( FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* HitData, INT* HitSize )
{
	guard(UViewport::Lock);
	check(RenDev);

	// Set info.
	CurrentTime = appSeconds();
	HitTesting = HitData!=NULL;
	RenderFlags = RenderLockFlags;
	ExtraPolyFlags = ((Actor->RendMap==REN_PolyCuts) || (Actor->RendMap==REN_Zones)) ? PF_NoMerge : 0;
	FrameCount++;

	// Lock rendering device.
	RenDev->Lock( FlashScale, FlashFog, ScreenClear, RenderLockFlags, HitData, HitSize );

	// Successfully locked it.
	return 1;
	unguard;
}

//
// Unlock the viewport.
//
void UViewport::Unlock( UBOOL Blit )
{
	guard(UViewport::Unlock);
	check(Actor);
	check(RenDev);
	check(HitSizes.Num()==0);

	// Unlock rendering device.
	RenDev->Unlock( Blit );

	// Update time.
	if( Blit )
		LastUpdateTime = CurrentTime;

	unguard;
}

/*-----------------------------------------------------------------------------
	Command line.
-----------------------------------------------------------------------------*/

//
// UViewport command line.
//
UBOOL UViewport::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(UViewport::Exec);
	check(Actor);
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
	// UT99_ANDROID_V109_ANDROID_DEDICATED_TO_LISTEN:
	// The stock UT99 "Dedicated" button executes RELAUNCH <url> -server, which
	// forks ./ucc on desktop Unix.  Android has no writable/executable ./ucc in
	// the app data path, so that path logs "Failed to launch process" and can
	// leave the would-be host dead before LAN clients can discover it.  Treat the
	// command as an in-process listen server travel instead.  The normal "Start"
	// button already does this; this makes accidental Dedicated clicks harmless.
	const TCHAR* RelaunchCmd = Cmd;
	if( ParseCommand( &RelaunchCmd, TEXT("RELAUNCH") ) )
	{
		FString ListenURL = UT99AndroidV109BuildListenURLFromRelaunch( RelaunchCmd );
		if( ListenURL.Len() > 0 && Actor && Actor->GetLevel() && Actor->GetLevel()->Engine )
		{
			debugf( NAME_Log, TEXT("UT99_ANDROID_V109_RELAUNCH_AS_LISTEN %s"), *ListenURL );
			Actor->eventPreClientTravel();
			Actor->GetLevel()->Engine->SetClientTravel( this, *ListenURL, 0, TRAVEL_Absolute );
		}
		else
		{
			debugf( NAME_Warning, TEXT("UT99_ANDROID_V109_RELAUNCH_IGNORED empty url: %s"), RelaunchCmd );
		}
		return 1;
	}
#endif
	if( Input && Input->Exec(Cmd,Ar) )
	{
		return 1;
	}
	else if( RenDev && RenDev->Exec(Cmd,Ar) )
	{
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("GETPING")) )
	{
		Ar.Logf( TEXT("0") );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("INJECT")) )
	{
		UNetDriver* Driver = Actor->GetLevel()->NetDriver;
		if( Driver && Driver->ServerConnection )
		{
			Ar.Logf(TEXT("Injecting: %s"),Cmd);
			Driver->ServerConnection->Logf( Cmd );
		}
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("NETSPEED")) )
	{
		INT Rate = appAtoi(Cmd);
		UNetDriver* Driver = Actor->GetLevel()->NetDriver;
		GetDefault<UPlayer>()->ConfiguredInternetSpeed = Rate;
		GetDefault<UPlayer>()->SaveConfig();
		if( Rate>=500 && Driver && Driver->ServerConnection )
		{
			if( !Driver->ServerConnection->URL.HasOption(TEXT("LAN")) )
			{
				CurrentNetSpeed = Driver->ServerConnection->CurrentNetSpeed = Clamp<INT>( Rate, 500, Driver->MaxClientRate );
				Driver->ServerConnection->Logf( TEXT("NETSPEED %i"), Rate );
			}
		}
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("LANSPEED")) )
	{
		INT Rate = appAtoi(Cmd);
		UNetDriver* Driver = Actor->GetLevel()->NetDriver;
		GetDefault<UPlayer>()->ConfiguredInternetSpeed = Rate;
		GetDefault<UPlayer>()->SaveConfig();
		if( Rate>=500 && Driver && Driver->ServerConnection )
		{
			if( Driver->ServerConnection->URL.HasOption(TEXT("LAN")) )
			{
				CurrentNetSpeed = Driver->ServerConnection->CurrentNetSpeed = Clamp<INT>( Rate, 500, Driver->MaxClientRate );
				Driver->ServerConnection->Logf( TEXT("NETSPEED %i"), Rate );
			}
		}
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("SHOWALL")) )
	{
		for( INT i=0; i<Actor->GetLevel()->Actors.Num(); i++ )
			if( Actor->GetLevel()->Actors(i) )
				Actor->GetLevel()->Actors(i)->bHidden = 0;
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("REPORT")) )
	{
		FStringOutputDevice Str;
		Str.Log ( TEXT("Report:\r\n") );
		Str.Logf( TEXT("   Version: %s %s\r\n"), appFromAnsi(__DATE__), appFromAnsi(__TIME__) );
		Str.Logf( TEXT("   Player class: %s\r\n"), Actor->GetClass()->GetName() );
		Str.Logf( TEXT("   URL: %s\r\n"), *Actor->GetLevel()->URL.String() );
		Str.Logf( TEXT("   Location: %i %i %i\r\n"), (INT)Actor->Location.X, (INT)Actor->Location.Y, (INT)Actor->Location.Z );
		if( Actor->Level->Game==NULL )
		{
			Str.Logf( TEXT("   Network client\r\n") );
		}
		else
		{
			Str.Logf( TEXT("   Game class: %s\r\n"), Actor->Level->Game->GetClass()->GetName() );
			Str.Logf( TEXT("   Difficulty: %i\r\n"), Actor->Level->Game->Difficulty );
		}
		appClipboardCopy( *Str );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("SHOT")) )
	{
		// Screenshot.
		TCHAR File[32];
		for( INT i=0; i<256; i++ )
		{
			appSprintf( File, TEXT("Shot%04i.bmp"), i );
			if( GFileManager->FileSize(File) < 0 )
				break;
		}
		if( GFileManager->FileSize(File)<0 )
		{
			FMemMark Mark(GMem);
			FColor* Buf = new(GMem,SizeX*SizeY)FColor;
			RenDev->ReadPixels( Buf );
			FArchive* Ar = GFileManager->CreateFileWriter( File );
			if( Ar )
			{
				// Types.
				#if _MSC_VER
					#pragma pack (push,1)
				#endif
				struct BITMAPFILEHEADER
				{
					_WORD   bfType GCC_PACK(1);
					DWORD   bfSize GCC_PACK(1);
					_WORD   bfReserved1 GCC_PACK(1); 
					_WORD   bfReserved2 GCC_PACK(1);
					DWORD   bfOffBits GCC_PACK(1);
				} FH; 
				struct BITMAPINFOHEADER
				{
					DWORD  biSize GCC_PACK(1); 
					INT    biWidth GCC_PACK(1);
					INT    biHeight GCC_PACK(1);
					_WORD  biPlanes GCC_PACK(1);
					_WORD  biBitCount GCC_PACK(1);
					DWORD  biCompression GCC_PACK(1);
					DWORD  biSizeImage GCC_PACK(1);
					INT    biXPelsPerMeter GCC_PACK(1); 
					INT    biYPelsPerMeter GCC_PACK(1);
					DWORD  biClrUsed GCC_PACK(1);
					DWORD  biClrImportant GCC_PACK(1); 
				} IH;
				#if _MSC_VER
					#pragma pack (pop)
				#endif

				// File header.
				FH.bfType		= 'B' + 256*'M';
				FH.bfSize		= sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + 3 * SizeX * SizeY;
				FH.bfReserved1	= 0;
				FH.bfReserved2	= 0;
				FH.bfOffBits	= sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
				Ar->Serialize( &FH, sizeof(FH) );

				// Info header.
				IH.biSize			= sizeof(BITMAPINFOHEADER);
				IH.biWidth			= SizeX;
				IH.biHeight			= SizeY;
				IH.biPlanes			= 1;
				IH.biBitCount		= 24;
				IH.biCompression	= 0; //BI_RGB
				IH.biSizeImage		= SizeX * SizeY * 3;
				IH.biXPelsPerMeter	= 0;
				IH.biYPelsPerMeter	= 0;
				IH.biClrUsed		= 0;
				IH.biClrImportant	= 0;
				Ar->Serialize( &IH, sizeof(IH) );

				// Colors.
				#if _MSC_VER
				for( INT i=SizeY-1; i>=0; i-- )
					for( INT j=0; j<SizeX; j++ )
						Ar->Serialize( &Buf[i*SizeX+j], 3 );
				#else
				for( INT i=SizeY; i>0; i-- )
					for( INT j=0; j<SizeX; j++ )
						Ar->Serialize( &Buf[i*SizeX+j], 3 );
				#endif

				// Success.
				delete Ar;
			}
			Mark.Pop();
		}
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("SHOWACTORS")) )
	{
		Actor->ShowFlags |= SHOW_Actors;
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("HIDEACTORS")) )
	{
		Actor->ShowFlags &= ~SHOW_Actors;
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("RMODE")) )
	{
		INT Mode = appAtoi(Cmd);
		if( Mode>REN_None && Mode<REN_MAX )
			Actor->RendMap = Mode;
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("EXEC")) )
	{
		TCHAR Filename[64];
		if( ParseToken( Cmd, Filename, ARRAY_COUNT(Filename), 0 ) )
			ExecMacro( Filename, Ar );
		return 1;
	}
	else if( Console && Console->ScriptConsoleExec(Cmd,Ar,Actor) )
	{
		return 1;
	}
	else if( UPlayer::Exec(Cmd,Ar) )
	{
		return 1;
	}
	else return 0;
	unguard;
}

//
// Execute a macro on this viewport.
//
void UViewport::ExecMacro( const TCHAR* Filename, FOutputDevice& Ar )
{
	guard(UViewport::ExecMacro);

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
	else Ar.Logf( NAME_ExecWarning, LocalizeError("FileNotFound",TEXT("Core")), Filename );

	unguard;
}

/*-----------------------------------------------------------------------------
	UViewport FArchive interface.
-----------------------------------------------------------------------------*/

//
// Output a message on the viewport's console.
//
void UViewport::Serialize( const TCHAR* Data, EName MsgType )
{
	guard(UViewport::Serialize);

	// Pass to console.
	if( Console )
		Console->Serialize( Data, MsgType );

	unguard;
}

/*-----------------------------------------------------------------------------
	Input.
-----------------------------------------------------------------------------*/

//
// Read input from the viewport.
//
void UViewport::ReadInput( FLOAT DeltaSeconds )
{
	guard(UViewport::ReadInput);
	check(Input);

	// Update input.
	if( DeltaSeconds!=-1.0 )
		UpdateInput( 0 );

	// Get input from input system.
	Input->ReadInput( DeltaSeconds, *GLog );

	unguard;
}

/*-----------------------------------------------------------------------------
	Viewport hit testing.
-----------------------------------------------------------------------------*/

//
// Viewport hit-test pushing.
//
void UViewport::PushHit( const HHitProxy& Hit, INT Size )
{
	guard(UViewport::PushHit);

	Hit.Size = Size;
	HitSizes.AddItem(Size);
	RenDev->PushHit( (BYTE*)&Hit, Size );

	unguard;
}

//
// Pop the most recently pushed hit.
//
void UViewport::PopHit( UBOOL bForce )
{
	guard(UViewport::PopHit);
	checkSlow(RenDev);
	checkSlow(HitTesting);
	check(HitSizes.Num());

	RenDev->PopHit( HitSizes.Pop(), bForce );

	unguard;
}

//
// Execute all hits in the hit buffer.
//
void UViewport::ExecuteHits( const FHitCause& Cause, BYTE* HitData, INT HitSize )
{
	guard(UViewport::ExecuteHits);

	// String together the hit stack.
	HHitProxy* TopHit=NULL;
	while( HitSize>0 )
	{
		HHitProxy* ThisHit = (HHitProxy*)HitData;
		HitData           += ThisHit->Size;
		HitSize           -= ThisHit->Size;
		ThisHit->Parent    = TopHit;
		TopHit             = ThisHit;
	}
	check(HitSize==0);

	// Call the innermost hit.
	if( TopHit )
		TopHit->Click( Cause );

	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
