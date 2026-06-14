class UBrowserScreenshotCW expands UWindowClientWindow;

var Texture Screenshot;
var string MapName;

function Paint(Canvas C, float MouseX, float MouseY)
{
	local float X, Y, W, H;
	local int i;
	local string M;
	local UBrowserServerList L;

	L = UBrowserInfoClientWindow(GetParent(class'UBrowserInfoClientWindow')).Server;
	
	if( L != None )
	{
		M = L.MapName;
		if( M != MapName )
		{
			MapName = M;
			if( MapName == "" )
				ScreenShot = None;
			else
			{
				i = InStr(Caps(MapName), ".UNR");
				if(i != -1)
					MapName = Left(MapName, i);

				Screenshot = Texture(DynamicLoadObject(MapName$".Screenshot", class'Texture'));
			}
		}
	}
	else
	{
		ScreenShot = None;
		MapName = "";
	}

	DrawStretchedTexture(C, 0, 0, WinWidth, WinHeight, Texture'BlackTexture');

	if(Screenshot != None)
	{
		W = Min(WinWidth, Screenshot.USize);
		H = Min(WinHeight, Screenshot.VSize);
		
		if(W > H)
			W = H;
		if(H > W)
			H = W;

		X = (WinWidth - W) / 2;
		Y = (WinHeight - H) / 2;
			
		DrawStretchedTexture(C, X, Y, W, H, Screenshot);	
	}	
}