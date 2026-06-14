class UBrowserIRCSystemMenu expands UWindowRightClickMenu;

var UWindowPulldownMenuItem Connect;
var UWindowPulldownMenuItem Disconnect;
var UWindowPulldownMenuItem Join;
var localized string ConnectText;
var localized string DisconnectText;
var localized string JoinText;

function Created()
{
	Super.Created();
	
	Connect = AddMenuItem(ConnectText, None);
	Disconnect = AddMenuItem(DisconnectText, None);
	AddMenuItem("-", None);
	Join = AddMenuItem(JoinText, None);
	Join.CreateSubMenu(class'UBrowserIRCJoinMenu', OwnerWindow);
}

function ExecuteItem(UWindowPulldownMenuItem I) 
{
	local UBrowserIRCSystemPage S;

	S = UBrowserIRCSystemPage(OwnerWindow);

	switch(I)
	{
	case Connect:
		S.SetupClient.DoJoin();
		break;
	case Disconnect:
		S.Disconnect();
		break;
	}

	Super.ExecuteItem(I);
}

function ShowWindow()
{
	local UBrowserIRCSystemPage S;
	S = UBrowserIRCSystemPage(OwnerWindow);
	Super.ShowWindow();
	Connect.bDisabled = S.bConnected;
	Disconnect.bDisabled = !S.bConnected;
	Join.bDisabled = !S.bConnected;
}

defaultproperties
{
	ConnectText="&Connect"
	DisconnectText="&Disconnect"
	JoinText="&Join"
}
