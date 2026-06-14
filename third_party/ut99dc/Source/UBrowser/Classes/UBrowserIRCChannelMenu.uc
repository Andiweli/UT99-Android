class UBrowserIRCChannelMenu expands UWindowRightClickMenu;

var UWindowPulldownMenuItem Part;
var UWindowPulldownMenuItem Join;
var localized string PartText;
var localized string JoinText;

function Created()
{
	Super.Created();
	
	Join = AddMenuItem(JoinText, None);
	Join.CreateSubMenu(class'UBrowserIRCJoinMenu', UBrowserIRCChannelPage(OwnerWindow).SystemPage);
	Part = AddMenuItem(PartText, None);
}

function ExecuteItem(UWindowPulldownMenuItem I) 
{
	switch(I)
	{
	case Part:
		UBrowserIRCChannelPage(OwnerWindow).ClosePage();
		break;
	}
	Super.ExecuteItem(I);
}

function ShowWindow()
{
	Super.ShowWindow();
	Part.Caption = PartText@UBrowserIRCChannelPage(OwnerWindow).ChannelName;
}

defaultproperties
{
	PartText="&Leave"
	JoinText="&Join"
}
