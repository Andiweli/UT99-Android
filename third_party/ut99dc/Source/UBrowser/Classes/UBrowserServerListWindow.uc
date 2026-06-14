class UBrowserServerListWindow extends UWindowPageWindow
	PerObjectConfig;

var config string				ServerListTitle;	// Non-localized page title
var config string				ListFactories[10];
var config string				URLAppend;
var config int					AutoRefreshTime;
var config bool					bNoAutoSort;
var config bool					bHidden;
var config bool					bFallbackFactories;

var string						ServerListClassName;
var class<UBrowserServerList>	ServerListClass;

var UBrowserServerList			PingedList;
var UBrowserServerList			UnpingedList;

var UBrowserServerListFactory	Factories[10];
var int							QueryDone[10];
var UBrowserServerGrid			Grid;
var string						GridClass;
var float						TimeElapsed;
var bool						bPingSuspend;
var bool						bPingResume;
var bool						bPingResumeIntial;
var bool						bNoSort;
var bool						bSuspendPingOnClose;
var UBrowserSubsetList			SubsetList;
var UBrowserSupersetList		SupersetList;
var class<UBrowserRightClickMenu>	RightClickMenuClass;
var bool						bShowFailedServers;
var bool						bHadInitialRefresh;
var int							FallbackFactory;

var UWindowVSplitter			VSplitter;
var UBrowserInfoWindow			InfoWindow;
var UBrowserInfoClientWindow	InfoClient;
var UBrowserServerList			InfoItem;
var localized string			InfoName;

const MinHeightForSplitter = 384;

// Status info
enum EPingState
{
	PS_QueryServer,
	PS_QueryFailed,
	PS_Pinging,
	PS_RePinging,
	PS_Done
};

var localized string			PlayerCountName;
var localized string			ServerCountName;
var	localized string			QueryServerText;
var	localized string			QueryFailedText;
var	localized string			PingingText;
var	localized string			CompleteText;

var string						ErrorString;
var EPingState					PingState;

function WindowShown()
{
	local UBrowserSupersetList l;

	Super.WindowShown();
	if(VSplitter.bWindowVisible)
	{
		if(UWindowVSplitter(InfoClient.ParentWindow) != None)
			VSplitter.SplitPos = UWindowVSplitter(InfoClient.ParentWindow).SplitPos;

		InfoClient.SetParent(VSplitter);
	}

	InfoClient.Server = InfoItem;
	if(InfoItem != None)
		InfoWindow.WindowTitle = InfoName$" - "$InfoItem.HostName;
	else
		InfoWindow.WindowTitle = InfoName;

	ResumePinging();

	for(l = UBrowserSupersetList(SupersetList.Next); l != None; l = UBrowserSupersetList(l.Next))
		l.SuperSetWindow.ResumePinging();
}

function WindowHidden()
{
	local UBrowserSupersetList l;

	Super.WindowHidden();
	SuspendPinging();

	for(l = UBrowserSupersetList(SupersetList.Next); l != None; l = UBrowserSupersetList(l.Next))
		l.SuperSetWindow.SuspendPinging();
}

function SuspendPinging()
{
	if(bSuspendPingOnClose)
		bPingSuspend = True;
}

function ResumePinging()
{
	if(!bHadInitialRefresh)
		Refresh(False, True);	

	bPingSuspend = False;
	if(bPingResume)
	{
		bPingResume = False;
		UnpingedList.PingNext(bPingResumeIntial, bNoSort);
	}
}

function Created()
{
	local Class<UBrowserServerGrid> C;
	
	ServerListClass = class<UBrowserServerList>(DynamicLoadObject(ServerListClassName, class'Class'));
	C = class<UBrowserServerGrid>(DynamicLoadObject(GridClass, class'Class'));
	Grid = UBrowserServerGrid(CreateWindow(C, 0, 0, WinWidth, WinHeight));
	Grid.SetAcceptsFocus();

	SubsetList = new class'UBrowserSubsetList';
	SubsetList.SetupSentinel();

	SupersetList = new class'UBrowserSupersetList';
	SupersetList.SetupSentinel();

	VSplitter = UWindowVSplitter(CreateWindow(class'UWindowVSplitter', 0, 0, WinWidth, WinHeight));
	VSplitter.SetAcceptsFocus();
	VSplitter.MinWinHeight = 60;
	VSplitter.HideWindow();
	InfoWindow = UBrowserMainClientWindow(GetParent(class'UBrowserMainClientWindow')).InfoWindow;
	InfoClient = UBrowserInfoClientWindow(InfoWindow.ClientArea);

	if(Root.WinHeight >= MinHeightForSplitter)
		ShowInfoArea(True, False);
}

function ShowInfoArea(bool bShow, optional bool bFloating, optional bool bNoActivate)
{
	if(bShow)
	{
		if(bFloating)
		{
			VSplitter.HideWindow();
			VSplitter.TopClientWindow = None;
			VSplitter.BottomClientWindow = None;
			InfoClient.SetParent(InfoWindow);
			Grid.SetParent(Self);
			Grid.SetSize(WinWidth, WinHeight);
			if(!InfoWindow.bWindowVisible)
				InfoWindow.ShowWindow();
			if(!bNoActivate)
				InfoWindow.BringToFront();
		}
		else
		{
			InfoWindow.HideWindow();
			VSplitter.ShowWindow();
			VSplitter.SetSize(WinWidth, WinHeight);
			Grid.SetParent(VSplitter);
			InfoClient.SetParent(VSplitter);
			VSplitter.TopClientWindow = Grid;
			VSplitter.BottomClientWindow = InfoClient;
		}
	}
	else
	{
		InfoWindow.HideWindow();
		VSplitter.HideWindow();
		VSplitter.TopClientWindow = None;
		VSplitter.BottomClientWindow = None;
		InfoClient.SetParent(InfoWindow);
		Grid.SetParent(Self);
		Grid.SetSize(WinWidth, WinHeight);
	}
}

function AutoInfo(UBrowserServerList I)
{
	if(Root.WinHeight >= MinHeightForSplitter || InfoWindow.bWindowVisible)
		ShowInfo(I, True);
}

function ShowInfo(UBrowserServerList I, optional bool bAutoInfo)
{
	if(I == None) return;
	ShowInfoArea(True, Root.WinHeight < MinHeightForSplitter, bAutoInfo);

	InfoItem = I;
	InfoClient.Server = InfoItem;
	InfoWindow.WindowTitle = InfoName$" - "$InfoItem.HostName;
	I.ServerStatus();
}

function ResolutionChanged(float W, float H)
{
	if(Root.WinHeight >= MinHeightForSplitter)
		ShowInfoArea(True, False);
	else
		ShowInfoArea(False, True);
	
	if(InfoWindow != None)
		InfoWindow.ResolutionChanged(W, H);

	Super.ResolutionChanged(W, H);
}

function Resized()
{
	Super.Resized();
	if(VSplitter.bWindowVisible)
	{
		VSplitter.SetSize(WinWidth, WinHeight);
		VSplitter.OldWinHeight = VSplitter.WinHeight;
		VSplitter.SplitPos = VSplitter.WinHeight - Min(VSplitter.WinHeight / 2, 250);
	}
	else
		Grid.SetSize(WinWidth, WinHeight);
}

function AddSubset(UBrowserSubsetFact Subset)
{
	local UBrowserSubsetList l;

	for(l = UBrowserSubsetList(SubsetList.Next); l != None; l = UBrowserSubsetList(l.Next))
		if(l.SubsetFactory == Subset)
			return;
	
	l = UBrowserSubsetList(SubsetList.Append(class'UBrowserSubsetList'));
	l.SubsetFactory = Subset;
}

function AddSuperSet(UBrowserServerListWindow Superset)
{
	local UBrowserSupersetList l;

	for(l = UBrowserSupersetList(SupersetList.Next); l != None; l = UBrowserSupersetList(l.Next))
		if(l.SupersetWindow == Superset)
			return;
	
	l = UBrowserSupersetList(SupersetList.Append(class'UBrowserSupersetList'));
	l.SupersetWindow = Superset;
}

function RemoveSubset(UBrowserSubsetFact Subset)
{
	local UBrowserSubsetList l;

	for(l = UBrowserSubsetList(SubsetList.Next); l != None; l = UBrowserSubsetList(l.Next))
		if(l.SubsetFactory == Subset)
			l.Remove();
}

function RemoveSuperset(UBrowserServerListWindow Superset)
{
	local UBrowserSupersetList l;

	for(l = UBrowserSupersetList(SupersetList.Next); l != None; l = UBrowserSupersetList(l.Next))
		if(l.SupersetWindow == Superset)
			l.Remove();
}

function UBrowserServerList AddFavorite(UBrowserServerList Server)
{
	return UBrowserServerListWindow(UBrowserMainClientWindow(GetParent(class'UBrowserMainClientWindow')).Favorites.Page).AddFavorite(Server);
}

function Refresh(optional bool bBySuperset, optional bool bInitial, optional bool bSaveExistingList, optional bool bInNoSort)
{
	bHadInitialRefresh = True;

	if(!bSaveExistingList)
	{
		InfoItem = None;
		InfoClient.Server = None;
	}

	if(!bSaveExistingList && PingedList != None)
	{
		PingedList.DestroyList();
		PingedList = None;
		Grid.SelectedServer = None;
	}

	if(PingedList == None)
	{
		PingedList=New ServerListClass;
		PingedList.Owner = Self;
		PingedList.SetupSentinel(True);
		PingedList.bSuspendableSort = True;
	}
	else
	{
		TagServersAsOld();
	}

	if(UnpingedList != None)
		UnpingedList.DestroyList();
	
	if(!bSaveExistingList)
	{
		UnpingedList = New ServerListClass;
		UnpingedList.Owner = Self;
		UnpingedList.SetupSentinel(False);
	}

	PingState = PS_QueryServer;
	ShutdownFactories(bBySuperset);
	CreateFactories(bSaveExistingList);
	Query(bBySuperset, bInitial, bInNoSort);

	if(!bInitial)
		RefreshSubsets();
}

function TagServersAsOld()
{
	local UBrowserServerList l;

	for(l = UBrowserServerList(PingedList.Next);l != None;l = UBrowserServerList(l.Next)) 
		l.bOldServer = True;
}

function RemoveOldServers()
{
	local UBrowserServerList l, n;

	l = UBrowserServerList(PingedList.Next);
	while(l != None) 
	{
		n = UBrowserServerList(l.Next);

		if(l.bOldServer)
		{
			if(Grid.SelectedServer == l)
				Grid.SelectedServer = n;

			l.Remove();
		}
		l = n;
	}
}

function RefreshSubsets()
{
	local UBrowserSubsetList l, NextSubset;

	for(l = UBrowserSubsetList(SubsetList.Next); l != None; l = UBrowserSubsetList(l.Next))
		l.bOldElement = True;

	l = UBrowserSubsetList(SubsetList.Next);
	while(l != None && l.bOldElement)
	{
		NextSubset = UBrowserSubsetList(l.Next);
		l.SubsetFactory.Owner.Owner.Refresh(True);
		l = NextSubset;
	}
}

function RePing()
{
	PingState = PS_RePinging;
	PingedList.InvalidatePings();
	PingedList.PingServers(True, False);
}

function QueryFinished(UBrowserServerListFactory Fact, bool bSuccess, optional string ErrorMsg)
{
	local int i;
	local bool bDone;

	bDone = True;
	for(i=0;i<10;i++)
	{
		if(Factories[i] != None)
		{
			if(Factories[i] == Fact)
				QueryDone[i] = 1;
			if(QueryDone[i] == 0)
				bDone = False;
		}
	}

	if(!bSuccess)
	{
		PingState = PS_QueryFailed;
		ErrorString = ErrorMsg;

		// don't ping and report success if we have no servers.
		if(bDone && UnpingedList.Count() == 0)
		{
			if( bFallbackFactories )
			{
				FallbackFactory++;
				if( ListFactories[FallbackFactory] != "" )
					Refresh();	// try the next fallback master server
				else
					FallbackFactory = 0;
			}
			return;
		}
	}
	else
		ErrorString = "";

	if(bDone)
	{
		RemoveOldServers();

		PingState = PS_Pinging;
		if(!bNoSort && !Fact.bIncrementalPing)
			PingedList.Sort();
		UnpingedList.PingServers(True, bNoSort || Fact.bIncrementalPing);
	}
}

function PingFinished()
{
	PingState = PS_Done;
}

function CreateFactories(bool bUsePingedList)
{
	local int i;

	for(i=0;i<10;i++)
	{
		if(ListFactories[i] == "")
			break;
		if(!bFallbackFactories || FallbackFactory == i)
		{
			Factories[i] = UBrowserServerListFactory(BuildObjectWithProperties(ListFactories[i]));
			
			Factories[i].PingedList = PingedList;
			Factories[i].UnpingedList = UnpingedList;
		
			if(bUsePingedList)
				Factories[i].Owner = PingedList;
			else
				Factories[i].Owner = UnpingedList;
		}
		QueryDone[i] = 0;
	}	
}

function ShutdownFactories(optional bool bBySuperset)
{
	local int i;

	for(i=0;i<10;i++)
	{
		if(Factories[i] != None) 
		{
			Factories[i].Shutdown(bBySuperset);
			Factories[i] = None;
		}
	}	
}

function Query(optional bool bBySuperset, optional bool bInitial, optional bool bInNoSort)
{
	local int i;

	bNoSort = bInNoSort;

	// Query all our factories
	for(i=0;i<10;i++)
	{
		if(Factories[i] != None)
			Factories[i].Query(bBySuperset, bInitial);
	}
}

function Paint(Canvas C, float X, float Y)
{
	DrawStretchedTexture(C, 0, 0, WinWidth, WinHeight, Texture'BlackTexture');
}

function Tick(float Delta)
{
	PingedList.Tick(Delta);

	if(PingedList.bNeedUpdateCount)
	{
		PingedList.UpdateServerCount();
		PingedList.bNeedUpdateCount = False;
	}

	// AutoRefresh local servers
	if(AutoRefreshTime > 0)
	{
		TimeElapsed += Delta;
		
		if(TimeElapsed > AutoRefreshTime)
		{
			TimeElapsed = 0;
			Refresh(,,True, bNoAutoSort);
		}
	}	
}

function BeforePaint(Canvas C, float X, float Y)
{
	local UBrowserMainWindow W;
	local UBrowserSupersetList l;
	local EPingState P;
	local int PercentComplete;
	local int TotalReturnedServers;
	local string E;
	local int TotalServers;
	local int PingedServers;
	local int MyServers;

	Super.BeforePaint(C, X, Y);

	W = UBrowserMainWindow(GetParent(class'UBrowserMainWindow'));
	l = UBrowserSupersetList(SupersetList.Next);

	if(l != None && PingState != PS_RePinging)
	{
		P = l.SupersetWindow.PingState;
		PingState = P;

		if(P == PS_QueryServer)
			TotalReturnedServers = l.SupersetWindow.UnpingedList.Count();

		PingedServers = l.SupersetWindow.PingedList.Count();
		TotalServers = l.SupersetWindow.UnpingedList.Count() + PingedServers;
		MyServers = PingedList.Count();
	
		E = l.SupersetWindow.ErrorString;
	}
	else
	{
		P = PingState;
		if(P == PS_QueryServer)
			TotalReturnedServers = UnpingedList.Count();

		PingedServers = PingedList.Count();
		TotalServers = UnpingedList.Count() + PingedServers;
		MyServers = PingedList.Count();

		E = ErrorString;
	}

	if(TotalServers > 0)
		PercentComplete = PingedServers*100.0/TotalServers;

	switch(P)
	{
	case PS_QueryServer:
		if(TotalReturnedServers > 0)
			W.DefaultStatusBarText(QueryServerText$" ("$TotalReturnedServers$" "$ServerCountName$")");
		else
			W.DefaultStatusBarText(QueryServerText);
		break;
	case PS_QueryFailed:
		W.DefaultStatusBarText(QueryFailedText$E);
		break;
	case PS_Pinging:
	case PS_RePinging:
		W.DefaultStatusBarText(PingingText$" "$PercentComplete$"% "$CompleteText$". "$MyServers$" "$ServerCountName$", "$PingedList.TotalPlayers$" "$PlayerCountName);
		break;
	case PS_Done:
		W.DefaultStatusBarText(MyServers$" "$ServerCountName$", "$PingedList.TotalPlayers$" "$PlayerCountName);
		break;
	}
}

defaultproperties
{
	GridClass="UBrowser.UBrowserServerGrid";
	bSuspendPingOnClose=True
	PlayerCountName="Players"
	ServerCountName="Servers"
	QueryServerText="Querying master server"
	QueryFailedText="Master Server Failed: "
	PingingText="Pinging Servers"
	CompleteText="Complete"
	ServerListClassName="UBrowser.UBrowserServerList"
	RightClickMenuClass=class'UBrowserRightClickMenu'
	bShowFailedServers=False
	InfoName="Info"
	bFallbackFactories=False
	FallbackFactory=0
}
