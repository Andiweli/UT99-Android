/*=============================================================================
	UnRender.h: Rendering functions and structures
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

/*------------------------------------------------------------------------------------
	Forward declarations.
------------------------------------------------------------------------------------*/

class  URenderDevice;
class  FSpan;
class  FSpanBuffer;
class  FRasterPoly;
struct FTransTexture;
struct FScreenBounds;
struct FSurfaceInfo;
struct FSurfaceFacet;
struct FSceneNode;
struct FDynamicItem;
struct FDynamicSprite;
struct FBspDrawList;
struct FSavedPoly;

/*------------------------------------------------------------------------------------
	Includes.
------------------------------------------------------------------------------------*/

#include "UnRenDev.h"

/*------------------------------------------------------------------------------------
	Defines.
------------------------------------------------------------------------------------*/

#define ORTHO_LOW_DETAIL 40000.0

/*------------------------------------------------------------------------------------
	FSceneNode.
------------------------------------------------------------------------------------*/

//
// A scene frame is a temporary object representing a portion of
// the view of the world to render.
//
class FSpanBuffer;
struct FBspDrawList;
struct FDynamicSprite;
struct ENGINE_API FSceneNode
{
	// Variables.
	UViewport*		Viewport;		// Viewport the scene frame is attached to.
	ULevel*			Level;			// Level this scene is being rendered from.
	FSceneNode*		Parent;			// Frame from whence this was created, NULL=top level.
	FSceneNode*		Sibling;		// Next sibling scene frame.
	FSceneNode*		Child;			// Next child scene frame.
	INT				iSurf;			// Surface seen through (Parent,iSurface pair is unique).
	INT				ZoneNumber;		// Inital rendering zone of viewport in destination level (NOT the zone of the viewpoint!)
	INT				Recursion;		// Recursion depth, 0 if initial.
	FLOAT			Mirror;			// Mirror value, 1.0 or -1.0.
	FPlane			NearClip;		// Near-clipping plane in screenspace.
	FCoords			Coords;			// Transform coordinate system.
	FCoords			Uncoords;		// Inverse coordinate system.
	FSpanBuffer*	Span;			// Initial span buffer for the scene.
	FBspDrawList*	Draw[3];		// Draw lists (portals, occluding, non-occluding).
	FDynamicSprite* Sprite;			// Sprites to draw.
	INT				X, Y;			// Frame size.
	INT				XB, YB;			// Offset of top left active viewport.

	// Precomputes.
	FLOAT			FX, FY;			// Floating point X,Y.
	FLOAT			FX15, FY15;		// (Floating point SXR + 1.0001)/2.0.
	FLOAT			FX2, FY2;		// Floating point SXR / 2.0.
	FLOAT			Zoom;			// Zoom value, based on OrthoZoom and size.
	FVector			Proj;      		// Projection vector.
	FVector			RProj;			// Reverse projection vector.
	FLOAT			PrjXM, PrjYM;	// Minus clipping numbers.
	FLOAT			PrjXP, PrjYP;	// Plus clipping numbers.
	FVector			ViewSides [4];	// 4 unit vectors indicating view frustrum extent lines.
	FPlane			ViewPlanes[4];	// 4 planes indicating view frustrum extent planes.

	// Functions.
	BYTE* Screen( INT X, INT Y ) {return Viewport->ScreenPointer + (X+XB+(Y+YB)*Viewport->Stride)*Viewport->ColorBytes;}
	void ComputeRenderSize();
	void ComputeRenderCoords( FVector& Location, FRotator& Rotation );
};

/*------------------------------------------------------------------------------------
	Transformations.
------------------------------------------------------------------------------------*/

//
// Transformed vector with outcode info.
//
struct FOutVector
{
	FVector Point;
	BYTE    Flags;
};

//
// Transformed and projected vector.
//
struct FTransform : public FOutVector
{
	FLOAT ScreenX;
	FLOAT ScreenY;
	INT   IntY;
	FLOAT RZ;
	void Project( const FSceneNode* Frame )
	{
		RZ      = Frame->Proj.Z / Point.Z;
		ScreenX = Point.X * RZ + Frame->FX15;
		ScreenY = Point.Y * RZ + Frame->FY15;
		IntY    = appFloor( ScreenY );
	}
	void ComputeOutcode( const FSceneNode* Frame )
	{
		static FLOAT ClipXM, ClipXP, ClipYM, ClipYP;
		static const BYTE OutXMinTab [2] = { 0, FVF_OutXMin };
		static const BYTE OutXMaxTab [2] = { 0, FVF_OutXMax };
		static const BYTE OutYMinTab [2] = { 0, FVF_OutYMin };
		static const BYTE OutYMaxTab [2] = { 0, FVF_OutYMax };
		ClipXM = Frame->PrjXM * Point.Z + Point.X;
		ClipXP = Frame->PrjXP * Point.Z - Point.X;
		ClipYM = Frame->PrjYM * Point.Z + Point.Y;
		ClipYP = Frame->PrjYP * Point.Z - Point.Y;
		Flags  =
		(	OutXMinTab [ClipXM < 0.0]
		+	OutXMaxTab [ClipXP < 0.0]
		+	OutYMinTab [ClipYM < 0.0]
		+	OutYMaxTab [ClipYP < 0.0]);
	}
	FTransform operator+( const FTransform& V ) const
	{
		FTransform Temp;
		Temp.Point = Point + V.Point;
		return Temp;
	}
	FTransform operator-( const FTransform& V ) const
	{
		FTransform Temp;
		Temp.Point = Point - V.Point;
		return Temp;
	}
	FTransform operator*(FLOAT Scale ) const
	{
		FTransform Temp;
		Temp.Point = Point * Scale;
		return Temp;
	}
};

//
// Transformed sample point.
//
struct FTransSample : public FTransform
{
	FPlane Normal, Light, Fog;
	FTransSample operator+( const FTransSample& T ) const
	{
		FTransSample Temp;
		Temp.Point = Point + T.Point;
		Temp.Light = Light + T.Light;
		Temp.Fog.X = Fog.X + T.Fog.X;
		Temp.Fog.Y = Fog.Y + T.Fog.Y;
		Temp.Fog.Z = Fog.Z + T.Fog.Z;
		Temp.Fog.W = Fog.W + T.Fog.W;
		return Temp;
	}
	FTransSample operator-( const FTransSample& T ) const
	{
		FTransSample Temp;
		Temp.Point = Point - T.Point;
		Temp.Light = Light - T.Light;
		Temp.Fog.X = Fog.X - T.Fog.X;
		Temp.Fog.Y = Fog.Y - T.Fog.Y;
		Temp.Fog.Z = Fog.Z - T.Fog.Z;
		Temp.Fog.W = Fog.W - T.Fog.W;
		return Temp;
	}
	FTransSample operator*( FLOAT Scale ) const
	{
		FTransSample Temp;
		Temp.Point = Point * Scale;
		Temp.Light = Light * Scale;
		Temp.Fog.X = Fog.X * Scale;
		Temp.Fog.Y = Fog.Y * Scale;
		Temp.Fog.Z = Fog.Z * Scale;
		Temp.Fog.W = Fog.W * Scale;
		return Temp;
	}
};

//
// Transformed texture mapped point.
//
struct FTransTexture : public FTransSample
{
	FLOAT U, V;
	FTransTexture operator+( const FTransTexture& T ) const
	{
		FTransTexture Temp;
		Temp.Point = Point + T.Point;
		Temp.Light = Light + T.Light;
		Temp.Fog.X = Fog.X + T.Fog.X;
		Temp.Fog.Y = Fog.Y + T.Fog.Y;
		Temp.Fog.Z = Fog.Z + T.Fog.Z;
		Temp.Fog.W = Fog.W + T.Fog.W;
		Temp.U     = U     + T.U;
		Temp.V     = V     + T.V;
		return Temp;
	}
	FTransTexture operator-( const FTransTexture& T ) const
	{
		FTransTexture Temp;
		Temp.Point = Point - T.Point;
		Temp.Light = Light - T.Light;
		Temp.Fog.X = Fog.X - T.Fog.X;
		Temp.Fog.Y = Fog.Y - T.Fog.Y;
		Temp.Fog.Z = Fog.Z - T.Fog.Z;
		Temp.Fog.W = Fog.W - T.Fog.W;
		Temp.U     = U     - T.U; 
		Temp.V     = V     - T.V;
		return Temp;
	}
	FTransTexture operator*( FLOAT Scale ) const
	{
		FTransTexture Temp;
		Temp.Point = Point * Scale;
		Temp.Light = Light * Scale;
		Temp.Fog.X = Fog.X * Scale;
		Temp.Fog.Y = Fog.Y * Scale;
		Temp.Fog.Z = Fog.Z * Scale;
		Temp.Fog.W = Fog.W * Scale;
		Temp.U     = U     * Scale;
		Temp.V     = V     * Scale;
		return Temp;
	}
};

/*------------------------------------------------------------------------------------
	FSurfaceInfo.
------------------------------------------------------------------------------------*/

//
// Description of a renderable surface.
//
struct FSurfaceInfo
{
	DWORD			PolyFlags;		// Surface flags.
	FColor			FlatColor;		// Flat-shaded color.
	ULevel*			Level;			// Level to render.
	FTextureInfo*	Texture;		// Regular texture mapping info, if any.
	FTextureInfo*	LightMap;		// Light map, if any.
	FTextureInfo*	MacroTexture;	// Macrotexture, if any.
	FTextureInfo*	DetailTexture;	// Detail map, if any.
	FTextureInfo*	FogMap;			// Fog map, if any.
};

//
// A saved polygon.
//
struct FSavedPoly
{
	FSavedPoly* Next;
	INT			iNode;
	void*       User;
	INT         NumPts;
	FTransform* Pts[ZEROARRAY];
};

//
// Description of a surface facet, represented as either
// a convex polygon or a concave span buffer.
//
struct FSurfaceFacet
{
	FCoords			MapCoords;		// Mapping coordinates.
	FCoords			MapUncoords;	// Inverse mapping coordinates.
	FSpanBuffer*	Span;			// Span buffer, if rendering device wants it.
	FSavedPoly*		Polys;			// Polygon list.
};

/*------------------------------------------------------------------------------------
	FScreenBounds.
------------------------------------------------------------------------------------*/

//
// Screen extents of an axis-aligned bounding box.
//
struct ENGINE_API FScreenBounds
{
	FLOAT MinX, MinY;
	FLOAT MaxX, MaxY;
	FLOAT MinZ;
};

/*------------------------------------------------------------------------------------
	URenderBase.
------------------------------------------------------------------------------------*/

//
// Line drawing flags.
//
enum ELineFlags
{
	LINE_None,
	LINE_Transparent,
	LINE_DepthCued
};

//
// Pure virtual base class of the rendering subsytem.
//
class ENGINE_API URenderBase : public USubsystem
{
	DECLARE_ABSTRACT_CLASS(URenderBase,USubsystem,CLASS_Transient|CLASS_Config)
	NO_DEFAULT_CONSTRUCTOR(URenderBase);

	// Variables.
	UEngine* Engine;

	// Init/exit functions.
	virtual void Init( UEngine* InEngine ) {Engine=InEngine;}
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog )=0;

	// Prerender/postrender functions.
	virtual void PreRender( FSceneNode* Frame )=0;
	virtual void PostRender( FSceneNode* Frame )=0;

	// Scene frame management.
	virtual FSceneNode* CreateMasterFrame( UViewport* Viewport, FVector Location, FRotator Rotation, FScreenBounds* Bounds )=0;
	virtual FSceneNode* CreateChildFrame( FSceneNode* Parent, FSpanBuffer* Span, ULevel* Level, INT iSurf, INT iZone, FLOAT Mirror, const FPlane& NearClip, const FCoords& Coords, FScreenBounds* Bounds )=0;
	virtual void FinishMasterFrame()=0;

	// Major rendering functions.
	virtual void DrawWorld( FSceneNode* Frame )=0;
	virtual void DrawActor( FSceneNode* Frame, AActor* Actor )=0;

	// Other functions.
	virtual UBOOL Project( FSceneNode* Frame, const FVector &V, FLOAT &ScreenX, FLOAT &ScreenY, FLOAT* Scale )=0;
	virtual UBOOL Deproject( FSceneNode* Frame, INT ScreenX, INT ScreenY, FVector& V )=0;
	virtual UBOOL BoundVisible( FSceneNode* Frame, FBox* Bound, FSpanBuffer* SpanBuffer, FScreenBounds& Results )=0;
	virtual void GetVisibleSurfs( UViewport* Viewport, TArray<INT>& iSurfs )=0;
	virtual void GlobalLighting( UBOOL Realtime, AActor* Owner, FLOAT& Brightness, FPlane& Color )=0;
	virtual void Precache( UViewport* Viewport )=0;

	// High level primitive drawing.
	virtual void DrawCircle( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector& Location, FLOAT Radius )=0;
	virtual void DrawBox( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector Min, FVector Max )=0;
};

/*-----------------------------------------------------------------------------
	Hit proxies.
-----------------------------------------------------------------------------*/

// Hit a Bsp surface.
struct ENGINE_API HBspSurf : public HHitProxy
{
	DECLARE_HIT_PROXY(HBspSurf,HHitProxy)
	INT iSurf;
	HBspSurf( INT iInSurf ) : iSurf( iInSurf ) {}
};

// Hit an actor.
struct ENGINE_API HActor : public HHitProxy
{
	DECLARE_HIT_PROXY(HActor,HHitProxy)
	AActor* Actor;
	HActor( AActor* InActor ) : Actor( InActor ) {}
};

// Hit ray descriptor.
struct ENGINE_API HCoords : public HHitProxy
{
	DECLARE_HIT_PROXY(HCoords,HHitProxy)
	FCoords Coords, Uncoords;
	FVector Direction;
	HCoords( FSceneNode* InFrame )
	:	Coords  ( InFrame->Coords   )
	,	Uncoords( InFrame->Uncoords )
	{
		FLOAT X = InFrame->Viewport->HitX+InFrame->Viewport->HitXL/2;
		FLOAT Y = InFrame->Viewport->HitY+InFrame->Viewport->HitYL/2;
		Direction
		=	InFrame->Coords.ZAxis
		+	InFrame->Coords.XAxis * (X - InFrame->FX2) * InFrame->RProj.Z
		+	InFrame->Coords.YAxis * (Y - InFrame->FY2) * InFrame->RProj.Z;
	}
};

/*------------------------------------------------------------------------------------
	The End.
------------------------------------------------------------------------------------*/
