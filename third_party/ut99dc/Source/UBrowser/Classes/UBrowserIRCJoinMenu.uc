class UBrowserIRCJoinMenu expands UWindowRightClickMenu;

function ExecuteItem(UWindowPulldownMenuItem I) 
{
	UBrowserIRCSystemPage(OwnerWindow).JoinChannel(I.Caption);

	Super.ExecuteItem(I);
}

function ShowWindow()
{
	local UBrowserIRCSystemPage S;
	local int i;

	S = UBrowserIRCSystemPage(OwnerWindow);
	Super.ShowWindow();
	Clear();
	for (i=0; i<10; i++)
		if (S.SetupClient.IRCChannelHistory[i] != "")
			AddMenuItem(S.SetupClient.IRCChannelHistory[i], None);	
}