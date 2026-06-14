//=============================================================================
// UBrowserLocalLink: Receives LAN beacons from servers.
//=============================================================================
class UBrowserLocalLink extends UdpLink
	transient;

// Misc
var UBrowserLocalFact			OwnerFactory;

// Config
var string						BeaconProduct;
var int							ServerBeaconPort;

function Start()
{
	local int p;

	if( BindPort() == 0 )
	{
		OwnerFactory.QueryFinished(False, "UBrowserLocalLink: Could not bind to a free port.");
		return;
	}
	BroadcastBeacon();
}

function Timer()
{
	OwnerFactory.QueryFinished(True);
}

function BroadcastBeacon()
{
	local IpAddr Addr;
	local int i;

	Addr.Addr = BroadcastAddr;

	for(i=0;i<10;i++)
	{
		Addr.Port = ServerBeaconPort + i;
		SendText( Addr, "REPORTQUERY" );
	}
}

event ReceivedText( IpAddr Addr, string Text )
{
	local int n;
	local int QueryPort;
	local string Address;

	n = len(BeaconProduct);
	if( Left(Text,n+1) ~= (BeaconProduct$" ") )
	{
		QueryPort = int(Mid(Text, n+1));
		Address = IpAddrToString(Addr);
		Address = Left(Address, InStr(Address, ":"));
		OwnerFactory.FoundServer(Address, QueryPort, "", BeaconProduct);
	}
}
