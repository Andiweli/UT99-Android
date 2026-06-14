class UBrowserIRCPageBase expands UWindowPageWindow;

var UWindowDynamicTextArea TextArea;
var UWindowEditControl	EditControl;

var config string TextAreaClass;

var localized string HasLeftText;
var localized string HasJoinedText;
var localized string WasKickedByText;
var localized string NowKnownAsText;
var localized string QuitText;
var localized string SetsModeText;

var class<UWindowPulldownMenu> RightClickMenuClass;
var UWindowPulldownMenu Menu;

function Created()
{
	local class<UWindowDynamicTextArea> TAClass;

	Super.Created();

	TAClass = class<UWindowDynamicTextArea>(DynamicLoadObject(TextAreaClass, class'Class'));
	if(TAClass == None)
		TAClass = class'UBrowserIRCTextArea';

	TextArea = UWindowDynamicTextArea(CreateControl(TAClass, 0, 0, WinWidth, WinHeight, Self));
	EditControl = UWindowEditControl(CreateControl(class'UWindowEditControl', 0, WinHeight-16, WinWidth, 16));
	EditControl.SetFont(F_Normal);
	EditControl.SetNumericOnly(False);
	EditControl.SetMaxLength(400);
	EditControl.SetHistory(True);
}

function Setup()
{
	if(RightClickMenuClass != None)
	{
		Menu = UWindowPulldownMenu(Root.CreateWindow(RightClickMenuClass, 0, 0, 100, 100, Self));
		Menu.HideWindow();
	}
}

function RMouseUp(float X, float Y)
{
	local float MenuX, MenuY;
	
	if(Menu != None)
	{
		WindowToGlobal(X, Y, MenuX, MenuY);
		Menu.WinLeft = MenuX;
		Menu.WinTop = MenuY;
		Menu.ShowWindow();
	}
}

function Close(optional bool bByParent)
{
	Super.Close(bByParent);

	if(Menu != None && Menu.bWindowVisible)
		Menu.CloseUp();
}

function ClosePage()
{
}

function WindowShown()
{
	Super.WindowShown();
	OwnerTab.bFlash = False;
}

function AddedText()
{
	if(!bWindowVisible)
		OwnerTab.bFlash = True;
	else
		OwnerTab.bFlash = False;
}

function BeforePaint(Canvas C, float X, float Y)
{
	Super.BeforePaint(C, X, Y);

	EditControl.SetSize(WinWidth, 17);
	EditControl.WinLeft = 0;
	EditControl.WinTop = WinHeight - EditControl.WinHeight;
	EditControl.EditBoxWidth = WinWidth;

	TextArea.SetSize(WinWidth, WinHeight - EditControl.WinHeight);
}

function Paint(Canvas C, float X, float Y)
{
	DrawStretchedTexture(C, 0, 0, WinWidth, WinHeight, Texture'BlackTexture');
}

function Notify(UWindowDialogControl C, byte E)
{
	Super.Notify(C, E);

	switch(E)
	{
	case DE_EnterPressed:
		switch(C)
		{
		case EditControl:
			ProcessInput(EditControl.GetValue());
			EditControl.Clear();
			break;
		}
		break;
	case DE_WheelUpPressed:
		switch(C)
		{
		case EditControl:
			TextArea.VertSB.Scroll(-1);
			break;
		}
		break;
	case DE_WheelDownPressed:
		switch(C)
		{
		case EditControl:
			TextArea.VertSB.Scroll(1);
			break;
		}
		break;
	}
}

function ProcessInput(string Text);

defaultproperties
{
	HasLeftText="has left"
	HasJoinedText="has joined"
	WasKickedByText="was kicked by"
	NowKnownAsText="is now known as"
	QuitText="Quit"
	SetsModeText="sets mode"
	TextAreaClass="UBrowser.UBrowserIRCTextArea"
	//This is SLOW, but it does work:
	//TextAreaClass="UBrowser.UBrowserColorIRCTextArea"
}