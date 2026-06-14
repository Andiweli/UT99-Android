class UBrowserIRCPrivateMenu expands UWindowRightClickMenu;

var UWindowPulldownMenuItem CloseChat;
var UWindowPulldownMenuItem Join;
var localized string CloseText;
var localized string JoinText;

function Created()
{
	Super.Created();
	
	Join = AddMenuItem(JoinText, None);
	Join.CreateSubMenu(class'UBrowserIRCJoinMenu', UBrowserIRCPrivPage(OwnerWindow).SystemPage);
	CloseChat = AddMenuItem(CloseText, None);
}

function ExecuteItem(UWindowPulldownMenuItem I) 
{
	switch(I)
	{
	case CloseChat:
		UBrowserIRCPrivPage(OwnerWindow).ClosePage();
		break;
	}
	Super.ExecuteItem(I);
}

defaultproperties
{
	CloseText="&Close "
	JoinText="&Join"
}
