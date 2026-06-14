/*=============================================================================
	UnSoftLn.cpp: Unreal software line drawing.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "RenderPrivate.h"

/*-----------------------------------------------------------------------------
	Projection.
-----------------------------------------------------------------------------*/

//
// Figure out the unclipped screen location of a 3D point taking into account either
// a perspective or orthogonal projection.  Returns 1 if view is orthogonal or point 
// is visible in 3D view, 0 if invisible in 3D view (behind the viewer).
//
// Scale = scale of one world unit (at this point) relative to screen pixels,
// for example 0.5 means one world unit is 0.5 pixels.
//
UBOOL URender::Project( FSceneNode* Frame, const FVector& V, FLOAT& ScreenX, FLOAT& ScreenY, FLOAT* Scale )
{
	guard(URender::Project);

	FVector	Temp = V - Frame->Coords.Origin;
	if( Frame->Viewport->Actor->RendMap == REN_OrthXY )
	{
		ScreenX = +Temp.X / Frame->Zoom + Frame->FX2;
		ScreenY = +Temp.Y / Frame->Zoom + Frame->FY2;
		if( Scale )
			*Scale = 1.0/Frame->Zoom;
		return 1;
	}
	else if( Frame->Viewport->Actor->RendMap==REN_OrthXZ )
	{
		ScreenX = +Temp.X / Frame->Zoom + Frame->FX2;
		ScreenY = -Temp.Z / Frame->Zoom + Frame->FY2;
		if( Scale )
			*Scale = 1.0/Frame->Zoom;
		return 1;
	}
	else if( Frame->Viewport->Actor->RendMap==REN_OrthYZ )
	{
		ScreenX = +Temp.Y / Frame->Zoom + Frame->FX2;
		ScreenY = -Temp.Z / Frame->Zoom + Frame->FY2;
		if( Scale )
			*Scale = 1.0/Frame->Zoom;
		return 1;
	}
	else
	{
		Temp     = Temp.TransformVectorBy( Frame->Coords );
		FLOAT Z  = Temp.Z; if (Abs (Z)<0.01) Z+=0.02;
		FLOAT RZ = Frame->Proj.Z / Z;
		ScreenX = Temp.X * RZ + Frame->FX2;
		ScreenY = Temp.Y * RZ + Frame->FY2;

		if( Scale  )
			*Scale = RZ;

		return Z > 1.0;
	}
	unguard;
}

//
// Convert a particular screen location to a world location.  In ortho views,
// sets non-visible component to zero.  In persp views, places at viewport location
// unless UseEdScan=1 and the user just clicked on a wall (a Bsp polygon).
// Sets V to location and returns 1, or returns 0 if couldn't perform conversion.
//
UBOOL URender::Deproject( FSceneNode* Frame, INT ScreenX, INT ScreenY, FVector& V )
{
	guard(URender::Deproject);

	FVector  Origin = Frame->Coords.Origin;
	FLOAT	 SX		= (FLOAT)ScreenX - Frame->FX2;
	FLOAT	 SY		= (FLOAT)ScreenY - Frame->FY2;

	switch( Frame->Viewport->Actor->RendMap )
	{
		case REN_OrthXY:
			V.X = +SX * Frame->Zoom + Origin.X;
			V.Y = +SY * Frame->Zoom + Origin.Y;
			V.Z = 0;
			return 1;
		case REN_OrthXZ:
			V.X = +SX * Frame->Zoom + Origin.X;
			V.Y = 0.0;
			V.Z = -SY * Frame->Zoom + Origin.Z;
			return 1;
		case REN_OrthYZ:
			V.X = 0.0;
			V.Y = +SX * Frame->Zoom + Origin.Y;
			V.Z = -SY * Frame->Zoom + Origin.Z;
			return 1;
		default:
			V = Origin;
			return 0;
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	Circle drawing.
-----------------------------------------------------------------------------*/

//
// Draw a circle.
//
void URender::DrawCircle
(
	FSceneNode*		Frame,
	FPlane			Color,
	DWORD			LineFlags,
	FVector&		Location,
	FLOAT			Radius
)
{
	guard(URender::DrawCircle);

	FVector A = Frame->Coords.XAxis;
	FVector B = Frame->Coords.YAxis;

	int Subdivide = 8;
	for
	(	FLOAT Thresh = Frame->Viewport->Actor->OrthoZoom/Radius
	;	Thresh<2048 && Subdivide<256
	;	Thresh*=2,Subdivide*=2 );

	FLOAT   F  = 0.0;
	FLOAT   AngleDelta = 2.0f*PI / Subdivide;

	FVector P1 = Location + Radius * (A * appCos(F) + B * appSin(F));

	for( int i=0; i<Subdivide; i++ )
	{
		F          += AngleDelta;
		FVector P2  = Location + Radius * (A * appCos(F) + B * appSin(F));
		Frame->Viewport->RenDev->Draw3DLine( Frame, Color, LineFlags, P1, P2 );
		P1 = P2;
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	Box drawing.
-----------------------------------------------------------------------------*/

//
// Draw a box centered about a location.
//
void URender::DrawBox
(
	FSceneNode*		Frame,
	FPlane			Color,
	DWORD			LineFlags,
	FVector			Min,
	FVector			Max
)
{
	guard(URender::DrawBox);

	FVector A,B;
	FVector Location = Min+Max;
	if	   ( Frame->Viewport->Actor->RendMap==REN_OrthXY )	{A=FVector(Max.X-Min.X,0,0); B=FVector(0,Max.Y-Min.Y,0);}
	else if( Frame->Viewport->Actor->RendMap==REN_OrthXZ )	{A=FVector(Max.X-Min.X,0,0); B=FVector(0,0,Max.Z-Min.Z);}
	else													{A=FVector(0,Max.Y-Min.Y,0); B=FVector(0,0,Max.Z-Min.Z);}

	Frame->Viewport->RenDev->Draw3DLine( Frame, Color, LineFlags, (Location+A+B)/2, (Location+A-B)/2 );
	Frame->Viewport->RenDev->Draw3DLine( Frame, Color, LineFlags, (Location-A+B)/2, (Location-A-B)/2 );
	Frame->Viewport->RenDev->Draw3DLine( Frame, Color, LineFlags, (Location+A+B)/2, (Location-A+B)/2 );
	Frame->Viewport->RenDev->Draw3DLine( Frame, Color, LineFlags, (Location+A-B)/2, (Location-A-B)/2 );

	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
