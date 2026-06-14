class UBrowserRightClickMenu extends UWindowRightClickMenu;

var UWindowPulldownMenuItem Play, Copy, Refresh, RefreshServer, PingAll, Info, Favorites, OpenLocation;

var localized string PlayName;
var localized string RefreshName;
var localized string InfoName;
var localized string FavoritesName;
var localized string RefreshServerName;
var localized string PingAllName;
var localized string OpenLocationName;
var localized string CopyName;

var UBrowserServerGrid	Grid;
var UBrowserServerList	List;

function Created()
{
	Super.Created();
	
	Info = AddMenuItem(InfoName, None);
	Copy = AddMenuItem(CopyName, None);
	Play = AddMenuItem(PlayName, None);
	OpenLocation = AddMenuItem(OpenLocationName, None);
	AddMenuItem("-", None);
	AddFavoriteItems();
	AddMenuItem("-", None);
	RefreshServer = AddMenuItem(RefreshServerName, None);
	PingAll = AddMenuItem(PingAllName, None);
	Refresh = AddMenuItem(RefreshName, None);
}

function AddFavoriteItems()
{
	Favorites = AddMenuItem(FavoritesName, None);
}

function ExecuteItem(UWindowPulldownMenuItem I) 
{
	switch(I)
	{
	case Play:
		Grid.JoinServer(List);
		break;
	case Info:
		if(!Info.bDisabled) 
			Grid.ShowInfo(List);
		break;
	case Favorites:
		UBrowserServerListWindow(Grid.GetParent(class'UBrowserServerListWindow')).AddFavorite(List);
		break;
	case Refresh:
		Grid.Refresh();
		break;
	case PingAll:
		Grid.RePing();
		break;
	case RefreshServer:
		Grid.RefreshServer();
		break;
	case OpenLocation:
		UBrowserMainWindow(Grid.GetParent(class'UBrowserMainWindow')).ShowOpenWindow();
		break;
	case Copy:
		GetPlayerOwner().CopyToClipboard("unreal://"$List.IP$":"$string(List.GamePort));
		break;		
	}

	Super.ExecuteItem(I);
}

function ShowWindow()
{
	Info.bDisabled = List == None || List.GamePort == 0;
	Play.bDisabled = List == None || List.GamePort == 0;
	Copy.bDisabled = List == None || List.GamePort == 0;

	Favorites.bDisabled = List == None;
	RefreshServer.bDisabled = List == None;
	Selected = None;

	Super.ShowWindow();
}

defaultproperties
{
	PlayName="&Play on This Server"
	RefreshServerName="P&ing This Server"
	RefreshName="&Refresh All Servers"
	PingAllName="Ping &All Servers"
	InfoName="&Server and Player Info"
	FavoritesName="Add to &Favorites"
	OpenLocationName="Open &Location"
	CopyName="&Copy This Server Location"
}
