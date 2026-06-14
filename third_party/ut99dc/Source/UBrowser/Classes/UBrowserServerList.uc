//=============================================================================
// UBrowserServerList
//		Stores a server entry in an Unreal Server List
//=============================================================================

class UBrowserServerList extends UWindowList;

// Valid for sentinel only
var	UBrowserServerListWindow	Owner;
var int					TotalServers;
var int					TotalPlayers;
var int					TotalMaxPlayers;
var bool				bNeedUpdateCount;

// Config
var config int			MaxSimultaneousPing;

// Master server variables
var string				IP;
var int					QueryPort;
var string				Category;		// Master server categorization
var string				GameName;		// Unreal, Unreal Tournament

// State of the ping
var UBrowserServerPing	ServerPing;
var bool				bPinging;
var bool				bPingFailed;
var bool				bPinged;
var bool				bNoInitalPing;
var bool				bOldServer;

// Rules and Lists
var UBrowserRulesList	RulesList;
var UBrowserPlayerList  PlayerList;
var bool				bKeepDescription;	// don't overwrite HostName

// Unreal server variables
var bool				bLocalServer;
var float				Ping;
var string				HostName;
var int					GamePort;
var string				MapName;
var string				MapTitle;
var string				MapDisplayName;
var string				GameType;
var string				GameMode;
var int					NumPlayers;
var int					MaxPlayers;
var int					GameVer;
var int					MinNetVer;

function DestroyListItem() 
{
	Owner = None;

	if(ServerPing != None)
	{
		ServerPing.Destroy();
		ServerPing = None;
	}
	Super.DestroyListItem();
}

function QueryFinished(UBrowserServerListFactory Fact, bool bSuccess, optional string ErrorMsg)
{
	Owner.QueryFinished(Fact, bSuccess, ErrorMsg);
}


// Functions for server list entries only.
function PingServer(bool bInitial, bool bJustThisServer, bool bNoSort)
{
	// Create the UdpLink to ping the server
	ServerPing = GetPlayerOwner().GetEntryLevel().Spawn(class'UBrowserServerPing');
	ServerPing.Server = Self;
	ServerPing.StartQuery('GetInfo', 2);
	ServerPing.bInitial = bInitial;
	ServerPing.bJustThisServer = bJustThisServer;
	ServerPing.bNoSort = bNoSort;
	bPinging = True;
}

function ServerStatus()
{
	// Create the UdpLink to ping the server
	ServerPing = GetPlayerOwner().GetEntryLevel().Spawn(class'UBrowserServerPing');
	ServerPing.Server = Self;
	ServerPing.StartQuery('GetStatus', 2);
	bPinging = True;
}

function StatusDone(bool bSuccess)
{
	// Destroy the UdpLink
	ServerPing.Destroy();
	ServerPing = None;

	bPinging = False;

	RulesList.SortByColumn(RulesList.SortColumn);
	PlayerList.SortByColumn(PlayerList.SortColumn);
}

function CancelPing()
{
	if(bPinging && ServerPing != None && ServerPing.bJustThisServer)
		PingDone(False, True, False, True);
}

function PingDone(bool bInitial, bool bJustThisServer, bool bSuccess, bool bNoSort)
{
	local UBrowserServerListWindow W;
	local UBrowserServerList OldSentinel;

	// Destroy the UdpLink
	if(ServerPing != None)
		ServerPing.Destroy();
	
	ServerPing = None;

	bPinging = False;
	bPingFailed = !bSuccess;
	bPinged = True;

	OldSentinel = UBrowserServerList(Sentinel);
	if(!bNoSort)
	{
		Remove();

		// Move to the ping list
		if(!bPingFailed || (OldSentinel != None && OldSentinel.Owner != None && OldSentinel.Owner.bShowFailedServers))
		{
			if(OldSentinel.Owner.PingedList != None)
				OldSentinel.Owner.PingedList.AppendItem(Self);
		}
	}
	else
	{
		if(OldSentinel != None && OldSentinel.Owner != None && OldSentinel != OldSentinel.Owner.PingedList)
			Log("Unsorted PingDone lost as it's not in ping list!");
	}

	if(Sentinel != None)
	{
		UBrowserServerList(Sentinel).bNeedUpdateCount = True;

		if(bInitial)
			ConsiderForSubsets();
	}

	if(!bJustThisServer)
		if(OldSentinel != None)
		{
			W = OldSentinel.Owner;

			if(W.bPingSuspend)
			{
				W.bPingResume = True;
				W.bPingResumeIntial = bInitial;
			}
			else
				OldSentinel.PingNext(bInitial, bNoSort);
		}
}

function ConsiderForSubsets()
{
	local UBrowserSubsetList l;

	for(l = UBrowserSubsetList(UBrowserServerList(Sentinel).Owner.SubsetList.Next); l != None; l = UBrowserSubsetList(l.Next))
	{
		l.SubsetFactory.ConsiderItem(Self);
	}
}

// Functions for sentinel only

function InvalidatePings()
{
	local UBrowserServerList l;

	for(l = UBrowserServerList(Next);l != None;l = UBrowserServerList(l.Next)) 
		l.Ping = 9999;
}

function PingServers(bool bInitial, bool bNoSort)
{
	local UBrowserServerList l;
	
	bPinging = False;

	for(l = UBrowserServerList(Next);l != None;l = UBrowserServerList(l.Next)) 
	{
		l.bPinging = False;
		l.bPingFailed = False;
		l.bPinged = False;
	}

	PingNext(bInitial, bNoSort);
}

function PingNext(bool bInitial, bool bNoSort)
{
	local int TotalPinging;
	local UBrowserServerList l;
	local bool bDone;
	
	TotalPinging = 0;
	
	bDone = True;
	for(l = UBrowserServerList(Next);l != None;l = UBrowserServerList(l.Next)) 
	{
		if(!l.bPinged)
			bDone = False;
		if(l.bPinging)
			TotalPinging ++;
	}
	
	if(bDone && Owner != None)
	{
		bPinging = False;
		Owner.PingFinished();
	}
	else
	if(TotalPinging < MaxSimultaneousPing)
	{
		for(l = UBrowserServerList(Next);l != None;l = UBrowserServerList(l.Next))
		{
			if(		!l.bPinging 
				&&	!l.bPinged 
				&&	(!bInitial || !l.bNoInitalPing)
				&&	TotalPinging < MaxSimultaneousPing
			)
			{
				TotalPinging ++;		
				l.PingServer(bInitial, False, bNoSort);
			}

			if(TotalPinging >= MaxSimultaneousPing)
				break;
		}
	}
}

function UBrowserServerList FindExistingServer(string FindIP, int FindQueryPort)
{
	local UWindowList l;

	for(l = Next;l != None;l = l.Next)
	{
		if(UBrowserServerList(l).IP == FindIP && UBrowserServerList(l).QueryPort == FindQueryPort)
			return UBrowserServerList(l);
	}
	return None;
}

function PlayerPawn GetPlayerOwner()
{
	return UBrowserServerList(Sentinel).Owner.GetPlayerOwner();
}

function UWindowList CopyExistingListItem(Class<UWindowList> ItemClass, UWindowList SourceItem)
{
	local UBrowserServerList L;

	L = UBrowserServerList(Super.CopyExistingListItem(ItemClass, SourceItem));

	L.bLocalServer	= UBrowserServerList(SourceItem).bLocalServer;
	L.IP			= UBrowserServerList(SourceItem).IP;
	L.QueryPort		= UBrowserServerList(SourceItem).QueryPort;
	L.Ping			= UBrowserServerList(SourceItem).Ping;
	L.HostName		= UBrowserServerList(SourceItem).HostName;
	L.GamePort		= UBrowserServerList(SourceItem).GamePort;
	L.MapName		= UBrowserServerList(SourceItem).MapName;
	L.MapTitle		= UBrowserServerList(SourceItem).MapTitle;
	L.MapDisplayName= UBrowserServerList(SourceItem).MapDisplayName;
	L.MapName		= UBrowserServerList(SourceItem).MapName;
	L.GameType		= UBrowserServerList(SourceItem).GameType;
	L.GameMode		= UBrowserServerList(SourceItem).GameMode;
	L.NumPlayers	= UBrowserServerList(SourceItem).NumPlayers;
	L.MaxPlayers	= UBrowserServerList(SourceItem).MaxPlayers;
	L.GameVer		= UBrowserServerList(SourceItem).GameVer;
	L.MinNetVer		= UBrowserServerList(SourceItem).MinNetVer;
	L.bKeepDescription = UBrowserServerList(SourceItem).bKeepDescription;

	return L;
}

function int Compare(UWindowList T, UWindowList B)
{
	CompareCount++;
	return UBrowserServerList(Sentinel).Owner.Grid.Compare(UBrowserServerList(T), UBrowserServerList(B));
}

function AppendItem(UWindowList L)
{
	Super.AppendItem(L);
	UBrowserServerList(Sentinel).bNeedUpdateCount = True;
}

function Remove()
{
	local UBrowserServerList S;

	S = UBrowserServerList(Sentinel);
	Super.Remove();

	if(S != None)
		S.bNeedUpdateCount = True;
}

// Sentinel only
// FIXME: slow when lots of servers!!
function UpdateServerCount()
{
	local UBrowserServerList l;

	TotalServers = 0;
	TotalPlayers = 0;
	TotalMaxPlayers = 0;

	for(l = UBrowserServerList(Next);l != None;l = UBrowserServerList(l.Next))
	{
		TotalServers++;
		TotalPlayers += l.NumPlayers;
		TotalMaxPlayers += l.MaxPlayers;
	}
}

function bool DecodeServerProperties(string Data)
{
	return True;
}


defaultproperties
{
	MaxSimultaneousPing=10
}