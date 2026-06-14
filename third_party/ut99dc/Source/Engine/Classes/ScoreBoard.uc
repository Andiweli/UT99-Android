//=============================================================================
// ScoreBoard
//=============================================================================
class ScoreBoard extends Info;

var font RegFont;
var HUD OwnerHUD;

function ShowScores( canvas Canvas );
function ShowMiniScores( Canvas Canvas );

function PreBeginPlay()
{
}

defaultproperties
{
     bHidden=True
}
