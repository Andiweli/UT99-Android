class UBrowserInfoMenu extends UWindowRightClickMenu;

var UWindowPulldownMenuItem Refresh, CloseItem;

var localized string RefreshName;
var localized string CloseName;

var UBrowserInfoWindow Info;

function Created()
{
	Super.Created();
	
	Refresh = AddMenuItem(RefreshName, None);
	AddMenuItem("-", None);
	CloseItem = AddMenuItem(CloseName, None);
}

function ExecuteItem(UWindowPulldownMenuItem I) 
{
	switch(I)
	{
	case Refresh:
		UBrowserInfoClientWindow(Info.ClientArea).Server.ServerStatus();
		break;
	case CloseItem:
		Info.Close();
		break;
	}

	Super.ExecuteItem(I);
}

defaultproperties
{
	RefreshName="&Refresh Info"
	CloseName="&Close"
}
