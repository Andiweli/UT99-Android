//=============================================================================
// SheetBuilder: Builds a simple sheet.
//=============================================================================
class SheetBuilder
	expands BrushBuilder;

var() float Height, Width;
var() enum ESheetAxis
{
	AX_Horizontal,
	AX_XAxis,
	AX_YAxis,
} Axis;
var() name GroupName;

event bool Build()
{
	if( Height<=0 || Width<=0 )
		return BadParameters();

	BeginBrush( false, GroupName );
	if( Axis==AX_Horizontal )
	{
		Vertex3f(  Height,  Width, 0 );
		Vertex3f(  Height, -Width, 0 );
		Vertex3f( -Height, -Width, 0 );
		Vertex3f( -Height,  Width, 0 );
	}
	else if( Axis==AX_XAxis )
	{
		Vertex3f( 0,  Height,  Width );
		Vertex3f( 0,  Height, -Width );
		Vertex3f( 0, -Height, -Width );
		Vertex3f( 0, -Height,  Width );
	}
	else
	{
		Vertex3f(  Width, 0,  Height );
		Vertex3f( -Width, 0,  Height );
		Vertex3f( -Width, 0, -Height );
		Vertex3f(  Width, 0, -Height );
	}
	Poly4i(+1,0,1,2,3,'Sheet',0x00000108); // PF_TwoSided|PF_NotSolid.
	return EndBrush();
}

defaultproperties
{
	Height=256
	Width=256
	Axis=SHEETAXIS_FloorCeiling
	GroupName=Sheet
}
