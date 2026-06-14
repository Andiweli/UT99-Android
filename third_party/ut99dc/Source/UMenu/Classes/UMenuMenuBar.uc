class UMenuMenuBar extends UWindowMenuBar;

var UWindowPulldownMenu Game, Multiplayer, Stats, Tool, Help;
var UMenuModMenu Mods;
var UMenuOptionsMenu Options;

var UWindowMenuBarItem GameItem, MultiplayerItem, OptionsItem, StatsItem, ToolItem, HelpItem, ModItem;

var UWindowMenuBarItem OldHelpItem;

var UMenuHelpWindow HelpWindow;
var config bool ShowHelp;

var UWindowMenuBarItem OldSelected;
var string VersionText;
var bool bShowMenu;

var localized string GameName;
var localized string GameHelp;
var localized string MultiplayerName;
var localized string MultiplayerHelp;
var localized string OptionsName;
var localized string OptionsHelp;
var localized string StatsName;
var localized string StatsHelp;
var localized string ToolName;
var localized string ToolHelp;
var localized string HelpName;
var localized string HelpHelp;
var localized string VersionName;
var localized string ModName;
var localized string ModHelp;

var UMenuModMenuItem ModItems;

var config string GameUMenuDefault;
var config string MultiplayerUMenuDefault;
var config string OptionsUMenuDefault;

function Created()
{
	local Class<UWindowPulldownMenu> GameUMenuType;
	local Class<UWindowPulldownMenu> MultiplayerUMenuType;
	local Class<UWindowPulldownMenu> OptionsUMenuType;

	local string GameUMenuName;
	local string MultiplayerUMenuName;
	local string OptionsUMenuName;

	Super.Created();

	bAlwaysOnTop = True;

	GameItem = AddItem(GameName);
	if(GetLevel().Game != None)
		GameUMenuName = GetLevel().Game.Default.GameUMenuType;
	else
		GameUMenuName = GameUMenuDefault;				
	GameUMenuType = Class<UWindowPulldownMenu>(DynamicLoadObject(GameUMenuName, class'Class'));
	Game = GameItem.CreateMenu(GameUMenuType);

	MultiplayerItem = AddItem(MultiplayerName);
	if(GetLevel().Game != None)
		MultiplayerUMenuName = GetLevel().Game.Default.MultiplayerUMenuType;
	else
		MultiplayerUMenuName = MultiplayerUMenuDefault;
	MultiplayerUMenuType = Class<UWindowPulldownMenu>(DynamicLoadObject(MultiplayerUMenuName, class'Class'));
	Multiplayer = MultiplayerItem.CreateMenu(MultiplayerUMenuType);

	OptionsItem = AddItem(OptionsName);
	if(GetLevel().Game != None)
		OptionsUMenuName = GetLevel().Game.Default.GameOptionsMenuType;
	else
		OptionsUMenuName = OptionsUMenuDefault;
	OptionsUMenuType = Class<UWindowPulldownMenu>(DynamicLoadObject(OptionsUMenuName, class'Class'));
	Options = UMenuOptionsMenu(OptionsItem.CreateMenu(OptionsUMenuType));

	// UT99_ANDROID_V135_LEAN_MENU: Stats is obsolete/offline on Android/OUYA.
	// Do not create the top-level Stats menu item.

	ToolItem = AddItem(ToolName);
	Tool = ToolItem.CreateMenu(class'UMenuToolsMenu');

	if(LoadMods())
	{
		ModItem = AddItem(ModName);
		Mods = UMenuModMenu(ModItem.CreateMenu(class'UMenuModMenu'));
		Mods.SetupMods(ModItems);
	}

	// UT99_ANDROID_V135_LEAN_MENU: Help only contains dead web/support links and credits.
	// Do not create the top-level Help menu item; keep context help disabled.
	ShowHelp = False;

	bShowMenu = True;

	Spacing = 12;
}

function SetHelp(string NewHelpText)
{
	if(UMenuRootWindow(Root) != None)
		if(UMenuRootWindow(Root).StatusBar != None)
			UMenuRootWindow(Root).StatusBar.SetHelp(NewHelpText);
}

function CloseUp()
{
	OldSelected = None;
	Super.CloseUp();
	ShowHelpItem(OldHelpItem);
}

function HideWindow()
{
	if(UMenuRootWindow(Root) != None)
		if(UMenuRootWindow(Root).StatusBar != None)
			UMenuRootWindow(Root).StatusBar.HideWindow();
	Super.HideWindow();
}

function ShowWindow()
{
	if (ShowHelp)
	{
		if(UMenuRootWindow(Root) != None)
			if(UMenuRootWindow(Root).StatusBar != None)
				UMenuRootWindow(Root).StatusBar.ShowWindow();
	}
	Super.ShowWindow();
}


function ShowHelpItem(UWindowMenuBarItem I)
{
	switch(I)
	{
	case GameItem:
		SetHelp(GameHelp);
		break;	
	case MultiplayerItem:
		SetHelp(MultiplayerHelp);
		break;	
	case OptionsItem:
		SetHelp(OptionsHelp);
		break;	
	case StatsItem:
		SetHelp(StatsHelp);
		break;
	case ToolItem:
		SetHelp(ToolHelp);
		break;	
	case HelpItem:
		SetHelp(HelpHelp);
		break;	
	case ModItem:
		SetHelp(ModHelp);
	default:
		SetHelp("");
		break;	
	}
}

function Select(UWindowMenuBarItem I)
{
	Super.Select(I);
	OldSelected = I;

	ShowHelpItem(I);
	Super.Select(I);
}

function BeforePaint(Canvas C, float X, float Y)
{
	Super.BeforePaint(C, X, Y);

	if(Over != OldHelpItem)
	{
		OldHelpItem = Over;
		ShowHelpItem(Over);
	}


	if(bShowMenu)
	{
		// pull the game menu down first time menu is created
		Selected = GameItem;
		Selected.Select();
		Select(Selected);
		bShowMenu = False;
	}
}

function DrawItem(Canvas C, UWindowList Item, float X, float Y, float W, float H)
{
	C.DrawColor.R = 255;
	C.DrawColor.G = 255;
	C.DrawColor.B = 255;
	
	if(UWindowMenuBarItem(Item).bHelp) W = W - 16;

	UWindowMenuBarItem(Item).ItemLeft = X;
	UWindowMenuBarItem(Item).ItemWidth = W;

	LookAndFeel.Menu_DrawMenuBarItem(Self, UWindowMenuBarItem(Item), X, Y, W, H, C);
}

function DrawMenuBar(Canvas C)
{
	local float W, H;

	VersionText = VersionName@GetLevel().EngineVersion;

	LookAndFeel.Menu_DrawMenuBar(Self, C);

	C.Font = Root.Fonts[F_Normal];
	C.DrawColor.R = 0;
	C.DrawColor.G = 0;
	C.DrawColor.B = 0;

	TextSize(C, VersionText, W, H);
	ClipText(C, WinWidth - W - 20, 3, VersionText);
}

function LMouseDown(float X, float Y)
{
	if(X > WinWidth - 13) GetPlayerOwner().ConsoleCommand("togglefullscreen");
	Super.LMouseDown(X, Y);
}

function bool LoadMods()
{
	local int NumModClasses;
	local string NextModClass;

	NextModClass = GetPlayerOwner().GetNextInt("UMenu.UMenuModMenuItem", 0);

	if(NextModClass == "")
		return False;

	ModItems = New class'UMenuModMenuItem';
	ModItems.SetupSentinel();

	while( (NextModClass != "") && (NumModClasses < 50) )
	{
		ModItems.Append(class<UMenuModMenuItem>(DynamicLoadObject(NextModClass, class'Class')));		
		NumModClasses++;
		NextModClass = GetPlayerOwner().GetNextInt("UMenu.UMenuModMenuItem", NumModClasses);
	}
	
	return True;
}

function NotifyQuitUnreal()
{
	local UWindowMenuBarItem I;

	for(I = UWindowMenuBarItem(Items.Next); I != None; I = UWindowMenuBarItem(I.Next))
		if(I.Menu != None)
			I.Menu.NotifyQuitUnreal();
}

function NotifyBeforeLevelChange()
{
	local UWindowMenuBarItem I;

	for(I = UWindowMenuBarItem(Items.Next); I != None; I = UWindowMenuBarItem(I.Next))
		if(I.Menu != None)
			I.Menu.NotifyBeforeLevelChange();
}

function NotifyAfterLevelChange()
{
	local UWindowMenuBarItem I;

	for(I = UWindowMenuBarItem(Items.Next); I != None; I = UWindowMenuBarItem(I.Next))
		if(I.Menu != None)
			I.Menu.NotifyAfterLevelChange();
}

function MenuCmd(int Menu, int Item)
{
	bShowMenu = False;	
	Super.MenuCmd(Menu, Item);
}

defaultproperties
{
	GameName="&Game"
	MultiplayerName="&Multiplayer"
	OptionsName="&Options"
	StatsName="&Stats"
	ToolName="&Tools"
	HelpName="&Help"
	VersionName="Version"
	GameHelp="Start a new game, load a game, or quit."
	MultiplayerHelp="Host or join a multiplayer game."
	OptionsHelp="Configure settings."
	StatsHelp="Manage your local and world stats."
	ToolHelp="Enable various system tools."
	HelpHelp="Enable or disable help."
	ModName="M&od"
	ModHelp="Configure user-created mods you have installed."
}

