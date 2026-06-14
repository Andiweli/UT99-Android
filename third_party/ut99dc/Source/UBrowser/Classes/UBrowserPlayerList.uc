//=============================================================================
// UBrowserPlayerList - The player list returned by the server.
//=============================================================================
class UBrowserPlayerList extends UWindowList;

var string			PlayerName;
var string			PlayerMesh;
var string			PlayerSkin;
var string			PlayerFace;
var string			PlayerTeam;
var int				PlayerFrags;
var int				PlayerPing;
var int				PlayerID;

// Sentinel Only
var int				SortColumn;
var bool			bDescending;


function SortByColumn(int Column)
{
	if(SortColumn == Column)
	{
		bDescending = !bDescending;
	}
	else
	{
		SortColumn = Column;
		bDescending = False;
	}

	Sort();
}

function int Compare(UWindowList T, UWindowList B)
{
	local int Result;
	local UBrowserPlayerList PT, PB;

	if(B == None) return -1; 

	PT = UBrowserPlayerList(T);
	PB = UBrowserPlayerList(B);

	switch(UBrowserPlayerList(Sentinel).SortColumn)
	{
	case 0:
		if(Caps(PT.PlayerName) < Caps(PB.PlayerName))
			Result = -1;
		else
		if(PT.PlayerName > PB.PlayerName)
			Result = 1;
		else
			Result = (PT.PlayerPing - PB.PlayerPing);
		break;
	case 1:
		if(PT.PlayerFrags > PB.PlayerFrags)
			Result = -1;
		else
		if(PT.PlayerFrags < PB.PlayerFrags)
			Result = 1;
		else
		{
			if(PT.PlayerName < PB.PlayerName)
				Result = -1;
			else
				Result = 1;
		}
		break;
	case 2:
		if(PT.PlayerPing < PB.PlayerPing)
			Result = -1;
		else
		if(PT.PlayerPing > PB.PlayerPing)
			Result = 1;
		else
		{
			if(PT.PlayerName < PB.PlayerName)
				Result = -1;
			else
				Result = 1;
		}
		break;
	case 3:
		if(PT.PlayerTeam > PB.PlayerTeam)
			Result = -1;
		else if(PT.PlayerTeam < PB.PlayerTeam)
			Result = 1;
		else
		{
			if(PT.PlayerName < PB.PlayerName)
				Result = -1;
			else
				Result = 1;
		}
		break;
	case 4:
		if(PT.PlayerMesh < PB.PlayerMesh)
			Result = -1;
		else
		if(PT.PlayerMesh > PB.PlayerMesh)
			Result = 1;
		else
		{
			if(PT.PlayerName < PB.PlayerName)
				Result = -1;
			else
				Result = 1;
		}
		break;
	case 5:
		if(PT.PlayerSkin < PB.PlayerSkin)
			Result = -1;
		else
		if(PT.PlayerSkin > PB.PlayerSkin)
			Result = 1;
		else
		{
			if(PT.PlayerName < PB.PlayerName)
				Result = -1;
			else
				Result = 1;
		}
		break;
	case 6:
		if(PT.PlayerFace < PB.PlayerFace)
			Result = -1;
		else
		if(PT.PlayerFace > PB.PlayerFace)
			Result = 1;
		else
		{
			if(PT.PlayerSkin < PB.PlayerSkin)
				Result = -1;
			else
			if(PT.PlayerSkin > PB.PlayerSkin)
				Result = 1;
			else
			{
				if(PT.PlayerName < PB.PlayerName)
					Result = -1;
				else
					Result = 1;
			}
		}
		break;
	case 7:
		if(PT.PlayerID < PB.PlayerID)
			Result = -1;
		else
		if(PT.PlayerID > PB.PlayerID)
			Result = 1;
		else
		{
			if(PT.PlayerName < PB.PlayerName)
				Result = -1;
			else
				Result = 1;
		}
		break;
	}

	if(UBrowserPlayerList(Sentinel).bDescending) Result = -Result;

	return Result;
}

function UBrowserPlayerList FindID(int ID)
{
	local UBrowserPlayerList l;

	l = UBrowserPlayerList(Next);
	while(l != None)
	{
		if(l.PlayerID == ID) return l;
		l = UBrowserPlayerList(l.Next);
	}
	return None;
}

defaultproperties
{
	SortColumn=1
	bDescending=False
}