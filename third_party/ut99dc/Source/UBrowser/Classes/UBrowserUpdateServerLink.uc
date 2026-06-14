class UBrowserUpdateServerLink expands UBrowserHTTPClient;

var config string		UpdateServerAddress;
var config int			UpdateServerPort;
var config int			UpdateServerTimeout;
var string				URIs[10];
var int					CurrentURI;
var int					MaxURI;
var UBrowserUpdateServerWindow	UpdateWindow;

const GetMOTD = 3;
const GetFallback = 2;
const GetMaster = 1;
const GetIRC = 0;

function QueryUpdateServer()
{
	SetupURIs();
	CurrentURI = MaxURI;
	BrowseCurrentURI();
}

function SetupURIs()
{
	MaxURI = 3;
	URIs[3] = "/UpdateServer/motd"$Level.EngineVersion$".html";
	URIs[2] = "/UpdateServer/motdfallback.html";
	URIs[1] = "/UpdateServer/masterserver.txt";
	URIs[0] = "/UpdateServer/ircserver.txt";
}

function BrowseCurrentURI()
{
	Browse(UpdateServerAddress, URIs[CurrentURI], UpdateServerPort, UpdateServerTimeout);
}

function Failure()
{
	UpdateWindow.Failure();
}

function Success()
{
	UpdateWindow.Success();
}

function ProcessData(string Data)
{
	switch(CurrentURI)
	{
	case GetMOTD:
	case GetFallback:
		UpdateWindow.SetMOTD(Data);
		break;
	case GetMaster:
		UpdateWindow.SetMasterServer(Data);
		break;
	case GetIRC:
		UpdateWindow.SetIRCServer(Data);
		break;
	}
}

//////////////////////////////////////////////////////////////////
// HTTPClient functions
//////////////////////////////////////////////////////////////////

function HTTPError(int ErrorCode)
{	
	if(ErrorCode == 404 && CurrentURI == GetMOTD)
	{
		CurrentURI = GetFallback;
		BrowseCurrentURI();
	}
	else
		Failure();
}

function HTTPReceivedData(string Data)
{
	ProcessData(Data);

	if(CurrentURI == MaxURI)
		CurrentURI--;

	if(CurrentURI == 0)
		Success();
	else
	{
		CurrentURI--;
		BrowseCurrentURI();
	}
}

defaultproperties
{
	UpdateServerAddress="unreal.epicgames.com"
	UpdateServerPort=80
	UpdateServerTimeout=5
}
