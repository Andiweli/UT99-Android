class UBrowserHTTPClient extends UBrowserBufferedTcpLink;

var IpAddr		ServerIpAddr;
var string		ServerAddress;
var string		ServerURI;
var int			ServerPort;
var int			CurrentState;
var int			ErrorCode;
var bool		bClosed;

var globalconfig string	ProxyServerAddress;
var globalconfig int	ProxyServerPort;

const Connecting		= 0;
const WaitingForHeader	= 1;
const ReceivingHeader	= 2;
const ReceivingData		= 3;
const HadError			= 4;

function PostBeginPlay()
{
	Super.PostBeginPlay();
	ServerIpAddr.Addr = 0;
	Disable('Tick');
}

function Browse(string InAddress, string InURI, optional int InPort, optional int InTimeout)
{
	CurrentState = Connecting;

	ServerAddress = InAddress;
	ServerURI = InURI;
	if(InPort == 0)
		ServerPort = 80;
	else
		ServerPort = InPort;
	
	if(InTimeout > 0 )
		SetTimer(InTimeout, False);

	ResetBuffer();

	if(ProxyServerAddress != "")
	{
		ServerIpAddr.Port = ProxyServerPort;
		if(ServerIpAddr.Addr == 0)
			Resolve( ProxyServerAddress );
		else
			DoBind();
	}
	else
	{
		ServerIpAddr.Port = ServerPort;
		if(ServerIpAddr.Addr == 0)
			Resolve( ServerAddress );
		else
			DoBind();
	}
}

function Resolved( IpAddr Addr )
{
	// Set the address
	ServerIpAddr.Addr = Addr.Addr;

	if( ServerIpAddr.Addr == 0 )
	{
		Log( "UBrowserHTTPClient: Invalid server address" );
		SetError(-1);
		return;
	}
	
	DoBind();
}

function DoBind()
{
	if( BindPort() == 0 )
	{
		Log( "UBrowserHTTPLink: Error binding local port." );
		SetError(-2);
		return;
	}

	Open( ServerIpAddr );
	bClosed = False;
}

event Timer()
{
	SetError(-3);	
}

event Opened()
{
	Enable('Tick');
	if(ProxyServerAddress != "")
		SendBufferedData("GET http://"$ServerAddress$":"$string(ServerPort)$ServerURI$" HTTP/1.1"$CR$LF);
	else
		SendBufferedData("GET "$ServerURI$" HTTP/1.1"$CR$LF);
	SendBufferedData("User-Agent: Unreal"$CR$LF);
	SendBufferedData("Connection: close"$CR$LF);
	SendBufferedData("Host: "$ServerAddress$":"$ServerPort$CR$LF$CR$LF);

	CurrentState = WaitingForHeader;
}

function SetError(int Code)
{
	Disable('Tick');
	SetTimer(0, False);
	ResetBuffer();

	CurrentState = HadError;
	ErrorCode = Code;

	if(!IsConnected() || !Close())
		HTTPError(ErrorCode);
}

event Closed()
{
	bClosed = True;
}

function HTTPReceivedData(string Data)
{
}

function HTTPError(int Code)
{
}

event Tick(float DeltaTime)
{
	local string Line;
	local bool bGotData;
	local int NextState;
	local int i;
	local int Result;

	Super.Tick(DeltaTime);
	DoBufferQueueIO();

	do
	{
		NextState = CurrentState;
		switch(CurrentState)
		{
		case WaitingForHeader:
			bGotData = ReadBufferedLine(Line);
			if(bGotData)
			{
				i = InStr(Line, " ");
				Result = Int(Mid(Line, i+1));
				if(Result != 200)
				{
					SetError(Result);
					return;
				}
					
				NextState = ReceivingHeader;
			}	
			break;
		case ReceivingHeader:
			bGotData = ReadBufferedLine(Line);
			if(bGotData)
			{
				if(Line == "")
					NextState = ReceivingData;
			}	
			break;
		case ReceivingData:
			bGotData = False;
			break;
		default:
			bGotData = False;
			break;
		}
		CurrentState = NextState;
	} until(!bGotData);

	if(bClosed)
	{
		Disable('Tick');
		if(CurrentState == ReceivingData)
			HTTPReceivedData(InputBuffer);

		if(CurrentState == HadError)
			HTTPError(ErrorCode);
	}
}
