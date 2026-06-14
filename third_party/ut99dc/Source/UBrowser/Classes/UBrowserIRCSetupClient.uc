class UBrowserIRCSetupClient expands UWindowDialogClientWindow;

var UWindowComboControl ServerCombo;
var UWindowComboControl ChannelCombo;
var UWindowSmallButton ConnectButton;

var config string		IRCServerHistory[10];
var config string		IRCChannelHistory[10];
var config bool			bHasReadWarning;

var localized string	ServerText;
var localized string	ChannelText;
var localized string	ServerHelp;
var localized string	ChannelHelp;

var localized string	ConnectText;
var localized string	DisconnectText;
var localized string	ConnectHelp;
var localized string	DisconnectHelp;

var localized string	WarningText;
var localized string	WarningTitle;

var UWindowMessageBox ConfirmJoin;
var UBrowserIRCSystemPage SystemPage;

function Created()
{
	local Color TC;
	local int i;

	Super.Created();

	SystemPage = UBrowserIRCSystemPage(OwnerWindow);

	ServerCombo = UWindowComboControl(CreateControl(class'UWindowComboControl', 10, 5, 250, 1));
	ServerCombo.EditBoxWidth = 160;
	ServerCombo.SetText(ServerText);
	ServerCombo.SetHelpText(ServerHelp);
	ServerCombo.SetFont(F_Normal);
	ServerCombo.SetEditable(True);
	for (i=0; i<10; i++)
		if (IRCServerHistory[i] != "")
			ServerCombo.AddItem(IRCServerHistory[i]);

	TC.R = 255;
	TC.G = 255;
	TC.B = 255;
	ServerCombo.SetTextColor(TC);
	ServerCombo.SetSelectedIndex(0);

	ChannelCombo = UWindowComboControl(CreateControl(class'UWindowComboControl', 10, 25, 180, 1));
	ChannelCombo.EditBoxWidth = 90;
	ChannelCombo.SetText(ChannelText);
	ChannelCombo.SetHelpText(ChannelHelp);
	ChannelCombo.SetFont(F_Normal);
	ChannelCombo.SetEditable(True);
	for (i=0; i<10; i++)
		if (IRCChannelHistory[i] != "")
			ChannelCombo.AddItem(IRCChannelHistory[i]);
	ChannelCombo.SetSelectedIndex(0);
	ChannelCombo.SetTextColor(TC);

	ConnectButton = UWindowSmallButton(CreateControl(class'UWindowSmallButton', 196, 26, 64, 16));
	ConnectButton.bIgnoreLDoubleclick = True;
}

function BeforePaint(Canvas C, float X, float Y)
{
	Super.BeforePaint(C, X, Y);

	if(SystemPage.bConnected)
	{
		ConnectButton.SetText(DisconnectText);
		ConnectButton.SetHelpText(DisconnectHelp);
	}
	else
	{
		ConnectButton.SetText(ConnectText);
		ConnectButton.SetHelpText(ConnectHelp);
	}

	ConnectButton.AutoWidth(C);

}

function Paint(Canvas C, float X, float Y)
{
	DrawStretchedTexture(C, 0, 0, WinWidth, WinHeight, Texture'BlackTexture');
}

function DoJoin()
{
	SystemPage.Server = ServerCombo.GetValue();
	SystemPage.DefaultChannel = ChannelCombo.GetValue();
	SystemPage.Connect();

	SaveServerCombo();
	SaveChannelCombo();
	SaveConfig();
}

function MessageBoxDone(UWindowMessageBox W, MessageBoxResult Result)
{
	if(W == ConfirmJoin && Result == MR_Yes)
	{
		bHasReadWarning = True;		
		DoJoin();
	}
}

function Notify(UWindowDialogControl C, byte E)
{
	Super.Notify(C, E);

	if(C == ConnectButton && E == DE_Click)
	{
		if(SystemPage.bConnected)
			SystemPage.Disconnect();
		else
		{
			if(bHasReadWarning)
				DoJoin();
			else
				ConfirmJoin = MessageBox(WarningTitle, WarningText, MB_YesNo, MR_No);
		}
	}

	if(C == ChannelCombo && E == DE_EnterPressed)
	{
		SystemPage.JoinChannel(ChannelCombo.GetValue());
		SaveChannelCombo();
		SaveConfig();
	}
}

function NewIRCServer(string S)
{
	if(ServerCombo.List.Items.Count() == 1 && ServerCombo.GetValue() != S)
	{
		Log("Received new IRC server from UpdateServer: "$S);
		ServerCombo.Clear();
		ServerCombo.AddItem(S);
		ServerCombo.SetSelectedIndex(0);
		SaveServerCombo();
		SaveConfig();
	}
}

function SaveServerCombo()
{
	local UWindowComboListItem Item;
	local int i;

	ServerCombo.RemoveItem(ServerCombo.FindItemIndex(ServerCombo.GetValue()));
	ServerCombo.InsertItem(ServerCombo.GetValue());
	while(ServerCombo.List.Items.Count() > 10)
		ServerCombo.List.Items.Last.Remove();

	Item = UWindowComboListItem(ServerCombo.List.Items.Next);
	for (i=0; i<10; i++)
	{
		if(Item != None)
		{
			IRCServerHistory[i] = Item.Value;
			Item = UWindowComboListItem(Item.Next);
		}
		else
			IRCServerHistory[i] = "";
	}			
}

function SaveChannelCombo()
{
	local UWindowComboListItem Item;
	local int i;

	ChannelCombo.RemoveItem(ChannelCombo.FindItemIndex(ChannelCombo.GetValue()));
	ChannelCombo.InsertItem(ChannelCombo.GetValue());
	while(ChannelCombo.List.Items.Count() > 10)
		ChannelCombo.List.Items.Last.Remove();

	Item = UWindowComboListItem(ChannelCombo.List.Items.Next);
	for (i=0; i<10; i++)
	{
		if(Item != None)
		{
			IRCChannelHistory[i] = Item.Value;
			Item = UWindowComboListItem(Item.Next);
		}
		else
			IRCChannelHistory[i] = "";
	}			
}

defaultproperties
{
	ServerText="IRC Server";
	ServerHelp="Choose an IRC server from the list or type in your own IRC server name or IP address.";
	ChannelText="Default Channel";
	ChannelHelp="Choose a default channel to join once the server has connected, or type in your own channel name.";
	IRCServerHistory(0)="irc.gameslink.net"
	IRCChannelHistory(0)="#utgames"
	IRCChannelHistory(1)="#utchat"
	IRCChannelHistory(2)="#utmods"
	IRCChannelHistory(3)="#utlevels"
	IRCChannelHistory(4)="#uthelp"
	ConnectText="Connect"
	DisconnectText="Logoff"
	ConnectHelp="Connect to the IRC chat server."
	DisconnectHelp="Disconnect from the IRC chat server."
	WarningTitle="Warning"
	WarningText="The Chat facility will connect you to the Internet Relay Chat (IRC) network.\\n\\nEpic Games is not responsible for the content of any channels in IRC. You enter these channels at your own risk.\\n\\nAre you sure you still want to connect?"
}
