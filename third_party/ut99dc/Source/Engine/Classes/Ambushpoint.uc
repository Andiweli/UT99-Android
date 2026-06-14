//=============================================================================
// Ambushpoint.
//=============================================================================
class AmbushPoint extends NavigationPoint;

var vector lookdir; //direction to look while ambushing
//at start, ambushing creatures will pick either their current location, or the location of
//some ambushpoint belonging to their team
var byte survivecount; //used when picking ambushpoint 
var() float SightRadius; // How far bot at this point should look for enemies
var() bool	bSniping;	// bots should snipe from this position

function PreBeginPlay()
{
	lookdir = 2000 * vector(Rotation);

	Super.PreBeginPlay();
}

defaultproperties
{
     bDirectional=True
     SoundVolume=128
	 SightRadius=+5000.0
	 bSniping=False
}
