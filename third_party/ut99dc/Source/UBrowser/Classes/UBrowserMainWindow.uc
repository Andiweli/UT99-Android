//=============================================================================
// UBrowserMainWindow - The main window
//=============================================================================
class UBrowserMainWindow extends UWindowFramedWindow;

var UBrowserBannerBar			BannerWindow;
var string						StatusBarDefaultText;
var bool						bStandaloneBrowser;
var localized string			WindowTitleString;

function DefaultStatusBarText(string Text)
{
	StatusBarDefaultText = Text;
	StatusBarText = Text;
}

function BeginPlay()
{
	Super.BeginPlay();

	WindowTitle = WindowTitleString;
	ClientClass = class'UBrowserMainClientWindow';
}

function WindowShown()
{
	Super.WindowShown();
	if(WinLeft < 0 || WinTop < 16 || WinLeft + WinWidth > Root.WinWidth || WinTop + WinHeight > Root.WinHeight)
		SetSizePos();
}

function Created()
{
	bSizable = True;
	bStatusBar = True;

	Super.Created();

	MinWinWidth = 300;

	SetSizePos();
}

function BeforePaint(Canvas C, float X, float Y)
{
	if(StatusBarText == "")
		StatusBarText = StatusBarDefaultText;

	Super.BeforePaint(C, X, Y);
}

function Close(optional bool bByParent) 
{
	if(bStandaloneBrowser)
		Root.Console.CloseUWindow();
	else
		Super.Close(bByParent);
}

function ResolutionChanged(float W, float H)
{
	SetSizePos();
	Super.ResolutionChanged(W, H);
}

function SetSizePos()
{
	if(Root.WinHeight < 400)
		SetSize(Min(580, Root.WinWidth - 10), Root.WinHeight-32);
	else
		SetSize(Min(580, Root.WinWidth - 10), Root.WinHeight-50);

	WinLeft = Int((Root.WinWidth - WinWidth) / 2);
	WinTop = Int((Root.WinHeight - WinHeight) / 2);

	MinWinHeight = Min(300, WinHeight - 20);
}

// External entry points
function ShowOpenWindow()
{
	local UBrowserOpenWindow W;

	W = UBrowserOpenWindow(Root.CreateWindow(class'UBrowserOpenWindow', 300, 80, 100, 100, Self, True));
	ShowModal(W);	
}

function OpenURL(string URL)
{
	if( Left(URL, 7) ~= "http://" )
		GetPlayerOwner().ConsoleCommand("start "$URL);
	else
	if( Left(URL, 9) ~= "unreal://" )
		GetPlayerOwner().ClientTravel(URL, TRAVEL_Absolute, false);
	else
		GetPlayerOwner().ClientTravel("unreal://"$URL, TRAVEL_Absolute, false);

	Close();
	Root.Console.CloseUWindow();
}

function SelectInternet()
{
	UBrowserMainClientWindow(ClientArea).SelectInternet();
}

function SelectLAN()
{
	UBrowserMainClientWindow(ClientArea).SelectLAN();
}

defaultproperties
{
	WindowTitleString="Unreal Server Browser"
}