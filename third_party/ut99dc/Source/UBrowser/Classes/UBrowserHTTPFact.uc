class UBrowserHTTPFact extends UBrowserServerListFactory;

var UBrowserHTTPLink Link;

var() config string		MasterServerAddress;	// Address of the master server
var() config string		MasterServerURI;
var() config int		MasterServerTCPPort;	// Optional port that the master server is listening on
var() config int		MasterServerTimeout;

function Query(optional bool bBySuperset, optional bool bInitial)
{
	Super.Query(bBySuperset, bInitial);

	Link = GetPlayerOwner().GetEntryLevel().Spawn(class'UBrowserHTTPLink');

	Link.MasterServerAddress = MasterServerAddress;
	Link.MasterServerURI = MasterServerURI;
	Link.MasterServerTCPPort = MasterServerTCPPort;
	Link.MasterServerTimeout = MasterServerTimeout;
	Link.OwnerFactory = Self;
	Link.Start();

}

function QueryFinished(bool bSuccess, optional string ErrorMsg)
{
	Link.Destroy();
	Link = None;

	Super.QueryFinished(bSuccess, ErrorMsg);	
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
	MasterServerTCPPort=80
	MasterServerAddress="master.telefragged.com"
	MasterServerURI="/servers.txt"
	MasterServerTimeout=10
}