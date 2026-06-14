class UBrowserIRCChannelPage expands UBrowserIRCPageBase;

var string ChannelName;
var UBrowserIRCSystemPage SystemPage;

var UBrowserIRCUserListBox UserList;
var UWindowHSplitter Splitter;

function Created()
{
	Super.Created();

	Splitter = UWindowHSplitter(CreateWindow(class'UWindowHSplitter', 0, 0, WinWidth, WinHeight));
	UserList = UBrowserIRCUserListBox(Splitter.CreateWindow(class'UBrowserIRCUserListBox', 0, 0, 100, WinHeight, Self));

	TextArea.SetParent(Splitter);
	Splitter.LeftClientWindow = TextArea;
	Splitter.RightClientWindow = UserList;
	Splitter.SplitPos = Splitter.WinWidth - 100;
	Splitter.MinWinWidth = 100;
}

function ClosePage()
{
	SystemPage.PartChannel(ChannelName);
}

function BeforePaint(Canvas C, float X, float Y)
{
	Super.BeforePaint(C, X, Y);

	Splitter.SetSize(WinWidth, WinHeight - EditControl.WinHeight);
}

function ChannelText(string Nick, string Text)
{
	TextArea.AddText("<"$Nick$"> "$Text);
}

function ChannelAction(string Nick, string Text)
{
	TextArea.AddText("* "$Nick$" "$Text);
}

function UserNotice(string Nick, string Text)
{
	TextArea.AddText("-"$Nick$"- "$Text);
}

function ProcessInput(string Text)
{
	if(Left(Text, 4) ~= "/me ")
	{
		ChannelAction(SystemPage.NickName, Mid(Text, 4));
		SystemPage.Link.SendChannelAction(ChannelName, Mid(Text, 4));
	}
	else
	if(Left(Text, 1) == "/")
		SystemPage.ProcessInput(Text);
	else
	{
		if(Text != "")
		{
			ChannelText(SystemPage.NickName, Text);
			SystemPage.Link.SendChannelText(ChannelName, Text);
		}
	}
}

function PartedChannel(string Nick)
{
	TextArea.AddText("*** "$Nick@HasLeftText@ChannelName$".");
	UserList.RemoveUser(Nick);
}

function JoinedChannel(string Nick)
{
	TextArea.AddText("*** "$Nick@HasJoinedText@ChannelName$".");
	UserList.AddUser(Nick);
}

function KickUser(string KickedNick, string Kicker, string Reason)
{
	TextArea.AddText("*** "$KickedNick@WasKickedByText@Kicker$" ("$Reason$")");
	UserList.RemoveUser(KickedNick);
}

function UserInChannel(string Nick)
{
	UserList.AddUser(Nick);
}

function ChangedNick(string OldNick, string NewNick)
{
	TextArea.AddText("*** "$OldNick@NowKnownAsText@NewNick$".");
	UserList.ChangeNick(OldNick, NewNick);
}

function UserQuit(string Nick, string Reason)
{
	TextArea.AddText("*** "$Nick@QuitText@"("$Reason$").");
	UserList.RemoveUser(Nick);
}

function ChangeMode(string Nick, string Mode)
{
	TextArea.AddText("*** "$Nick@SetsModeText$": "$Mode);
}

function ChangeOp(string Nick, bool bOp)
{
	UserList.ChangeOp(Nick, bOp);
}

function ChangeVoice(string Nick, bool bVoice)
{
	UserList.ChangeVoice(Nick, bVoice);
}

defaultproperties
{
	RightClickMenuClass=class'UBrowserIRCChannelMenu'
}
