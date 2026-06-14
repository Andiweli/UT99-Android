/*=============================================================================
	UnEdCsg.cpp: High-level CSG tracking functions for editor
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EditorPrivate.h"

//
// Globals:
//!!wastes 128K, fixed size
//
BYTE GFlags1 [MAXWORD+1]; // For fast polygon selection
BYTE GFlags2 [MAXWORD+1];

/*-----------------------------------------------------------------------------
	Level brush tracking.
-----------------------------------------------------------------------------*/

//
// Prepare a moving brush.
//
void UEditorEngine::csgPrepMovingBrush( ABrush* Actor )
{
	guard(UEditorEngine::csgPrepMovingBrush);
	check(Actor);
	check(Actor->Brush);
	check(Actor->Brush->RootOutside);
	debugf( NAME_Log, TEXT("Preparing brush %s"), Actor->GetName() );

	// Allocate tables
	Actor->Brush->EmptyModel( 1, 0 );

	// Build bounding box.
	Actor->Brush->BuildBound();

	// Build BSP for the brush.
	bspBuild( Actor->Brush, BSP_Good, 15, 1, 0 );
	bspRefresh( Actor->Brush, 1 );
	bspBuildBounds( Actor->Brush );

	unguard;
}

//
// Duplicate the specified brush and make it into a CSG-able level brush.
// Returns new brush, or NULL if the original was empty.
//
void UEditorEngine::csgCopyBrush
(
	ABrush*		Dest,
	ABrush*		Src,
	DWORD		PolyFlags, 
	DWORD		ResFlags,
	UBOOL		IsMovingBrush
)
{
	guard(UEditorEngine::csgCopyBrush);
	check(Src);
	check(Src->Brush);

	// Handle empty brush.
	if( !Src->Brush->Polys->Element.Num() )
	{
		Dest->Brush = NULL;
		return;
	}

	// Duplicate the brush and its polys.
	Dest->PolyFlags				= PolyFlags;
	Dest->Brush					= new( Src->Brush->GetOuter(), NAME_None, ResFlags )UModel( NULL, Src->Brush->RootOutside );
	Dest->Brush->Polys			= new( Src->Brush->GetOuter(), NAME_None, ResFlags )UPolys;
	check(Dest->Brush->Polys->Element.GetOwner()==Dest->Brush->Polys);
	Dest->Brush->Polys->Element = Src->Brush->Polys->Element;
	check(Dest->Brush->Polys->Element.GetOwner()==Dest->Brush->Polys);

	// Update poly textures.
	for( INT i=0; i<Dest->Brush->Polys->Element.Num(); i++ )
		Dest->Brush->Polys->Element(i).iBrushPoly = INDEX_NONE;

	// Copy positioning, and build bounding box.
	Dest->CopyPosRotScaleFrom( Src );

	// If it's a moving brush, prep it.
	if( IsMovingBrush )
		csgPrepMovingBrush( Dest );

	unguard;
}

//
// Add a brush to the list of CSG brushes in the level, using a CSG operation, and return 
// a newly-created copy of it.
//
ABrush* UEditorEngine::csgAddOperation
(
	ABrush*		Actor,
	ULevel*		Level,
	DWORD		PolyFlags,
	ECsgOper	CsgOper
)
{
	guard(UEditorEngine::csgAddOperation);
	check(Actor);
	check(Actor->Brush);
	check(Actor->Brush->Polys);

	// Can't do this if brush has no polys.
	if( !Actor->Brush->Polys->Element.Num() )
		return NULL;

	// Spawn a new actor for the brush.
	ABrush* Result  = Level->SpawnBrush();
	Result->SetFlags( RF_NotForClient | RF_NotForServer );

	// Duplicate the brush.
	csgCopyBrush
	(
		Result,
		Actor,
		PolyFlags,
		RF_NotForClient | RF_NotForServer | RF_Transactional,
		0
	);
	check(Result->Brush);

	// Set add-info.
	Result->CsgOper = CsgOper;

	return Result;
	unguard;
}

const TCHAR* UEditorEngine::csgGetName( ECsgOper CSG )
{
	guard(UEditorEngine::csgGetName);
	return *(FindObjectChecked<UEnum>( ANY_PACKAGE, TEXT("ECsgOper") ) )->Names(CSG);
	unguard;
}

/*-----------------------------------------------------------------------------
	CSG Rebuilding.
-----------------------------------------------------------------------------*/

//
// Rebuild the level's Bsp from the level's CSG brushes
//
// Note: Needs to be expanded to defragment Bsp polygons as needed (by rebuilding
// the Bsp), so that it doesn't slow down to a crawl on complex levels.
//
#if 0
void UEditorEngine::csgRebuild( ULevel* Level )
{
	guard(UEditorEngine::csgRebuild);

	INT NodeCount,PolyCount,LastPolyCount;
	TCHAR TempStr[80];

	BeginSlowTask( "Rebuilding geometry", 1, 0 );
	FastRebuild = 1;

	FinishAllSnaps(Level);

	// Empty the model out.
	Level->Lock( LOCK_Trans );
	Level->Model->EmptyModel( 1, 1 );
	Level->Unlock( LOCK_Trans );

	// Count brushes.
	int BrushTotal=0, BrushCount=0;
	for( FStaticBrushIterator TempIt(Level); TempIt; ++TempIt )
		if( *TempIt != Level->Brush() )
			BrushTotal++;

	LastPolyCount = 0;
	for( FStaticBrushIterator It(Level); It; ++It )
	{
		if( *It == Level->Brush() )
			continue;
		BrushCount++;
		appSprintf(TempStr,"Applying brush %i of %i",BrushCount,BrushTotal);
		StatusUpdate( TempStr, BrushCount, BrushTotal );

		// See if the Bsp has become badly fragmented and, if so, rebuild.
		PolyCount = Level->Model->Surfs->Num();
		NodeCount = Level->Model->Nodes->Num();
		if( PolyCount>2000 && PolyCount>=3*LastPolyCount )
		{
			appStrcat( TempStr, ": Refreshing Bsp..." );
			StatusUpdate( TempStr, BrushCount, BrushTotal );

			debugf 				( NAME_Log, "Map: Rebuilding Bsp" );
			bspBuildFPolys		( Level->Model, 1, 0 );
			bspMergeCoplanars	( Level->Model, 0, 0 );
			bspBuild			( Level->Model, BSP_Lame, 25, 0, 0 );
			debugf				( NAME_Log, "Map: Reduced nodes by %i%%, polys by %i%%", (100*(NodeCount-Level->Model->Nodes->Num()))/NodeCount,(100*(PolyCount-Level->Model->Surfs->Num()))/PolyCount );

			LastPolyCount = Level->Model->Surfs->Num();
		}

		// Perform this CSG operation.
		bspBrushCSG( *It, Level->Model, It->PolyFlags, (ECsgOper)It->CsgOper, 0 );
	}

	// Build bounding volumes.
	Level->Lock( LOCK_Trans );
	bspBuildBounds( Level->Model );
	Level->Unlock( LOCK_Trans );

	// Done.
	FastRebuild = 0;
	EndSlowTask();
	unguard;
}
#endif

#if 1

//
// Repartition the bsp.
//
void UEditorEngine::bspRepartition( UModel* Model, INT iNode, INT Simple )
{
	guard(UEditorEngine::bspRepartition);

	bspBuildFPolys( Level->Model, 1, iNode );
	bspMergeCoplanars( Level->Model, 0, 0 );
	bspBuild( Level->Model, BSP_Good, 12, Simple, iNode );
	bspRefresh( Level->Model, 1 );

	unguard;
}

//
// Build list of leaves.
//
static void EnlistLeaves( UModel* Model, TArray<INT>& iFronts, TArray<INT>& iBacks, INT iNode )
{
	guard(EnlistLeaves);
	FBspNode& Node=Model->Nodes(iNode);

	if( Node.iFront==INDEX_NONE ) iFronts.AddItem(iNode);
	else EnlistLeaves( Model, iFronts, iBacks, Node.iFront );

	if( Node.iBack==INDEX_NONE ) iBacks.AddItem(iNode);
	else EnlistLeaves( Model, iFronts, iBacks, Node.iBack );

	unguard;
}

//
// Rebuild the level's Bsp from the level's CSG brushes.
//
void UEditorEngine::csgRebuild( ULevel* Level )
{
	guard(UEditorEngine::csgRebuild);
	FStaticBrushIterator It(Level);

	GWarn->BeginSlowTask( TEXT("Rebuilding geometry"), 1, 0 );
	FastRebuild = 1;

	FinishAllSnaps(Level);

	// Empty the model out.
	Level->Model->EmptyModel( 1, 1 );

	// Count brushes.
	INT BrushTotal=0, BrushCount=0;
	for( FStaticBrushIterator TempIt(Level); TempIt; ++TempIt )
		if( *TempIt != Level->Brush() )
			BrushTotal++;

	// Compose all structural brushes and portals.
	for( FStaticBrushIterator It(Level); It; ++It )
	{
		if( *It!=Level->Brush() )
		{
			if
			(  !(It->PolyFlags&PF_Semisolid)
			||	(It->CsgOper!=CSG_Add)
			||	(It->PolyFlags&PF_Portal) )
			{
				// Treat portals as solids for cutting.
				if( It->PolyFlags & PF_Portal )
					It->PolyFlags = (It->PolyFlags & ~PF_Semisolid) | PF_NotSolid;
				BrushCount++;
				GWarn->StatusUpdatef( BrushCount, BrushTotal, TEXT("Applying structural brush %i of %i"), BrushCount, BrushTotal );
				bspBrushCSG( *It, Level->Model, It->PolyFlags, (ECsgOper)It->CsgOper, 0 );
			}
		}
	}

	// Repartition the structural BSP.
	bspRepartition( Level->Model, 0, 0 );
	TestVisibility( Level, Level->Model, 0, 0 );

	// Remember leaves.
	TArray<INT> iFronts, iBacks;
	if( Level->Model->Nodes.Num() )
		EnlistLeaves( Level->Model, iFronts, iBacks, 0 );

	// Compose all detail brushes.
	for( It=FStaticBrushIterator(Level); It; ++It )
	{
		if
		(	*It!=Level->Brush()
		&&	(It->PolyFlags&PF_Semisolid)
		&& !(It->PolyFlags&PF_Portal)
		&&	It->CsgOper==CSG_Add )
		{
			BrushCount++;
			GWarn->StatusUpdatef( BrushCount, BrushTotal, TEXT("Applying detail brush %i of %i"), BrushCount, BrushTotal );
			bspBrushCSG( *It, Level->Model, It->PolyFlags, (ECsgOper)It->CsgOper, 0 );
		}
	}

	// Optimize the sub-bsp's.
	INT iNode;
	for( TArray<INT>::TIterator ItF(iFronts); ItF; ++ItF )
		if( (iNode=Level->Model->Nodes(*ItF).iFront)!=INDEX_NONE )
			bspRepartition( Level->Model, iNode, 2 );
	for( TArray<INT>::TIterator ItB(iBacks); ItB; ++ItB )
		if( (iNode=Level->Model->Nodes(*ItB).iBack)!=INDEX_NONE )
			bspRepartition( Level->Model, iNode, 2 );

	// Build bounding volumes.
	bspOptGeom( Level->Model );
	bspBuildBounds( Level->Model );

	// Done.
	FastRebuild = 0;
	GWarn->EndSlowTask();
	unguard;
}
#endif

/*---------------------------------------------------------------------------------------
	Flag setting and searching
---------------------------------------------------------------------------------------*/

//
// Sets and clears all Bsp node flags.  Affects all nodes, even ones that don't
// really exist.
//
void UEditorEngine::polySetAndClearPolyFlags(UModel *Model, DWORD SetBits, DWORD ClearBits,int SelectedOnly, int UpdateMaster)
{
	guard(UEditorEngine::polySetAndClearPolyFlags);
	for( INT i=0; i<Model->Surfs.Num(); i++ )
	{
		FBspSurf& Poly = Model->Surfs(i);
		if( !SelectedOnly || (Poly.PolyFlags & PF_Selected) )
		{
			DWORD NewFlags = (Poly.PolyFlags & ~ClearBits) | SetBits;
			if( NewFlags != Poly.PolyFlags )
			{
				Model->ModifySurf( i, UpdateMaster );
				Poly.PolyFlags = NewFlags;
				if( UpdateMaster )
					polyUpdateMaster( Model, i, 0, 0 );
			}
		}
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	Polygon searching
-----------------------------------------------------------------------------*/

//
// Find the Brush EdPoly corresponding to a given Bsp surface.
//
int UEditorEngine::polyFindMaster(UModel *Model, INT iSurf, FPoly &Poly)
{
	guard(UEditorEngine::polyFindMaster);

	FBspSurf &Surf = Model->Surfs(iSurf);
	if( !Surf.Actor )
	{
		return 0;
	}
	else
	{
		Poly = Surf.Actor->Brush->Polys->Element(Surf.iBrushPoly);
		return 1;
	}
	unguard;
}

//
// Update a the master brush EdPoly corresponding to a newly-changed
// poly to reflect its new properties.
//
// Doesn't do any transaction tracking.  Assumes you've called transSelectedBspSurfs.
//
void UEditorEngine::polyUpdateMaster
(
	UModel*	Model,
	INT  	iSurf,
	INT		UpdateTexCoords,
	INT		UpdateBase
)
{
	guard(UEditorEngine::polyUpdateMaster);

	FBspSurf &Poly = Model->Surfs(iSurf);
	if( !Poly.Actor )
		return;

	FModelCoords Uncoords;
	if( UpdateTexCoords || UpdateBase )
		Poly.Actor->BuildCoords( NULL, &Uncoords );

	for( INT iEdPoly = Poly.iBrushPoly; iEdPoly < Poly.Actor->Brush->Polys->Element.Num(); iEdPoly++ )
	{
		FPoly& MasterEdPoly = Poly.Actor->Brush->Polys->Element(iEdPoly);
		if( iEdPoly==Poly.iBrushPoly || MasterEdPoly.iLink==Poly.iBrushPoly )
		{
			Poly.Actor->Brush->Polys->Element.ModifyItem( iEdPoly );

			MasterEdPoly.Texture   = Poly.Texture;
			MasterEdPoly.PanU      = Poly.PanU;
			MasterEdPoly.PanV      = Poly.PanV;
			MasterEdPoly.PolyFlags = Poly.PolyFlags & ~(PF_NoEdit);

			if( UpdateTexCoords || UpdateBase )
			{
				if( UpdateTexCoords )
				{
					MasterEdPoly.TextureU = Model->Vectors(Poly.vTextureU).TransformVectorBy(Uncoords.VectorXform);
					MasterEdPoly.TextureV = Model->Vectors(Poly.vTextureV).TransformVectorBy(Uncoords.VectorXform);
				}
				if( UpdateBase )
				{
					MasterEdPoly.Base
					=	(Model->Points(Poly.pBase) - Poly.Actor->Location)
					.	TransformVectorBy(Uncoords.PointXform)
					+	Poly.Actor->PrePivot;
				}
			}
		}
	}
	unguard;
}

//
// Find all Bsp polys with flags such that SetBits are clear or ClearBits are set.
//
void UEditorEngine::polyFindByFlags(UModel *Model, DWORD SetBits, DWORD ClearBits, POLY_CALLBACK Callback)
	{
	guard(UEditorEngine::polyFindByFlags);
	FBspSurf *Poly = &Model->Surfs(0);
	//
	for (INT i=0; i<Model->Surfs.Num(); i++)
		{
		if (((Poly->PolyFlags&SetBits)!=0) || ((Poly->PolyFlags&~ClearBits)!=0))
			{
			Callback (Model,i);
			};
		Poly++;
		};
	unguard;
	};

//
// Find all BspSurfs corresponding to a particular editor brush object
// and polygon index. Call with BrushPoly set to INDEX_NONE to find all Bsp 
// polys corresponding to the Brush.
//
void UEditorEngine::polyFindByBrush( UModel* Model, ABrush* Actor, INT iBrushPoly, POLY_CALLBACK Callback )
	{
	guard(UEditorEngine::polyFindByBrush);
	for (INT i=0; i<Model->Surfs.Num(); i++)
		{
		FBspSurf &Poly = Model->Surfs(i);
		if (
			(Poly.Actor == Actor) && 
			((iBrushPoly == INDEX_NONE) || (Poly.iBrushPoly == iBrushPoly))
			)
			{
			Callback (Model,i);
			};
		};
	unguard;
	};

/*-----------------------------------------------------------------------------
   All transactional polygon selection functions
-----------------------------------------------------------------------------*/

void UEditorEngine::polyResetSelection(UModel *Model)
	{
	guard(UEditorEngine::polyResetSelection);
	for (INT i=0; i<Model->Surfs.Num(); i++)
		{
		FBspSurf *Poly = &Model->Surfs(i);
		Poly->PolyFlags |= ~(PF_Selected | PF_Memorized);
		Poly++;
		};
	unguard;
	};

void UEditorEngine::polySelectAll(UModel *Model)
	{
	guard(UEditorEngine::polySelectAll);
	polySetAndClearPolyFlags(Model,PF_Selected,0,0,0);
	unguard;
	};

void UEditorEngine::polySelectMatchingGroups( UModel* Model )
{
	guard(UEditorEngine::polySelectMatchingGroups);
	INT i;

	appMemzero( GFlags1, sizeof(GFlags1) );
	for( INT i=0; i<Model->Surfs.Num(); i++ )
	{
		FBspSurf *Surf = &Model->Surfs(i);
		if( Surf->PolyFlags&PF_Selected )
		{
			FPoly Poly; polyFindMaster(Model,i,Poly);
			GFlags1[Poly.Actor->Group.GetIndex()]=1;
		}
	}
	for( i=0; i<Model->Surfs.Num(); i++ )
	{
		FBspSurf *Surf = &Model->Surfs(i);
		FPoly Poly; polyFindMaster(Model,i,Poly);
		if
		(	(GFlags1[Poly.Actor->Group.GetIndex()]) 
		&&	(!(Surf->PolyFlags & PF_Selected)) )
			{
			Model->ModifySurf( i, 0 );
			Surf->PolyFlags |= PF_Selected;
			};
	}
	unguard;
}

void UEditorEngine::polySelectMatchingItems(UModel *Model)
{
	guard(UEditorEngine::polySelectMatchingItems);
	INT i;

	appMemzero(GFlags1,sizeof(GFlags1));
	appMemzero(GFlags2,sizeof(GFlags2));

	for( INT i=0; i<Model->Surfs.Num(); i++ )
	{
		FBspSurf *Surf = &Model->Surfs(i);
		if( Surf->Actor )
		{
			if( Surf->PolyFlags & PF_Selected )
				GFlags2[Surf->Actor->Brush->GetIndex()]=1;
		}
		if( Surf->PolyFlags&PF_Selected )
		{
			FPoly Poly; polyFindMaster(Model,i,Poly);
			GFlags1[Poly.ItemName.GetIndex()]=1;
		}
	}
	for( i=0; i<Model->Surfs.Num(); i++ )
	{
		FBspSurf *Surf = &Model->Surfs(i);
		if( Surf->Actor )
		{
			FPoly Poly; polyFindMaster(Model,i,Poly);
			if ((GFlags1[Poly.ItemName.GetIndex()]) &&
				( GFlags2[Surf->Actor->Brush->GetIndex()]) &&
				(!(Surf->PolyFlags & PF_Selected)))
			{
				Model->ModifySurf( i, 0 );
				Surf->PolyFlags |= PF_Selected;
			}
		}
	}
	unguard;
}

enum EAdjacentsType
{
	ADJACENT_ALL,		// All adjacent polys
	ADJACENT_COPLANARS,	// Adjacent coplanars only
	ADJACENT_WALLS,		// Adjacent walls
	ADJACENT_FLOORS,	// Adjacent floors or ceilings
	ADJACENT_SLANTS,	// Adjacent slants
};

//
// Select all adjacent polygons (only coplanars if Coplanars==1) and
// return number of polygons newly selected.
//
int TagAdjacentsType(UModel *Model, EAdjacentsType AdjacentType)
	{
	guard(TagAdjacentsType);
	FVert	*VertPool;
	FVector		*Base,*Normal;
	BYTE		b;
	INT		    i;
	int			Selected,Found;
	//
	Selected = 0;
	appMemzero( GFlags1, sizeof(GFlags1) );
	//
	// Find all points corresponding to selected vertices:
	//
	for (i=0; i<Model->Nodes.Num(); i++)
		{
		FBspNode &Node = Model->Nodes(i);
		FBspSurf &Poly = Model->Surfs(Node.iSurf);
		if (Poly.PolyFlags & PF_Selected)
			{
			VertPool = &Model->Verts(Node.iVertPool);
			//
			for (b=0; b<Node.NumVertices; b++) GFlags1[(VertPool++)->pVertex] = 1;
			};
		};
	//
	// Select all unselected nodes for which two or more vertices are selected:
	//
	for (i=0; i<Model->Nodes.Num(); i++)
		{
		FBspNode &Node = Model->Nodes(i);
		FBspSurf &Poly = Model->Surfs(Node.iSurf);
		if (!(Poly.PolyFlags & PF_Selected))
			{
			Found    = 0;
			VertPool = &Model->Verts(Node.iVertPool);
			//
			Base   = &Model->Points (Poly.pBase);
			Normal = &Model->Vectors(Poly.vNormal);
			//
			for (b=0; b<Node.NumVertices; b++) Found += GFlags1[(VertPool++)->pVertex];
			//
			if (AdjacentType == ADJACENT_COPLANARS)
				{
				if (!GFlags2[Node.iSurf]) Found=0;
				}
			else if (AdjacentType == ADJACENT_FLOORS)
				{
				if (Abs(Normal->Z) <= 0.85) Found = 0;
				}
			else if (AdjacentType == ADJACENT_WALLS)
				{
				if (Abs(Normal->Z) >= 0.10) Found = 0;
				}
			else if (AdjacentType == ADJACENT_SLANTS)
				{
				if (Abs(Normal->Z) > 0.85) Found = 0;
				if (Abs(Normal->Z) < 0.10) Found = 0;
				};
			if (Found > 0)
			{
				Model->ModifySurf( Node.iSurf, 0 );
				Poly.PolyFlags |= PF_Selected;
				Selected++;
			}
		}
	}
	return Selected;
	unguard;
	};

void TagCoplanars(UModel *Model)
	{
	guard(TagCoplanars);
	FBspSurf	*SelectedPoly,*Poly;
	FVector		*SelectedBase,*SelectedNormal,*Base,*Normal;
	//
	appMemzero(GFlags2,sizeof(GFlags2));
	//
	for (INT i=0; i<Model->Surfs.Num(); i++)
		{
		SelectedPoly = &Model->Surfs(i);
		if (SelectedPoly->PolyFlags & PF_Selected)
			{
			SelectedBase   = &Model->Points (SelectedPoly->pBase);
			SelectedNormal = &Model->Vectors(SelectedPoly->vNormal);
			//
			for (INT j=0; j<Model->Surfs.Num(); j++)
				{
				Poly = &Model->Surfs(j);
				Base   = &Model->Points (Poly->pBase);
				Normal = &Model->Vectors(Poly->vNormal);
				//
				if (FCoplanar(*Base,*Normal,*SelectedBase,*SelectedNormal) && (!(Poly->PolyFlags & PF_Selected)))
					{
					GFlags2[j]=1;
					};
				};
			};
		};
	unguard;
	};

void UEditorEngine::polySelectAdjacents(UModel *Model)
	{
	guard(UEditorEngine::polySelectAdjacents);
	do {} while (TagAdjacentsType (Model,ADJACENT_ALL) > 0);
	unguard;
	};

void UEditorEngine::polySelectCoplanars(UModel *Model)
	{
	guard(UEditorEngine::polySelectCoplanars);
	TagCoplanars(Model);
	do {} while (TagAdjacentsType(Model,ADJACENT_COPLANARS) > 0);
	unguard;
	};

void UEditorEngine::polySelectMatchingBrush(UModel *Model)
	{
	guard(UEditorEngine::polySelectMatchingBrush);
	INT i;
	//
	appMemzero( GFlags1, sizeof(GFlags1) );
	//
	for (INT i=0; i<Model->Surfs.Num(); i++)
		{
		FBspSurf *Poly = &Model->Surfs(i);
		if( Poly->Actor->Brush )
			if( Poly->PolyFlags & PF_Selected )
				GFlags1[Poly->Actor->Brush->GetIndex()]=1;
		Poly++;
		};
	for (i=0; i<Model->Surfs.Num(); i++)
		{
		FBspSurf *Poly = &Model->Surfs(i);
		if( Poly->Actor->Brush )
			{
			if ((GFlags1[Poly->Actor->Brush->GetIndex()])&&(!(Poly->PolyFlags&PF_Selected)))
				{
				Model->ModifySurf( i, 0 );
				Poly->PolyFlags |= PF_Selected;
				};
			};
		Poly++;
		};
	unguard;
	};

void UEditorEngine::polySelectMatchingTexture(UModel *Model)
	{
	guard(UEditorEngine::polySelectMatchingTexture);
	INT		i,Blank=0;
	appMemzero( GFlags1, sizeof(GFlags1) );
	//
	for (i=0; i<Model->Surfs.Num(); i++)
		{
		FBspSurf *Poly = &Model->Surfs(i);
		if (Poly->Texture && (Poly->PolyFlags&PF_Selected)) GFlags1[Poly->Texture->GetIndex()]=1;
		else if (!Poly->Texture) Blank=1;
		Poly++;
		};
	for (i=0; i<Model->Surfs.Num(); i++)
		{
		FBspSurf *Poly = &Model->Surfs(i);
		if (Poly->Texture && (GFlags1[Poly->Texture->GetIndex()]) && (!(Poly->PolyFlags&PF_Selected)))
			{
			Model->ModifySurf( i, 0 );
			Poly->PolyFlags |= PF_Selected;
			}
		else if (Blank & !Poly->Texture) Poly->PolyFlags |= PF_Selected;
		Poly++;
		};
	unguard;
	};

void UEditorEngine::polySelectAdjacentWalls(UModel *Model)
	{
	guard(UEditorEngine::polySelectAdjacentWalls);
	do {} while (TagAdjacentsType  (Model,ADJACENT_WALLS) > 0);
	unguard;
	};

void UEditorEngine::polySelectAdjacentFloors(UModel *Model)
	{
	guard(UEditorEngine::polySelectAdjacentFloors);
	do {} while (TagAdjacentsType (Model,ADJACENT_FLOORS) > 0);
	unguard;
	};

void UEditorEngine::polySelectAdjacentSlants(UModel *Model)
	{
	guard(UEditorEngine::polySelectAdjacentSlants);
	do {} while (TagAdjacentsType  (Model,ADJACENT_SLANTS) > 0);
	unguard;
	};

void UEditorEngine::polySelectReverse(UModel *Model)
	{
	guard(UEditorEngine::polySelectReverse);
	for (INT i=0; i<Model->Surfs.Num(); i++)
		{
		FBspSurf *Poly = &Model->Surfs(i);
		Model->ModifySurf( i, 0 );
		Poly->PolyFlags ^= PF_Selected;
		//
		Poly++;
		};
	unguard;
	};

void UEditorEngine::polyMemorizeSet(UModel *Model)
	{
	guard(UEditorEngine::polyMemorizeSet);
	for (INT i=0; i<Model->Surfs.Num(); i++)
		{
		FBspSurf *Poly = &Model->Surfs(i);
		if (Poly->PolyFlags & PF_Selected) 
			{
			if (!(Poly->PolyFlags & PF_Memorized))
				{
				Model->ModifySurf( i, 0 );
				Poly->PolyFlags |= (PF_Memorized);
				};
			}
		else
			{
			if (Poly->PolyFlags & PF_Memorized)
				{
				Model->ModifySurf( i, 0 );
				Poly->PolyFlags &= (~PF_Memorized);
				};
			};
		Poly++;
		};
	unguard;
	};

void UEditorEngine::polyRememberSet(UModel *Model)
	{
	guard(UEditorEngine::polyRememberSet);
	for (INT i=0; i<Model->Surfs.Num(); i++)
		{
		FBspSurf *Poly = &Model->Surfs(i);
		if (Poly->PolyFlags & PF_Memorized) 
			{
			if (!(Poly->PolyFlags & PF_Selected))
				{
				Model->ModifySurf( i, 0 );
				Poly->PolyFlags |= (PF_Selected);
				};
			}
		else
			{
			if (Poly->PolyFlags & PF_Selected)
				{
				Model->ModifySurf( i, 0 );
				Poly->PolyFlags &= (~PF_Selected);
				};
			};
		Poly++;
		};
	unguard;
	};

void UEditorEngine::polyXorSet(UModel *Model)
	{
	int			Flag1,Flag2;
	//
	guard(UEditorEngine::polyXorSet);
	for (INT i=0; i<Model->Surfs.Num(); i++)
		{
		FBspSurf *Poly = &Model->Surfs(i);
		Flag1 = (Poly->PolyFlags & PF_Selected ) != 0;
		Flag2 = (Poly->PolyFlags & PF_Memorized) != 0;
		//
		if (Flag1 ^ Flag2)
			{
			if (!(Poly->PolyFlags & PF_Selected))
				{
				Model->ModifySurf( i, 0 );
				Poly->PolyFlags |= PF_Selected;
				};
			}
		else
			{
			if (Poly->PolyFlags & PF_Selected)
				{
				Model->ModifySurf( i, 0 );
				Poly->PolyFlags &= (~PF_Selected);
				};
			};
		Poly++;
		};
	unguard;
	};

void UEditorEngine::polyUnionSet(UModel *Model)
	{
	guard(UEditorEngine::polyUnionSet);
	for (INT i=0; i<Model->Surfs.Num(); i++)
		{
		FBspSurf *Poly = &Model->Surfs(i);
		if (!(Poly->PolyFlags & PF_Memorized))
			{
			if (Poly->PolyFlags | PF_Selected)
				{
				Model->ModifySurf( i, 0 );
				Poly->PolyFlags &= (~PF_Selected);
				};
			};
		Poly++;
		};
	unguard;
	};

void UEditorEngine::polyIntersectSet(UModel *Model)
	{
	guard(UEditorEngine::polyIntersectSet);
	for (INT i=0; i<Model->Surfs.Num(); i++)
		{
		FBspSurf *Poly = &Model->Surfs(i);
		if ((Poly->PolyFlags & PF_Memorized) && !(Poly->PolyFlags & PF_Selected))
			{
			Poly->PolyFlags |= PF_Selected;
			};
		Poly++;
		};
	unguard;
	};

#if 1 //LEGEND
void UEditorEngine::polySelectZone( UModel* Model )
{
	guard(UEditorEngine::polySelectZone);
	INT i;

	// identify the list of currently selected zones
	TArray<INT> iZoneList;
	for( INT i = 0; i < Model->Nodes.Num(); i++ )
	{
		FBspNode* Node = &Model->Nodes(i);
		FBspSurf* Poly = &Model->Surfs( Node->iSurf );
		if( Poly->PolyFlags & PF_Selected )
		{
			if( Node->iZone[1] != 0 )
				iZoneList.AddUniqueItem( Node->iZone[1] ); //front zone
			if( Node->iZone[0] != 0 )
				iZoneList.AddUniqueItem( Node->iZone[0] ); //back zone
		}
	}

	// select all polys that are match one of the zones identified above
	for( i = 0; i < Model->Nodes.Num(); i++ )
	{
		FBspNode* Node = &Model->Nodes(i);
		for( INT j = 0; j < iZoneList.Num(); j++ ) 
		{
			if( Node->iZone[1] == iZoneList(j) || Node->iZone[0] == iZoneList(j) )
			{
				FBspSurf* Poly = &Model->Surfs( Node->iSurf );
				Poly->PolyFlags |= PF_Selected;
			}
		}
	}
	unguard;
}
#endif

/*---------------------------------------------------------------------------------------
   Brush selection functions
---------------------------------------------------------------------------------------*/

//
// Generic selection routines
//

typedef int (*BRUSH_SEL_FUNC)( ABrush* Brush, int Tag );

void MapSelect( ULevel* Level, BRUSH_SEL_FUNC Func, int Tag )
{
	guard(MapSelect);
	for( FStaticBrushIterator It(Level); It; ++It )
	{
		ABrush* Actor = *It;
		if( Func( Actor, Tag ) )
		{
			// Select it.
			if( !Actor->bSelected )
			{
				Actor->Modify();
				Actor->bSelected = 1;
			}
		}
		else
		{
			// Deselect it.
			if( Actor->bSelected )
			{
				Actor->Modify();
				Actor->bSelected = 0;
			}
		}
	}
	unguard;
}

//
// Select none
//
static int BrushSelectNoneFunc( ABrush* Actor, int Tag )
{
	return 0;
}

//
// Select by CSG operation
//
int BrushSelectOperationFunc( ABrush* Actor, int Tag )
{
	return ((ECsgOper)Actor->CsgOper == Tag) && !(Actor->PolyFlags & (PF_NotSolid | PF_Semisolid));
}
void UEditorEngine::mapSelectOperation(ULevel *Level,ECsgOper CsgOper)
{
	guard(UEditorEngine::mapSelectOperation);
	MapSelect( Level, BrushSelectOperationFunc, CsgOper );
	unguard;
}

int BrushSelectFlagsFunc( ABrush* Actor, int Tag )
{
	return Actor->PolyFlags & Tag;
}
void UEditorEngine::mapSelectFlags(ULevel *Level,DWORD Flags)
	{
	guard(UEditorEngine::mapSelectFlags);					   
	MapSelect( Level, BrushSelectFlagsFunc, (int)Flags );
	unguard;
	};

//
// Select first.
//
void UEditorEngine::mapSelectFirst( ULevel *Level )
{
	guard(UEditorEngine::mapSelectFirst);

	MapSelect( Level, BrushSelectNoneFunc, 0 );
	for( FStaticBrushIterator It(Level); It; ++It )
	{
		if( *It != Level->Brush() )
		{
			It->Modify();
			It->bSelected = 1;
			break;
		}
	}
	unguard;
}

//
// Select last.
//
void UEditorEngine::mapSelectLast( ULevel *Level )
{
	guard(UEditorEngine::mapSelectLast);

	MapSelect( Level, BrushSelectNoneFunc, 0 );
	
	ABrush* Found=NULL;
	for( FStaticBrushIterator It(Level); It; ++It )
		Found = *It;

	if( Found )
	{
		Found->Modify();
		Found->bSelected = 1;
	}
	unguard;
}

/*---------------------------------------------------------------------------------------
   Other map brush functions
---------------------------------------------------------------------------------------*/

//
// Put the first selected brush into the current Brush.
//
void UEditorEngine::mapBrushGet( ULevel* Level )
{
	guard(UEditorEngine::mapBrushGet);
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		ABrush* Actor = Cast<ABrush>(Level->Actors(i));
		if( Actor && Actor!=Level->Brush() && Actor->bSelected )
		{
			Level->Brush()->Modify();
			Level->Brush()->Brush->Polys->Element = Actor->Brush->Polys->Element;
			Level->Brush()->CopyPosRotScaleFrom( Actor );
			break;
		}
	}
	unguard;
}

//
// Replace all selected brushes with the current Brush.
//
void UEditorEngine::mapBrushPut( ULevel* Level )
{
	guard(UEditorEngine::mapBrushPut);
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		ABrush* Actor = Cast<ABrush>(Level->Actors(i));
		if( Actor && Actor!=Level->Brush() && Actor->bSelected )
		{
			Actor->Modify();
			Actor->Brush->Polys->Element = Level->Brush()->Brush->Polys->Element;
			Actor->CopyPosRotScaleFrom( Level->Brush() );
		}
	}
	unguard;
}

//
// Generic private routine for send to front / send to back
//
void SendTo( ULevel* Level, int bSendToFirst )
{
	guard(SendTo);
	INT i;
	FMemMark Mark(GMem);

	// Partition.
	TArray<AActor*> Lists[2];
	for( INT i=2; i<Level->Actors.Num(); i++ )
		if( Level->Actors(i) )
			Lists[Level->Actors(i)->bSelected ^ bSendToFirst ^ 1].AddItem( Level->Actors(i) );

	// Refill.
	check(Level->Actors.Num()>=2);
	Level->Actors.Remove(2,Level->Actors.Num()-2);
	for( i=0; i<2; i++ )
		for( INT j=0; j<Lists[i].Num(); j++ )
			Level->Actors.AddItem( Lists[i](j) );

	Mark.Pop();
	unguard;
}

//
// Send all selected brushes in a level to the front of the hierarchy
//
void UEditorEngine::mapSendToFirst( ULevel* Level )
{
	guard(UEditorEngine::mapSendToFirst);
	SendTo( Level, 0 );
	unguard;
}

//
// Send all selected brushes in a level to the back of the hierarchy
//
void UEditorEngine::mapSendToLast( ULevel* Level )
{
	guard(UEditorEngine::mapSendToLast);
	SendTo( Level, 1 );
	unguard;
}

void UEditorEngine::mapSetBrush
(
	ULevel*				Level,
	EMapSetBrushFlags	PropertiesMask,
	_WORD				BrushColor,
	FName				GroupName,
	DWORD				SetPolyFlags,
	DWORD				ClearPolyFlags
)
{
	guard(UEditorEngine::mapSetBrush);
	for( FStaticBrushIterator It(Level); It; ++It )
	{
		if( *It!=Level->Brush() && It->bSelected )
		{
			if( PropertiesMask & MSB_PolyFlags )
			{
				It->Modify();
				It->PolyFlags = (It->PolyFlags & ~ClearPolyFlags) | SetPolyFlags;
			}
		}
	}
	unguard;
}

/*---------------------------------------------------------------------------------------
   Poly texturing operations
---------------------------------------------------------------------------------------*/

//
// Pan textures on selected polys.  Doesn't do transaction tracking.
//
void UEditorEngine::polyTexPan(UModel *Model,int PanU,int PanV,int Absolute)
	{
	guard(UEditorEngine::polyTexPan);
	for (INT i=0; i<Model->Surfs.Num(); i++)
		{
		FBspSurf *Poly = &Model->Surfs(i);
		if (Poly->PolyFlags & PF_Selected)
			{
			if (Absolute)
				{
				Poly->PanU = PanU;
				Poly->PanV = PanV;
				}
			else // Relative
				{
				Poly->PanU += PanU;
				Poly->PanV += PanV;
				};
			polyUpdateMaster (Model,i,0,0);
			};
		Poly++;
		};
	unguard;
	};

//
// Scale textures on selected polys. Doesn't do transaction tracking.
//
void UEditorEngine::polyTexScale( UModel* Model, FLOAT UU, FLOAT UV, FLOAT VU, FLOAT VV, INT Absolute )
{
	guard(UEditorEngine::polyTexScale);

	for( INT i=0; i<Model->Surfs.Num(); i++ )
	{
		FBspSurf *Poly = &Model->Surfs(i);
		if (Poly->PolyFlags & PF_Selected)
		{
			FVector OriginalU = Model->Vectors(Poly->vTextureU);
			FVector OriginalV = Model->Vectors(Poly->vTextureV);

			if( Absolute )
			{
				OriginalU *= 1.0/OriginalU.Size();
				OriginalV *= 1.0/OriginalV.Size();
			}

			// Calc new vectors.
			FVector NewU = OriginalU * UU + OriginalV * UV;
			FVector NewV = OriginalU * VU + OriginalV * VV;

			// Update Bsp poly.
			Poly->vTextureU = bspAddVector (Model,&NewU,0); // Add U vector
			Poly->vTextureV = bspAddVector (Model,&NewV,0); // Add V vector

			// Update generating brush poly.
			polyUpdateMaster( Model, i, 1, 0 );
			Poly->iLightMap = INDEX_NONE;
		}
		Poly++;
	}
	unguard;
}

//
// Align textures on selected polys.  Doesn't do any transaction tracking.
//
void UEditorEngine::polyTexAlign( UModel *Model, ETexAlign TexAlignType, DWORD Texels )
{
	guard(UEditorEngine::polyTexAlign);
	FPoly			EdPoly;
	FVector			Base,Normal,U,V,Temp;
	FModelCoords	Coords,Uncoords;
	FLOAT			Orientation,k;

	for( INT i=0; i<Model->Surfs.Num(); i++ )
	{
		FBspSurf* Poly = &Model->Surfs(i);
		if( Poly->PolyFlags & PF_Selected )
		{
			polyFindMaster( Model, i, EdPoly );
			Normal = Model->Vectors( Poly->vNormal );
			switch( TexAlignType )
			{
				case TEXALIGN_Default:

					Orientation = Poly->Actor->BuildCoords(&Coords,&Uncoords);

					EdPoly.TextureU  = FVector(0,0,0);
					EdPoly.TextureV  = FVector(0,0,0);
					EdPoly.Base      = EdPoly.Vertex[0];
					EdPoly.PanU      = 0;
					EdPoly.PanV      = 0;
					EdPoly.Finalize( 0 );
					EdPoly.Transform( Coords, FVector(0,0,0), FVector(0,0,0), Orientation );

		      		Poly->vTextureU 	= bspAddVector (Model,&EdPoly.TextureU,0);
	      			Poly->vTextureV 	= bspAddVector (Model,&EdPoly.TextureV,0);
					Poly->PanU			= EdPoly.PanU;
					Poly->PanV			= EdPoly.PanV;
					Poly->iLightMap     = INDEX_NONE;

					polyUpdateMaster	(Model,i,1,1);
					break;
				case TEXALIGN_Floor:
					if( Abs(Normal.Z) > 0.05 )
					{
						// Shouldn't change base point, just base U,V.
						Base           	= Model->Points( Poly->pBase );
						Base       		= FVector(0,0,(Base | Normal) / Normal.Z);
			      		Poly->pBase 	= bspAddPoint( Model, &Base, 1 );

						Temp			= FVector(1,0,0);
						Temp			= Temp - Normal * (Temp | Normal);
						Poly->vTextureU	= bspAddVector( Model, &Temp, 0 );

						Temp			= FVector(0,1,0);
						Temp			= Temp - Normal * (Temp | Normal);
						Poly->vTextureV	= bspAddVector( Model, &Temp, 0 );

						Poly->PanU      = 0;
						Poly->PanV      = 0;
						Poly->iLightMap = INDEX_NONE;
					}
					polyUpdateMaster( Model, i, 1, 1 );
					break;
				case TEXALIGN_WallDir:
					if( Abs(Normal.Z)<0.95 )
					{
						U.X = +Normal.Y;
						U.Y = -Normal.X;
						U.Z = 0.0;
						U  *= 1.0/U.Size();
						V   = (U ^ Normal);
						V  *= 1.0/V.Size();

						if( V.Z > 0.0 )
						{
							V *= -1.0;
							U *= -1.0;
						}
						Poly->vTextureU = bspAddVector (Model,&U,0);
						Poly->vTextureV = bspAddVector (Model,&V,0);

						Poly->PanU		= 0;
						Poly->PanV		= 0;
						Poly->iLightMap = INDEX_NONE;

						polyUpdateMaster (Model,i,1,0);
					}
					break;
				case TEXALIGN_WallPan:
					Base = Model->Points (Poly->pBase);
					U    = Model->Vectors(Poly->vTextureU);
					V    = Model->Vectors(Poly->vTextureV);
					if( Abs(Normal.Z)<0.95 && Abs(V.Z)>0.05 )
					{
						k     = -Base.Z/V.Z;
						V    *= k;
						Base += V;
			      		Poly->pBase = bspAddPoint (Model,&Base,1);
						Poly->iLightMap = INDEX_NONE;

						polyUpdateMaster(Model,i,1,1);
					}
					break;
				case TEXALIGN_OneTile:
					Poly->iLightMap = INDEX_NONE;
					polyUpdateMaster (Model,i,1,1);
					break;
			}
		}
		Poly++;
	}
	unguardf(( TEXT("(Type=%i,Texels=%i)"), TexAlignType, Texels ));
}

/*---------------------------------------------------------------------------------------
   Map geometry link topic handler
---------------------------------------------------------------------------------------*/

AUTOREGISTER_TOPIC(TEXT("Map"),MapTopicHandler);
void MapTopicHandler::Get( ULevel* Level, const TCHAR* Item, FOutputDevice& Ar )
{
	guard(MapTopicHandler::Get);

	int NumBrushes  = 0;
	int NumAdd	    = 0;
	int NumSubtract	= 0;
	int NumSpecial  = 0;
	int NumPolys    = 0;

	for( FStaticBrushIterator It(Level); It; ++It )
	{
		NumBrushes++;
		UModel* Brush        = It->Brush;
		UPolys* BrushEdPolys = Brush->Polys;

		if      (It->CsgOper == CSG_Add)		NumAdd++;
		else if (It->CsgOper == CSG_Subtract)	NumSubtract++;
		else									NumSpecial++;

		NumPolys += BrushEdPolys->Element.Num();
	}

	if     ( appStricmp(Item,TEXT("Brushes"       ))==0 ) Ar.Logf(TEXT("%i"),NumBrushes-1);
	else if( appStricmp(Item,TEXT("Add"           ))==0 ) Ar.Logf(TEXT("%i"),NumAdd);
	else if( appStricmp(Item,TEXT("Subtract"      ))==0 ) Ar.Logf(TEXT("%i"),NumSubtract);
	else if( appStricmp(Item,TEXT("Special"       ))==0 ) Ar.Logf(TEXT("%i"),NumSpecial);
	else if( appStricmp(Item,TEXT("AvgPolys"      ))==0 ) Ar.Logf(TEXT("%i"),NumPolys/Max(1,NumBrushes-1));
	else if( appStricmp(Item,TEXT("TotalPolys"    ))==0 ) Ar.Logf(TEXT("%i"),NumPolys);
	else if( appStricmp(Item,TEXT("Points"		  ))==0 ) Ar.Logf(TEXT("%i"),Level->Model->Points.Num());
	else if( appStricmp(Item,TEXT("Vectors"		  ))==0 ) Ar.Logf(TEXT("%i"),Level->Model->Vectors.Num());
	else if( appStricmp(Item,TEXT("Sides"		  ))==0 ) Ar.Logf(TEXT("%i"),Level->Model->NumSharedSides);
	else if( appStricmp(Item,TEXT("Zones"		  ))==0 ) Ar.Logf(TEXT("%i"),Level->Model->NumZones-1);
	else if( appStricmp(Item,TEXT("Bounds"		  ))==0 ) Ar.Logf(TEXT("%i"),Level->Model->Bounds.Num());
	else if( appStricmp(Item,TEXT("DuplicateBrush"))==0 )
	{
		// Duplicate brush.
		for( INT i=0; i<Level->Actors.Num(); i++ )
			if
			(	Level->Actors(i)
			&&	Cast<ABrush>(Level->Actors(i))
			&&	Level->Actors(i)->bSelected )
			{
				ABrush* Actor    = (ABrush*)Level->Actors(i);
				Actor->Location  = Level->Brush()->Location;
				Actor->Rotation  = Level->Brush()->Rotation;
				Actor->PrePivot  = Level->Brush()->PrePivot;
				GEditor->csgCopyBrush( Actor, Level->Brush(), 0, 0, 1 );
				break;
			}
		debugf( NAME_Log, TEXT("Duplicated brush") );
	}
	unguard;
}
void MapTopicHandler::Set( ULevel* Level, const TCHAR* Item, const TCHAR* Data )
{}

/*---------------------------------------------------------------------------------------
   Polys link topic handler
---------------------------------------------------------------------------------------*/

AUTOREGISTER_TOPIC(TEXT("Polys"),PolysTopicHandler);
void PolysTopicHandler::Get( ULevel* Level, const TCHAR* Item, FOutputDevice& Ar )
{
	guard(PolysTopicHandler::Get);
	DWORD		OnFlags,OffFlags;

	int n=0, StaticLights=0, Meshels=0, MeshU=0, MeshV=0;
	OffFlags = (DWORD)~0;
	OnFlags  = (DWORD)~0;
	for( INT i=0; i<Level->Model->Surfs.Num(); i++ )
	{
		FBspSurf *Poly = &Level->Model->Surfs(i);
		if( Poly->PolyFlags&PF_Selected )
		{
			OnFlags  &=  Poly->PolyFlags;
			OffFlags &= ~Poly->PolyFlags;
			n++;
			if( Poly->iLightMap != INDEX_NONE )
			{
				FLightMapIndex& Index = Level->Model->LightMap(Poly->iLightMap);
				Meshels			+= Index.UClamp * Index.VClamp;
				MeshU            = Index.UClamp;
				MeshV            = Index.VClamp;
				if( Index.iLightActors != INDEX_NONE )
					for( int j=0; Level->Model->Lights(j+Index.iLightActors); j++ )
						StaticLights++;
			}
		}
	}
	if      (!appStricmp(Item,TEXT("NumSelected")))				Ar.Logf(TEXT("%i"),n);
	else if (!appStricmp(Item,TEXT("StaticLights")))			Ar.Logf(TEXT("%i"),StaticLights);
	else if (!appStricmp(Item,TEXT("Meshels")))					Ar.Logf(TEXT("%i"),Meshels);
	else if (!appStricmp(Item,TEXT("SelectedSetFlags")))		Ar.Logf(TEXT("%u"),OnFlags  & ~PF_NoEdit);
	else if (!appStricmp(Item,TEXT("SelectedClearFlags")))		Ar.Logf(TEXT("%u"),OffFlags & ~PF_NoEdit);
	else if (!appStricmp(Item,TEXT("MeshSize")) && n==1)		Ar.Logf(TEXT("%ix%i"),MeshU,MeshV);

	unguard;
}
void PolysTopicHandler::Set( ULevel* Level, const TCHAR* Item, const TCHAR* Data )
{
	guard(PolysTopicHandler::Set);
	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
