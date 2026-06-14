/*=============================================================================
	UnDynBsp.cpp: Unreal dynamic Bsp object support
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	This code handles all moving brushes with a level.  These are objects which move
	once in a while and are added to and maintained within the level's Bsp whenever they 
	move.

Revision history:
	* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"

/*---------------------------------------------------------------------------------------
	Brush class implementation.
---------------------------------------------------------------------------------------*/

void ABrush::InitPosRotScale()
{
	guard(ABrush::InitPosRotScale);
	check(Brush);
	
	MainScale = GMath.UnitScale;
	PostScale = GMath.UnitScale;
	Location  = FVector(0,0,0);
	Rotation  = FRotator(0,0,0);
	PrePivot  = FVector(0,0,0);

	unguard;
}
void ABrush::PostLoad()
{
	guard(ABrush::PostLoad);
	Super::PostLoad();
	unguard;
}
IMPLEMENT_CLASS(ABrush);

/*---------------------------------------------------------------------------------------
	Globals.
---------------------------------------------------------------------------------------*/

// Binds an actor and index.
struct FActorIndex
{
	AMover* Actor;
	INT Index;
	FActorIndex( AMover* InActor=NULL, INT InIndex=0 )
	: Actor(InActor), Index(InIndex)
	{}
	operator UBOOL()
	{
		return Actor!=NULL;
	}
};

//
// Tracks moving brushes within a level.  One of these structures is kept for each 
// active level.  The structure is not saved with levels, but is rather rebuild at 
// load-time.
//
class FMovingBrushTracker : public FMovingBrushTrackerBase
{
public:
	// FMovingBrushTrackerBase interface.
	FMovingBrushTracker( ULevel *ThisLevel );
	~FMovingBrushTracker();
	void Flush( AActor *Actor );
	void Update( AActor *Actor );
	void CountBytes( FArchive& Ar );
	UBOOL SurfIsDynamic( INT iSurf );

private:
	// Internal variables.
	ULevel*				Level;
	FVector				FPolyNormal;
	INT					iNodeRover, iVertRover;
	INT					iFirstRovingPoint;
	INT					iOriginalTopSurf, iOriginalTopVector, iOriginalTopPoint, iOriginalTopNode, iOriginalTopVert;
	AMover*				AddActor;
	INT*				iActorNodePrevLink;
	FActorLink*			GroupActors;

	// Free lists.
	INT					iFreePoints;

	// Arrays.
	TArray<AMover*>		PointOwners;
	TArray<FActorIndex>	NodeMaps;
	TArray<AMover*>		VertPoolOwners;

	// Internal functions.
	INT NewVertPoolIndex( AMover* Actor, INT NumVerts );
	void FreeVertPoolIndex( DWORD i, INT NumVerts );
	void SetupActorBrush( AMover* Actor );
	void FlushActorBrush( AMover* Actor );
	void ForceGroupFlush( INT iNode );
	void FilterFPoly( INT iAddSurf, INT iNode, INT iCoplanarParent, FPoly *EdPoly, INT Outside );
	void AddPolyFragment( INT iAddSurf, INT iNode, INT iCoplanarParent, INT IsFront, FPoly* EdPoly );
	void AddGroupActor( AActor* InActor )
	{
		if( !InActor->bAssimilated )
		{
			InActor->bAssimilated = 1;
			GroupActors = new(GEngineMem)FActorLink( InActor, GroupActors );
			((AMover*)InActor)->SavedPos = InActor->Location;
			((AMover*)InActor)->SavedRot = InActor->Rotation;
		}
	}
};

/*---------------------------------------------------------------------------------------
	Index management.
---------------------------------------------------------------------------------------*/

//
// Allocate an array of INDEX's for the elements in a database object from
// Num to Max.  These elements of level objects are reserved for use by moving 
// brush pieces.
//
template <class T, class U> void AllocDbThing( TArray<T>& Result, TArray<U>& Array, INT OriginalNum )
{
	guard(FMovingBrushTracker::AllocDbThing);
	Result.Empty    ( Array.Num() - OriginalNum );
	Result.AddZeroed( Array.Num() - OriginalNum );
	unguard;
}

//
// Shorten a database object's maximum element count to prevent
// moving brush data from thrashing it as a sparse array.  Returns
// the number of active elements in the object.
//
template <class T> INT ExpandDb( TArray<T>& Array, INT Slack, INT MaxCount )
{
	guard(ExpandDb);

	INT Index = Array.Num();
#ifdef PLATFORM_DREAMCAST
	INT Add   = Min<INT>(Array.Num() + Slack, MaxCount) - Array.Num();
#else
	INT Add   = Min<INT>(2*Array.Num() + Slack, MaxCount) - Array.Num();
#endif
	if( Add > 0 )
		Array.AddZeroed( Add );
	Array.Shrink();
	return Index;

	unguard;
}

//
// Get an index for a new thing.
//
template <class T> INT NewThing
(
	INT&		TopThing, 
	const INT&	NumThings,
	const INT&	MaxThings,
	TArray<T>&	ThingOwners,
	const T&	Actor
)
{
	INT StartThing = TopThing;
	T* ThingOwner = &ThingOwners(TopThing-NumThings);
	while( TopThing < MaxThings )
	{
		if( !*ThingOwner )
		{
			*ThingOwner = Actor;
			return TopThing;
		}
		TopThing++;
		ThingOwner++;
	}
	TopThing = NumThings;
	ThingOwner = &ThingOwners(0);
	while( TopThing < StartThing )
	{
		if( !*ThingOwner )
		{
			*ThingOwner = Actor;
			return TopThing;
		}
		TopThing++;
		ThingOwner++;
	}
	return INDEX_NONE;
}

//
// Create new item.
//
template <class T, class U> INT NewThang
(
	INT&			FirstMaster,
	TArray<T>&		Master,
	TArray<U>&		Helper,
	INT&			FirstLink,
	const U&		HelperValue
)
{
	guard(NewThang);
	check(Master.Num()-FirstMaster==Helper.Num());
	INT Index;
	if( FirstLink!=INDEX_NONE )
	{
		Index = FirstLink;
		FirstLink = *(INT*)&Helper( FirstLink - FirstMaster );
	}
	else
	{
		Index = Master.AddZeroed();
		Helper.AddZeroed();
	}
	Helper( Index - FirstMaster ) = HelperValue;
	return Index;
	unguard;
}

template <class T> void FreeThang
(
	INT				Index,
	INT				FirstMaster,
	TArray<T>&		Helper,
	INT&			FirstLink
)
{
	INT& Ptr  = *(INT*)&Helper( Index - FirstMaster );
	Ptr       = FirstLink;
	FirstLink = Index;
}

/*---------------------------------------------------------------------------------------
	Public functions.
---------------------------------------------------------------------------------------*/

//
// Flush an actor's brush.
//
void FMovingBrushTracker::Flush( AActor* Actor )
{
	guard(FMovingBrushTracker::Flush);
	if( Actor->IsMovingBrush() )
		FlushActorBrush( CastChecked<AMover>(Actor) );
	unguard;
}

//
// Update an actor's brush.
//
void FMovingBrushTracker::Update( AActor* InActor )
{
	guard(FMovingBrushTracker::Update);
	AMover* Actor = Cast<AMover>( InActor );
	FMemMark DynMark(GEngineMem);
	GroupActors = NULL;
	if( Actor==NULL )
	{
		// Update all actor brushes.
		for( INT iActor=0; iActor<Level->Actors.Num(); iActor++ )
			if( Level->Actors(iActor) && Level->Actors(iActor)->IsMovingBrush() )
				AddGroupActor( Level->Actors(iActor) );
	}
	else if( Actor->SavedPos!=Actor->Location || Actor->SavedRot!=Actor->Rotation )
	{
		// Update the single brush.
		AddGroupActor( Actor );
	}

	// Make a complete list of all actors that need flushing.
	FActorLink* Link;
	for( Link=GroupActors; Link; Link=Link->Next )
	{
		INT iNode = ((AMover*)Link->Actor)->Brush->MoverLink;
		while( iNode != INDEX_NONE )
		{
			FBspNode* Node = &Level->Model->Nodes(iNode);
			ForceGroupFlush( iNode );
#ifdef PLATFORM_DREAMCAST
			iNode = (INT)Node->iRenderBound;
#else
			iNode = Node->iRenderBound;
#endif
		}
	}

	// Recursively flush all affected brushes.
	for( Link=GroupActors; Link; Link=Link->Next )
		FlushActorBrush( (AMover*)Link->Actor );

	// Add all moving brush sporadic data to the world.
	for( Link=GroupActors; Link; Link=Link->Next )
	{
		FMemMark Mark(GMem);
		AMover* Actor = (AMover*)Link->Actor;
		Actor->bAssimilated = 0;

		AddActor           = Actor;
		UModel* Brush      = Actor->Brush;

		// Build transformed polys.
		FModelCoords Coords;
		FLOAT Orientation = ((ABrush*)Actor)->BuildCoords(&Coords,NULL);
		INT Total=0;
		INT i;
		for( i=0; i<Brush->Polys->Element.Num(); i++ )
			Total += Brush->Polys->Element(i).NumVertices;
		FPoly* TransformedPolys = new( GMem, Brush->Polys->Element.Num() )FPoly;
		FVector* Points = new( GMem, Total )FVector;
		INT Count=0;
		for( i=0; i<Brush->Polys->Element.Num(); i++ )
		{
			TransformedPolys[i] = Brush->Polys->Element(i);
			TransformedPolys[i].Transform( Coords, ((ABrush*)Actor)->PrePivot, Actor->Location, Orientation );
			for( INT j=0; j<TransformedPolys[i].NumVertices; j++ )
				Points[Count++] = TransformedPolys[i].Vertex[j];
		}
		FSphere Sphere( Points, Count );
		Level->Model->PrecomputeSphereFilter( Sphere );

		// Go through list of all brush FPolys and update their corresponding Bsp surfaces.
		iActorNodePrevLink = &Actor->Brush->MoverLink;
		for( i=0; i<Brush->Polys->Element.Num(); i++ )
		{
			FPoly* Poly                            = &TransformedPolys[i];
			INT iSurf                              = Poly->iLink;
			FBspSurf* Surf                         = &Level->Model->Surfs(iSurf);
			FPolyNormal                            = Poly->Normal;
#ifdef PLATFORM_DREAMCAST
			Level->Model->Points ((INT)Surf->pBase    ) = Poly->Base;
			Level->Model->Vectors((INT)Surf->vNormal  ) = Poly->Normal;
			Level->Model->Vectors((INT)Surf->vTextureU) = Poly->TextureU;
			Level->Model->Vectors((INT)Surf->vTextureV) = Poly->TextureV;
#else
			Level->Model->Points (Surf->pBase    ) = Poly->Base;
			Level->Model->Vectors(Surf->vNormal  ) = Poly->Normal;
			Level->Model->Vectors(Surf->vTextureU) = Poly->TextureU;
			Level->Model->Vectors(Surf->vTextureV) = Poly->TextureV;
#endif

			// Filter the brush's FPoly through the Bsp, creating new sporadic Bsp nodes (and their 
			// corresponding VertPools and points) for all outside leaves the FPoly fragments fall into:
			if( Level->Model->Nodes.Num() > 0 )
				FilterFPoly( iSurf, 0, INDEX_NONE, Poly, 1 );
		}
#ifdef PLATFORM_DREAMCAST
		if( iActorNodePrevLink == &AddActor->Brush->MoverLink )
		{
			// Writing to MoverLink (INT field) - safe to write INT
			*iActorNodePrevLink = INDEX_NONE;
		}
		else
		{
			// Writing to iRenderBound (SWORD field) - must write as SWORD
			*((SWORD*)iActorNodePrevLink) = (SWORD)INDEX_NONE;
		}
#else
		*iActorNodePrevLink = INDEX_NONE;
#endif

		// Tag all newly-added nodes as non-new.
		INT iNode = Brush->MoverLink;
		while( iNode != INDEX_NONE )
		{
			FBspNode* Node   = &Level->Model->Nodes(iNode);
			check(NodeMaps(iNode-iOriginalTopNode).Actor==Actor);
			Node->NodeFlags &= ~NF_IsNew;
#ifdef PLATFORM_DREAMCAST
			iNode = (INT)Node->iRenderBound;
#else
			iNode = Node->iRenderBound;
#endif
		}
		Mark.Pop();
	}
	DynMark.Pop();
	unguard;
}

UBOOL FMovingBrushTracker::SurfIsDynamic( INT iSurf )
{
	return iSurf>=iOriginalTopSurf;
}

/*---------------------------------------------------------------------------------------
	FMovingBrushTracker init & exit.
---------------------------------------------------------------------------------------*/

//
// Initialize or reinitialize everything, and allocate all working tables.  Must be 
// followed by a call to UpdateAllBrushes to actually add moving brushes to the world 
// Bsp.  This function assumes that the Bsp is clean when it is called, i.e. it has no 
// references to dynamic Bsp nodes in it.
//
FMovingBrushTracker::FMovingBrushTracker( ULevel* ThisLevel )
{
	guard(FMovingBrushTracker::FMovingBrushTracker);

	// Surface items.
	Level				= ThisLevel;
	iOriginalTopSurf    = Level->Model->Surfs  .Num();
	iOriginalTopVector	= Level->Model->Vectors.Num();
	iOriginalTopPoint   = Level->Model->Points .Num();
	iOriginalTopNode	= Level->Model->Nodes  .Num();
	iOriginalTopVert	= Level->Model->Verts  .Num();

	// Free lists.
	iFreePoints			= INDEX_NONE;

	// Setup all brushes.
	for( INT i=0; i<Level->Actors.Num(); i++ )
		if( Level->Actors(i) && Level->Actors(i)->IsMovingBrush() )
			SetupActorBrush( CastChecked<AMover>(Level->Actors(i)) );

	// Save roving info.
	iFirstRovingPoint   = Level->Model->Points.Num();

	// Shrink tables.
	Level->Model->Vectors.Shrink();
	Level->Model->Surfs.Shrink();

	// Allocate changing structures.
	iNodeRover		       = ExpandDb( Level->Model->Nodes, 512, MAX_NODES );
#ifdef PLATFORM_DREAMCAST
	iVertRover		       = ExpandDb( Level->Model->Verts, 64, MAXSWORD    );
#else
	iVertRover		       = ExpandDb( Level->Model->Verts, 512, MAXINT    );
#endif
	AllocDbThing( NodeMaps, Level->Model->Nodes,  iOriginalTopNode );
	VertPoolOwners.Empty    ( Level->Model->Verts.Num() - iOriginalTopVert );
	VertPoolOwners.AddZeroed( Level->Model->Verts.Num() - iOriginalTopVert );

	// Now setup and update all brushes.
	Update( NULL );

	debugf( NAME_Init, TEXT("Initialized moving brush tracker for %s"), Level->GetFullName() );
	unguard;
}

//
// Destructor.
//
FMovingBrushTracker::~FMovingBrushTracker()
{
	guard(FMovingBrushTracker::~FMovingBrushTracker);

	Level->Model->Surfs  .Remove( iOriginalTopSurf,   Level->Model->Surfs  .Num() - iOriginalTopSurf );
	Level->Model->Points .Remove( iOriginalTopPoint,  Level->Model->Points .Num() - iOriginalTopPoint  );
	Level->Model->Vectors.Remove( iOriginalTopVector, Level->Model->Vectors.Num() - iOriginalTopVector );
	Level->Model->Nodes  .Remove( iOriginalTopNode,   Level->Model->Nodes  .Num() - iOriginalTopNode   );
	Level->Model->Verts  .Remove( iOriginalTopVert,   Level->Model->Verts  .Num() - iOriginalTopVert   );
	for( INT i=0,Num=Level->Model->Nodes.Num(); i<Num; i++ )
	{
		FBspNode& Node = Level->Model->Nodes(i);
		if( Node.iFront >= Num ) Node.iFront = INDEX_NONE;
		if( Node.iBack  >= Num ) Node.iBack  = INDEX_NONE;
		if( Node.iPlane >= Num ) Node.iPlane = INDEX_NONE;
	}
	debugf( NAME_Init, TEXT("Shut down moving brush tracker for %s"), Level->GetFullName() );

	unguard;
}

//
// Free all moving brush data.
//
void FMovingBrushTracker::CountBytes( FArchive& Ar )
{
	guard(FMovingBrushTracker::CountBytes);

	NodeMaps		.CountBytes( Ar );
	VertPoolOwners	.CountBytes( Ar );
	PointOwners		.CountBytes( Ar );

	unguard;
}

/*---------------------------------------------------------------------------------------
	Routines to allocate new elements of particular types, for moving brush usage.
	These all call NewThing to do their work.
---------------------------------------------------------------------------------------*/

// Get a new vertex pool index.
inline INT FMovingBrushTracker::NewVertPoolIndex( AMover* Actor, INT NumVerts )
{
	guardSlow(FMovingBrushTracker::NewVertPoolIndex);

	INT		iStart		= iVertRover;
	INT		NumFree		= 0;
	INT     OwnerIndex  = iVertRover - iOriginalTopVert;
	while( iVertRover+NumVerts < Level->Model->Verts.Num() )
	{
		if( VertPoolOwners(OwnerIndex)==NULL )
		{
			if( ++NumFree >= NumVerts )
			{
				while( NumFree-- > 0 )
					VertPoolOwners(OwnerIndex--) = Actor;
				return iVertRover + 1 - NumVerts;
			}
		}
		else NumFree=0;
		iVertRover++;
		OwnerIndex++;
	}

	iVertRover		= iOriginalTopVert;
	NumFree			= 0;
	OwnerIndex      = 0;
	while( iVertRover+NumVerts < iStart )
	{
		if( VertPoolOwners(OwnerIndex) == NULL )
		{
			if( ++NumFree >= NumVerts )
			{
				while( NumFree-- > 0 )
					VertPoolOwners(OwnerIndex--) = Actor;
				return iVertRover + 1 - NumVerts;
			}
		}
		else NumFree=0;
		iVertRover++;
		OwnerIndex++;
	}
	return INDEX_NONE;
	unguardSlow;
}

/*---------------------------------------------------------------------------------------
	Functions to free things.
---------------------------------------------------------------------------------------*/

// Free a vertex pool index.
inline void FMovingBrushTracker::FreeVertPoolIndex( DWORD i, INT NumVerts )
{
	for( INT j=0; j<NumVerts; j++ )
		VertPoolOwners(j+i-iOriginalTopVert) = NULL;
}

/*---------------------------------------------------------------------------------------
	Private, permanent per brush operations.
---------------------------------------------------------------------------------------*/

//
// Setup permanenent information for a moving brush.
//
void FMovingBrushTracker::SetupActorBrush( AMover* Actor )
{
	guard(FMovingBrushTracker::SetupActorBrush);
	check(Actor);
	check(Actor->IsMovingBrush());

	// Create permanent maps for all moving brush FPolys.
	UModel* Brush       = Actor->Brush;
	Brush->MoverLink    = INDEX_NONE;
	Actor->SavedPos     = FVector(-1,-1,-1);
	Actor->SavedRot     = FRotator(123,456,789);
	Actor->bAssimilated = 0;
	for( INT i=0; i<Brush->Polys->Element.Num(); i++ )
	{
		// Create new surface elements.
		FPoly* Poly               = &Brush->Polys->Element(i);
		INT iSurf                 = Level->Model->Surfs.AddZeroed();
		FBspSurf* Surf            = &Level->Model->Surfs(iSurf);
		Surf->vNormal             = Level->Model->Vectors.AddZeroed();
		Surf->vTextureU           = Level->Model->Vectors.AddZeroed();
		Surf->vTextureV           = Level->Model->Vectors.AddZeroed();
		Surf->pBase               = Level->Model->Points .AddZeroed();
		Surf->iLightMap           = Poly->iBrushPoly;
		Surf->Texture  		      = Poly->Texture;
		Surf->PanU 		 	      = Poly->PanU;
		Surf->PanV 		 	      = Poly->PanV;
		Surf->iBrushPoly	      = i;
		Surf->Actor			      = Actor;
		Surf->PolyFlags 	      = Poly->PolyFlags & ~PF_NoAddToBSP;
		if( Actor->bSpecialLit )
			Surf->PolyFlags      |= PF_SpecialLit;
		Poly->iLink               = iSurf;
	}

	unguard;
}

/*---------------------------------------------------------------------------------------
	Polygon filtering.
---------------------------------------------------------------------------------------*/

// Add a fragment of a polygon.
void FMovingBrushTracker::AddPolyFragment
(
	INT		iAddSurf,
	INT		iParent,
	INT		iCoplanarParent,
	INT		IsFront,
	FPoly*	EdPoly
)
{
	guard(FMovingBrushTracker::AddPolyFragment);
	INT iFirstParent = iParent;
	INT ZoneFront = IsFront;

	// If this node is meant to be added as a coplanar, handle it now.
	INT Different=0;
	if( iCoplanarParent != INDEX_NONE )
	{
		Different = iParent!=iCoplanarParent;
		iParent   = iCoplanarParent;
		IsFront   = 2;
		FBspNode* Parent = &Level->Model->Nodes(iParent);
		while( Parent->iPlane != INDEX_NONE )
		{
			iParent = Parent->iPlane;
			Parent = &Level->Model->Nodes(iParent);
		}
	}

	// Create a new sporadic node.
	INT iNode = NewThing( iNodeRover, iOriginalTopNode, Level->Model->Nodes.Num(), NodeMaps, FActorIndex(AddActor,iParent) );
	if( iNode == INDEX_NONE )
	{
		return;
	}
	FBspNode* Node        = &Level->Model->Nodes(iNode);
	FBspNode* Parent      = &Level->Model->Nodes(iParent);
	FBspNode* FirstParent = &Level->Model->Nodes(iFirstParent);

	// Set node's info.
	Node->iSurf       	  = iAddSurf;
	Node->iCollisionBound = INDEX_NONE;
	Node->iRenderBound    = INDEX_NONE;
	Node->ZoneMask		  = Parent->ZoneMask;
	Node->NumVertices	  = EdPoly->NumVertices;
	Node->iFront		  = INDEX_NONE;
	Node->iBack			  = INDEX_NONE;
	Node->iPlane		  = INDEX_NONE;
	Node->Plane			  = FPlane( EdPoly->Base, EdPoly->Normal );
	Node->NodeFlags   	  = NF_IsNew;

	// Set node flags.
	if( EdPoly->PolyFlags & PF_NotSolid              ) Node->NodeFlags |= NF_NotCsg;
	if( EdPoly->PolyFlags & (PF_Invisible|PF_Portal) ) Node->NodeFlags |= NF_NotVisBlocking;
	if( EdPoly->PolyFlags & PF_Masked                ) Node->NodeFlags |= NF_ShootThrough;

	// Set leaf and zone info.
	if( iCoplanarParent==INDEX_NONE || Different )
	{
		Node->iZone[0] = Node->iZone[1]	= FirstParent->iZone[ZoneFront];
		Node->iLeaf[0] = Node->iLeaf[1]	= FirstParent->iLeaf[ZoneFront];
	}
	else
	{
		INT IsFlipped  = (Node->Plane|Parent->Plane)<0.0;
		Node->iZone[0] = Parent->iZone[IsFlipped  ]; Node->iLeaf[0] = Parent->iLeaf[IsFlipped  ];
		Node->iZone[1] = Parent->iZone[1-IsFlipped]; Node->iLeaf[1] = Parent->iLeaf[1-IsFlipped];
	}
	FirstParent->ZoneMask |= ((QWORD)1 << Node->iZone[0]) | ((QWORD)1 << Node->iZone[1]);

	// Allocate this node's vertex pool and vertices.
	INT iVertPool = Node->iVertPool = NewVertPoolIndex(AddActor,EdPoly->NumVertices);
	if( iVertPool==INDEX_NONE )
	{
		NodeMaps( iNode-iOriginalTopNode ) = FActorIndex();
		return;
	}
	FVert* VertPool = &Level->Model->Verts( iVertPool );

	// Add vertices.
	for( INT i=0; i<EdPoly->NumVertices; i++ )
	{
		INT pVertex = NewThang( iFirstRovingPoint, Level->Model->Points, PointOwners, iFreePoints, AddActor );
		if( pVertex==INDEX_NONE )
		{
			while( --i >= 0 )
				FreeThang( (--VertPool)->pVertex, iFirstRovingPoint, PointOwners, iFreePoints );
			NodeMaps( iNode-iOriginalTopNode ) = FActorIndex();
			return;
		}
		VertPool->iSide		          = INDEX_NONE;
		VertPool->pVertex	          = pVertex;
		Level->Model->Points(pVertex) = EdPoly->Vertex[i];
		VertPool++;
	}

	// Update linked lists.
	Parent->iChild[IsFront] = iNode;
#ifdef PLATFORM_DREAMCAST
	// On Dreamcast, iRenderBound is SWORD (2 bytes), not INT (4 bytes)
	// We can't write INT directly to SWORD field through INT* pointer - it causes unaligned access
	// Check if we're writing to MoverLink (INT) or iRenderBound (SWORD)
	// MoverLink is in ABrush, iRenderBound is in FBspNode
	// We can detect by checking if iActorNodePrevLink points to a SWORD field
	// For now, we'll write as SWORD and cast the pointer appropriately
	if( iActorNodePrevLink == (INT*)&AddActor->Brush->MoverLink )
	{
		// Writing to MoverLink (INT field) - safe to write INT
		*iActorNodePrevLink = iNode;
	}
	else
	{
		// Writing to iRenderBound (SWORD field) - must write as SWORD
		*((SWORD*)iActorNodePrevLink) = (SWORD)iNode;
	}
	iActorNodePrevLink = (INT*)&Node->iRenderBound;
#else
	*iActorNodePrevLink = iNode;
	iActorNodePrevLink  = &Node->iRenderBound;
#endif

	unguard;
}

void FMovingBrushTracker::FilterFPoly
(
	INT		iAddSurf,
	INT		iNode, 
	INT		iCoplanarParent,
	FPoly*	EdPoly, 
	INT		Outside
)
{
	FPoly* TempFrontEdPoly = new(GMem)FPoly;
	FPoly* TempBackEdPoly  = new(GMem)FPoly;

FilterLoop:
	FBspNode* Node = &Level->Model->Nodes(iNode);
	if( EdPoly->NumVertices >= FPoly::VERTEX_THRESHOLD )
	{
		// Must split to avoid vertex overflow.
		TempFrontEdPoly = new(GMem)FPoly;
		EdPoly->SplitInHalf( TempFrontEdPoly );
		FilterFPoly( iAddSurf, iNode, iCoplanarParent, TempFrontEdPoly, Outside );
	}
	INT SplitResult
	=	(Node->NodeFlags & NF_IsFront) ? SP_Front
	:	(Node->NodeFlags & NF_IsBack)  ? SP_Back
	:	EdPoly->SplitWithPlaneFast( Node->Plane, TempFrontEdPoly, TempBackEdPoly );
	if( SplitResult == SP_Front )
	{
	Front:
		Outside = Outside || Node->IsCsg();
	#ifdef PLATFORM_DREAMCAST
		if( (INT)Node->iFront != INDEX_NONE )
		{
			iNode = (INT)Node->iFront;
			goto FilterLoop;
		}
#else
		if( Node->iFront != INDEX_NONE )
		{
			iNode = Node->iFront;
			goto FilterLoop;
		}
#endif
		else if( Outside )
			AddPolyFragment( iAddSurf, iNode, iCoplanarParent, 1, EdPoly );
	}
	else if( SplitResult == SP_Back )
	{
	Back:
		Outside = Outside && !Node->IsCsg();
#ifdef PLATFORM_DREAMCAST
		if( (INT)Node->iBack != INDEX_NONE )
		{
			iNode = (INT)Node->iBack;
			goto FilterLoop;
		}
#else
		if( Node->iBack != INDEX_NONE )
		{
			iNode = Node->iBack;
			goto FilterLoop;
		}
#endif
		else if( Outside )
			AddPolyFragment( iAddSurf, iNode, iCoplanarParent, 0, EdPoly );
	}
	else if( SplitResult == SP_Coplanar )
	{
		if( (Node->Plane | FPolyNormal) >= 0.0 )
			iCoplanarParent = iNode;
		goto Front;
	}
	else if( SplitResult == SP_Split )
	{
		// Handle front fragment.
#ifdef PLATFORM_DREAMCAST
		if( (INT)Node->iFront != INDEX_NONE )
			FilterFPoly( iAddSurf, (INT)Node->iFront, iCoplanarParent, TempFrontEdPoly, Outside || Node->IsCsg() );
#else
		if( Node->iFront != INDEX_NONE )
			FilterFPoly( iAddSurf, Node->iFront, iCoplanarParent, TempFrontEdPoly, Outside || Node->IsCsg() );
#endif
		else if( Outside || Node->IsCsg() )
			AddPolyFragment( iAddSurf, iNode, iCoplanarParent, 1, TempFrontEdPoly );

		// Handle back fragment.
		Node            = &Level->Model->Nodes(iNode);
		EdPoly			= TempBackEdPoly;
		TempBackEdPoly	= new(GMem)FPoly;
		goto Back;
	}
}

/*---------------------------------------------------------------------------------------
	Private, sporadic per brush operations.
---------------------------------------------------------------------------------------*/

//
// Force the brush that owns a specified Bsp node to be flushed and later
// updated as part of a group flushing operation.
//
void FMovingBrushTracker::ForceGroupFlush( INT iNode )
{
	guard(FMovingBrushTracker::ForceGroupFlush);
	FBspNode& Node = Level->Model->Nodes( iNode );
	if( !(Node.NodeFlags & NF_IsNew) )
	{
		Node.NodeFlags |= NF_IsNew;
#ifdef PLATFORM_DREAMCAST
		if( (INT)Node.iFront!=INDEX_NONE )
			ForceGroupFlush( (INT)Node.iFront );
		if( (INT)Node.iBack!=INDEX_NONE )
			ForceGroupFlush( (INT)Node.iBack );
		if( (INT)Node.iPlane!=INDEX_NONE )
			ForceGroupFlush( (INT)Node.iPlane );
#else
		if( Node.iFront!=INDEX_NONE )
			ForceGroupFlush( Node.iFront );
		if( Node.iBack!=INDEX_NONE )
			ForceGroupFlush( Node.iBack );
		if( Node.iPlane!=INDEX_NONE )
			ForceGroupFlush( Node.iPlane );
#endif

		AActor* OwnerActor = NodeMaps(iNode-iOriginalTopNode).Actor;
		if( OwnerActor )
			AddGroupActor( OwnerActor );
	}
	unguard;
}

//
// Flush all sporadic information for a moving brush.
//
void FMovingBrushTracker::FlushActorBrush( AMover* Actor )
{
	guard(FMovingBrushTracker::FlushActorBrush);

	// Go through all sporadic nodes in the Bsp and find ones owned by this actor.
	INT iNode = Actor->Brush->MoverLink;
	while( iNode != INDEX_NONE )
	{
		FBspNode* Node   = &Level->Model->Nodes(iNode);
		INT iParent      = NodeMaps(iNode-iOriginalTopNode).Index;
		FBspNode* Parent = &Level->Model->Nodes(iParent);

		// Remove references to this node from its parents.
#ifdef PLATFORM_DREAMCAST
		if	   ( (INT)Parent->iFront==iNode ) Parent->iFront=(SWORD)INDEX_NONE;
		else if( (INT)Parent->iBack ==iNode ) Parent->iBack =(SWORD)INDEX_NONE;
		else if( (INT)Parent->iPlane==iNode ) Parent->iPlane=(SWORD)INDEX_NONE;
#else
		if	   ( Parent->iFront==iNode ) Parent->iFront=INDEX_NONE;
		else if( Parent->iBack ==iNode ) Parent->iBack =INDEX_NONE;
		else if( Parent->iPlane==iNode ) Parent->iPlane=INDEX_NONE;
#endif

		// Free all sporadic data.
		FVert* VertPool = &Level->Model->Verts( Node->iVertPool );
		for( DWORD j=0; j<Node->NumVertices; j++ )
			FreeThang( VertPool[j].pVertex, iFirstRovingPoint, PointOwners, iFreePoints );

		FreeVertPoolIndex( Node->iVertPool, Node->NumVertices );
		NodeMaps( iNode-iOriginalTopNode ) = FActorIndex();
#ifdef PLATFORM_DREAMCAST
		iNode = (INT)Node->iRenderBound;
#else
		iNode = Node->iRenderBound;
#endif
	}
	Actor->Brush->MoverLink = INDEX_NONE;
	unguard;
}

/*---------------------------------------------------------------------------------------
	Instantiation.
---------------------------------------------------------------------------------------*/

ENGINE_API FMovingBrushTrackerBase* GNewBrushTracker( ULevel* InLevel )
{
	guard(GNewBrushTracker);
	return new(TEXT("FMovingBrushTracker"))FMovingBrushTracker( InLevel );
	unguard;
}

/*---------------------------------------------------------------------------------------
	The End.
---------------------------------------------------------------------------------------*/
