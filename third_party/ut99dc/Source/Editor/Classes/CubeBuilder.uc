//=============================================================================
// CubeBuilder: Builds a 3D cube brush.
//=============================================================================
class CubeBuilder
	expands BrushBuilder;

var() float Height, Width, Breadth;
var() float WallThickness;
var() name GroupName;
var() bool Hollow;

function BuildCube( int Direction, float dx, float dy, float dz )
{
	local int n,i,j,k;
	n = GetVertexCount();

	for( i=-1; i<2; i+=2 )
		for( j=-1; j<2; j+=2 )
			for( k=-1; k<2; k+=2 )
				Vertex3f( i*dx/2, j*dy/2, k*dz/2 );

	Poly4i(Direction,n+0,n+1,n+3,n+2);
	Poly4i(Direction,n+2,n+3,n+7,n+6);
	Poly4i(Direction,n+6,n+7,n+5,n+4);
	Poly4i(Direction,n+4,n+5,n+1,n+0);
	Poly4i(Direction,n+3,n+1,n+5,n+7);
	Poly4i(Direction,n+0,n+2,n+6,n+4);
}

event bool Build()
{
	if( Height<=0 || Width<=0 || Breadth<=0 )
		return BadParameters();
	if( Hollow && (Height<=WallThickness || Width<=WallThickness || Breadth<=WallThickness) )
		return BadParameters();

	BeginBrush( false, GroupName );
	BuildCube( +1, Height, Width, Breadth );
	if( Hollow )
		BuildCube( -1, Height-WallThickness, Width-WallThickness, Breadth-WallThickness );
	return EndBrush();
}

defaultproperties
{
	Height=256
	Width=256
	Breadth=256
	WallThickness=16
	GroupName=Cube
	Hollow=false
}
