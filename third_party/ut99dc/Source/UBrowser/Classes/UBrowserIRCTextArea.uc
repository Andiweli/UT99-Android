class UBrowserIRCTextArea expands UWindowURLTextArea;

function UWindowDynamicTextRow AddText(string Text)
{
	local UWindowDynamicTextRow R;
	local int i, j;

	ReplaceText(Text, Chr(2), "");
	ReplaceText(Text, Chr(15), "");
	ReplaceText(Text, Chr(22), "");
	ReplaceText(Text, Chr(31), "");

	i = InStr(Text, Chr(3));
	while(i != -1)
	{
		j = 1;
		While(i+j < Len(Text) && InStr("0123456789,", Mid(Text, i+j, 1)) != -1)
			j++;

		Text = Left(Text, i) $ Mid(Text, i+j);

		i = InStr(Text, Chr(3));
	}
			
	R = Super.AddText(Text);
	UBrowserIRCPageBase(OwnerWindow).AddedText();
	return R;
}

function LaunchUnrealURL(string URL)
{
	Super.LaunchUnrealURL(URL);

	GetParent(class'UWindowFramedWindow').Close();
	Root.Console.CloseUWindow();
}

function RMouseUp(float X, float Y)
{
	local UBrowserIRCPageBase P;
	local float GX, GY;

	P = UBrowserIRCPageBase(GetParent(class'UBrowserIRCPageBase'));
	WindowToGlobal(X, Y, GX, GY);
	P.GlobalToWindow(GX, GY, X, Y);
	P.RMouseUp(X, Y);
}

defaultproperties
{
	bScrollOnResize=True
	bVariableRowHeight=False
	bTopCentric=False
	MaxLines=500
}
