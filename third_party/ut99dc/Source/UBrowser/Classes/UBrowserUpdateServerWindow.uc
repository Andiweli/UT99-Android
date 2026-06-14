class UBrowserUpdateServerWindow expands UWindowPageWindow;

var UBrowserUpdateServerLink Link;
var UBrowserUpdateServerTextArea TextArea;

var localized string QueryText;
var localized string FailureText;
var class<UBrowserUpdateServerLink> LinkClass;
var class<UBrowserUpdateServerTextArea> TextAreaClass;
var bool bGotMOTD;
var string StatusBarText;
var bool bHadInitialQuery;
var int Tries;
var int NumTries;

function Created()
{
	Super.Created();
	TextArea = UBrowserUpdateServerTextArea(CreateControl(TextAreaClass, 0, 0, WinWidth, WinHeight, Self));

	SetAcceptsFocus();
}

function Query()
{
	bHadInitialQuery = True;
	StatusBarText = QueryText;
	if(Link != None)
	{
		Link.UpdateWindow = None;
		Link.Destroy();
	}
	Link = GetEntryLevel().Spawn(LinkClass);
	Link.UpdateWindow = Self;
	Link.QueryUpdateServer();
	bGotMOTD = False;
}

function BeforePaint(Canvas C, float X, float Y)
{
	local UBrowserMainWindow W;

	if(!bHadInitialQuery)
		Query();

	Super.BeforePaint(C, X, Y);
	TextArea.SetSize(WinWidth, WinHeight);

	W = UBrowserMainWindow(GetParent(class'UBrowserMainWindow'));
	if(StatusBarText == "")
		W.DefaultStatusBarText(TextArea.StatusURL);
	else
		W.DefaultStatusBarText(StatusBarText);
}

function Paint(Canvas C, float X, float Y)
{
	DrawStretchedTexture(C, 0, 0, WinWidth, WinHeight, Texture'BlackTexture');
}

function SetMOTD(string MOTD)
{
	TextArea.SetHTML(MOTD);
}

function SetMasterServer(string Value)
{
	ReplaceText(Value, Chr(10), "");
	if(Value != "")
		UBrowserMainClientWindow(UBrowserMainWindow(GetParent(class'UBrowserMainWindow')).ClientArea).NewMasterServer(Value);
}

function SetIRCServer(string Value)
{
	StripCRLF(Value);

	if(Value != "")
		UBrowserMainClientWindow(UBrowserMainWindow(GetParent(class'UBrowserMainWindow')).ClientArea).NewIRCServer(Value);
}

function Failure()
{
	Link.UpdateWindow = None;
	Link.Destroy();
	Link = None;
	Tries++;
	if(Tries < NumTries)
	{
		Query();
		return;
	}

	StatusBarText = FailureText;
	Tries = 0;
}

function Success()
{
	StatusBarText = "";

	Link.UpdateWindow = None;
	Link.Destroy();
	Link = None;
	Tries = 0;
}

function KeyDown(int Key, float X, float Y) 
{
	switch(Key)
	{
	case 0x74: // IK_F5;
		TextArea.Clear();
		Query();
		break;
	}
}

defaultproperties
{
	FailureText="The server did not respond."
	QueryText="Querying Server..."
	LinkClass=class'UBrowserUpdateServerLink'
	TextAreaClass=class'UBrowserUpdateServerTextArea'
	NumTries=3
}