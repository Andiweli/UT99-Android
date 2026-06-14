//=============================================================================
// MessagingSpectator - spectator base class for game helper spectators which receive messages
//=============================================================================

class MessagingSpectator expands Spectator
	abstract;

function PostBeginPlay()
{
	Super.PostBeginPlay();
	bIsPlayer = False;
}
