class UBrowserSubsetFact extends UBrowserServerListFactory;

// Config
var() config string		GameMode;
var() config string		GameType;
var() config float		Ping;
var() config name		SupersetTag;
var() config bool		bLocalServersOnly;
var() config bool		bCompatibleServersOnly;
var() config int		MinPlayers;
var() config int		MaxPing;

// Errors
var localized string		NotFoundError;
var localized string		NotReadyError;

var UBrowserServerListWindow	SupersetWindow;

function Query(optional bool bBySuperset, optional bool bInitial)
{
	local UBrowserMainClientWindow W;
	local int i;
	local UBrowserServerList l, List;

	Super.Query(bBySuperset, bInitial);

	W = UBrowserMainClientWindow(Owner.Owner.GetParent(class'UBrowserMainClientWindow'));

	for(i=0;i<20;i++)
	{
		if(W.ServerListNames[i] == SupersetTag)
		{
			SupersetWindow = W.FactoryWindows[i];
			List = W.FactoryWindows[i].PingedList;
			break;
		}
	}
	
	if(SupersetWindow != None)
	{
		SupersetWindow.AddSubset(Self);
		Owner.Owner.AddSuperSet(SupersetWindow);
	}
	else
	{
		QueryFinished(False, NotFoundError$SupersetTag);	
	}

	if(List == None)
	{
		QueryFinished(False, NotReadyError$SupersetTag);	
	}
	else
	{
		if(!bBySuperset && !bInitial)
		{
			List.Owner.Refresh();
			return;
		}

		for(l = UBrowserServerList(List.Next);l != None;l = UBrowserServerList(l.Next)) 
			ConsiderItem(l);
		
		QueryFinished(True);
	}
}

function Shutdown(optional bool bBySuperset)
{
	Super.Shutdown(bBySuperset);
	SupersetWindow.RemoveSubset(Self);
}

function ConsiderItem(UBrowserServerList L)
{
	local UBrowserServerList NewItem;
	local int MinNetVer;
	local int GameVer;

	if(!L.bPinged)
		return;	

	if(bLocalServersOnly && !L.bLocalServer)
		return;

	MinNetVer = int(Owner.Owner.GetPlayerOwner().Level.MinNetVersion);
	GameVer = int(Owner.Owner.GetPlayerOwner().Level.EngineVersion);

	if(bCompatibleServersOnly && (L.MinNetVer > GameVer || MinNetVer > L.GameVer))
		return;

	if(GameMode != "" && GameMode != L.GameMode)
		return;

	if(GameType != "" && GameType != L.GameType)
		return;

	if(MinPlayers != 0 && L.NumPlayers < MinPlayers)
		return;

	if(MaxPing != 0 && L.Ping > MaxPing)
		return;

	if( PingedList.FindExistingServer( L.IP, L.QueryPort ) != None)
		return;

	NewItem = UBrowserServerList(Owner.CopyExistingListItem(L.Class, L));
	NewItem.Remove();
	PingedList.AppendItem(NewItem);
	Owner.bNeedUpdateCount = True;
}

function QueryFinished(bool bSuccess, optional string ErrorMsg)
{
	Super.QueryFinished(bSuccess, ErrorMsg);	
}

defaultproperties
{
	SupersetTag="All"
	bLocalServersOnly=False
	GameMode=""
	GameType=""
	Ping=9999
	NotFoundError="Could not find the window: "
	NotReadyError="Window is not ready: "
}
