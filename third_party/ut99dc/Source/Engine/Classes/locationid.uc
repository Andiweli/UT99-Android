//=============================================================================
// LocationID - marks and names an area in a zone
//=============================================================================
class LocationID extends KeyPoint
	native;

var() localized string LocationName;
var() float Radius;
var LocationID NextLocation;

function PostBeginPlay()
{
	local LocationID L;
	Super.PostBeginPlay();

	// add self to zone list
	if ( Region.Zone.LocationID == None )
	{
		Region.Zone.LocationID = self;
		return;
	}

	for ( L=Region.Zone.LocationID; L!=None; L=L.NextLocation )
		if ( L.NextLocation == None )
		{
			L.NextLocation = self;
			return;
		}
}

defaultproperties
{
}

defaultproperties
{
}
