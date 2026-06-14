//=============================================================================
// UBrowserMainClientWindow - The main client area
//=============================================================================
class UBrowserMainClientWindow extends UWindowClientWindow;

var globalconfig string		LANTabName;
var globalconfig name ServerListNames[20];
var globalconfig bool bKeepMasterServer;

var UWindowPageControl		PageControl;
var UWindowPageControlPage	Favorites, IRC, MOTD;
var localized string		FavoritesName, IRCName, MOTDName;
var string					ServerListWindowClass;
var string					FavoriteServersClass;
var string					UpdateServerClass;
var UWindowPageControlPage	LANPage;
var UWindowTabControlItem	PageBeforeLAN;
var UBrowserServerListWindow FactoryWindows[20];
var UBrowserInfoWindow		InfoWindow;

function Created() 
{
	local int i, f;
	local UWindowPageControlPage P;
	local UBrowserServerListWindow W;
	local class<UBrowserServerListWindow> C;
	local class<UBrowserFavoriteServers> FC;
	local class<UBrowserUpdateServerWindow> MC;

	Super.Created();

	InfoWindow = UBrowserInfoWindow(Root.CreateWindow(class'UBrowserInfoWindow', 10, 40, 310, 170));
	InfoWindow.HideWindow();

	PageControl = UWindowPageControl(CreateWindow(class'UWindowPageControl', 0, 0, WinWidth, WinHeight));
	PageControl.SetMultiLine(True);

	// Add MOTD
	MC = class<UBrowserUpdateServerWindow>(DynamicLoadObject(UpdateServerClass, class'Class'));
	MOTD = PageControl.AddPage(MOTDName, MC);

	IRC = PageControl.AddPage(IRCName, class'UBrowserIRCWindow');

	// Add favorites
	FC = class<UBrowserFavoriteServers>(DynamicLoadObject(FavoriteServersClass, class'Class'));
	Favorites = PageControl.AddPage(FavoritesName, FC);

	C = class<UBrowserServerListWindow>(DynamicLoadObject(ServerListWindowClass, class'Class'));

	for(i=0; i<20; i++)
	{
		if(ServerListNames[i] == '')
			break;

		P = PageControl.AddPage("", C, ServerListNames[i]);
		if(string(ServerListNames[i]) ~= LANTabName)
			LANPage = P;

		W = UBrowserServerListWindow(P.Page);
		if(W.bHidden)
			PageControl.DeletePage(P);

		if(W.ServerListTitle != "")
			P.SetCaption(W.ServerListTitle);
		else
			P.SetCaption(Localize("ServerListTitles", string(ServerListNames[i]), "UBrowser"));

		FactoryWindows[i] = W;
	}
}

function SelectLAN()
{
	if(LANPage != None)
	{
		PageBeforeLAN = PageControl.SelectedTab;
		PageControl.GotoTab(LANPage, True);
	}
}

function SelectInternet()
{
	if(PageBeforeLAN != None && PageControl.SelectedTab == LANPage)
		PageControl.GotoTab(PageBeforeLAN, True);
	PageBeforeLAN = None;
}

function NewMasterServer(string M)
{
	local int i, j;
	local string NewServers[10];
	local string T;
	local bool bHadNewServer;

	i = 0;
	while(M != "")
	{
		j = InStr(M, Chr(13));
		if(j != -1)
		{
			T = Left(M, j);
			M = Mid(M, j+1);
		}
		else
		{
			T = M;
			M = "";
		}
		if(T != "")
			NewServers[i++] = T;
	}	

	if(!bKeepMasterServer)
	{
		for(i=0; i<20; i++)
		{
			if(ServerListNames[i] == 'UBrowserAll')
			{
				bHadNewServer = False;
				for(j=0; j<9; j++)
				{
					if(FactoryWindows[i].ListFactories[j] != NewServers[j])
					{
						Log("Received new master server ["$j$"] from UpdateServer: "$NewServers[j]);
						FactoryWindows[i].ListFactories[j] = NewServers[j];
						FactoryWindows[i].ListFactories[j+1] = "";
						bHadNewServer = True;
					}
				}
				if(bHadNewServer)
				{
					if(FactoryWindows[i].bHadInitialRefresh)
						FactoryWindows[i].Refresh(False, True);
					FactoryWindows[i].SaveConfig();
				}
			}
		}
	}
}

function NewIRCServer(string S)
{
	UBrowserIRCWindow(IRC.Page).SystemPage.SetupClient.NewIRCServer(S);
}

function Paint(Canvas C, float X, float Y)
{
	DrawStretchedTexture(C, 0, 0, WinWidth, WinHeight, Texture'BlackTexture');
}

function Resized()
{
	Super.Resized();
	PageControl.SetSize(WinWidth, WinHeight);
}

function SaveConfigs()
{
	SaveConfig();
}

defaultproperties
{
	FavoritesName="Favorites"
	IRCName="Chat"
	MOTDName="News"
	ServerListWindowClass="UBrowser.UBrowserServerListWindow"
	FavoriteServersClass="UBrowser.UBrowserFavoriteServers"
	UpdateServerClass="UBrowser.UBrowserUpdateServerWindow"
	bKeepMasterServer=False
	LANTabName="UBrowserLAN"
}
