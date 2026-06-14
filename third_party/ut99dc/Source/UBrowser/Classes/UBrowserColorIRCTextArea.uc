class UBrowserColorIRCTextArea expands UWindowHTMLTextArea;

// Converts to IRC codes to HTML and uses the HTML renderer.
// ***SLOW***

function UWindowDynamicTextRow AddText(string Text)
{
	local UWindowDynamicTextRow R;
	local string OutText, NextBlock;
	local byte BoldState, UnderlineState, ColorState;
	
	ReplaceText(Text, "&", "&amp;");
	ReplaceText(Text, ">", "&gt;");
	ReplaceText(Text, "<", "&lt;");

	OutText = "";
	while(Text != "")
	{
		ProcessText(Text, NextBlock, BoldState, UnderlineState, ColorState);
		OutText = OutText $ NextBlock;
	}
	
	if(BoldState != 0)
		OutText = OutText $ "</b>";
	if(UnderlineState != 0)
		OutText = OutText $ "</u>";
	if(ColorState != 0)
		OutText = OutText $ "</font>";

	R = Super.AddText(OutText);
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

function ProcessText(out string Text, out string NextBlock, out byte BoldState, out byte UnderlineState, out byte ColorState)
{
	local int i;
	local string FG, BG;
	local bool bColor, bUnderline, bBold, bNormal, bReverse;

	i = InStr(Text, "http://");
	MultiInStr(Text, "www.", i);
	MultiInStr(Text, "unreal://", i);
	MultiInStr(Text, "ftp://", i);
	MultiInStr(Text, "ftp.", i);
	MultiInStr(Text, "telnet://", i);
	bBold =  MultiInStr(Text, Chr(2), i);
	bColor = MultiInStr(Text, Chr(3), i);
	if(bColor)
		bBold = False;
	bNormal = MultiInStr(Text, Chr(15), i);
	if(bNormal)
	{
		bBold = False;
		bColor = False;
	}
	bReverse = MultiInStr(Text, Chr(22), i);
	if(bReverse)
	{
		bBold = False;
		bColor = False;
		bNormal = False;
	}
	bUnderline = MultiInStr(Text, Chr(31), i);
	if(bUnderline)
	{
		bBold = False;
		bColor = False;
		bNormal = False;
		bReverse = False;
	}

	if(i == -1)
	{
		NextBlock = Text;
		Text = "";
	}
	else
	if(i == 0)
	{
		if(bUnderline || bBold || bNormal || bReverse)
			i = 1;
		else
		if(bColor)
		{
			for(i = 1;i<Len(Text);i++)
				if(InStr(",0123456789", Mid(Text, i, 1)) == -1)
					break;
			if(i == Len(Text))
				i = -1;
		}
		else
		{
			i = InStr(Text, " ");
			MultiInStr(Text, Chr(2), i);
			MultiInStr(Text, Chr(3), i);
			MultiInStr(Text, Chr(15), i);
			MultiInStr(Text, Chr(31), i);
		}
		if(i == -1)
		{
			NextBlock = Text;
			Text = "";
		}
		else
		{
			NextBlock = Left(Text, i);
			Text = Mid(Text, i);
		}				

		if(bColor)
		{
			NextBlock = Mid(NextBlock, 1);
			if(NextBlock == "")
			{
				if(ColorState != 0)
					NextBlock = "</font>";
				ColorState = 0;
			}
			else
			{
				ColorState = 1;
				i = InStr(NextBlock, ",");
				if(i == -1)
					FG = GetColorString(Int(NextBlock));
				else
				{
					FG = GetColorString(Int(Left(NextBlock, i)));
					BG = GetColorString(Int(Mid(NextBlock, i + 1)));
				}
				if(FG == "")
					FG = "#ffffff";
				if(BG == "")
					NextBlock = "<font color="$FG$">";
				else
					NextBlock = "<font color="$FG$" bgcolor="$BG$">";
			}
		}
		else if(bUnderline)
		{
			if(UnderlineState != 0)
				NextBlock = "</u>";	
			else
				NextBlock = "<u>";
			UnderlineState = 1-UnderlineState;
		}
		else
		if(bBold)
		{
			if(BoldState != 0)
				NextBlock = "</b>";	
			else
				NextBlock = "<b>";
			BoldState = 1-BoldState;
			
		}
		else
		if(bNormal)
		{
			NextBlock = "";
			if(BoldState != 0)
				NextBlock = "</b>";	
			BoldState = 0;
			if(UnderlineState != 0)
				NextBlock = "</b>";	
			UnderlineState = 0;
			if(ColorState != 0)
				NextBlock = "</font>";	
			ColorState = 0;
		}
		else
		if(bReverse)
		{
			if(ColorState != 0)
				NextBlock = "</font>";
			else
				NextBlock = "<font color=#000000 bgcolor=#ffffff>";
			ColorState = 1 - ColorState;
		}
		else
			NextBlock = "<a href=\""$NextBlock$"\">"$NextBlock$"</a>";
	}
	else
	{
		NextBlock = Left(Text, i);
		Text = Mid(Text, i);
	}
}

function bool MultiInStr(string Text, string In, out int i)
{
	local int j;

	j = InStr(Text, In);
	if(i == -1 || j == -1)
		i = Max(i, j);
	else
		i = Min(i, j);

	return j!=-1 && i==j;
}

function string GetColorString(int Num)
{
	switch(Num)
	{
	case 0:return "#ffffff"; 
	case 1:return "#000000"; 
	case 2:return "#0000ff"; 
	case 3:return "#00ff00"; 
	case 4:return "#ff0000"; 
	case 5:return "#7f0000"; 
	case 6:return "#7f007f"; 
	case 7:return "#ff7f00"; 
	case 8:return "#ffff00"; 
	case 9:return "#00ff00"; 
	case 10:return "#00ffff"; 
	case 11:return "#00ffff"; 
	case 12:return "#0000ff"; 
	case 13:return "#ff00ff"; 
	case 13:return "#ff00ff"; 
	case 14:return "#7f7f7f"; 
	case 15:return "#c0c0c0"; 
	}
	return "";
}


defaultproperties
{
	bScrollOnResize=True
	bVariableRowHeight=False
	bTopCentric=False
	MaxLines=500
}
