//=============================================================================
// UBrowserInfoWindow
//=============================================================================
class UBrowserInfoWindow extends UWindowFramedWindow;

var UBrowserInfoMenu Menu;

function Created()
{
	bSizable = True;
	bStatusBar = True;

	Menu = UBrowserInfoMenu(Root.CreateWindow(class'UBrowserInfoMenu', 0, 0, 100, 100));
	Menu.Info = Self;
	Menu.HideWindow();

	Super.Created();
	SetSizes();
}

function ResolutionChanged(float W, float H)
{
	Super.ResolutionChanged(W, H);
	SetSizes();
}

function SetSizes()
{
	local UBrowserInfoClientWindow C;
	
	MinWinHeight = 100;
	SetSize(Min(Root.WinWidth - 20, 500), Min(Root.WinHeight - 30, 230));
}

defaultproperties
{
	ClientClass=class'UBrowserInfoClientWindow'
}
