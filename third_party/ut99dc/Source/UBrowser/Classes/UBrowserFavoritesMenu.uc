class UBrowserFavoritesMenu expands UBrowserRightClickMenu;

var UWindowPulldownMenuItem EditFavorite, NewFavorite;
var localized string EditFavoriteName, NewFavoriteName;

function AddFavoriteItems()
{
	Favorites = AddMenuItem(FavoritesName, None);
	EditFavorite = AddMenuItem(EditFavoriteName, None);
	NewFavorite = AddMenuItem(NewFavoriteName, None);
}

function ExecuteItem(UWindowPulldownMenuItem I) 
{
	switch(I)
	{
	case EditFavorite:
		Grid.GetParent(class'UWindowFramedWindow').ShowModal(Root.CreateWindow(class'UBrowserEditFavoriteWindow', 300, 80, 100, 100, Self, True));
		break;
	case NewFavorite:
		Grid.GetParent(class'UWindowFramedWindow').ShowModal(Root.CreateWindow(class'UBrowserNewFavoriteWindow', 300, 80, 100, 100, Self, True));
		break;
	case Favorites:
		UBrowserFavoriteServers(Grid.GetParent(class'UBrowserServerListWindow')).RemoveFavorite(List);
		Super(UWindowRightClickMenu).ExecuteItem(I);
		return;
		break;
	}
	Super.ExecuteItem(I);
}

function ShowWindow()
{
	EditFavorite.bDisabled = List == None;
	Super.ShowWindow();
}

defaultproperties
{
	FavoritesName="Remove from &Favorites"
	EditFavoriteName="&Edit Favorite"
	NewFavoriteName="&New Favorite"
}
