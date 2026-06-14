class UBrowserIRCUserListBox expands UWindowListBox;

function Created()
{
	Super.Created();
}

function AddUser(string NickName)
{
	local UBrowserIRCUserList NewUser;

	if(FindNick(NickName) == None)
	{
		NewUser = UBrowserIRCUserList(Items.Append(ListClass));
		NewUser.NickName = NickName;
		Items.MoveItemSorted(NewUser);
	}
}

function RemoveUser(string NickName)
{
	local UBrowserIRCUserList User;
	User = FindNick(NickName);
	if(User != None)
		User.Remove();
}

function ChangeNick(string OldNick, string NewNick)
{
	local UBrowserIRCUserList User;
	User = FindNick(OldNick);
	if(User != None)
		User.NickName = NewNick;

	Sort();
}

function UBrowserIRCUserList FindNick(string NickName)
{
	local UBrowserIRCUserList User;

	for(User=UBrowserIRCUserList(Items.Next);User != None; User=UBrowserIRCUserList(User.Next))
		if(User.NickName == NickName)
			return User;

	return None;	
}

function DrawItem(Canvas C, UWindowList Item, float X, float Y, float W, float H)
{
	local string Prefix;

	if(SelectedItem == Item)
	{
		C.DrawColor.r = 0;
		C.DrawColor.g = 0;
		C.DrawColor.b = 128;
		DrawStretchedTexture(C, X, Y, W, H-1, Texture'WhiteTexture');
	}

	C.DrawColor.r = 255;
	C.DrawColor.g = 255;
	C.DrawColor.b = 255;
	
	C.Font = Root.Fonts[F_Normal];

	if(UBrowserIRCUserList(Item).bChOp)
		Prefix = "@";
	else if(UBrowserIRCUserList(Item).bVoice)
		Prefix = "+";
	else
		Prefix = "";

	ClipText(C, X, Y, Prefix$UBrowserIRCUserList(Item).NickName);
}

function ChangeOp(string Nick, bool bOp)
{
	local UBrowserIRCUserList User;

	User = FindNick(Nick);
	if(User != None)
	{
		User.bChOp = bOp;
		Items.MoveItemSorted(User);
	}
}

function ChangeVoice(string Nick, bool bVoice)
{
	local UBrowserIRCUserList User;

	User = FindNick(Nick);
	if(User != None)
	{
		User.bVoice = bVoice;
		Items.MoveItemSorted(User);
	}
}

function DoubleClickItem(UWindowListBoxItem I)
{
	UBrowserIRCChannelPage(OwnerWindow).SystemPage.FindPrivateWindow(UBrowserIRCUserList(I).NickName);
}

defaultproperties
{
	ListClass=class'UBrowserIRCUserList'
	ItemHeight=13
}