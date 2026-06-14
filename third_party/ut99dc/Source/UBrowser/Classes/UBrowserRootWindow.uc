//=============================================================================
// UBrowserRootWindow - root window subclass for UnrealBrowser
//=============================================================================
class UBrowserRootWindow extends UWindowRootWindow;

var	UBrowserMainWindow MainWindow;


function Created()
{
	Super.Created();

	MainWindow = UBrowserMainWindow(CreateWindow(class'UBrowserMainWindow', 50, 30, 500, 300));
	MainWindow.bStandaloneBrowser = True;
	MainWindow.WindowTitle = "Unreal Browser";
	Resized();
}


function Resized()
{
	Super.Resized();
	
	MainWindow.SetSize(Min(500, WinWidth - 10), WinHeight-30);

	MainWindow.WinLeft = Int((WinWidth - MainWindow.WinWidth) / 2);
	MainWindow.WinTop = Int((WinHeight - MainWindow.WinHeight) / 2);
}

defaultproperties
{
	LookAndFeelClass="UWindow.UWindowWin95LookAndFeel"
}