//=============================================================================
// LiftCenter.
//=============================================================================
class LiftCenter extends NavigationPoint
	native;

var() name LiftTag;
var	 mover MyLift;
var() name LiftTrigger;
var trigger RecommendedTrigger;
var float LastTriggerTime;
var() float MaxZDiffAdd;  //added threshold for Z difference between pawn and lift (for lifts which are at the end of a ramp or stairs)
var() float MaxDist2D;
var vector LiftOffset;

function PostBeginPlay()
{
	if ( LiftTag != '' )
		ForEach AllActors(class'Mover', MyLift, LiftTag )
		{
			MyLift.myMarker = self;
			SetBase(MyLift);
			LiftOffset = Location - MyLift.Location;
			if ( MyLift.InitialState == 'BumpOpenTimed' )
				log("Warning: "$MyLift$" is BumpOpenTimed.  Bots don't understand this well - use StandOpenTimed instead!");
			break;
		}
	// log(self$" attached to "$MyLift);
	if ( LiftTrigger != '' )
		ForEach AllActors(class'Trigger', RecommendedTrigger, LiftTrigger )
			break;
	Super.PostBeginPlay();
}

/* SpecialHandling is called by the navigation code when the next path has been found.  
It gives that path an opportunity to modify the result based on any special considerations
*/

function Actor SpecialHandling(Pawn Other)
{
	local float dist2d;
	local NavigationPoint N, Exit;

	if ( MyLift == None )
		return self;
	if ( Other.base == MyLift )
	{
		if ( (RecommendedTrigger != None) 
		&& (myLift.SavedTrigger == None)
		&& (Level.TimeSeconds - LastTriggerTime > 5) )
		{
			Other.SpecialGoal = RecommendedTrigger;
			LastTriggerTime = Level.TimeSeconds;
			return RecommendedTrigger;
		}

		return self;
	}

	if ( (LiftExit(Other.MoveTarget) != None) 
		&& (LiftExit(Other.MoveTarget).RecommendedTrigger != None)
		&& (LiftExit(Other.MoveTarget).LiftTag == LiftTag)
		&& (Level.TimeSeconds - LiftExit(Other.MoveTarget).LastTriggerTime > 5)
		&& (MyLift.SavedTrigger == None)
		&& (Abs(Other.Location.X - Other.MoveTarget.Location.X) < Other.CollisionRadius)
		&& (Abs(Other.Location.Y - Other.MoveTarget.Location.Y) < Other.CollisionRadius)
		&& (Abs(Other.Location.Z - Other.MoveTarget.Location.Z) < Other.CollisionHeight) )
	{
		LiftExit(Other.MoveTarget).LastTriggerTime = Level.TimeSeconds;
		Other.SpecialGoal = LiftExit(Other.MoveTarget).RecommendedTrigger;
		return LiftExit(Other.MoveTarget).RecommendedTrigger;
	}

	SetLocation(MyLift.Location + LiftOffset);
	SetBase(MyLift);
	dist2d = square(Location.X - Other.Location.X) + square(Location.Y - Other.Location.Y);
	if ( (Location.Z - CollisionHeight - MaxZDiffAdd < Other.Location.Z - Other.CollisionHeight + Other.MaxStepHeight)
		&& (Location.Z - CollisionHeight > Other.Location.Z - Other.CollisionHeight - 1200)
		&& ( dist2D < MaxDist2D * MaxDist2D) )
	{
		return self;
	}

	if ( MyLift.BumpType == BT_PlayerBump && !Other.bIsPlayer )
		return None;
	Other.SpecialGoal = None;
		
	// make sure Other is at valid lift exit
	if ( LiftExit(Other.MoveTarget) == None )
	{
		for ( N=Level.NavigationPointList; N!=None; N=N.NextNavigationPoint )
			if ( N.IsA('LiftExit') && (LiftExit(N).LiftTag == LiftTag) 
				&& (Abs(Other.Location.X - N.Location.X) < Other.CollisionRadius)
				&& (Abs(Other.Location.Y - N.Location.Y) < Other.CollisionRadius)
				&& (Abs(Other.Location.Z - N.Location.Z) < Other.CollisionHeight) )
			{
				Exit = N;
				break;
			}
		if ( Exit == None )
			return self;
	}

	MyLift.HandleDoor(Other);
	MyLift.RecommendedTrigger = None;

	if ( (Other.SpecialGoal == MyLift) || (Other.SpecialGoal == None) )
		Other.SpecialGoal = self;

	return Other.SpecialGoal;
}

defaultproperties
{
	RemoteRole=ROLE_None
	bNoDelete=true
	bStatic=false
	ExtraCost=400
	MaxDist2D=+400.000
}