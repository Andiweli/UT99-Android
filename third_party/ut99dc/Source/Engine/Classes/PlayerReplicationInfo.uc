//=============================================================================
// PlayerReplicationInfo.
//=============================================================================
class PlayerReplicationInfo expands ReplicationInfo
	native nativereplication;

var string				PlayerName;		// Player name, or blank if none.
var string				OldName;		// Temporary value.
var int					PlayerID;		// Unique id number.
var string				TeamName;		// Team name, or blank if none.
var byte				Team;			// Player Team, 255 = None for player.
var int					TeamID;			// Player position in team.
var float				Score;			// Player's current score.
var float				Deaths;			// Number of player's deaths.
var class<VoicePack>	VoiceType;
var Decoration			HasFlag;
var int					Ping;
var byte				PacketLoss;
var bool				bIsFemale;
var	bool				bIsABot;
var bool				bFeigningDeath;
var bool				bIsSpectator;
var bool				bWaitingPlayer;
var bool				bAdmin;
var Texture				TalkTexture;
var ZoneInfo			PlayerZone;
var LocationID			PlayerLocation;

// Time elapsed.
var int					StartTime;
var int					TimeAcc;

replication
{
	// Things the server should send to the client.
	reliable if ( Role == ROLE_Authority )
		PlayerName, OldName, PlayerID, TeamName, Team, TeamID, Score, Deaths, VoiceType,
		HasFlag, Ping, PacketLoss, bIsFemale, bIsABot, bFeigningDeath, bIsSpectator, bWaitingPlayer,
		bAdmin, TalkTexture, PlayerZone, PlayerLocation, StartTime;
}

function PostBeginPlay()
{
	StartTime = Level.TimeSeconds;
	Timer();
	SetTimer(2.0, true);
	bIsFemale = Pawn(Owner).bIsFemale;
}
 					
function Timer()
{
	local float MinDist, Dist;
	local LocationID L;

	MinDist = 1000000;
	PlayerLocation = None;
	if ( PlayerZone != None )
		for ( L=PlayerZone.LocationID; L!=None; L=L.NextLocation )
		{
			Dist = VSize(Owner.Location - L.Location);
			if ( (Dist < L.Radius) && (Dist < MinDist) )
			{
				PlayerLocation = L;
				MinDist = Dist;
			}
		}
	if ( FRand() < 0.65 )
		return;

	if (PlayerPawn(Owner) != None)
		Ping = int(PlayerPawn(Owner).ConsoleCommand("GETPING"));

	if (PlayerPawn(Owner) != None)
		PacketLoss = int(PlayerPawn(Owner).ConsoleCommand("GETLOSS"));
}

defaultproperties
{
	bAlwaysRelevant=True
	team=255
    NetUpdateFrequency=5
}
