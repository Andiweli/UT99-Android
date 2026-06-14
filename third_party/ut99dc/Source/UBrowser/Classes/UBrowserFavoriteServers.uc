class UBrowserFavoriteServers extends UBrowserServerListWindow;

function Created()
{
	Super.Created();
	Refresh();
}

function UBrowserServerList AddFavorite(UBrowserServerList Server)
{
	local UBrowserServerList NewItem;

	if(PingedList.FindExistingServer(Server.IP, Server.QueryPort) == None)
		NewItem = UBrowserServerList(PingedList.CopyExistingListItem(ServerListClass, Server));

	PingedList.Sort();

	UBrowserFavoritesFact(Factories[0]).SaveFavorites();

	return NewItem;
}

function RemoveFavorite(UBrowserServerList Item)
{
	Item.Remove();
	UBrowserFavoritesFact(Factories[0]).SaveFavorites();
}

defaultproperties
{
	ListFactories(0)="UBrowser.UBrowserFavoritesFact"
	RightClickMenuClass=class'UBrowserFavoritesMenu'
	bShowFailedServers=True
}

