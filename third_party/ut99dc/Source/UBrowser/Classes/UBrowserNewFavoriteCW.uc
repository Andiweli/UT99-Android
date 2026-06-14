class UBrowserNewFavoriteCW expands UBrowserEditFavoriteCW;

function LoadCurrentValues()
{
	GamePortEdit.SetValue("7777");
	QueryPortEdit.SetValue("7778");
}

function OKPressed()
{
	local UBrowserServerListFactory F;
	local UBrowserServerList L;
	local UBrowserFavoriteServers W;

	W = UBrowserFavoriteServers(UBrowserRightClickMenu(ParentWindow.OwnerWindow).Grid.GetParent(class'UBrowserFavoriteServers'));
	F = W.Factories[0];
	
	L = UBrowserServerList(F.PingedList.CreateItem(F.PingedList.Class));

	L.HostName = DescriptionEdit.GetValue();
	L.IP = IPEdit.GetValue();
	L.Ping = 9999;
	L.QueryPort = Int(QueryPortEdit.GetValue()); 
	L.bKeepDescription = !UpdateDescriptionCheck.bChecked;
	L.GamePort = Int(GamePortEdit.GetValue());

	L = W.AddFavorite(L);

	if(L != None)
		L.PingServer(False, True, True);

	ParentWindow.Close();
}

