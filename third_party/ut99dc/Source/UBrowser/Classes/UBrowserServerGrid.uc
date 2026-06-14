//=============================================================================
// UBrowserServerGrid - base class for server listings
//=============================================================================
class UBrowserServerGrid extends UWindowGrid;

#exec TEXTURE IMPORT NAME=Highlight FILE=Textures\Highlight.bmp GROUP="Icons" FLAGS=2 MIPS=OFF

var UBrowserRightClickMenu Menu;
var UWindowGridColumn Server, Ping, MapName, Players, SortByColumn;
var bool bSortDescending;
var localized string ServerName, PingName, MapNameName, PlayersName;

var UBrowserServerList SelectedServer;
var int Count;

var float TimePassed;
var int AutoPingInterval;
var UBrowserServerList OldPingServer;

function Created()
{
	Super.Created();

	RowHeight = 12;

	CreateColumns();

	Menu = UBrowserRightClickMenu(Root.CreateWindow(UBrowserServerListWindow(GetParent(class'UBrowserServerListWindow')).RightClickMenuClass, 0, 0, 100, 100, Self));
	Menu.HideWindow();
}

function Close(optional bool bByParent)
{
	Super.Close(bByParent);
	if(Menu != None && Menu.bWindowVisible)
		Menu.CloseUp();
}

function CreateColumns()
{
	Server	= AddColumn(ServerName, 300);
	Ping	= AddColumn(PingName, 30);
	MapName	= AddColumn(MapNameName, 100);
	Players	= AddColumn(PlayersName, 50);

	SortByColumn = Ping;
}

function DrawCell(Canvas C, float X, float Y, UWindowGridColumn Column, UBrowserServerList List)
{
	switch(Column)
	{
	case Server:
		Column.ClipText( C, X, Y, List.HostName );
		break;
	case Ping:
		Column.ClipText( C, X, Y, Int(List.Ping) );
		break;
	case MapName:
		Column.ClipText( C, X, Y, List.MapDisplayName );
		break;
	case Players:
		Column.ClipText( C, X, Y, List.NumPlayers$"/"$List.MaxPlayers );
		break;
	}
}

function PaintColumn(Canvas C, UWindowGridColumn Column, float MouseX, float MouseY) 
{
	local UBrowserServerList List;
	local float Y;
	local int Visible;
	local int Skipped;
	local int TopMargin;
	local int BottomMargin;

	C.Font = Root.Fonts[F_Normal];


	List = UBrowserServerListWindow(GetParent(class'UBrowserServerListWindow')).PingedList;

	if(List == None)
		Count = 0;
	else
		Count = List.Count();

	if(bShowHorizSB)
		BottomMargin = LookAndFeel.Size_ScrollbarWidth;
	else
		BottomMargin = 0;

	TopMargin = LookAndFeel.ColumnHeadingHeight;

	Visible = int((WinHeight - (TopMargin + BottomMargin))/RowHeight);
	
	VertSB.SetRange(0, Count+1, Visible);
	TopRow = VertSB.Pos;

	Skipped = 0;

	List = UBrowserServerList(List.Next);

	if(List != None) 
	{
		Y = 1;

		while((Y < RowHeight + WinHeight - RowHeight - (TopMargin + BottomMargin)) && (List != None))
		{
			// FIXME: make more efficient - cache top server in list if TopRow doesn't change
			if(Skipped >= VertSB.Pos)
			{
				// Draw highlight
				if(List == SelectedServer)
					Column.DrawStretchedTexture( C, 0, Y-1 + TopMargin, Column.WinWidth, RowHeight + 1, Texture'Highlight');

				DrawCell(C, 2, Y + TopMargin, Column, List);
				Y = Y + RowHeight;			
			} 
			Skipped ++;

			List = UBrowserServerList(List.Next);
		}
	}
}

function SortColumn(UWindowGridColumn Column)
{
	if(SortByColumn == Column)
		bSortDescending = !bSortDescending;
	else
		bSortDescending = False;

	SortByColumn = Column;

	UBrowserServerListWindow(GetParent(class'UBrowserServerListWindow')).PingedList.Sort();	
}

function Tick(float DeltaTime)
{
	local UBrowserServerListWindow W;

	W = UBrowserServerListWindow(GetParent(class'UBrowserServerListWindow'));

	if(W.PingState == PS_Done && SelectedServer == None)
	{
		SelectedServer = UBrowserServerList(W.PingedList.Next);
		if(SelectedServer == None || SelectedServer.bPinging)
			SelectedServer = None;
		else
			UBrowserServerListWindow(GetParent(class'UBrowserServerListWindow')).AutoInfo(SelectedServer);
	}

	if(W.PingState == PS_Done)
	{
		if(TimePassed >= AutoPingInterval)
		{
			TimePassed = 0;

			if(SelectedServer != OldPingServer)
			{
				if(OldPingServer != None)
					OldPingServer.CancelPing();
				OldPingServer = SelectedServer;
			}

			if(SelectedServer != None && !SelectedServer.bPinging)
				SelectedServer.PingServer(False, True, True);
		}
		TimePassed = TimePassed + DeltaTime;
	}
}

function SelectRow(int Row)
{
	local UBrowserServerList S;

	S = GetServerUnderRow(Row);

	if(SelectedServer != S)
	{
		if(S != None)
			UBrowserServerListWindow(GetParent(class'UBrowserServerListWindow')).AutoInfo(S);
		TimePassed = 0;
	}

	if(S != None)
		SelectedServer = S;
}

function RightClickRow(int Row, float X, float Y)
{
	local float MenuX, MenuY;

	WindowToGlobal(X, Y, MenuX, MenuY);
	Menu.WinLeft = MenuX;
	Menu.WinTop = MenuY;
	Menu.List = GetServerUnderRow(Row);
	Menu.Grid = Self;
	Menu.ShowWindow();
}

function UBrowserServerList GetServerUnderRow(int Row)
{
	local int i;
	local UBrowserServerList List;

	List = UBrowserServerListWindow(GetParent(class'UBrowserServerListWindow')).PingedList;
	if(List != None)
	{
		i = 0;
		List = UBrowserServerList(List.Next);
		while(List != None)
		{
			if(i == Row)
				return List;

			List = UBrowserServerList(List.Next);
			i++;
		}
	}
	return None;
}

function int GetSelectedRow()
{
	local int i;
	local UBrowserServerList List;

	List = UBrowserServerListWindow(GetParent(class'UBrowserServerListWindow')).PingedList;
	if(List != None)
	{
		i = 0;
		List = UBrowserServerList(List.Next);
		while(List != None)
		{
			if(List == SelectedServer)
				return i;

			List = UBrowserServerList(List.Next);
			i++;
		}
	}
	return -1;
}

function JoinServer(UBrowserServerList Server)
{
	if(Server != None && Server.GamePort != 0) 
	{
		GetPlayerOwner().ClientTravel("unreal://"$Server.IP$":"$Server.GamePort$UBrowserServerListWindow(GetParent(class'UBrowserServerListWindow')).URLAppend, TRAVEL_Absolute, false);
		GetParent(class'UWindowFramedWindow').Close();
		Root.Console.CloseUWindow();
	}
}

function DoubleClickRow(int Row)
{
	local UBrowserServerList Server;

	Server = GetServerUnderRow(Row);
	if(SelectedServer != Server) return; 
	JoinServer(Server);
}

function MouseLeaveColumn(UWindowGridColumn Column)
{
	ToolTip("");
}

function KeyDown(int Key, float X, float Y) 
{
	switch(Key)
	{
	case 0x74: // IK_F5;
		Refresh();
		break;
	case 0x26: // IK_Up
		SelectRow(Clamp(GetSelectedRow() - 1, 0, Count - 1));
		VertSB.Show(GetSelectedRow());
		break;
	case 0x28: // IK_Down
		SelectRow(Clamp(GetSelectedRow() + 1, 0, Count - 1));
		VertSB.Show(GetSelectedRow());
		break;
	case 0x0D: // IK_Enter:
		DoubleClickRow(GetSelectedRow());
		break;
	default:
		Super.KeyDown(Key, X, Y);
		break;
	}
}

function int Compare(UBrowserServerList T, UBrowserServerList B)
{
	switch(SortByColumn)
	{
	case Server:
		return ByName(T, B);
	case Ping:
		return ByPing(T, B);
	case MapName:
		return ByMap(T, B);
	case Players:
		return ByPlayers(T, B);
	default:
		return 0;
	}	
}

function int ByPing(UBrowserServerList T, UBrowserServerList B)
{
	local int Result;

	if(B == None) return -1;
	
	if(T.Ping < B.Ping)
	{
		Result = -1;
	}
	else
	if (T.Ping > B.Ping)
	{
		Result = 1;
	}
	else
	{
/*		if(T.HostName < B.HostName)
			Result = -1;
		else
		if(T.HostName > B.HostName)
			Result = 1;
		else
*/
			Result = 0;
	}

	if(bSortDescending)
		Result = -Result;

	return Result;
}

function int ByName(UBrowserServerList T, UBrowserServerList B)
{
	local int Result;

	if(B == None) return -1;
	if(T.Ping == 9999) return 1;
	if(B.Ping == 9999) return -1;
	
	if(T.HostName < B.HostName)
	{
		Result = -1;
	}
	else
	if (T.HostName > B.HostName)
	{
		Result = 1;
	}
	else
	{
		Result = 0;//T.Ping - B.Ping;
	}

	if(bSortDescending)
		Result = -Result;

	return Result;
}

function int ByMap(UBrowserServerList T, UBrowserServerList B)
{
	local int Result;

	if(B == None) return -1;
	
	if(T.Ping == 9999) return 1;
	if(B.Ping == 9999) return -1;

	if(T.MapDisplayName < B.MapDisplayName)
	{
		Result = -1;
	}
	else 
	if (T.MapDisplayName > B.MapDisplayName)
	{
		Result = 1;
	}
	else
	{
		Result = T.Ping - B.Ping;
	}

	if(bSortDescending)
		Result = -Result;
	
	return Result;
}

function int ByPlayers(UBrowserServerList T, UBrowserServerList B)
{
	local int Result;

	if(B == None) return -1;
	
	if(T.Ping == 9999) return 1;
	if(B.Ping == 9999) return -1;

	if(T.NumPlayers > B.NumPlayers)
	{
		Result = -1;
	}
	else
	if (T.NumPlayers < B.NumPlayers)
	{
		Result = 1;
	}
	else
	{
		if (T.MaxPlayers > B.MaxPlayers)
		{
			Result = -1;
		}
		else
		if(T.MaxPlayers < B.MaxPlayers)
		{
			Result = 1;
		}
		else
		{
			Result = T.Ping - B.Ping;
		}
	}

	if(bSortDescending)
		Result = -Result;

	return Result;
}

function ShowInfo(UBrowserServerList List)
{
	UBrowserServerListWindow(GetParent(class'UBrowserServerListWindow')).ShowInfo(List);
}

function Refresh()
{
	UBrowserServerListWindow(GetParent(class'UBrowserServerListWindow')).Refresh();
}

function RefreshServer()
{
	TimePassed = AutoPingInterval;
}

function RePing()
{
	UBrowserServerListWindow(GetParent(class'UBrowserServerListWindow')).RePing();
}

defaultproperties
{
	ServerName="Server"
	PingName="Ping"
	MapNameName="Map Name"
	PlayersName="Players"
	AutoPingInterval=5
}