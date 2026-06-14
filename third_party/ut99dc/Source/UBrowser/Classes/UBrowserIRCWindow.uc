class UBrowserIRCWindow expands UWindowPageWindow;

var UWindowPageControl		PageControl;
var UBrowserIRCSystemPage	SystemPage;

var localized string SystemName;

function Created()
{
	Super.Created();

	PageControl = UWindowPageControl(CreateWindow(class'UWindowPageControl', 0, 0, WinWidth, WinHeight));
	PageControl.SetMultiLine(True);
	PageControl.bSelectNearestTabOnRemove = True;
	SystemPage = UBrowserIRCSystemPage(PageControl.AddPage(SystemName, class'UBrowserIRCSystemPage').Page);
	SystemPage.PageParent = PageControl;
}

function Resized()
{
	PageControl.SetSize(WinWidth, WinHeight);
}

function BeforePaint(Canvas C, float X, float Y)
{
	local UBrowserMainWindow W;
	Super.BeforePaint(C, X, Y);

	W = UBrowserMainWindow(GetParent(class'UBrowserMainWindow'));
	W.DefaultStatusBarText("");
	SystemPage.IRCVisible();
}

function WindowHidden()
{
	Super.WindowHidden();
	SystemPage.IRCClosed();
}

function Close(optional bool bByParent)
{
	Super.Close(bByParent);
	if(bByParent)
		SystemPage.IRCClosed();
}

defaultproperties
{
	SystemName="System"
}
