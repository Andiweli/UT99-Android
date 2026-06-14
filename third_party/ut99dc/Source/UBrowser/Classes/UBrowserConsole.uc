class UBrowserConsole expands WindowConsole;

event bool KeyEvent( EInputKey Key, EInputAction Action, FLOAT Delta )
{
	return Super(Console).KeyEvent( Key, Action, Delta );
}

exec function ShowUBrowser()
{
	Super.LaunchUWindow();
}
