/*=============================================================================
	UnPath.cpp: Unreal pathnode placement

	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	These methods are members of the FPathBuilder class, which adds pathnodes to a level.  
	Paths are placed when the level is built.  FPathBuilder is not used during game play
 
   General comments:
   Path building
   The FPathBuilder does a tour of the level (or a part of it) using the "Scout" actor, starting from
   every actor in the level.  
   This guarantees that correct reachable paths are generated to all actors.  It starts by going to the
   wall to the right of the actor, and following that wall, keeping it to the left.  NOTE: my definition
   of left and right is based on normal Cartesian coordinates, and not screen coordinates (e.g. Y increases
   upward, not downward).  This wall following is accomplished by moving along the wall, constantly looking
   for a leftward passage.  If the FPathBuilder is stopped, he rotates clockwise until he can move forward 
   again.  While performing this tour, it keeps track of the set of previously placed pathnodes which are
   visible and reachable.  If a pathnode is removed from this set, then the FPathBuilder searches for an 
   acceptable waypoint.  If none is found, a new pathnode is added to provide a waypoint back to the no longer
   reachable pathnode.  The tour ends when a full circumlocution has been made, or if the FPathBuilder 
   encounters a previously placed path node going in the same direction.  Pathnodes that were not placed as
   the result of a left turn are marked, since they are due to some possibly unmapped obstruction.  In the
   final phase, these obstructions are inspected.  Then, the pathnode due to the obstruction is removed (since
   the obstruction is now mapped, and any needed waypoints have been built).

  FIXME - ledge and landing marking
  FIXME - mark door centers with left turns (reduces total paths)
  FIXME - paths on top of all platforms
	Revision history:
		* Created by Steven Polge 3/97
=============================================================================*/

#include "EnginePrivate.h"

#if DEBUGGINGPATHS
    static inline void CDECL DebugPrint(const TCHAR * Message, ...)
    {
        static int Count = 0;
        if( Count <= 300 ) // To reduce total messages.
        {
            TCHAR Text[4096];
			GET_VARARGS( Text, ARRAY_COUNT(Text), Message );
            debugf( NAME_Log, Text );
        }
    }
    static inline void CDECL DebugVector(const TCHAR *Text, const FVector & Vector )
    {
        TCHAR VectorText[100];
        TCHAR Message[100];
        appSprintf( VectorText, TEXT("[%4.4f,%4.4f,%4.4f]"), Vector.X, Vector.Y, Vector.Z );
        appSprintf( Message, Text, VectorText );
        DebugPrint(Message);
		DebugPrint(VectorText);
    }
	static inline void CDECL DebugFloat(const TCHAR *Text, const FLOAT & val )
    {
        TCHAR VectorText[100];
        TCHAR Message[100];
        appSprintf( VectorText, TEXT("[%4.4f]"), val);
        appSprintf( Message, Text, VectorText );
        DebugPrint(Message);
		DebugPrint(VectorText);
    }
	static inline void CDECL DebugInt(const TCHAR *Text, const int & val )
    {
        TCHAR VectorText[100];
        TCHAR Message[100];
        appSprintf( VectorText, TEXT("[%6d]"), val);
        appSprintf( Message, Text, VectorText );
        DebugPrint(Message);
		DebugPrint(VectorText);
    }
#else
    static inline void CDECL DebugPrint(const TCHAR * Message, ...)
    {
    }
     static inline void CDECL DebugVector(const TCHAR *Text, const FVector & Vector )
    {
    }
	static inline void CDECL DebugFloat(const TCHAR *Text, const FLOAT & val )
    {
    }
	static inline void CDECL DebugInt(const TCHAR *Text, const int & val )
    {
    }
   
#endif

//FIXME: add hiding places, camping and sniping spots (not pathnodes, but other keypoints)
int ENGINE_API FPathBuilder::buildPaths (ULevel *ownerLevel, int optimization)
{
	guard(FPathBuilder::buildPaths);

	int numpaths = 0;
	numMarkers = 0;
	Level = ownerLevel;
	pathMarkers = new(TEXT("FPathMarker"))FPathMarker[MAXMARKERS]; //FIXME - use DArray?
	getScout();

	Scout->SetCollision(1, 1, 1);
	Scout->bCollideWorld = 1;

	Scout->JumpZ = -1.0; //NO jumping
	Scout->GroundSpeed = 320; 
	Scout->MaxStepHeight = 24; 

	numpaths = numpaths + createPaths(optimization);
	Level->DestroyActor(Scout);
	return numpaths;
	unguard;
}

int ENGINE_API FPathBuilder::removePaths (ULevel *ownerLevel)
{
	guard(FPathBuilder::removePaths);
	Level = ownerLevel;
	int removed = 0;

	for (INT i=0; i<Level->Actors.Num(); i++)
	{
		AActor *Actor = Level->Actors(i); 
		if (Actor && Actor->IsA(APathNode::StaticClass()))
		{
			removed++;
			Level->DestroyActor( Actor ); 
		}
	}
	return removed;
	unguard;
}

int ENGINE_API FPathBuilder::showPaths (ULevel *ownerLevel)
{
	guard(FPathBuilder::showPaths);
	Level = ownerLevel;
	int shown = 0;

	for (INT i=0; i<Level->Actors.Num(); i++)
	{
		AActor *Actor = Level->Actors(i); 
		if (Actor && Actor->IsA(APathNode::StaticClass()))
		{
			shown++;
			Actor->DrawType = DT_Sprite; 
		}
	}
	return shown;
	unguard;
}

int ENGINE_API FPathBuilder::hidePaths (ULevel *ownerLevel)
{
	guard(FPathBuilder::hidePaths);
	Level = ownerLevel;
	int shown = 0;

	for (INT i=0; i<Level->Actors.Num(); i++)
	{
		AActor *Actor = Level->Actors(i); 
		if (Actor && Actor->IsA(APathNode::StaticClass()))
		{
			shown++;
			Actor->DrawType = DT_None; 
		}
	}
	return shown;
	unguard;
}

void ENGINE_API FPathBuilder::undefinePaths (ULevel *ownerLevel)
{
	guard(FPathBuilder::undefinePaths);
	Level = ownerLevel;

	//remove all reachspecs
	debugf(NAME_DevPath,TEXT("Remove %d old reachspecs"), Level->ReachSpecs.Num());
	Level->ReachSpecs.Empty();

	// clear navigationpointlist
	Level->GetLevelInfo()->NavigationPointList = NULL;

	//clear pathnodes
	for (INT i=0; i<Level->Actors.Num(); i++)
	{
		AActor *Actor = Level->Actors(i); 
		if (Actor && Actor->IsA(ANavigationPoint::StaticClass()))
		{
			if ( Actor->IsA(AWarpZoneMarker::StaticClass()) || Actor->IsA(ATriggerMarker::StaticClass()) 
					|| Actor->IsA(AInventorySpot::StaticClass()) || Actor->IsA(AButtonMarker::StaticClass()) )
			{
				if ( Actor->IsA(AInventorySpot::StaticClass()) && ((AInventorySpot *)Actor)->markedItem )
					((AInventorySpot *)Actor)->markedItem->myMarker = NULL;
				Level->DestroyActor(Actor);
			}
			else
			{
				((ANavigationPoint *)Actor)->nextNavigationPoint = NULL;
				((ANavigationPoint *)Actor)->nextOrdered = NULL;
				((ANavigationPoint *)Actor)->prevOrdered = NULL;
				((ANavigationPoint *)Actor)->startPath = NULL;
				((ANavigationPoint *)Actor)->previousPath = NULL;
				for (INT i=0; i<16; i++)
				{
					((ANavigationPoint *)Actor)->Paths[i] = -1;
					((ANavigationPoint *)Actor)->upstreamPaths[i] = -1;
					((ANavigationPoint *)Actor)->PrunedPaths[i] = -1;
					((ANavigationPoint *)Actor)->VisNoReachPaths[i] = NULL;
				}
			}
		}
	}
	unguard;
}

void ENGINE_API FPathBuilder::definePaths (ULevel *ownerLevel)
{
	guard(FPathBuilder::definePaths);
	Level = ownerLevel;
	getScout();
	Level->GetLevelInfo()->NavigationPointList = NULL;

	// Add WarpZoneMarkers and InventorySpots
	debugf( NAME_DevPath, TEXT("Add WarpZone and Inventory markers") );
	for (INT i=0; i<Level->Actors.Num(); i++)
	{
		AActor *Actor = Level->Actors(i); 
		if ( Actor )
		{
			if ( Actor->IsA(AWarpZoneInfo::StaticClass()) )
			{
				if ( !findScoutStart(Actor->Location) || (Scout->Region.Zone != Actor->Region.Zone) )
				{
					Scout->SetCollisionSize(20, Scout->CollisionHeight);
					if ( !findScoutStart(Actor->Location) || (Scout->Region.Zone != Actor->Region.Zone) )
						Level->FarMoveActor(Scout, Actor->Location, 1, 1);
					Scout->SetCollisionSize(MINCOMMONRADIUS, Scout->CollisionHeight);
				}
				UClass* pathClass = FindObjectChecked<UClass>( ANY_PACKAGE, TEXT("WarpZoneMarker") );
				AWarpZoneMarker *newMarker = (AWarpZoneMarker *)Level->SpawnActor(pathClass, NAME_None, NULL, NULL, Scout->Location);
				newMarker->markedWarpZone = (AWarpZoneInfo *)Actor;
			}
			else if ( Actor->IsA(AInventory::StaticClass()) )
			{
				if ( !findScoutStart(Actor->Location) || (Abs(Scout->Location.Z - Actor->Location.Z) > Scout->CollisionHeight) )
					Level->FarMoveActor(Scout, Actor->Location + FVector(0,0,40 - Actor->CollisionHeight), 1, 1);
				UClass *pathClass = FindObjectChecked<UClass>( ANY_PACKAGE, TEXT("InventorySpot") );
				AInventorySpot *newMarker = (AInventorySpot *)Level->SpawnActor(pathClass, NAME_None, NULL, NULL, Scout->Location);
				newMarker->markedItem = (AInventory *)Actor;
				((AInventory *)Actor)->myMarker = newMarker;
			}
		}
	}

	//calculate and add reachspecs to pathnodes
	debugf(NAME_DevPath,TEXT("Add reachspecs"));
	INT i;
	for (i=0; i<Level->Actors.Num(); i++)
	{
		AActor *Actor = Level->Actors(i); 
		if (Actor && Actor->IsA(ANavigationPoint::StaticClass()))
		{
			((ANavigationPoint *)Actor)->nextNavigationPoint = Level->GetLevelInfo()->NavigationPointList;
			Level->GetLevelInfo()->NavigationPointList = (ANavigationPoint *)Actor;
			addReachSpecs(Actor);
			debugf( NAME_DevPath, TEXT("Added reachspecs to %s"),Actor->GetName() );
		}
	}

	debugf(NAME_DevPath,TEXT("Added %d reachspecs"), Level->ReachSpecs.Num()); 
	//remove extra reachspecs from teleporters


	//prune excess reachspecs
	debugf(NAME_DevPath,TEXT("Prune reachspecs"));
	ANavigationPoint *Nav = Level->GetLevelInfo()->NavigationPointList;
	int numPruned = 0;
	while (Nav)
	{
		numPruned += Prune(Nav);
		Nav = Nav->nextNavigationPoint;
	}
	debugf(NAME_DevPath,TEXT("Pruned %d reachspecs"), numPruned);

	// Generate VisNoReach list
	for (ANavigationPoint *Path=Level->GetLevelInfo()->NavigationPointList;
		Path!=NULL;
		Path=Path->nextNavigationPoint)
	{
		addVisNoReach(Path);
	}

	Level->DestroyActor(Scout);
	debugf(NAME_DevPath,TEXT("All done"));
	unguard;
}

//------------------------------------------------------------------------------------------------
//Private methods

/*	if Node is an acceptable waypoint between an upstream path and a downstreampath
	who are also connected, remove their connection
*/
int FPathBuilder::Prune(AActor *Node)
{
	guard(FPathBuilder::Prune);
	
	int pruned = 0;
	ANavigationPoint* NavNode = (ANavigationPoint *)Node;

	int n,j;
	FReachSpec alternatePath, straightPath, part1, part2;
	int i=0;
	while ( (i<16) && (NavNode->upstreamPaths[i] != -1) )
	{
		part1 = Level->ReachSpecs(NavNode->upstreamPaths[i]);
		n=0;
		while ( (n<16) && (NavNode->Paths[n] != -1) )
		{
			part2 = Level->ReachSpecs(NavNode->Paths[n]);
			INT straightPathIndex = specFor(part1.Start, part2.End);
			if (straightPathIndex != -1)
			{
				straightPath = Level->ReachSpecs(straightPathIndex);
				alternatePath = part1 + part2;
				if ( ((float)straightPath.distance * 1.2 >= (float)alternatePath.distance) 
					&& ((alternatePath <= straightPath) || straightPath.BotOnlyPath() 
						|| alternatePath.MonsterPath()) )
				{
					//prune straightpath
					pruned++;
					j=0;
					ANavigationPoint* StartNode = (ANavigationPoint *)(straightPath.Start);
					ANavigationPoint* EndNode = (ANavigationPoint *)(straightPath.End);
					//debugf("Prune reachspec %d from %s to %s because of %s", straightPathIndex,
					//	StartNode->GetName(), EndNode->GetName(), NavNode->GetName()); 
					while ( (j<15) && (StartNode->Paths[j] != straightPathIndex) )
						j++;
					if ( StartNode->Paths[j] == straightPathIndex )
					{
						while ( (j<15) && (StartNode->Paths[j] != -1) )
						{
							StartNode->Paths[j] = StartNode->Paths[j+1];
							j++;
						}
						StartNode->Paths[15] = -1;
					}

					j=0;
					while ( (j<15) && (StartNode->PrunedPaths[j] != -1) )
						j++;
					StartNode->PrunedPaths[j] = straightPathIndex;
					Level->ReachSpecs(straightPathIndex).bPruned = 1;

					j=0;
					while ( (j<15) && (EndNode->upstreamPaths[j] != straightPathIndex) )
						j++;
					if ( EndNode->upstreamPaths[j] == straightPathIndex )
					{
						while ( (j<15) && (EndNode->upstreamPaths[j] != -1) )
						{
							EndNode->upstreamPaths[j] = EndNode->upstreamPaths[j+1];
							j++;
						}
						EndNode->upstreamPaths[15] = -1;
					}
					// Note that all specs remain in reachspec list and still referenced in PrunedPaths[]
					// but removed from visible Navigation graph:
				}
			}
			n++;
		}
		i++;
	}

	return pruned;
	unguard;
}

int FPathBuilder::specFor(AActor* Start, AActor* End)
{
	guard(FPathBuilder::specFor);

	FReachSpec pathSpec;
	ANavigationPoint* StartNode = (ANavigationPoint *)Start;
	int i=0;
	while ( (i<16) && (StartNode->Paths[i] != -1) )
	{
		pathSpec = Level->ReachSpecs(StartNode->Paths[i]);
		if (pathSpec.End == End)
			return StartNode->Paths[i];
		i++;
	}
	return -1;
	unguard;
}

/* insert a reachspec into the array, ordered by longest to shortest distance.
However, if there is no room left in the array, remove longest first
*/
int FPathBuilder::insertReachSpec(INT *SpecArray, FReachSpec &Spec)
{
	guard(FPathBuilder::insertReachSpec);

	int n = 0;
	while ( (n < 16) && (SpecArray[n] != -1) && (Level->ReachSpecs(SpecArray[n]).distance > Spec.distance) )
		n++;
	int pos = n;

	if (SpecArray[15] == -1) //then not full
	{
		if (SpecArray[n] != -1)
		{
			int current = SpecArray[n];
			while (n < 15)
			{
				int next = SpecArray[n+1];
				SpecArray[n+1] = current;
				current = next;
				if (next == -1)
					n = 15;
				n++;
			}
		}
	}
	else if (n == 0) // current is bigger than biggest
	{
		//debugf("Node out of Reachspecs - don't add!");
		return -1;
	}
	else //full, so remove from front
	{
		//debugf("Node out of Reachspecs -add at %d !", pos-1);

		n--;
		pos = n;
		int current = SpecArray[n];
		while (n > 0)
		{
			int next = SpecArray[n-1];
			SpecArray[n-1] = current;
			current = next;
			n--;
		}
	}
	return pos;

	unguard;
}

/* AddVisNoReach()
To the start NavigationPoint's VisNoReach array, add pathnodes which are visible, but not reachable by a direct path,
and are within 3000 units
*/

void FPathBuilder::addVisNoReach(AActor *start)
{
	guard(FPathBuilder::addVisNoReach);

	ANavigationPoint *node = (ANavigationPoint *)start;
	//debugf("Add Reachspecs for node at (%f, %f, %f)", node->Location.X,node->Location.Y,node->Location.Z);

	if ( node->IsA(ALiftCenter::StaticClass()) )
		return;

	Scout->SetCollisionSize( HUMANRADIUS, HUMANHEIGHT );
	Level->FarMoveActor(Scout, node->Location, 1);
	Scout->MoveTarget = node;
	Scout->bCanDoSpecial = 1;
	AActor* newPath;
	INT i = 0;

	//debugf("VisNoReach for %s", node->GetName());
	for (ANavigationPoint *Path=Level->GetLevelInfo()->NavigationPointList;
		Path!=NULL;
		Path=Path->nextNavigationPoint)
	{
		FLOAT distSq = (node->Location - Path->Location).SizeSquared(); 
		if ( !Path->IsA(ALiftCenter::StaticClass()) && (Path != node) 
			&& (distSq < 4000000) && (i < 16) ) //FIXME - Pick right value!!!
		{
			FCheckResult Hit(1.0);
			Level->SingleLineCheck(Hit, Scout, Path->Location, node->Location, TRACE_VisBlocking);
			if ( !Hit.Actor )
			{
				FLOAT pathDist;
				if ( Scout->findPathToward(Path, 0, newPath, 1) )
					pathDist = ((ANavigationPoint *)newPath)->visitedWeight;
				else
				{
					pathDist = 200000000;
					//debugf(TEXT("NO PATH from %s to %s"), node->GetName(), Path->GetName());
				}
				//debugf("Path cost to %s = %f vs. dist %f",Path->GetName(), pathDist, distSq);
				if ( (pathDist != 10000000)
					&& (pathDist * pathDist > 4 * distSq) )
				{
					//debugf("Add %s to %s", Path->GetName(), node->GetName());
					node->VisNoReachPaths[i] = Path;
					i++;
				}
			}
		}
	}
	unguard;
}

/* add reachspecs to path for every path reachable from it. Also add the reachspec to that
paths upstreamPath list
*/
void FPathBuilder::addReachSpecs(AActor *start)
{
	guard(FPathBuilder::addReachspecs);

	FReachSpec newSpec;
	ANavigationPoint *node = (ANavigationPoint *)start;
	//debugf("Add Reachspecs for node at (%f, %f, %f)", node->Location.X,node->Location.Y,node->Location.Z);

	if ( node->IsA(ALiftCenter::StaticClass()) )
	{
		FName myLiftTag = ((ALiftCenter *)node)->LiftTag;
		for (INT i=0; i<Level->Actors.Num(); i++)
		{
			AActor *Actor = Level->Actors(i); 
			if ( Actor && Actor->IsA(ALiftExit::StaticClass()) && (((ALiftExit *)Actor)->LiftTag == myLiftTag) ) 
			{
				newSpec.Init();
				newSpec.CollisionRadius = 60;
				newSpec.CollisionHeight = 60;
				newSpec.reachFlags = R_SPECIAL;
				newSpec.Start = node;
				newSpec.End = Actor;
				newSpec.distance = 500;
				int pos = insertReachSpec(node->Paths, newSpec);
				if (pos != -1)
				{
					int iSpec = Level->ReachSpecs.AddItem(newSpec);
					node->Paths[pos] = iSpec;
					pos = insertReachSpec(((ANavigationPoint *)Actor)->upstreamPaths, newSpec);
					if (pos != -1)
						((ANavigationPoint *)Actor)->upstreamPaths[pos] = iSpec;
				}
				newSpec.Init();
				newSpec.CollisionRadius = 60;
				newSpec.CollisionHeight = 60;
				newSpec.reachFlags = R_SPECIAL;
				newSpec.Start = Actor;
				newSpec.End = node;
				newSpec.distance = 500;
				pos = insertReachSpec(((ANavigationPoint *)Actor)->Paths, newSpec);
				if (pos != -1)
				{
					int iSpec = Level->ReachSpecs.AddItem(newSpec);
					((ANavigationPoint *)Actor)->Paths[pos] = iSpec;
					pos = insertReachSpec(node->upstreamPaths, newSpec);
					if (pos != -1)
						node->upstreamPaths[pos] = iSpec;
				}
			}
		}
		return;
	}

	if ( node->IsA(ATeleporter::StaticClass()) || node->IsA(AWarpZoneMarker::StaticClass()) )
	{
		for (INT i=0; i<Level->Actors.Num(); i++)
		{
			int bFoundMatch = 0;
			AActor *Actor = Level->Actors(i); 
			if ( node->IsA(ATeleporter::StaticClass()) )
			{
				if ( Actor && Actor->IsA(ATeleporter::StaticClass()) && (Actor != node) ) 
					bFoundMatch = (((ATeleporter *)node)->URL==*Actor->Tag);
			}
			else if ( Actor && Actor->IsA(AWarpZoneMarker::StaticClass()) && (Actor != node) )
				bFoundMatch = (((AWarpZoneMarker *)node)->markedWarpZone->OtherSideURL==*((AWarpZoneMarker *)Actor)->markedWarpZone->ThisTag);

			if ( bFoundMatch )
			{
				newSpec.Init();
				newSpec.CollisionRadius = 150;
				newSpec.CollisionHeight = 150;
				newSpec.reachFlags = R_SPECIAL;
				newSpec.Start = node;
				newSpec.End = Actor;
				newSpec.distance = 100;
				int pos = insertReachSpec(node->Paths, newSpec);
				if (pos != -1)
				{
					int iSpec = Level->ReachSpecs.AddItem(newSpec);
					//debugf("     Add teleport reachspec %d to node at (%f, %f, %f)", iSpec, Actor->Location.X,Actor->Location.Y,Actor->Location.Z);
					node->Paths[pos] = iSpec;
					pos = insertReachSpec(((ANavigationPoint *)Actor)->upstreamPaths, newSpec);
					if (pos != -1)
						((ANavigationPoint *)Actor)->upstreamPaths[pos] = iSpec;
				}
				break;
			}
		}
	}

	for (INT i=0; i<Level->Actors.Num(); i++)
	{
		AActor *Actor = Level->Actors(i); 
		if (Actor && Actor->IsA(ANavigationPoint::StaticClass()) && !Actor->IsA(ALiftCenter::StaticClass()) 
				&& (Actor != node) && ((node->Location - Actor->Location).SizeSquared() < 1000000)
				&& (!node->bOneWayPath || (((Actor->Location - node->Location) | node->Rotation.Vector()) > 0)) )
		{
			if ( (Actor->Location - node->Location).SizeSquared() < 1000 )
				debugf(TEXT("WARNING: %s and %s may be too close!"), Actor->GetName(), node->GetName());
			newSpec.Init();
			if (newSpec.defineFor(node, Actor, Scout))
			{
				int pos = insertReachSpec(node->Paths, newSpec);
				if (pos != -1)
				{
					int iSpec = Level->ReachSpecs.AddItem(newSpec);
					//debugf("     Add reachspec %d to node at (%f, %f, %f)", iSpec, Actor->Location.X,Actor->Location.Y,Actor->Location.Z);
					node->Paths[pos] = iSpec;
					pos = insertReachSpec(((ANavigationPoint *)Actor)->upstreamPaths, newSpec);
					if (pos != -1)
						((ANavigationPoint *)Actor)->upstreamPaths[pos] = iSpec;
				} 
			}
		}
	}
	unguard;
}

/* createPaths()
build paths for a given pawn type (which Scout is emulating)
*/
int FPathBuilder::createPaths (int optimization)
{
	guard(FPathBuilder::createPaths);

	int numpaths = 0;
	numMarkers = 0;

	//Add a permanent+leftturn+beacon+marked path for every pathnode actor in the level
	int newMarker;
	for (INT i=0; i<Level->Actors.Num(); i++) 
	{
		AActor *Actor = Level->Actors(i);
		if (Actor)
		{
			if (Actor->IsA(APathNode::StaticClass()))
			{
				DebugPrint(TEXT("Found a Pathnode"));
				newMarker = addMarker();
				pathMarkers[newMarker].initialize(Actor->Location,FVector(0,0,0),1,1,1);
				pathMarkers[newMarker].permanent = 1;
			}
		}
	}

	// build paths from every pawn or inventory position in level
	//FIXME - also start from existing navigationpoints?
	INT i;
	for (i=0; i<Level->Actors.Num(); i++) 
	{
		AActor *Actor = Level->Actors(i);
		if ( Actor && 
			(Actor->IsA(APawn::StaticClass()) || Actor->IsA(AInventory::StaticClass())) )
		{
			debugf(TEXT("----------------------Starting From %s"), Actor->GetName());
			createPathsFrom(Actor->Location); 
		}
	}
	
	//iteratively fill out paths by checking for obstructions and by checking for new connections
	
	DebugInt(TEXT("Markers before obstruction check ="), numMarkers);
	DebugPrint(TEXT("Check obstructions--------------------------------------------------"));

	for (i=0; i<numMarkers; i++)  
		if (pathMarkers[i].marked)
		{
			DebugInt(TEXT("Check obstruction at"),i);
			DebugInt(TEXT("Out of"), numMarkers);
			checkObstructionFrom(&pathMarkers[i]); //only check from left turns at opt2	
			pathMarkers[i].marked = 0; //once the obstruction is mapped, remove the path
		}

	// merge and prune left turn markers (based on reachability to beacons)
	for (i=0; i<numMarkers; i++)  
	{
		if (pathMarkers[i].leftTurn)
			mergePath(i);
	}

	DebugPrint(TEXT("Build Paths"));
	//Now create an actor for all remaining left turn markers (except permanent paths which already have a node
	for (i=0; i<numMarkers; i++)  
	{
		if (pathMarkers[i].leftTurn && !pathMarkers[i].permanent)
		{
			newPath(pathMarkers[i].Location);
			numpaths++;
		}
	}
	
	for (i=0; i<Level->Actors.Num(); i++) 
	{
		AActor *Actor = Level->Actors(i);
		if (Actor && (Actor->IsA(APawn::StaticClass())))
			Actor->SetCollision(1, 1, 1); //turn Pawn collision back on
	}

	DebugInt(TEXT("Optimization Level = "), optimization);
	DebugInt(TEXT("Number of Markers ="),numMarkers);
	return numpaths; 
	unguard;
}

//newPath() 
//- add new pathnode to level at position spot
void FPathBuilder::newPath(FVector spot)
{
	guard(FPathBuilder::newPath);
	
	if (Scout->CollisionHeight < 48) // fixme - base on Skaarj final height
		spot.Z = spot.Z + 48 - Scout->CollisionHeight;
	UClass *pathClass = FindObjectChecked<UClass>( ANY_PACKAGE, TEXT("PathNode") );
	APathNode *addedPath = (APathNode *)Level->SpawnActor( pathClass, NAME_None, NULL, NULL, spot );
	//clear pathnode reachspec lists
	for (INT i=0; i<16; i++)
	{
		addedPath->Paths[i] = -1;
		addedPath->upstreamPaths[i] = -1;
	}

	unguard;
	};


/*getScout()
Find the scout actor in the level. If none exists, add one.
*/ 

void FPathBuilder::getScout()
{
	guard(FPathBuilder::getScout);
	Scout = NULL;
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor *Actor = Level->Actors(i); 
		if (Actor && Actor->IsA(AScout::StaticClass()))
			Scout = (APawn *)Actor;
	}
	if( !Scout )
	{
		UClass *scoutClass = FindObjectChecked<UClass>( ANY_PACKAGE, TEXT("Scout") );
		Scout = (APawn *)Level->SpawnActor( scoutClass );
	}
	Scout->SetCollision(1,1,1);
	Scout->bCollideWorld = 1;
	Level->SetActorZone( Scout,1,1 );
	return;
	unguard;
}


int FPathBuilder::findScoutStart(FVector start)
{
	guard(FPathBuilder::findScoutStart);
	
	if (Level->FarMoveActor(Scout, start)) //move Scout to starting point
	{
		//slide down to floor
		FCheckResult Hit(1.0);
		FVector Down = FVector(0,0, -50);
		Hit.Normal.Z = 0.0;
		INT iters = 0;
		while (Hit.Normal.Z < 0.7)
		{
			Level->MoveActor(Scout, Down, Scout->Rotation, Hit, 1,1);
			if ((Hit.Time < 1.0) && (Hit.Normal.Z < 0.7)) 
			{
				//adjust and try again
				FVector OldHitNormal = Hit.Normal;
				FVector Delta = (Down - Hit.Normal * (Down | Hit.Normal)) * (1.0 - Hit.Time);
				if( (Delta | Down) >= 0 )
				{
					Level->MoveActor(Scout, Delta, Scout->Rotation, Hit, 1,1);
					if ((Hit.Time < 1.0) && (Hit.Normal.Z < 0.7))
					{
						FVector downDir = Down.SafeNormal();
						Scout->TwoWallAdjust(downDir, Delta, Hit.Normal, OldHitNormal, Hit.Time);
						Level->MoveActor(Scout, Delta, Scout->Rotation, Hit, 1,1);
					}
				}
			}
			iters++;
			if (iters >= 50)
			{
				debugf(NAME_DevPath,TEXT("No valid start found"));
				return 0;
			}
		}
		//debugf(NAME_DevPath,"scout placed on valid floor");
		return 1;
 	}

	debugf(NAME_DevPath,TEXT("Scout didn't fit"));
	return 0;
	unguard;
}

//createPathsFrom() -
//create paths beginning from wall to the right of start location
void FPathBuilder::createPathsFrom(FVector start)
{
	guard(FPathBuilder::createPathsFrom);
	
	if ( (!findScoutStart(start) 
		|| (Abs(Scout->Location.Z - start.Z) > Scout->CollisionHeight))
		&& !findScoutStart(start + FVector(0,0,20)) )
		return;

	exploreWall(FVector(1,0,0));

	unguard;
}

/* checkmergeSpot()
worker function for mergePath()
*/
int FPathBuilder::checkmergeSpot(const FVector &spot, FPathMarker *path1, FPathMarker *path2)
{
	guard(FPathBuilder::checkmergeSpot);
	int acceptable = 1;
	FLOAT oldRadius = Scout->CollisionRadius;
	
	for (INT i=0; i<numMarkers; i++) //check if reachable path list changed
	{
		if (acceptable && pathMarkers[i].visible && pathMarkers[i].beacon)
		{
			Scout->SetCollisionSize(MINCOMMONRADIUS, Scout->CollisionHeight);
			if (!fullyReachable(spot,pathMarkers[i].Location))
			{
				path1->leftTurn = 0;
				path2->leftTurn = 0; //don't use either of these as a waypoint
				acceptable = findPathTo(pathMarkers[i].Location);
				path1->leftTurn = 1;
				path2->leftTurn = 1;
			}
			//if (!acceptable) DebugVector("failed because of",pathMarkers[i].Location);
		}
		if (acceptable && pathMarkers[i].bigvisible && pathMarkers[i].leftTurn)
		{
			Scout->SetCollisionSize(Max(path1->radius, path2->radius), Scout->CollisionHeight);
			if (!fullyReachable(spot,pathMarkers[i].Location))
			{
				path1->leftTurn = 0;
				path2->leftTurn = 0; //don't use either of these as a waypoint
				acceptable = findPathTo(pathMarkers[i].Location);
				path1->leftTurn = 1;
				path2->leftTurn = 1;
			}
			//if (!acceptable) DebugVector("failed because of",pathMarkers[i].Location);
		}
	}
	Scout->SetCollisionSize(oldRadius, Scout->CollisionHeight);
	return acceptable;
	unguard;
}

int FPathBuilder::markReachableFromTwo(FPathMarker *path1, FPathMarker *path2)
{
	guard(FPathBuilder::markReachableFromTwo);

	FLOAT oldRadius = Scout->CollisionRadius;
	//mark human reachable as visible
	Scout->CollisionRadius = MINCOMMONRADIUS;
	markReachable(path1->Location);  //mark all markers reachable from marker 1
	int addedmarkers = 0;
	for (INT j=0; j<numMarkers; j++) // add those reachable from marker 2
	{
		if (!pathMarkers[j].visible && pathMarkers[j].beacon 
			&& ((path2->Location - pathMarkers[j].Location).SizeSquared() < 640000) )
		{
			pathMarkers[j].visible = fullyReachable(path2->Location, pathMarkers[j].Location);
			if (pathMarkers[j].visible)
				addedmarkers = 1;
		}
	}

	// mark big radius reachable leftturns as bigvisible
	Scout->SetCollisionSize(Max(path1->radius, path2->radius), Scout->CollisionHeight);
	if (Scout->CollisionRadius > MINCOMMONRADIUS)
	{
		for (INT i=0; i<numMarkers; i++) 
		{
			if (pathMarkers[i].leftTurn) 
				pathMarkers[i].bigvisible = fullyReachable(path1->Location,pathMarkers[i].Location);
		}
		INT j;
		for (j=0; j<numMarkers; j++) // add those reachable from marker 2
		{
			if (!pathMarkers[j].bigvisible && pathMarkers[j].leftTurn)
			{
				pathMarkers[j].bigvisible = fullyReachable(path2->Location, pathMarkers[j].Location);
				if (pathMarkers[j].bigvisible)
					addedmarkers = 1;
			}
		}
	}
	Scout->SetCollisionSize(oldRadius, Scout->CollisionHeight);
	return addedmarkers;
	unguard;
}

/* mergePath()
examine reachable path nodes. Try to merge this pathnode with another pathnode.  
Between it and every nearby reachable permanent pathnode, look for a point at which
all beacon pathnodes reachable from either can be reached from this point 

To be merged, the candidate point must support full reachability both at a human radius, and at the
maximum of the two path node radii.
*/
void FPathBuilder::mergePath(INT iMarker)
{
	guard(FPathBuilder::premergePath);
	
	FPathMarker *marker = &pathMarkers[iMarker];

	//check not floating
	FCheckResult Hit(1.0);
	Level->SingleLineCheck(Hit, NULL, marker->Location - FVector(0,0,Scout->MaxStepHeight + MINCOMMONHEIGHT), marker->Location, TRACE_VisBlocking);  
	if ( Hit.Time == 1.0 ) //floating
	{
		marker->leftTurn = 0; //mark other node for removal
		marker->beacon = 0; //other node no longer beacon
		return;
	}

	marker->radius = MINCOMMONRADIUS;
	FLOAT maxmergesqr = 2 * MAXCOMMONRADIUS * MAXCOMMONRADIUS; 
	for (INT i=0; i<numMarkers; i++) 
	{
		FPathMarker *candidate = &pathMarkers[i];
		if (candidate->leftTurn && !candidate->permanent && (i != iMarker))
		{
			Scout->SetCollisionSize(MINCOMMONRADIUS, MINCOMMONHEIGHT);
			if ( ((marker->Location - candidate->Location).SizeSquared() < maxmergesqr)  
				&& fullyReachable(marker->Location, candidate->Location) )
			{
				DebugVector(TEXT("Try to pre-merge path at"), marker->Location);
				DebugVector(TEXT("And path at"), candidate->Location);
				// test at MINCOMMONRADIUS, COMMONRADIUS, and MAXCOMMONRADIUS
				// see if one path or middle is superset of all reachability
				INT markerAcceptable = !candidate->permanent;
				INT candidateAcceptable = !marker->permanent;
				INT centerAcceptable = markerAcceptable && candidateAcceptable;
				FVector Center = ( candidate->mergeweight * candidate->Location + marker->mergeweight * marker->Location)/(candidate->mergeweight + marker->mergeweight);
				INT j = 0;
				while ( j<numMarkers )
				{
					if ( !markerAcceptable && !candidateAcceptable && !centerAcceptable )
						j = numMarkers;
					else
					{
						FLOAT BestMarkerRadius = 0;
						FLOAT BestCenterRadius = 0;
						FLOAT BestCandidateRadius = 0;
						Scout->SetCollisionSize(MINCOMMONRADIUS, MINCOMMONHEIGHT);
						if ( boundedReachable(marker->Location, pathMarkers[j].Location) )
							BestMarkerRadius = MINCOMMONRADIUS;
						if ( boundedReachable(candidate->Location, pathMarkers[j].Location) )
							BestCandidateRadius = MINCOMMONRADIUS;
						if ( centerAcceptable && boundedReachable(Center, pathMarkers[j].Location) )
							BestCenterRadius = MINCOMMONRADIUS;

						Scout->SetCollisionSize(COMMONRADIUS, MINCOMMONHEIGHT);
						if ( (BestMarkerRadius == MINCOMMONRADIUS)
							&& boundedReachable(marker->Location, pathMarkers[j].Location) )
							BestMarkerRadius = COMMONRADIUS;
						if ( (BestCandidateRadius == MINCOMMONRADIUS)
							&& boundedReachable(candidate->Location, pathMarkers[j].Location) )
							BestCandidateRadius = COMMONRADIUS;
						if ( centerAcceptable && (BestCenterRadius == MINCOMMONRADIUS)
							&& boundedReachable(Center, pathMarkers[j].Location) )
							BestCenterRadius = COMMONRADIUS;

						Scout->SetCollisionSize(MAXCOMMONRADIUS, MINCOMMONHEIGHT);
						if ( (BestMarkerRadius ==  COMMONRADIUS)
							&& boundedReachable(marker->Location, pathMarkers[j].Location) )
							BestMarkerRadius = MAXCOMMONRADIUS;
						if ( (BestCandidateRadius ==  COMMONRADIUS)
							&& boundedReachable(candidate->Location, pathMarkers[j].Location) )
							BestCandidateRadius = MAXCOMMONRADIUS;
						if ( centerAcceptable && (BestCenterRadius ==  COMMONRADIUS)
							&& boundedReachable(Center, pathMarkers[j].Location) )
							BestCenterRadius = MAXCOMMONRADIUS;

						markerAcceptable == markerAcceptable && (BestMarkerRadius >= BestCandidateRadius);
						candidateAcceptable == candidateAcceptable && (BestCandidateRadius >= BestMarkerRadius);
						centerAcceptable == centerAcceptable && (BestCenterRadius >= BestMarkerRadius)
											&& (BestCenterRadius >= BestCandidateRadius);
					}
					j++;
				}
				
				FVector MergeSpot = candidate->Location;
				if ( centerAcceptable )
					MergeSpot = Center;
				else if ( markerAcceptable )
					MergeSpot = marker->Location;

				if ( centerAcceptable || markerAcceptable || candidateAcceptable ) //found an acceptable merge point
				{
					DebugVector(TEXT("Successful merge at"), MergeSpot);
					marker->Location = MergeSpot; //move to merge point
					marker->mergeweight += 1;
					candidate->leftTurn = 0; //mark other node for removal
					candidate->beacon = 0; //other node no longer beacon
				}
			}
		}
	}
	
	return;
	unguard;
}

//checkObstructionsFrom() -
//pathnode was marked as not being created as result of left turn - which means some other (possibly unmapped)
//obstruction created it.  Explore this obstruction
//FIXME - why does it ever fail to find a path to the obstructed marker if the obstruction has been mapped?
//(e.g. cases where findPathTo fails, but no leftturn markers added to obstruction)
void FPathBuilder::checkObstructionFrom(FPathMarker *marker)
{
	guard(FPathBuilder::checkObstructionFrom);

	if (!Level->FarMoveActor(Scout, marker->Location, 0 ,1))
		debugf(NAME_DevPath,TEXT("obstruction far move failed"));
	Level->DropToFloor(Scout); //FIXME - why do I need this?
	if (marker->leftTurn) //if this is a left turn marker, then walk at current direction (look for outside wall)
	{
		DebugPrint(TEXT("exploring out from left turn"));
		exploreWall(marker->Direction);
	}
	else
	{
		markLeftReachable(marker->Location);
		FCheckResult Hit(1.0);
		Scout->walkMove(marker->Direction * 16.0, Hit, NULL, 4.1, 0);
	
		for (INT i=0; i<numMarkers; i++) //check if visible+reachable path list changed
		{
			FPathMarker *checkMarker = &pathMarkers[i];
			if ( checkMarker->visible )
			{
				Level->SingleLineCheck(Hit, Scout, checkMarker->Location, Scout->Location, TRACE_VisBlocking); 
				if ( (Hit.Time < 1.0) && !findPathTo(checkMarker->Location) )
				{
					DebugPrint(TEXT("found the obstruction"));
					FVector moveDirection = checkMarker->Location - Scout->Location;
					moveDirection.Z = 0;
					moveDirection.Normalize();
					exploreWall(moveDirection);
				}
			}
		}
	}
	return;
	unguard;
}

void FPathBuilder::exploreWall(FVector moveDirection)
{
	guard(FPathBuilder::exploreWall);
	int stillmoving = 1;

	Scout->SetCollisionSize(MAXCOMMONRADIUS, MINCOMMONHEIGHT);
	FCheckResult Hit(1.0);
	while (stillmoving == 1)
		stillmoving = Scout->walkMove(moveDirection * 16.0, Hit, NULL, 4.1, 0); 

	//  follow wall
	int oldMarkers = numMarkers;
	FVector BlockNormal = -1 * moveDirection;
	FindBlockingNormal(BlockNormal);

	followWall(FVector(BlockNormal.Y, -1 * BlockNormal.X, 0)); 
	DebugInt(TEXT("New paths created"), numMarkers - oldMarkers);

	return;
	unguard;
}

/*
needPath()
checks if any paths or markers marked reachable are no longer reachable from start
if so, returns true
*/

int FPathBuilder::needPath(const FVector &start)
{
	guard(FPathBuilder::needPath);

	FCheckResult Hit(1.0);
	for (INT i=0; i<numMarkers; i++) //check if visible+reachable path list changed
		if (pathMarkers[i].visible && pathMarkers[i].beacon 
			&& ((start - pathMarkers[i].Location).SizeSquared() < 640000) )
		{
			Level->SingleLineCheck(Hit, Scout, pathMarkers[i].Location, start, TRACE_VisBlocking); 
			if ( (Hit.Time < 1.0) && !findPathTo(pathMarkers[i].Location) )
				return 1;
		}

	return 0;
	unguard;
}

int FPathBuilder::sawNewLeft(const FVector &start)
{
	guard(FPathBuilder::sawNewLeft);

	for (INT i=0; i<numMarkers; i++) //check if visible+reachable path list changed
		if ( !pathMarkers[i].visible && !pathMarkers[i].routable && pathMarkers[i].leftTurn
			&& ((start - pathMarkers[i].Location).SizeSquared() < 640000) 
			&& fullyReachable(start,pathMarkers[i].Location) )
				return 1; //vis+reach changed - look for an acceptable waypoint

	return 0;
	unguard;
}
/*
markReachable()
marks all beacon paths and markers as reachable or not from start.
*/

void FPathBuilder::markReachable(const FVector &start)
{
	guard(FPathBuilder::markReachable);

	for (INT i=0; i<numMarkers; i++) 
		if (pathMarkers[i].beacon && ((start - pathMarkers[i].Location).SizeSquared() < 640000) ) 
			pathMarkers[i].visible = fullyReachable(start,pathMarkers[i].Location);

	unguard;
}

/*
markLeftReachable()
marks all left turn markers as reachable or not from start.
*/

void FPathBuilder::markLeftReachable(const FVector &start)
{
	guard(FPathBuilder::markLeftReachable);

	FCheckResult Hit(1.0);
	for (INT i=0; i<numMarkers; i++) 
	{
		if ( (start - pathMarkers[i].Location).SizeSquared() < 640000 )
		{
			pathMarkers[i].visible = 0;
			pathMarkers[i].routable = 0;
			Level->SingleLineCheck(Hit, Scout, pathMarkers[i].Location, start, TRACE_VisBlocking); 
			if ( Hit.Time == 1.0 )
				pathMarkers[i].visible = 1;
		}
		else
			pathMarkers[i].visible = 0;
	}

	unguard;
}
/* oneWaypointTo()
looks for a NEARBY permanent waypoint which will reach to upstream spot.  Since scout has just made a left turn,
we are looking for a closeby waypoint which was also dropped as a result of the left turn (perhaps with
a different collision radius).  
*/
int FPathBuilder::oneWaypointTo(const FVector &upstreamSpot)
{
	guard(FPathBuilder::oneWaypointTo);
	int success = 0;
	FLOAT maxdistSquared = MAXWAYPOINTDIST * MAXWAYPOINTDIST * Scout->CollisionRadius * Scout->CollisionRadius;
	for (INT i=0; i<numMarkers; i++) 
	{
		if (!success && pathMarkers[i].leftTurn)
		{
			FVector distance = pathMarkers[i].Location - Scout->Location;
			if (distance.SizeSquared() < maxdistSquared)
				success = (fullyReachable(pathMarkers[i].Location, upstreamSpot) && fullyReachable(Scout->Location,pathMarkers[i].Location));
		}			
	}
	if (success)
		DebugPrint(TEXT("Found an acceptable alternate left turn marker"));
	return success;
	unguard;
}

/* addMarker()
returns index to a new marker
*/
INT FPathBuilder::addMarker()
{
	guard(FPathBuilder::addMarker);
	if (numMarkers < MAXMARKERS - 1)
		numMarkers++;
	else  //try to remove an old obstruction marker
	{
		int compressed = 0;
		INT i = 0;
		while (!compressed)
		{
			if (pathMarkers[i].removable())
			{
				pathMarkers[i] = pathMarkers[numMarkers - 1];
				compressed = 1;
			}
			i++;
			if (i == MAXMARKERS)
				compressed = 1;
		}
		DebugPrint(TEXT("RAN OUT OF MARKERS!"));
	}
	if (numMarkers > MAXMARKERS - 10) DebugInt(TEXT("ADDED MARKER #"), numMarkers);
	return (numMarkers - 1);
	unguard;
}

//followWall() - 
//follow wall, always keeping wall to my left
//look for reachable paths, and drop paths when my reachability list changes by removal
//stop when either I touch a path which was placed while going in the same direction as my
//current direction, or if I pass the last path I dropped
//Compare current direction to heading for start location.  
//If angle >= 90 degrees then I've passed it , and if NetYaw > 360, I've gone all the way around
//FIXME - to improve speed, remove redundant reachability checks
void FPathBuilder::followWall(FVector currentDirection)
{
	guard(FPathBuilder::followWall);
	int stillmoving = 1;  
	FVector newDirection;
	FLOAT NetYaw = 0.0;
	INT LastDropped = 0;
	INT LastRightTurn = 0;
	int turnedLeft = 0;
	FVector tempV;
	int turning = 0;
	FVector upstreamSpot = Scout->Location;
	FVector startLocation = Scout->Location;
	int keepMapping = 1;
	DebugVector(TEXT("Following wall"), currentDirection);
	int stepcount = 0;
	FCheckResult Hit(1.0);
	FVector Up(0,0,2);
	FVector Down(0,0,-2);
	FVector oldPosition = Scout->Location + FVector(2,2,2);
	int newTurn = 0;
	FVector realPosition = Scout->Location;

	while (keepMapping)
	{
		newTurn = 0;
		oldPosition = Scout->Location;
		FVector oldDirection = currentDirection;
		if (checkLeftPassage(currentDirection)) //made a left turn
		{
			debugf(TEXT("made left turn"));
			NetYaw = NetYaw - 90.0;
			if (!fullyReachable(Scout->Location, upstreamSpot))  //can I still reach my anchor? 
				if (!oneWaypointTo(upstreamSpot)) //check if there is a nearby legal waypoint 
				{
					upstreamSpot = oldPosition;
					newTurn = 1;
					LastDropped = addMarker();
					turnedLeft = 1;
					pathMarkers[LastDropped].initialize(oldPosition,oldDirection,1,1,1);
					NetYaw = 0.0;
					startLocation = oldPosition;
					debugf(NAME_DevPath,TEXT("made left turn marker %d"),LastDropped);
					stepcount = 0;
				}
		}
		else
		{		
			stillmoving = Scout->walkMove(currentDirection * 16.0, Hit, NULL, 4.1, 0); 
			realPosition = Scout->Location;

			if (stillmoving == 1) //check for interior obstructions
			{	
				markLeftReachable(oldPosition); //mark all visible/reachable turn and permanent paths from oldPosition
				if (needPath(Scout->Location)) //check if I need an obstruction marker at oldPosition
				{
 					if (!fullyReachable(Scout->Location, upstreamSpot) && !oneWaypointTo(upstreamSpot)
						&& (Abs(upstreamSpot.Z - Scout->Location.Z) > 1 + Scout->MaxStepHeight))
					{
							LastDropped = addMarker(); //its a stairway/ramp
							pathMarkers[LastDropped].initialize(oldPosition,oldDirection,0,1,1);
							pathMarkers->stair = 1;
							upstreamSpot = oldPosition;
							NetYaw = 0.0;
							startLocation = oldPosition;
							DebugVector(TEXT("marked stairway at"), oldPosition);
							stepcount = 0;
					}
					else
					{
						newTurn = 1;
						LastDropped = addMarker();
						pathMarkers[LastDropped].initialize(oldPosition,oldDirection,1,0,0);
						NetYaw = 0.0;
						startLocation = oldPosition;
						DebugVector(TEXT("marked obstruction at"), oldPosition);
						stepcount = 0;
					}
				}
				else if (sawNewLeft(Scout->Location))
				{
					LastDropped = addMarker();
					pathMarkers[LastDropped].initialize(Scout->Location,-1 * currentDirection,1,0,0);
					NetYaw = 0.0;
					startLocation = Scout->Location;
					DebugVector(TEXT("marked out new obstruction at"), Scout->Location);
					stepcount = 0;
				}
			}
		}

		if (stillmoving == 1) //check if tour is complete
		{
			turning = 0; //not in a turn
			//Stop if I touch a marker with the same direction as my current direction
			FLOAT touchRangeSquared = Scout->CollisionRadius * Scout->CollisionRadius * 0.25;
			INT i = 0;
			while (i<numMarkers) 
			{
				if ( (i != LastDropped) 
					&& ((pathMarkers[i].Location - Scout->Location).SizeSquared() < touchRangeSquared) ) //touching path
				{
					DebugVector(TEXT("Near path at"), pathMarkers[i].Location);
					tempV = currentDirection - pathMarkers[i].Direction;
					tempV.Normalize();
					debugf(TEXT("Current dir %f %f path direction is %f %f"), currentDirection.X, currentDirection.Y, pathMarkers[i].Direction.X, pathMarkers[i].Direction.Y);
					keepMapping = !tempV.IsNearlyZero(); //check if direction same as when path was laid
					if (!keepMapping) 
					{
						i = numMarkers;
						DebugVector(TEXT("Touched a compatible marker at"), pathMarkers[i].Location);
						stepcount = 0;
					}
				}
				i++;
			}
		}	
		else //adjust direction clockwise (I couldn't go forward or left)
		{
			upstreamSpot = Scout->Location; //set anchor at turn point
			newTurn = 1;
			DebugVector(TEXT("turn right at"), Scout->Location);
			stepcount = 0;

			if (!turning) //mark turn
			{
				turning = 1;
				turnedLeft = 0;
				LastDropped = addMarker();
				LastRightTurn = LastDropped;
				pathMarkers[LastDropped].initialize(oldPosition,oldDirection,0,1,0);
				startLocation = oldPosition;
				NetYaw = 0.0;
				DebugPrint(TEXT("made right turn marker"));
			}
			FVector BlockNormal = -1 * currentDirection;
			FindBlockingNormal(BlockNormal);
			//if ( (currentDirection | BlockNormal) == 0 ) FIXME!!!!
				NetYaw += 90; 
			//else
			//	NetYaw += acos( currentDirection | BlockNormal );
			currentDirection = FVector(-1 * BlockNormal.Y, BlockNormal.X, 0);
			stillmoving = 1;
			DebugVector(TEXT("new direction"), currentDirection);
		}
				
		//Alternate test for stopping (in case pathnode touch fails because it was the lastDropped)
		if (keepMapping && (Abs(NetYaw) >= 360.0))  //have rotated all the way around from start
		{ 
			tempV = startLocation - Scout->Location;
			keepMapping = ((tempV | currentDirection) > 0.0); //keep mapping if angle < 90 degrees
			if (!keepMapping) DebugVector(TEXT("All the way around at"),Scout->Location);
		}
	} //while keepMapping

	return;
	unguard;
}

void FPathBuilder::FindBlockingNormal(FVector &BlockNormal)
{
	guard(FPathBuilder::FindBlockingNormal);

	FCheckResult Hit(1.0);
	Level->SingleLineCheck(Hit, Scout, Scout->Location - BlockNormal * 16, Scout->Location, TRACE_VisBlocking, Scout->GetCylinderExtent());
	if ( Hit.Time < 1.0 )
	{
		BlockNormal = Hit.Normal;
		return;
	}

	// find ledge
	FVector Destn = Scout->Location - BlockNormal * 16;
	FVector TestDown = FVector(0,0, -1 * Scout->MaxStepHeight);
	Level->SingleLineCheck(Hit, Scout, Destn + TestDown, Destn, TRACE_VisBlocking, Scout->GetCylinderExtent());
	if ( Hit.Time < 1.0 )
	{
		debugf(TEXT("Found landing when looking for ledge"));
		return;
	}
	Level->SingleLineCheck(Hit, Scout, Scout->Location + TestDown, Destn + TestDown, TRACE_VisBlocking, Scout->GetCylinderExtent());

	if ( Hit.Time < 1.0 )
		BlockNormal = Hit.Normal;
	
	unguard;
}

/* tryPathThrough()
recursively try to find a path to destination from Waypoint that fits into the distance budget
check budget first, since its cheaper than the reachability test
*/
int FPathBuilder::tryPathThrough(FPathMarker *Waypoint, const FVector &Destination, FLOAT budget)
{
	guard(FPathBuilder::tryPathThrough);

	int result = 0;
	if (fullyReachable(Waypoint->Location,Destination))
		result = 1; //I know it fits into my budget, since I wouldn't have tried this waypoint if it didn't
	else
	{
		Waypoint->budget = budget;
		FVector direction;
		FLOAT distance;
		FLOAT minTotal;
		FPathMarker *NextNode;

		for (INT iNext=0; iNext<numMarkers; iNext++)  //check all reachable pathnodes
		{
			NextNode = &pathMarkers[iNext];
			if (!result && NextNode->leftTurn)	
			{
				direction = Waypoint->Location - NextNode->Location;
				distance = direction.Size();
				direction = NextNode->Location - Destination;
				minTotal = distance + direction.Size();
				if ((NextNode->budget < (budget - distance)) && (minTotal < budget)) //check not visited with better budget, and current min distance fits budget
					if (fullyReachable(Waypoint->Location,NextNode->Location)) //if fits in budget, try this path
						result = tryPathThrough(NextNode,Destination, budget - distance);
			}
		}
	}
	return result;
	unguard;
}

/* findPathTo() -
// iDest is no longer reachable from Scout's position, but was from oldPosition
(because of some obstruction)  If the obstruction is marked, there is an almost straight path
to iDest - look for that
*/
int FPathBuilder::findPathTo(const FVector &Destination)
{
	guard(FPathBuilder::findPathTo);
	FVector direction = Destination - Scout->Location;
	FLOAT budget = direction.Size() + Scout->CollisionRadius * (1 + 2 * MAXWAYPOINTDIST); 
		
	//clear budgets (temp used for storing remaining budget through that pathnode)
	for (INT i=0; i<numMarkers; i++)  
	{
			pathMarkers[i].budget = 0.0;	
	}
	
	FPathMarker ScoutMarker;
	ScoutMarker.Location = Scout->Location;
	int acceptable = tryPathThrough(&ScoutMarker,Destination,budget);
	return acceptable;
	unguard;
}

/* checkLeft()
Worker function for checkLeftPassage()
*/
int FPathBuilder::checkLeft(FVector &leftDirection, FVector &currentDirection)
{
	guard(FPathBuilder::checkLeft);
	int leftTurn = 0;
	FVector oldLocation = Scout->Location;
	FCheckResult Hit(1.0);

	int walkresult = Scout->walkMove(leftDirection * 16.0, Hit, NULL, 4.1, 0);
	if (walkresult == 1) //check if it was a full move
	{
		FVector move = Scout->Location - oldLocation;
		walkresult = (move.Size() > 10.0);  //FIXME - problem with steep slopes?
	}
	if (walkresult == 1) //explore this path
	{
		DebugVector(TEXT("Follow left passage"), leftDirection);
		DebugVector(TEXT("Turned left at"), oldLocation);
		currentDirection.X = leftDirection.X;
		currentDirection.Y = leftDirection.Y;
		leftDirection.X = -1.0 * currentDirection.Y; //turn left 90 degrees
		leftDirection.Y = currentDirection.X;
		Scout->walkMove(leftDirection * 16.0, Hit, NULL, 4.1, 0); //get all the way over to wall
		leftTurn = 1;
		DebugVector(TEXT("New location"),Scout->Location);
	}
	// else if (walkresult == -1) //FIXME: mark ledge?
	else
		Level->FarMoveActor(Scout, oldLocation, 0, 1); //no left passage, go back to exploration

	return leftTurn;
	unguard;
}

/* checkLeftPassage()
Looks for a left turn.  Returns 1 if left turn was made, zero otherwise.
If possible, changes currentDirection, and moves Scout 
FIXME - should it be based on normal of barrier to the left?
*/
int FPathBuilder::checkLeftPassage(FVector &currentDirection)
{
	guard(FPathBuilder::checkLeftPassage);
	FVector oldLocation = Scout->Location;
	FVector leftDirection;
	leftDirection.X = -1.0 * currentDirection.Y; //turn left 90 degrees
	leftDirection.Y = currentDirection.X;
	leftDirection.Z = 0;
	int leftTurn = 0;
	int stillmoving = 1;
	FCheckResult Hit(1.0);

	leftTurn = checkLeft(leftDirection, currentDirection);

	if (!leftTurn)
	{
		stillmoving = Scout->walkMove(currentDirection * 6.0, Hit, NULL, 4.1, 0);
		leftTurn = checkLeft(leftDirection, currentDirection);
	}

	if (!leftTurn && stillmoving)
	{
		stillmoving = Scout->walkMove(currentDirection * 6.0, Hit, NULL, 4.1, 0);
		leftTurn = checkLeft(leftDirection, currentDirection);
	}

	if (!leftTurn)
		Level->FarMoveActor(Scout, oldLocation, 0, 1);

	return leftTurn;
	unguard;
}


int FPathBuilder::boundedReachable(FVector start,FVector destination)
{
	guard(FPathBuilder::boundedReachable);

	if ( (start - destination).SizeSquared() > 800 )
		return false;

	return fullyReachable(start, destination);

	unguard;
}

//check if there is a line of sight from start to destination, and if Scout can walk between the two
//only used for path building
//then check that here
int FPathBuilder::fullyReachable(FVector start,FVector destination)
{
	guard(FPathBuilder::fullyReachable);

	FVector oldPosition = Scout->Location;
	Scout->SetCollisionSize(Scout->CollisionRadius - 6.0, Scout->CollisionHeight);
	int result = Level->FarMoveActor(Scout,start);
	if (Scout->Physics != PHYS_Walking)
		debugf(NAME_DevPath,TEXT("Scout Physics is %d"), Scout->Physics);

	Scout->Physics = PHYS_Walking;
	if (result)
		result = Scout->pointReachable(destination); 

	if (result) //symmetric check
	{
		Level->FarMoveActor(Scout,destination);
		result = result	&& (Scout->walkReachable(start, 15.0, 0, NULL));
		// if (!result) DebugPrint("Movement not symmetric!");
	}
	Level->FarMoveActor(Scout,oldPosition, 0, 1);

	Scout->SetCollisionSize(Scout->CollisionRadius + 6.0, Scout->CollisionHeight);

	return result; 
	unguard;
}

/* walkToward()
walk Scout toward a point.  Returns 1 if Scout successfully moved
*/
inline int FPathBuilder::walkToward(const FVector &Destination, FLOAT Movesize)
{
	guard(FPathBuilder::walkToward);
	FVector Direction = Destination - Scout->Location;
	Direction.Z = 0; //this is a 2D move
	FLOAT DistanceSquared = Direction.SizeSquared();
	int success = 0;
	FCheckResult Hit(1.0);

	if (DistanceSquared > 1.0) //move not too small to do //FIXME - match with walkmove threshold (4.1?)
	{
		if (DistanceSquared < Movesize * Movesize)
			success = (	Scout->walkMove(Direction, Hit, NULL, 4.1, 0) == 1 );
		else
		{
			Direction.Normalize();
			success = (	Scout->walkMove(Direction * Movesize, Hit, NULL, 4.1, 0) == 1 );
		}
	}
	return success;
	unguard;
}



