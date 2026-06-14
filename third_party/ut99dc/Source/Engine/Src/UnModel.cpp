/*=============================================================================
	UnModel.cpp: Unreal model functions
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"

/*-----------------------------------------------------------------------------
	Struct serializers.
-----------------------------------------------------------------------------*/

ENGINE_API FArchive& operator<<( FArchive& Ar, FBspSurf& Surf )
{
	guard(FBspSurf<<);
	Ar << Surf.Texture;
	Ar << Surf.PolyFlags;
#ifdef PLATFORM_DREAMCAST
	INT pBase = 0, vNormal = 0, vTextureU = 0, vTextureV = 0, iLightMap = 0, iBrushPoly = 0;
	if( Ar.IsSaving() )
	{
		pBase = (INT)Surf.pBase;
		vNormal = (INT)Surf.vNormal;
		vTextureU = (INT)Surf.vTextureU;
		vTextureV = (INT)Surf.vTextureV;
		iLightMap = (INT)Surf.iLightMap;
		iBrushPoly = (INT)Surf.iBrushPoly;
	}
	Ar << AR_INDEX(pBase) << AR_INDEX(vNormal);
	Ar << AR_INDEX(vTextureU) << AR_INDEX(vTextureV);
	Ar << AR_INDEX(iLightMap) << AR_INDEX(iBrushPoly);
	if( Ar.IsLoading() )
	{
		Surf.pBase = (SWORD)pBase;
		Surf.vNormal = (SWORD)vNormal;
		Surf.vTextureU = (SWORD)vTextureU;
		Surf.vTextureV = (SWORD)vTextureV;
		Surf.iLightMap = (SWORD)iLightMap;
		Surf.iBrushPoly = (SWORD)iBrushPoly;
	}
#else
	Ar << AR_INDEX(Surf.pBase) << AR_INDEX(Surf.vNormal);
	Ar << AR_INDEX(Surf.vTextureU) << AR_INDEX(Surf.vTextureV);
	Ar << AR_INDEX(Surf.iLightMap) << AR_INDEX(Surf.iBrushPoly);
#endif
	Ar << Surf.PanU << Surf.PanV;
	Ar << Surf.Actor;
	if( Ar.IsLoading() )
		Surf.PolyFlags &= ~PF_Transient;
#ifndef NODECALS
	if( !Ar.IsLoading() && !Ar.IsSaving() )
		Ar << Surf.Decals;
#endif
	return Ar;
	unguard;
}

ENGINE_API FArchive& operator<<( FArchive& Ar, FPoly& Poly )
{
	guard(FPoly<<);
	Ar << AR_INDEX(Poly.NumVertices);
	Ar << Poly.Base << Poly.Normal << Poly.TextureU << Poly.TextureV;
	for( INT i=0; i<Poly.NumVertices; i++ )
		Ar << Poly.Vertex[i];
	Ar << Poly.PolyFlags;
	Ar << Poly.Actor << Poly.Texture << Poly.ItemName;
	Ar << AR_INDEX(Poly.iLink) << AR_INDEX(Poly.iBrushPoly) << Poly.PanU << Poly.PanV;
	if( Ar.IsLoading() )
		Poly.PolyFlags &= ~PF_Transient;
	return Ar;
	unguard;
}

ENGINE_API FArchive& operator<<( FArchive& Ar, FBspNode& N )
{
#ifdef PLATFORM_DREAMCAST
	guard(FBspNode<<);
	Ar << N.Plane << N.ZoneMask << N.NodeFlags << AR_INDEX(N.iVertPool);
	INT iSurf = 0, iChild0 = 0, iChild1 = 0, iChild2 = 0, iCollisionBound = 0, iRenderBound = 0;
	if( Ar.IsSaving() )
	{
		iSurf = (INT)N.iSurf;
		iChild0 = (INT)N.iChild[0];
		iChild1 = (INT)N.iChild[1];
		iChild2 = (INT)N.iChild[2];
		iCollisionBound = (INT)N.iCollisionBound;
		iRenderBound = (INT)N.iRenderBound;
	}
	Ar << AR_INDEX(iSurf) << AR_INDEX(iChild0) << AR_INDEX(iChild1) << AR_INDEX(iChild2);
	Ar << AR_INDEX(iCollisionBound) << AR_INDEX(iRenderBound);
	if( Ar.IsLoading() )
	{
		N.iSurf = (SWORD)iSurf;
		N.iChild[0] = (SWORD)iChild0;
		N.iChild[1] = (SWORD)iChild1;
		N.iChild[2] = (SWORD)iChild2;
		N.iCollisionBound = (SWORD)iCollisionBound;
		N.iRenderBound = (SWORD)iRenderBound;
	}
	Ar << N.iZone[0] << N.iZone[1];
	Ar << N.NumVertices;
	// iLeaf was serialized directly as INT in original, now SWORD - convert to INT for serialization
	INT iLeaf0 = 0, iLeaf1 = 0;
	if( Ar.IsSaving() )
	{
		iLeaf0 = (INT)N.iLeaf[0];
		iLeaf1 = (INT)N.iLeaf[1];
	}
	Ar << iLeaf0 << iLeaf1;
	if( Ar.IsLoading() )
	{
		N.iLeaf[0] = (SWORD)iLeaf0;
		N.iLeaf[1] = (SWORD)iLeaf1;
	}
	if( Ar.IsLoading() )
		N.NodeFlags &= ~(NF_IsNew|NF_IsFront|NF_IsBack);
	return Ar;
	unguard;
#else
	guard(FBspNode<<);
	Ar << N.Plane << N.ZoneMask << N.NodeFlags << AR_INDEX(N.iVertPool) << AR_INDEX(N.iSurf);
	Ar << AR_INDEX(N.iChild[0]) << AR_INDEX(N.iChild[1]) << AR_INDEX(N.iChild[2]);
	Ar << AR_INDEX(N.iCollisionBound) << AR_INDEX(N.iRenderBound);
	Ar << N.iZone[0] << N.iZone[1];
	Ar << N.NumVertices;
	Ar << N.iLeaf[0] << N.iLeaf[1];
	if( Ar.IsLoading() )
		N.NodeFlags &= ~(NF_IsNew|NF_IsFront|NF_IsBack);
	return Ar;
	unguard;
#endif
}

/*---------------------------------------------------------------------------------------
	Old database implementations.
---------------------------------------------------------------------------------------*/

class ENGINE_API UBspNodes : public UObject
{
	DECLARE_CLASS(UBspNodes,UObject,CLASS_RuntimeStatic)
	TTransArray<FBspNode> Element;
	INT _NumZones;
	FZoneProperties	_Zones[FBspNode::MAX_ZONES];
	UBspNodes()
	: Element( this )
	{
		guard(UBspNodes::UBspNodes);
		_NumZones = 0;
		for( INT i=0; i<FBspNode::MAX_ZONES; i++ )
		{
			_Zones[i].ZoneActor    = NULL;
			_Zones[i].Connectivity = ((QWORD)1)<<i;
			_Zones[i].Visibility   = ~(QWORD)0;
		}	
		unguard;
	}
	void Serialize( FArchive& Ar )
	{
		guard(UBspNodes::Serialize);
		Super::Serialize(Ar);
		if( Ar.IsLoading() )
		{
			INT DbNum=Element.Num(), DbMax=DbNum;
			Ar << DbNum << DbMax;
			Element.Empty( DbNum );
			Element.AddZeroed( DbNum );
			for( INT i=0; i<Element.Num(); i++ )
				Ar << Element(i);
			Ar << AR_INDEX(_NumZones);
			for( INT i=0; i<_NumZones; i++ )
				Ar << _Zones[i];
		}
		unguardobj;
	}
};
IMPLEMENT_CLASS(UBspNodes);

class ENGINE_API UBspSurfs : public UObject
{
	DECLARE_CLASS(UBspSurfs,UObject,CLASS_RuntimeStatic)
	TArray<FBspSurf> Element;
	void Serialize( FArchive& Ar )
	{
		guard(FBspSurfs::Serialize);
		Super::Serialize( Ar );
		if( Ar.IsLoading() )
		{
			INT DbNum=0, DbMax=0;
			Ar << DbNum << DbMax;
			Element.Empty( DbNum );
			Element.AddZeroed( DbNum );
			for( INT i=0; i<Element.Num(); i++ )
				Ar << Element(i);
		}
		unguard;
	}
};
IMPLEMENT_CLASS(UBspSurfs);

class ENGINE_API UVectors : public UObject
{
	DECLARE_CLASS(UVectors,UObject,CLASS_RuntimeStatic)
	TArray<FVector> Element;
	void Serialize( FArchive& Ar )
	{
		guard(UVectors::Serialize);
		Super::Serialize( Ar );
		if( Ar.IsLoading() )
		{
			INT DbNum=Element.Num(), DbMax=DbNum;
			Ar << DbNum << DbMax;
			Element.Empty( DbNum );
			Element.Add( DbNum );
			for( INT i=0; i<Element.Num(); i++ )
				Ar << Element(i);
		}
		unguard;
	}
};
IMPLEMENT_CLASS(UVectors);

class ENGINE_API UVerts : public UObject
{
	DECLARE_CLASS(UVerts,UObject,CLASS_RuntimeStatic)
	TArray<FVert> Element;
	INT NumSharedSides;
	void Serialize( FArchive& Ar )
	{
		guard(UVerts::Serialize);
		Super::Serialize( Ar );
		if( Ar.IsLoading() )
		{
			Element.CountBytes( Ar );
			INT DbNum=Element.Num(), DbMax=DbNum;
			Ar << DbNum << DbMax;
			Element.Empty( DbNum );
			Element.Add( DbNum );
			for( INT i=0; i<Element.Num(); i++ )
				Ar << Element(i);
		}
		Ar << AR_INDEX(NumSharedSides);
		unguardobj;
	}
};
IMPLEMENT_CLASS(UVerts);

/*---------------------------------------------------------------------------------------
	UModel object implementation.
---------------------------------------------------------------------------------------*/

void UModel::Serialize( FArchive& Ar )
{
	guard(UModel::Serialize);
	Super::Serialize( Ar );

	//oldver
	UBspSurfs* _Surfs   = NULL;
	UVectors*  _Vectors = NULL;
	UVectors*  _Points  = NULL;
	UVerts*    _Verts   = NULL;
	UBspNodes* _Nodes   = NULL;
	if( Ar.Ver()<=61 )
	{
		Ar << _Vectors << _Points << _Nodes << _Surfs << _Verts;
	}
	else
	{
		Ar << Vectors << Points << Nodes << Surfs << Verts << NumSharedSides << NumZones;
		for( INT i=0; i<NumZones; i++ )
			Ar << Zones[i];
	}
	Ar << Polys;
	if( _Vectors )
	{
		Ar.Preload( _Vectors );
		ExchangeArray( _Vectors->Element, Vectors );
	}
	if( _Points )
	{
		Ar.Preload( _Points );
		ExchangeArray( _Points->Element, Points );
	}
	if( _Surfs )
	{
		Ar.Preload( _Surfs );
		ExchangeArray( _Surfs->Element, Surfs );
	}
	if( _Verts )
	{
		Ar.Preload( _Verts );
		ExchangeArray( _Verts->Element, Verts );
		NumSharedSides = _Verts->NumSharedSides;
	}
	if( _Nodes )
	{
		Ar.Preload( _Nodes );
		ExchangeArray( _Nodes->Element, Nodes );
		NumZones = _Nodes->_NumZones;
		for( INT i=0; i<NumZones; i++ )
			Zones[i] = _Nodes->_Zones[i];
	}
	if( Polys && !Ar.IsTrans() )
	{
		Ar.Preload( Polys );
	}

	Ar << LightMap << LightBits << Bounds << LeafHulls << Leaves << Lights;
	if( Ar.Ver()<=61 )//oldver
	{
		UObject* Tmp=NULL;
		Ar << Tmp << Tmp;
	}
	Ar << RootOutside << Linked;

	unguard;
}
void UModel::PostLoad()
{
	guard(UModel::PostLoad);
	for( INT i=0; i<Nodes.Num(); i++ )
		Surfs(Nodes(i).iSurf).Nodes.AddItem(i);
	Super::PostLoad();
	unguard;
}
void UModel::ModifySurf( INT Index, INT UpdateMaster )
{
	guard(UModel::ModifySurf);

	Surfs.ModifyItem( Index );
	FBspSurf& Surf = Surfs(Index);
	if( UpdateMaster && Surf.Actor )
		Surf.Actor->Brush->Polys->Element.ModifyItem( Surf.iBrushPoly );

	unguard;
}
void UModel::ModifyAllSurfs( INT UpdateMaster )
{
	guard(UModel::ModifyAllSurfs);

	for( INT i=0; i<Surfs.Num(); i++ )
		ModifySurf( i, UpdateMaster );

	unguard;
}
void UModel::ModifySelectedSurfs( INT UpdateMaster )
{
	guard(UModel::ModifySelectedSurfs);

	for( INT i=0; i<Surfs.Num(); i++ )
		if( Surfs(i).PolyFlags & PF_Selected )
			ModifySurf( i, UpdateMaster );

	unguard;
}
IMPLEMENT_CLASS(UModel);

/*---------------------------------------------------------------------------------------
	UModel implementation.
---------------------------------------------------------------------------------------*/

//
// Lock a model.
//
void UModel::Modify( UBOOL DoTransArrays )
{
	guard(UModel::Modify);

	// Modify all child objects.
	//warning: Don't modify self because model contains a dynamic array.
	if( Polys   ) Polys->Modify();

	unguard;
}

//
// Empty the contents of a model.
//
void UModel::EmptyModel( INT EmptySurfInfo, INT EmptyPolys )
{
	guard(UModel::EmptyModel);

	// Init arrays.
	Nodes		.Empty();
	Bounds		.Empty();
	LeafHulls	.Empty();
	Leaves		.Empty();
	Lights		.Empty();
	LightMap	.Empty();
	LightBits	.Empty();
	Verts		.Empty();
	if( EmptySurfInfo )
	{
		Vectors.Empty();
		Points.Empty();
		for( INT i=0; i<Surfs.Num(); i++ )
			Surfs(i).Decals.Empty();
		Surfs.Empty();
	}
	if( EmptyPolys )
	{
		Polys = new( GetOuter(), NAME_None, RF_Transactional )UPolys;
	}

	// Init variables.
	NumZones		= 0;
	NumSharedSides	= 4;
	NumZones = 0;
	for( INT i=0; i<FBspNode::MAX_ZONES; i++ )
	{
		Zones[i].ZoneActor    = NULL;
		Zones[i].Connectivity = ((QWORD)1)<<i;
		Zones[i].Visibility   = ~(QWORD)0;
	}	

	unguard;
}


//
// Create a new model and allocate all objects needed for it.
//
UModel::UModel( ABrush* Owner, UBOOL InRootOutside )
:	RootOutside	( InRootOutside )
,	Surfs		( this )
,	Vectors		( this )
,	Points		( this )
,	Verts		( this )
,	Nodes		( this )
{
	guard(UModel::UModel);
	SetFlags( RF_Transactional );
	EmptyModel( 1, 1 );
	if( Owner )
	{
		Owner->Brush = this;
		Owner->InitPosRotScale();
	}
	unguard;
}

//
// Build the model's bounds (min and max).
//
void UModel::BuildBound()
{
	guard(UModel::BuildBound);
	if( Polys && Polys->Element.Num() )
	{
		TArray<FVector> Points;
		for( INT i=0; i<Polys->Element.Num(); i++ )
			for( INT j=0; j<Polys->Element(i).NumVertices; j++ )
				Points.AddItem(Polys->Element(i).Vertex[j]);
		BoundingBox    = FBox( &Points(0), Points.Num() );
		BoundingSphere = FSphere( &Points(0), Points.Num() );
	}
	else BoundingBox = FBox(0);
	unguard;
}

//
// Transform this model by its coordinate system.
//
void UModel::Transform( ABrush* Owner )
{
	guard(UModel::Transform);
	check(Owner);

	Polys->Element.ModifyAllItems();

	FModelCoords Coords;
	FLOAT Orientation = Owner->BuildCoords( &Coords, NULL );
	for( INT i=0; i<Polys->Element.Num(); i++ )
		Polys->Element( i ).Transform( Coords, Owner->PrePivot, Owner->Location, Orientation );

	unguard;
}

//
// Returns whether a BSP leaf is potentially visible from another leaf.
//
UBOOL UModel::PotentiallyVisible( INT iLeaf1, INT iLeaf2 )
{
	// This is the amazing superfast patent-pending 1 cpu cycle potential visibility 
	// algorithm programmed by the great Tim Sweeney!
	return 1;
}

/*---------------------------------------------------------------------------------------
	UModel basic implementation.
---------------------------------------------------------------------------------------*/

//
// Shrink all stuff to its minimum size.
//
void UModel::ShrinkModel()
{
	guard(UModel::ShrinkModel);

	Vectors		.Shrink();
	Points		.Shrink();
	Verts		.Shrink();
	Nodes		.Shrink();
	Surfs		.Shrink();
	if( Polys     ) Polys    ->Element.Shrink();
	Bounds		.Shrink();
	LeafHulls	.Shrink();

	unguard;
}

/*---------------------------------------------------------------------------------------
	The End.
---------------------------------------------------------------------------------------*/
