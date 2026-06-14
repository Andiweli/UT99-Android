/*=============================================================================
	UnEdCnst.cpp: Editor movement contraints.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EditorPrivate.h"

/*------------------------------------------------------------------------------
	Functions.
------------------------------------------------------------------------------*/

void FEditorConstraints::Snap( FVector& Point, FVector GridBase )
{
	guard(FEditorConstraints::Snap);
	if( GridEnabled )
		Point = (Point - GridBase).GridSnap( GridSize ) + GridBase;
	unguard;
}
void FEditorConstraints::Snap( FRotator& Rotation )
{
	guard(FEditorConstraints::Snap);
	if( RotGridEnabled )
		Rotation = Rotation.GridSnap( RotGridSize );
	unguard;
}
UBOOL FEditorConstraints::Snap( ULevel* Level, FVector& Location, FVector GridBase, FRotator& Rotation )
{
	guard(FEditorConstraints::Snap);

	UBOOL Snapped = 0;
	Snap( Rotation );
	if( Level && SnapVertices )
	{
		FVector	DestPoint;
		INT Temp;
		if( Level->Model->FindNearestVertex( Location, DestPoint, SnapDistance, Temp ) >= 0.0)
		{
			Location = DestPoint;
			Snapped = 1;
		}
	}
	if( !Snapped )
		Snap( Location, GridBase );
	return Snapped;
	unguard;
}

/*------------------------------------------------------------------------------
	The end.
------------------------------------------------------------------------------*/
