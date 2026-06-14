class UBrowserEditFavoriteCW expands UWindowDialogClientWindow;

var UWindowEditControl	DescriptionEdit;
var localized string	DescriptionText;

var UWindowCheckbox		UpdateDescriptionCheck;
var localized string	UpdateDescriptionText;

var UWindowEditControl	IPEdit;
var localized string	IPText;

var UWindowEditControl	GamePortEdit;
var localized string	GamePortText;

var UWindowEditControl	QueryPortEdit;
var localized string	QueryPortText;

function Created()
{
	local float ControlOffset, CenterPos, CenterWidth;

	Super.Created();
	
	DescriptionEdit = UWindowEditControl(CreateControl(class'UWindowEditControl', 10, 10, 220, 1));
	DescriptionEdit.SetText(DescriptionText);
	DescriptionEdit.SetFont(F_Normal);
	DescriptionEdit.SetNumericOnly(False);
	DescriptionEdit.SetMaxLength(300);
	DescriptionEdit.EditBoxWidth = 100;

	UpdateDescriptionCheck = UWindowCheckbox(CreateControl(class'UWindowCheckbox', 10, 30, 136, 1));
	UpdateDescriptionCheck.SetText(UpdateDescriptionText);
	UpdateDescriptionCheck.SetFont(F_Normal);

	IPEdit = UWindowEditControl(CreateControl(class'UWindowEditControl', 10, 50, 220, 1));
	IPEdit.SetText(IPText);
	IPEdit.SetFont(F_Normal);
	IPEdit.SetNumericOnly(False);
	IPEdit.SetMaxLength(40);
	IPEdit.EditBoxWidth = 100;
	
	GamePortEdit = UWindowEditControl(CreateControl(class'UWindowEditControl', 10, 70, 160, 1));
	GamePortEdit.SetText(GamePortText);
	GamePortEdit.SetFont(F_Normal);
	GamePortEdit.SetNumericOnly(True);
	GamePortEdit.SetMaxLength(5);
	GamePortEdit.EditBoxWidth = 40;

	QueryPortEdit = UWindowEditControl(CreateControl(class'UWindowEditControl', 10, 90, 160, 1));
	QueryPortEdit.SetText(QueryPortText);
	QueryPortEdit.SetFont(F_Normal);
	QueryPortEdit.SetNumericOnly(True);
	QueryPortEdit.SetMaxLength(5);
	QueryPortEdit.EditBoxWidth = 40;

	DescriptionEdit.BringToFront();
	LoadCurrentValues();
}

function LoadCurrentValues()
{
	local UBrowserServerList L;

	L = UBrowserRightClickMenu(ParentWindow.OwnerWindow).List;

	DescriptionEdit.SetValue(L.HostName);
	UpdateDescriptionCheck.bChecked = !L.bKeepDescription;
	IPEdit.SetValue(L.IP);
	GamePortEdit.SetValue(string(L.GamePort));
	QueryPortEdit.SetValue(string(L.QueryPort));
}

function BeforePaint(Canvas C, float X, float Y)
{
	Super.BeforePaint(C, X, Y);

	DescriptionEdit.WinWidth = WinWidth - 20;
	DescriptionEdit.EditBoxWidth = WinWidth - 140;
}

function Notify(UWindowDialogControl C, byte E)
{
	Super.Notify(C, E);

	if((C == UBrowserEditFavoriteWindow(ParentWindow).OKButton && E == DE_Click))
		OKPressed();
}

function OKPressed()
{
	local UBrowserServerList L;

	L = UBrowserRightClickMenu(ParentWindow.OwnerWindow).List;

	L.HostName = DescriptionEdit.GetValue();
	L.bKeepDescription = !UpdateDescriptionCheck.bChecked;
	L.IP = IPEdit.GetValue();
	L.GamePort = Int(GamePortEdit.GetValue());
	L.QueryPort = Int(QueryPortEdit.GetValue());
	
	UBrowserFavoritesFact(UBrowserFavoriteServers(UBrowserRightClickMenu(ParentWindow.OwnerWindow).Grid.GetParent(class'UBrowserFavoriteServers')).Factories[0]).SaveFavorites();
	L.PingServer(False, True, True);

	ParentWindow.Close();
}

defaultproperties
{
	DescriptionText="Description"
	UpdateDescriptionText="Auto-Update Description"
	IPText="Server IP Address"
	GamePortText="Server Port Number"
	QueryPortText="Query Port Number"
}