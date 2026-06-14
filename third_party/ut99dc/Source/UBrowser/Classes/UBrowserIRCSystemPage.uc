class UBrowserIRCSystemPage expands UBrowserIRCPageBase;

var UBrowserIRCLink Link;
var UWindowPageControl PageParent;

var string	Server;
var string DefaultChannel;

var config string	NickName;
var config string	FullName;
var config string	OldPlayerName;
var config string	UserIdent;

var UWindowVSplitter Splitter;
var UBrowserIRCSetupClient SetupClient;

var bool bConnected;
var bool bAway;

var localized string NotInAChannelText;
var localized string KickedFromText;
var localized string ByText;
var localized string IsAwayText;

function Created()
{
	Super.Created();

	Splitter = UWindowVSplitter(CreateWindow(class'UWindowVSplitter', 0, 0, WinWidth, WinHeight));
	SetupClient = UBrowserIRCSetupClient(Splitter.CreateWindow(class'UBrowserIRCSetupClient', 0, 0, WinWidth, WinHeight, Self));

	TextArea.SetParent(Splitter);
	Splitter.TopClientWindow = SetupClient;
	Splitter.BottomClientWindow = TextArea;
	Splitter.SplitPos = 45;
	Splitter.MaxSplitPos = 45;
	Splitter.MinWinHeight = 0;
	Splitter.bSizable = True;
	Splitter.bBottomGrow = True;

	Setup();
}

function BeforePaint(Canvas C, float X, float Y)
{
	Super.BeforePaint(C, X, Y);

	Splitter.SetSize(WinWidth, WinHeight - EditControl.WinHeight);
}

function ProcessInput(string Text)
{
	if(Left(Text, 1) != "/")
		SystemText("*** "$NotInAChannelText);
	else
		Link.SendCommandText(Mid(Text, 1));
}

function UBrowserIRCChannelPage FindChannelWindow(string Channel)
{
	local UWindowPageControlPage P;
	local UBrowserIRCChannelPage Chan;

	for(P = PageParent.FirstPage(); P != None; P = P.NextPage())
	{
		Chan = UBrowserIRCChannelPage(P.Page);
		if(Chan != None && (Chan.ChannelName ~= Channel))
			return Chan;
	}

	return None;
}

function UBrowserIRCPrivPage FindPrivateWindow(string Nick)
{
	local UWindowPageControlPage P;
	local UBrowserIRCPrivPage Priv;

	for(P = PageParent.FirstPage(); P != None; P = P.NextPage())
	{
		Priv = UBrowserIRCPrivPage(P.Page);
		if(Priv != None && (Priv.PrivNick ~= Nick))
			return Priv;
	}

	return CreatePrivChannel(Nick);
}

function Connect()
{
	local int i;
	if(Link != None)
		Disconnect();

	if(GetPlayerOwner().PlayerReplicationInfo.PlayerName != OldPlayerName)
	{
		NickName = GetPlayerOwner().PlayerReplicationInfo.PlayerName;
		OldPlayerName = NickName;
		if(FullName == "")
			FullName = NickName;
		SaveConfig();
	}

	if(UserIdent == "")
	{
		UserIdent = "u";
		for(i=0;i<7;i++)
			UserIdent = UserIdent $ Chr((Rand(10)+48));

		Log("Created new UserIdent: "$UserIdent);
		SaveConfig();
	}

	Link = GetPlayerOwner().GetEntryLevel().Spawn(class'UBrowserIRCLink');
	Link.Connect(Self, Server, NickName, UserIdent, FullName, DefaultChannel);
	bConnected = True;
}

function JoinChannel(string ChannelName)
{
	local UBrowserIRCChannelPage P;

	P = FindChannelWindow(ChannelName);
	if(P == None)
		Link.JoinChannel(ChannelName);
	else
		PageParent.GotoTab(P.OwnerTab, True);
}

function PartChannel(string ChannelName)
{
	local UBrowserIRCChannelPage P;

	P = FindChannelWindow(ChannelName);
	if(P != None)
		Link.PartChannel(ChannelName);
}

function Disconnect()
{
	local UWindowPageControlPage P, Next;

	if(Link != None)
	{
		// don't localize - sent to other clients
		Link.DisconnectReason = "Disconnected";
		Link.DestroyLink();
	}
	Link = None;
	
	P = PageParent.FirstPage();
	while( P != None )
	{
		Next = P.NextPage();

		if(P.Page != Self)
			PageParent.DeletePage(P);

		P = Next;
	}

	SystemText( "Server disconnected" );
	bConnected = False;
}

function NotifyQuitUnreal()
{
	Super.NotifyQuitUnreal();

	if(Link != None)
	{
		// don't localize - sent to other clients
		Link.DisconnectReason = "Exit Game";
		Link.DestroyLink();
	}
}

function SystemText(string Text)
{
	// FIXME!! should do something better with this

	if(Text != "You have been marked as being away" &&
       Text != "You are no longer marked as being away")
			TextArea.AddText(Text);
}

function ChannelText(string Channel, string Nick, string Text)
{
	local UBrowserIRCChannelPage P;

	P = FindChannelWindow(Channel);
	if(P != None)
		P.ChannelText(Nick, Text);
}

function PrivateText(string Nick, string Text)
{
	FindPrivateWindow(Nick).PrivateText(Nick, Text);
}

function UBrowserIRCPrivPage CreatePrivChannel(string Nick)
{
	local UBrowserIRCPrivPage P;

	P = UBrowserIRCPrivPage(PageParent.AddPage(Nick, class'UBrowserIRCPrivPage').Page);
	P.SystemPage = Self;
	P.PrivNick = Nick;
	P.Setup();

	return P;
}

function ChannelAction(string Channel, string Nick, string Text)
{
	local UBrowserIRCChannelPage P;

	P = FindChannelWindow(Channel);
	if(P != None)
		P.ChannelAction(Nick, Text);
}

function PrivateAction(string Nick, string Text)
{
	FindPrivateWindow(Nick).PrivateAction(Nick, Text);
}

function JoinedChannel(string Channel, optional string Nick)
{
	local UBrowserIRCChannelPage P;
	local UBrowserIRCChannelPage W;
	local UWindowPageControlPage NewPage;

	if(Nick == "")
	{
		NewPage = PageParent.AddPage(Channel, class'UBrowserIRCChannelPage');
		P = UBrowserIRCChannelPage(NewPage.Page);
		P.SystemPage = Self;
		P.ChannelName = Channel;
		P.Setup();
		PageParent.GotoTab(NewPage, True);
	}

	if(Nick == "")
		Nick = NickName;

	W = FindChannelWindow(Channel);
	if(W != None)
		W.JoinedChannel(Nick);
}

function KickUser(string Channel, string KickedNick, string Kicker, string Reason)
{
	local UWindowPageControlPage P;
	local UBrowserIRCChannelPage W;

	W = FindChannelWindow(Channel);

	if(KickedNick == NickName)
	{
		P = PageParent.GetPage(Channel);
		if(P != None)
			PageParent.DeletePage(P);
		SystemText("*** "$KickedFromText@Channel@ByText@Kicker$" ("$Reason$")");
	}
	else
	{
		if(W != None)
			W.KickUser(KickedNick, Kicker, Reason);
	}
}

function UserInChannel(string Channel, string Nick)
{
	local UBrowserIRCChannelPage W;
	W = FindChannelWindow(Channel);
	if(W != None)
		W.UserInChannel(Nick);
}

function PartedChannel(string Channel, optional string Nick)
{
	local UWindowPageControlPage P;
	local UBrowserIRCChannelPage W;

	W = FindChannelWindow(Channel);

	if(Nick == "")
	{
		P = PageParent.GetPage(Channel);
		if(P != None)
			PageParent.DeletePage(P);
	}
	else
	{
		if(W != None)
			W.PartedChannel(Nick);
	}
}

function ChangedNick(string OldNick, string NewNick)
{
	local UWindowPageControlPage P;
	local UBrowserIRCChannelPage Chan;
	local UBrowserIRCPrivPage Priv;

	if(OldNick == NickName)
	{
		NickName = NewNick;
		Link.NickName = NewNick;
		SaveConfig();
	}
	
	for(P = PageParent.FirstPage(); P != None; P = P.NextPage())
	{
		Chan = UBrowserIRCChannelPage(P.Page);
		if(Chan != None && Chan.UserList.FindNick(OldNick) != None)
			Chan.ChangedNick(OldNick, NewNick);

		Priv = UBrowserIRCPrivPage(P.Page);
		if(Priv != None && Priv.PrivNick == OldNick)
		{
			P.Caption = NewNick;
			Priv.ChangedNick(OldNick, NewNick);
		}
	}
}

function UserQuit(string Nick, string Reason)
{
	local UWindowPageControlPage P;
	local UBrowserIRCChannelPage Chan;
	local UBrowserIRCPrivPage Priv;

	for(P = PageParent.FirstPage(); P != None; P = P.NextPage())
	{
		Chan = UBrowserIRCChannelPage(P.Page);
		if(Chan != None && Chan.UserList.FindNick(Nick) != None)
			Chan.UserQuit(Nick, Reason);

		Priv = UBrowserIRCPrivPage(P.Page);
		if(Priv != None && Priv.PrivNick == Nick)
			Priv.UserQuit(Nick, Reason);
	}
}

function UserNotice(string Nick, string Text)
{
	local UWindowPageControlPage P;
	local UBrowserIRCChannelPage Chan;
	local UBrowserIRCPrivPage Priv;

	for(P = PageParent.FirstPage(); P != None; P = P.NextPage())
	{
		Chan = UBrowserIRCChannelPage(P.Page);
		if(Chan != None && Chan.UserList.FindNick(Nick) != None)
			Chan.UserNotice(Nick, Text);

		Priv = UBrowserIRCPrivPage(P.Page);
		if(Priv != None && Priv.PrivNick == Nick)
			Priv.UserNotice(Nick, Text);
	}
}

function ChangeMode(string Channel, string Nick, string Mode)
{
	local UBrowserIRCChannelPage W;
	W = FindChannelWindow(Channel);
	if(W != None)
		W.ChangeMode(Nick, Mode);
}

function ChangeOp(string Channel, string Nick, bool bOp)
{
	local UBrowserIRCChannelPage W;
	W = FindChannelWindow(Channel);
	if(W != None)
		W.ChangeOp(Nick, bOp);
}

function ChangeVoice(string Channel, string Nick, bool bVoice)
{
	local UBrowserIRCChannelPage W;
	W = FindChannelWindow(Channel);
	if(W != None)
		W.ChangeVoice(Nick, bVoice);
}

function Tick(float Delta)
{
	if(bConnected && GetPlayerOwner().PlayerReplicationInfo.PlayerName != OldPlayerName)
	{
		OldPlayerName = GetPlayerOwner().PlayerReplicationInfo.PlayerName;
		Link.SetNick(OldPlayerName);
		SystemText("SetNick: "$OldPlayerName);
	}

	Super.Tick(Delta);
}

function IsAway(string Nick, string Message)
{
	local UBrowserIRCPrivPage W;

	W = FindPrivateWindow(Nick);
	
	if(W != None)
		W.IsAway(Nick, Message);
	else
		SystemText(Nick@IsAwayText$": "$Message);
}

function IRCVisible()
{
	if(bAway)
	{
		if(bConnected)
			Link.SetAway("");
		bAway = False;
	}
}

function IRCClosed()
{
	CheckAway();
}

function NotifyAfterLevelChange()
{
	Super.NotifyAfterLevelChange();
	CheckAway();
}

function CheckAway()
{
	local string URL;

	if( bConnected )
	{
		bAway = True;

		URL = GetLevel().GetAddressURL();
		if(InStr(URL, ":") > 0)
			Link.SetAway("unreal://"$URL);
		else
		if(!Root.bWindowVisible)
			Link.SetAway("local game");
		else
			Link.SetAway("in menus");
	}
}

function CTCP(string Channel, string Nick, string Message)
{
	if(Channel == "" || Channel == NickName)
		SystemText("["$Nick$": "$Message$"]");
	else
		SystemText("["$Nick$":"$Channel$" "$Message$"]");
}

defaultproperties
{
	NotInAChannelText="Not in a channel!"
	KickedFromText="You were kicked from"
	ByText="by"
	RightClickMenuClass=class'UBrowserIRCSystemMenu'
	IsAwayText="is away"
}