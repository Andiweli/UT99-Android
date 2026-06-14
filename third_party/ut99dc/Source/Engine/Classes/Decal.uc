class Decal expands Actor
	native;

// properties
var int MultiDecalLevel;
var float LastRenderedTime;

// native stuff.
var const array<int> SurfList;

simulated native function bool AttachDecal( float TraceDistance, optional vector DecalDir ); // trace forward and attach this decal to surfaces.
simulated native function DetachDecal(); // detach this decal from all surfaces.

simulated event BeginPlay()
{
	if(!AttachDecal(100))	// trace 100 units ahead in direction of current rotation
		Destroy();
}

simulated event Destroyed()
{
	DetachDecal();
	Super.Destroyed();
}

event Update(Actor L);

defaultproperties
{
	DrawScale=1
	DrawType=DT_None
	MultiDecalLevel=4
	RemoteRole=ROLE_None
	bUnlit=true
    Physics=PHYS_None
	bNetTemporary=true
	bNetOptional=true
	bGameRelevant=true
	CollisionRadius=+0.00000
	CollisionHeight=+0.00000
	bStatic=false
	bStasis=false
	bHighDetail=true
}