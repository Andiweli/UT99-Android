class UBrowserLocalFact extends UBrowserServerListFactory;

var UBrowserLocalLink	Link;

// Config
var config string		BeaconProduct;
var config int			ServerBeaconPort;

function string UT99AndroidV129NextField(out string S)
{
	local int P;
	local string R;

	P = InStr(S, "|");
	if(P < 0)
	{
		R = S;
		S = "";
		return R;
	}

	R = Left(S, P);
	S = Mid(S, P + 1);
	return R;
}

function bool UT99AndroidV129AddNativeLANResult()
{
	local string Scan;
	local string IP;
	local string QueryPortText;
	local string GamePortText;
	local string HostName;
	local int QueryPort;

	// Native Android fallback:
	// The stock UBrowserLocalLink is fragile on Android/OUYA because the menu
	// sometimes never spawns/sends the REPORTQUERY path. LANQUERY performs the
	// same proven /24 direct scan as OPENLAN, but returns a browser-list tuple:
	//   ip|queryport|gameport|hostname
	Scan = GetPlayerOwner().ConsoleCommand("LANQUERY");

	// ConsoleCommand/Ar.Logf may append a line break. Trim a couple of common
	// trailing control chars without relying on newer UnrealScript helpers.
	while(Len(Scan) > 0 && (Right(Scan, 1) == Chr(10) || Right(Scan, 1) == Chr(13)))
		Scan = Left(Scan, Len(Scan) - 1);

	if(Scan == "" || InStr(Scan, "|") < 0)
		return False;

	IP            = UT99AndroidV129NextField(Scan);
	QueryPortText = UT99AndroidV129NextField(Scan);
	GamePortText  = UT99AndroidV129NextField(Scan);
	HostName      = Scan;

	QueryPort = int(QueryPortText);
	if(IP == "" || QueryPort <= 0)
		return False;

	if(HostName == "")
		HostName = "UT99 Android LAN";

	Log("UT99_ANDROID_V129_LANLIST add native server "$IP$":"$QueryPortText$" game="$GamePortText$" host="$HostName);

	FoundServer(IP, QueryPort, "", BeaconProduct, HostName);
	return True;
}

function Query(optional bool bBySuperset, optional bool bInitial)
{
	Super.Query(bBySuperset, bInitial);

	Owner = PingedList;

	// Update status bar
	Owner.Owner.PingFinished();

	// Android/OUYA direct list fallback. This does not auto-join; it only
	// injects the found host into the normal LAN Servers list so the user can
	// select it like a regular UT99 LAN server.
	if(UT99AndroidV129AddNativeLANResult())
	{
		QueryFinished(True);
		return;
	}

	Link = GetPlayerOwner().GetEntryLevel().Spawn(class'UBrowserLocalLink');

	Link.BeaconProduct = BeaconProduct;
	Link.ServerBeaconPort = ServerBeaconPort;

	Link.OwnerFactory = Self;
	Link.Start();
}

function UBrowserServerList FoundServer(string IP, int QueryPort, string Category, string GameName, optional string HostName)
{
	local UBrowserServerList l;

	l = Super.FoundServer(IP, QueryPort, Category, GameName, HostName);
	l.bLocalServer = True;

	if(!l.bPinging)
		l.PingServer(True, True, Owner.Owner.bNoSort);

	return l;
}

function QueryFinished(bool bSuccess, optional string ErrorMsg)
{
	if(Link != None)
	{
		Link.Destroy();
		Link = None;
	}

	Super.QueryFinished(bSuccess, ErrorMsg);	

	// Update status bar
	Owner.Owner.PingFinished();
}

function Shutdown(optional bool bBySuperset)
{
	if(Link != None)
		Link.Destroy();
	Link = None;
	Super.Shutdown(bBySuperset);
}

defaultproperties
{
	BeaconProduct="ut"
	ServerBeaconPort=8777
	bIncrementalPing=True
}
