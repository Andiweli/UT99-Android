class UBrowserIRCPrivPage expands UBrowserIRCPageBase;

var string PrivNick;
var UBrowserIRCSystemPage SystemPage;

function ClosePage()
{
	SystemPage.PageParent.DeletePage(OwnerTab);
}

function ProcessInput(string Text)
{
	if(Left(Text, 4) ~= "/me ")
	{
		PrivateAction(SystemPage.NickName, Mid(Text, 4));
		SystemPage.Link.SendChannelAction(PrivNick, Mid(Text, 4));
	}
	else
	if(Left(Text, 1) == "/")
		SystemPage.ProcessInput(Text);
	else
	{
		if(Text != "")
		{
			PrivateText(SystemPage.NickName, Text);
			SystemPage.Link.SendChannelText(PrivNick, Text);
		}
	}
}

function ChangedNick(string OldNick, string NewNick)
{
	TextArea.AddText("*** "$OldNick@NowKnownAsText@NewNick$".");
	PrivNick = NewNick;
}

function UserQuit(string Nick, string Reason)
{
	TextArea.AddText("*** "$Nick@QuitText@"("$Reason$").");
}

function PrivateText(string Nick, string Text)
{
	TextArea.AddText("<"$Nick$"> "$Text);
	if(!GetParent(class'UWindowFramedWindow').bWindowVisible && Nick != SystemPage.NickName)
		GetPlayerOwner().ClientMessage("IRC: <"$Nick$"> "$Text);
}

function PrivateAction(string Nick, string Text)
{
	TextArea.AddText("* "$Nick$" "$Text);
	if(!GetParent(class'UWindowFramedWindow').bWindowVisible && Nick != SystemPage.NickName)
		GetPlayerOwner().ClientMessage("IRC: * "$Nick$" "$Text);
}

function UserNotice(string Nick, string Text)
{
	TextArea.AddText("-"$Nick$"- "$Text);
}

function IsAway(string Nick, string Message)
{
	TextArea.AddText(Nick@SystemPage.IsAwayText$": "$Message);
}

defaultproperties
{
	RightClickMenuClass=class'UBrowserIRCPrivateMenu'
}