class WebApplication expands Object;
	
// Set by the webserver
var LevelInfo Level;
var WebServer WebServer;
var string Path;

function Init();
function Cleanup();
function Query(WebRequest Request, WebResponse Response);
