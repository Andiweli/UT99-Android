//=============================================================================
// DemoRecSpectator - spectator for demo recordings to replicate ClientMessages
//=============================================================================

class DemoRecSpectator extends MessagingSpectator;

var PlayerPawn PlaybackActor;
var GameReplicationInfo PlaybackGRI;

function ClientMessage( coerce string S, optional name Type, optional bool bBeep )
{
	RepClientMessage( S, Type, bBeep );
}

function TeamMessage( PlayerReplicationInfo PRI, coerce string S, name Type, optional bool bBeep )
{
	RepTeamMessage( PRI, S, Type );
}

function ClientVoiceMessage(PlayerReplicationInfo Sender, PlayerReplicationInfo Recipient, name messagetype, byte messageID)
{
	RepClientVoiceMessage(Sender, Recipient, messagetype, messageID);
}

function ReceiveLocalizedMessage( class<LocalMessage> Message, optional int Switch, optional PlayerReplicationInfo RelatedPRI_1, optional PlayerReplicationInfo RelatedPRI_2, optional Object OptionalObject )
{
	RepReceiveLocalizedMessage( Message, Switch, RelatedPRI_1, RelatedPRI_2, OptionalObject );
}

//==== Called during demo playback ============================================

simulated function Tick(float Delta)
{
	local PlayerPawn p;
	local GameReplicationInfo g;

	// find local playerpawn and attach.
	if(Level.NetMode == NM_Client)
	{
		if(PlaybackActor == None)
		{
			foreach AllActors(class'PlayerPawn', p)
			{
				if( p.Player.IsA('Viewport') )
				{
					PlaybackActor = p;
					if(PlaybackGRI != None)
						PlaybackActor.GameReplicationInfo = PlaybackGRI;

					Log("Attached to player "$p);
					
					break;
				}
			}
		}

		if(PlaybackGRI == None)
		{
			foreach AllActors(class'GameReplicationInfo', g)
			{
				PlaybackGRI = g;
				if(PlaybackActor != None)
					PlaybackActor.GameReplicationInfo = PlaybackGRI;
				break;
			}
		}

		if(PlaybackActor != None && PlaybackGRI != None)
			Disable('Tick');

	}
	else
	{
		Disable('Tick');
	}
}

simulated function RepClientMessage( coerce string S, optional name Type, optional bool bBeep )
{	
	if(PlaybackActor != None && PlaybackActor.Role == ROLE_Authority)
		PlaybackActor.ClientMessage( S, Type, bBeep );
}

simulated function RepTeamMessage( PlayerReplicationInfo PRI, coerce string S, name Type )
{
	if(PlaybackActor != None && PlaybackActor.Role == ROLE_Authority)
		PlaybackActor.TeamMessage( PRI, S, Type );
}

simulated function RepClientVoiceMessage(PlayerReplicationInfo Sender, PlayerReplicationInfo Recipient, name messagetype, byte messageID)
{
	if(PlaybackActor != None && PlaybackActor.Role == ROLE_Authority)
		PlaybackActor.ClientVoiceMessage(Sender, Recipient, messagetype, messageID);
}

simulated function RepReceiveLocalizedMessage( class<LocalMessage> Message, optional int Switch, optional PlayerReplicationInfo RelatedPRI_1, optional PlayerReplicationInfo RelatedPRI_2, optional Object OptionalObject )
{
	if(PlaybackActor != None && PlaybackActor.Role == ROLE_Authority)
		PlaybackActor.ReceiveLocalizedMessage( Message, Switch, RelatedPRI_1, RelatedPRI_2, OptionalObject );
}

replication
{
	reliable if ( bDemoRecording )
		RepClientMessage, RepTeamMessage, RepClientVoiceMessage, RepReceiveLocalizedMessage;
}
