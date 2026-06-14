/*=============================================================================
	UnLevCol.cpp: Actor list collision code.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Design goal:
	To be self-contained. This collision code maintains its own collision hash
	table and doesn't know about any far-away data structures like the level BSP.

Revision history:
	* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"

/*-----------------------------------------------------------------------------
	FCollisionHash.
-----------------------------------------------------------------------------*/

//
// A collision hash table.
//
class ENGINE_API FCollisionHash : public FCollisionHashBase
{
public:
	// FCollisionHashBase interface.
	FCollisionHash();
	~FCollisionHash();
	void Tick();
	void AddActor( AActor *Actor );
	void RemoveActor( AActor *Actor );
	FCheckResult* ActorLineCheck( FMemStack& Mem, FVector End, FVector Start, FVector Extent, BYTE ExtraNodeFlags );
	FCheckResult* ActorPointCheck( FMemStack& Mem, FVector Location, FVector Extent, DWORD ExtraNodeFlags );
	FCheckResult* ActorRadiusCheck( FMemStack& Mem, FVector Location, FLOAT Radius, DWORD ExtraNodeFlags );
	FCheckResult* ActorEncroachmentCheck( FMemStack& Mem, AActor* Actor, FVector Location, FRotator Rotation, DWORD ExtraNodeFlags );
	void CheckActorNotReferenced( AActor* Actor );

	// Constants.
	enum { BULK_ALLOC  = 1024              };
#ifdef PLATFORM_LOW_MEMORY
	enum { NUM_BUCKETS = 8192              };
	enum { GRAN_XY     = 512               };
	enum { GRAN_Z      = 512               };
#else
	enum { NUM_BUCKETS = 16384             };
	enum { GRAN_XY     = 256               };
	enum { GRAN_Z      = 256               };
#endif
	enum { BASIS_BITS  = 8                 };
	enum { BASIS_MASK  = (1<<BASIS_BITS)-1 };
	enum { XY_OFS      = 32768             };
	enum { Z_OFS       = 32768             };

	// Linked list item.
	struct FCollisionLink
	{
		// Varibles.
		AActor*          Actor;     // The actor.
		FCollisionLink*  Next;      // Next link belonging to this collision bucket.
		INT				 iLocation; // Based hash location.
	} *Hash[NUM_BUCKETS], *Available;
	TArray<FCollisionLink*> LinksToFree;

	// Statics.
	static UBOOL Inited;
	static INT CollisionTag;
	static INT HashX[NUM_BUCKETS];
	static INT HashY[NUM_BUCKETS];
	static INT HashZ[NUM_BUCKETS];

	// Implementation.
	void GetActorExtent( AActor* Actor, INT& iX0, INT& iX1, INT& iY0, INT& iY1, INT& iZ0, INT& iZ1 );
	void GetHashIndices( FVector Location, INT& iX, INT& iY, INT& iZ )
	{
		iX = Clamp<INT>(appRound( (Location.X + XY_OFS) * (1.f/GRAN_XY) ), 0, NUM_BUCKETS-1 );
		iY = Clamp<INT>(appRound( (Location.Y + XY_OFS) * (1.f/GRAN_XY) ), 0, NUM_BUCKETS-1 );
		iZ = Clamp<INT>(appRound( (Location.Z + Z_OFS ) * (1.f/GRAN_Z ) ), 0, NUM_BUCKETS-1 );
	}
	struct FCollisionLink*& GetHashLink( INT iX, INT iY, INT iZ, INT& iLocation )
	{
		iLocation = iX + (iY << BASIS_BITS) + (iZ << (BASIS_BITS*2));
		return Hash[ HashX[iX] ^ HashY[iY] ^ HashZ[iZ] ];
	}
};

ENGINE_API FCollisionHashBase* GNewCollisionHash()
{
	guard(GNewCollisionHash);
	return new(TEXT("FCollisionHash"))FCollisionHash;
	unguard;
}

/*-----------------------------------------------------------------------------
	FCollisionHash statics.
-----------------------------------------------------------------------------*/

// FCollisionHash statics.
UBOOL FCollisionHash::Inited=0;
INT FCollisionHash::CollisionTag=0;
INT FCollisionHash::HashX[NUM_BUCKETS];
INT FCollisionHash::HashY[NUM_BUCKETS];
INT FCollisionHash::HashZ[NUM_BUCKETS];	

// Global statistics.
static INT GActorsAdded=0, GFragsAdded=0, GUsed=0, GChecks=0;

/*-----------------------------------------------------------------------------
	FCollisionHash init/exit.
-----------------------------------------------------------------------------*/

//
// Initialize the actor collision information.
//
FCollisionHash::FCollisionHash()
: Available( NULL )
, LinksToFree(  )
{
	guard(FCollisionHash::FCollisionHash);

	// Initialize static collision basis tables if necessary.
	if( !Inited )
	{
		Inited = 1;
		for( INT i=0; i<NUM_BUCKETS; i++ )
		{
			HashX[i] = HashY[i] = HashZ[i] = i;
		}
		for( INT i=0; i<NUM_BUCKETS; i++ )
		{
			Exchange( HashX[i], HashX[appRand() % NUM_BUCKETS] );
			Exchange( HashY[i], HashY[appRand() % NUM_BUCKETS] );
			Exchange( HashZ[i], HashZ[appRand() % NUM_BUCKETS] );
		}
	}

	// Init hash table.
	for( INT i=0; i<NUM_BUCKETS; i++ )
		Hash[i] = NULL;

	unguard;
}

//
// Shut down the actor collision information.
//
FCollisionHash::~FCollisionHash()
{
	guard(FCollisionHash::~FCollisionHash);

	// Free all collision links.
	for( INT i=0; i<LinksToFree.Num(); i++ )
		appFree( LinksToFree(i) );

	unguard;
}

/*-----------------------------------------------------------------------------
	FCollisionHash tick - clean up collision info.
-----------------------------------------------------------------------------*/

//
// Cleanup the collision info.
//
void FCollisionHash::Tick()
{
	guard(FCollisionHash::Tick);

	// All we do here is stats.
	//debugf(NAME_Log,"Used=%i Added=%i Frags=%i Checks=%i",GUsed,GActorsAdded,GFragsAdded,GChecks);
	GActorsAdded = GFragsAdded = GChecks = 0;

	unguard;
}

/*-----------------------------------------------------------------------------
	FCollisionHash extent.
-----------------------------------------------------------------------------*/

//
// Compute the extent of an actor in hash coordinates.
//
void FCollisionHash::GetActorExtent
(
	AActor *Actor,
	INT &X0, INT &X1, INT &Y0, INT &Y1, INT &Z0, INT &Z1
)
{
	guard(FCollisionHash::GetActorExtent);

	// Get actor's bounding box.
	FBox Box = Actor->GetPrimitive()->GetCollisionBoundingBox( Actor );

	// Discretize to hash coordinates.
	GetHashIndices( Box.Min, X0, Y0, Z0 );
	GetHashIndices( Box.Max, X1, Y1, Z1 );

	unguard;
}

/*-----------------------------------------------------------------------------
	FCollisionHash adding/removing.
-----------------------------------------------------------------------------*/

//
// Add an actor to the collision info.
//
void FCollisionHash::AddActor( AActor *Actor )
{
	guard(FCollisionHash::AddActor);
	check(Actor->bCollideActors);
	if( Actor->bDeleteMe )
		return;
	CheckActorNotReferenced( Actor );
	GActorsAdded++;

	// Add actor in all the specified places.
	INT X0,Y0,Z0,X1,Y1,Z1;
	GetActorExtent( Actor, X0, X1, Y0, Y1, Z0, Z1 );
	for( INT X=X0; X<=X1; X++ )
	{
		for( INT Y=Y0; Y<=Y1; Y++ )
		{
			for( INT Z=Z0; Z<=Z1; Z++ )
			{
				INT iLocation;
				FCollisionLink*& Link = GetHashLink( X, Y, Z, iLocation );
				if( !Available )
				{
					// Allocate a lot of new links.
					Available = new(TEXT("FCollisionLink"))FCollisionLink[BULK_ALLOC];
					LinksToFree.AddItem( Available );
					for( INT i=0; i<BULK_ALLOC-1; i++ )
						Available[i].Next = &Available[i+1];
					Available[BULK_ALLOC-1].Next = NULL;
				}
				FCollisionLink* NewLink = Available;
				Available               = Available->Next;
				NewLink->Actor          = Actor;
				NewLink->Next           = Link;
				NewLink->iLocation      = iLocation;
				Link                    = NewLink;
				GUsed++;
				GFragsAdded++;
			}
		}
	}
	Actor->ColLocation = Actor->Location;
	unguard;
}

//
// Remove an actor from the collision info.
//
void FCollisionHash::RemoveActor( AActor* Actor )
{
	guard(FCollisionHash::RemoveActor);
	check(Actor->bCollideActors);
	if( Actor->bDeleteMe )
		return;
#ifdef PLATFORM_DREAMCAST
	UBOOL bAdjustedLocation = 0;
	FVector SavedLocation = Actor->Location;
	if( Actor->Location!=Actor->ColLocation )
	{
		debugf( NAME_Warning, TEXT("%s moved without proper hashing, correcting"), Actor->GetFullName() );
		Actor->Location = Actor->ColLocation;
		bAdjustedLocation = 1;
	}
#else
	if( Actor->Location!=Actor->ColLocation )
		appErrorf( TEXT("%s moved without proper hashing"), Actor->GetFullName() );
#endif

	// Remove actor.
	INT X0,Y0,Z0,X1,Y1,Z1;
	GetActorExtent( Actor, X0, X1, Y0, Y1, Z0, Z1 );
	for( INT X=X0; X<=X1; X++ )
	{
		for( INT Y=Y0; Y<=Y1; Y++ )
		{
			for( INT Z=Z0; Z<=Z1; Z++ )
			{
				INT iLocation;
				FCollisionLink** Link = &GetHashLink( X, Y, Z, iLocation );
				while( *Link )
				{
					if( (*Link)->Actor != Actor )
					{
						Link = &(*Link)->Next;
					}
					else
					{
						FCollisionLink* Scrap = *Link;
						*Link                 = (*Link)->Next;
						Scrap->Next           = Available;
						Available	          = Scrap;
						GUsed--;
					}
				}
			}
		}
	}
	CheckActorNotReferenced( Actor );
#ifdef PLATFORM_DREAMCAST
	if( bAdjustedLocation )
	{
		// Restore actor transform and update collision location to current to keep hash consistent.
		Actor->Location    = SavedLocation;
		Actor->ColLocation = SavedLocation;
	}
#endif
	unguard;
}

/*-----------------------------------------------------------------------------
	FCollisionHash collision checking.
-----------------------------------------------------------------------------*/

//
// Make a list of all actors which overlap with a cylinder at Location
// with the given collision size.
//
FCheckResult* FCollisionHash::ActorPointCheck
(
	FMemStack&		Mem,
	FVector			Location,
	FVector			Extent,
	DWORD			ExtraNodeFlags
)
{
	guard(FCollisionHash::ActorPointCheck);
	FCheckResult* Result=NULL;

	// Get extent indices.
	INT X0,Y0,Z0,X1,Y1,Z1;
	GetHashIndices( Location - Extent, X0, Y0, Z0 );
	GetHashIndices( Location + Extent, X1, Y1, Z1 );
	CollisionTag++;

	// Check all actors in this neighborhood.
	for( INT X=X0; X<=X1; X++ ) for( INT Y=Y0; Y<=Y1; Y++ ) for( INT Z=Z0; Z<=Z1; Z++ )
	{
		INT iLocation;
		for( FCollisionLink* Link = GetHashLink( X, Y, Z, iLocation ); Link; Link=Link->Next )
		{
			// Skip if we've already checked this actor.
			if
			(	Link->Actor->CollisionTag != CollisionTag
			&&	Link->iLocation           == iLocation )
			{
				// Collision test.
				Link->Actor->CollisionTag = CollisionTag;
				FCheckResult TestHit(1.0);
				if( Link->Actor->GetPrimitive()->PointCheck( TestHit, Link->Actor, Location, Extent, 0 )==0 )
				{
					check(TestHit.Actor==Link->Actor);
					FCheckResult* New = new(GMem)FCheckResult;
					*New = TestHit;
					New->GetNext() = Result;
					Result = New;
				}
			}
		}
	}
	return Result;
	unguard;
}

//
// Make a list of all actors which are within a given radius.
//
FCheckResult* FCollisionHash::ActorRadiusCheck
(
	FMemStack&		Mem,
	FVector			Location,
	FLOAT			Radius,
	DWORD			ExtraNodeFlags
)
{
	guard(FCollisionHash::ActorVisRadiusCheck);
	FCheckResult* Result=NULL;

	// Get extent indices.
	INT X0,Y0,Z0,X1,Y1,Z1;
	GetHashIndices( Location - FVector(Radius,Radius,Radius), X0, Y0, Z0 );
	GetHashIndices( Location + FVector(Radius,Radius,Radius), X1, Y1, Z1 );
	CollisionTag++;
	FLOAT RadiusSq = Radius * Radius;

	// Check all actors in this neighborhood.
	for( INT X=X0; X<=X1; X++ ) for( INT Y=Y0; Y<=Y1; Y++ ) for( INT Z=Z0; Z<=Z1; Z++ )
	{
		INT iLocation;
		for( FCollisionLink* Link = GetHashLink( X, Y, Z, iLocation ); Link; Link=Link->Next )
		{
			// Skip if we've already checked this actor.
			if
			(	Link->Actor->CollisionTag != CollisionTag 
			&&	Link->iLocation           == iLocation )
			{
				// Collision test.
				Link->Actor->CollisionTag = CollisionTag;
				if( (Link->Actor->Location - Location).SizeSquared() < RadiusSq )
				{
					FCheckResult* New = new(GMem)FCheckResult;
					New->Actor = Link->Actor;
					New->GetNext() = Result;
					Result = New;
				}
			}
		}
	}
	return Result;
	unguard;
}

//
// Check for encroached actors.
//
FCheckResult* FCollisionHash::ActorEncroachmentCheck
(
	FMemStack&		Mem,
	AActor*			Actor,
	FVector			Location,
	FRotator		Rotation,
	DWORD			ExtraNodeFlags
)
{
	guard(FCollisionHash::ActorEncroachmentCheck);
	check(Actor!=NULL);

	// Save actor's location and rotation.
	Exchange( Location, Actor->Location );
	Exchange( Rotation, Actor->Rotation );

	// Get extent indices.
	INT X0,Y0,Z0,X1,Y1,Z1;
	GetActorExtent( Actor, X0, X1, Y0, Y1, Z0, Z1 );
	FCheckResult *Result, **PrevLink = &Result;
	CollisionTag++;

	// Check all actors in this neighborhood.
	for( INT X=X0; X<=X1; X++ ) for( INT Y=Y0; Y<=Y1; Y++ ) for( INT Z=Z0; Z<=Z1; Z++ )
	{
		INT iLocation;
		for( FCollisionLink* Link = GetHashLink( X, Y, Z, iLocation ); Link; Link=Link->Next )
		{
			// Skip if we've already checked this actor.
			if
			(	Link->Actor->CollisionTag != CollisionTag
			&&	Link->iLocation           == iLocation )
			{
				Link->Actor->CollisionTag = CollisionTag;
				FCheckResult TestHit(1.0);
				if
				(	!Link->Actor->IsMovingBrush()
				&&	Link->Actor!=Actor
				&&	Actor->GetPrimitive()->PointCheck( TestHit, Actor, Link->Actor->Location, Link->Actor->GetCylinderExtent(), 0 )==0 )
				{
					TestHit.Actor     = Link->Actor;
					TestHit.Primitive = NULL;
					*PrevLink         = new(GMem)FCheckResult;
					**PrevLink        = TestHit;
					PrevLink          = &(*PrevLink)->GetNext();
				}
			}
		}
	}

	// Restore actor's location and rotation.
	Exchange( Location, Actor->Location );
	Exchange( Rotation, Actor->Rotation );

	*PrevLink = NULL;
	return Result;
	unguard;
}

//
// Make a time-sorted list of all actors which overlap a cylinder moving 
// along a line from Start to End. If LevelInfo is specified, also checks for
// collision with the level itself and terminates collision when the trace
// hits solid space.
//
// Note: This routine is very inefficient for large lines, because it checks
// collision with all actors inside a bounding box containing the line's start
// and end point. This is a reasonable approach for regular actor movement
// like player walking, but it becomes very inefficient for long line traces, such
// as checking the collision of a bullet. To handle these cases, it would be smart
// to do the following optimizations:
//
// * Only test hash cubes which the line actually falls into. This could be done using
//   a raycasting-style algorithm.
//
// * Stop tracing once we have hit an actor which we know is guaranteed to be the 
//   nearest possible actor along the line.
//
FCheckResult* FCollisionHash::ActorLineCheck
(
	FMemStack&		Mem,
	FVector			End,
	FVector			Start,
	FVector			Size,
	BYTE			ExtraNodeFlags
)
{
	guard(FCollisionHash::ActorLineCheck);
	FCheckResult* Result=NULL;

	// Get extent.
	CollisionTag++;
	INT X0,Y0,Z0,X1,Y1,Z1,X;
	FBox Box( FBox(0) + Start + End );
	GetHashIndices( Box.Min - Size, X0, Y0, Z0 );
	GetHashIndices( Box.Max + Size, X1, Y1, Z1 );

	// Check all potentially colliding actors in the hash.
	for( X=X0; X<=X1; X++ )
	{
		for( INT Y=Y0; Y<=Y1; Y++ )
		{
			for( INT Z=Z0; Z<=Z1; Z++ )
			{
				INT iLocation;
				for( FCollisionLink* Link = GetHashLink( X, Y, Z, iLocation ); Link; Link=Link->Next )
				{
					// Skip if we've already checked this actor.
					if
					(	Link->Actor->CollisionTag != CollisionTag
					&&	Link->iLocation           == iLocation )
					{
						// Check collision.
						FCheckResult Hit(0);
						Link->Actor->CollisionTag = CollisionTag;
						if( Link->Actor->GetPrimitive()->LineCheck( Hit, Link->Actor, End, Start, Size, ExtraNodeFlags )==0 )
						{
							FCheckResult* Link = new(Mem)FCheckResult(Hit);
							Link->GetNext() = Result;
							Result = Link;
						}
					}
				}
			}
		}
	}
	return Result;
	unguard;
}

/*-----------------------------------------------------------------------------
	Checks.
-----------------------------------------------------------------------------*/

//
// Make sure the actor isn't referenced in the collision hash.
//
void FCollisionHash::CheckActorNotReferenced( AActor* Actor )
{
	guard(FCollisionHash::CheckActorNotReferenced);
	if( DO_GUARD_SLOW && !GIsEditor )
		for( INT i=0; i<NUM_BUCKETS; i++ )
			for( FCollisionLink* Link=Hash[i]; Link; Link=Link->Next )
				if( Link->Actor == Actor )
					appErrorf( TEXT("%s has collision hash fragments"), Actor->GetFullName() );
	unguardf(( TEXT("(%s)"), Actor->GetFullName() ));
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
