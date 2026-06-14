/*------------------------------------------------------------------------------------
	Dependencies.
------------------------------------------------------------------------------------*/
#include "Engine.h"
#include "AudioEngine.h"
// AudioEngine.h defines debugf as printf, undef it to use Unreal Engine's debugf
#undef debugf
#undef verbosef
#include "AicaInterface.h"
/*------------------------------------------------------------------------------------
	AICA audio subsystem private definitions.
------------------------------------------------------------------------------------*/

#define MAX_SOURCES 64

#define INVALID_STREAM_ID -1

#define SOUND_SLOT_IS( Id, Slot ) ( ( (Id) & 14 ) == (Slot) * 2 )
#define AMBIENT_SOUND_ID( ActorIndex ) ( (ActorIndex) * 16 + SLOT_Ambient * 2 )

#define DEFAULT_OUTPUT_RATE 11050

// World scale related constants
#define DISTANCE_SCALE 0.023255814f
#define ROLLOFF_FACTOR 1.1f

class DLL_EXPORT UAICAAudioSubsystem : public UAudioSubsystem
{
	DECLARE_CLASS(UAICAAudioSubsystem, UAudioSubsystem, CLASS_Config)

	// Options
	INT OutputRate;
	BYTE MasterVolume;
	BYTE SoundVolume;
	BYTE MusicVolume;
	FLOAT AmbientFactor;
	FLOAT DopplerFactor;

	// Constructors.
	static void InternalClassInitializer( UClass* Class );
	UAICAAudioSubsystem();

	// UObject interface.
	virtual void Destroy();
	virtual void PostEditChange();
	virtual void ShutdownAfterError();

	// UAudioSubsystem interface.
	virtual UBOOL Init();
	virtual void SetViewport( UViewport* Viewport );
	virtual UBOOL Exec( const char* Cmd, FOutputDevice& Out=*GLog );
	virtual void Update( FPointRegion Region, FCoords& Listener );
	virtual void RegisterMusic( UMusic* Music );
	virtual void RegisterSound( USound* Sound );
	virtual void UnregisterSound( USound* Sound );
	virtual void UnregisterMusic( UMusic* Music );
	virtual UBOOL PlaySound( AActor* Actor, INT Id, USound* Sound, FVector Location, FLOAT Volume, FLOAT Radius, FLOAT Pitch );
	virtual void NoteDestroy( AActor* Actor );
	virtual UBOOL GetLowQualitySetting() { return true; };
	virtual UViewport* GetViewport();
	virtual void RenderAudioGeometry( FSceneNode* Frame );
	virtual void PostRender( FSceneNode* Frame );

	// Internals.
private:
	UViewport* Viewport;
	INT NextId;
	UBOOL Initialized;

	// Music state
	UMusic* Music;
	BYTE MusicSection;
	UBOOL MusicIsPlaying;

	struct FNVoice
	{
		INT StreamId = INVALID_STREAM_ID;
		AActor* Actor;
		INT Id;
		USound* Sound;
		FVector Location;
		FVector Velocity;
		FLOAT Volume;
		FLOAT Radius;
		FLOAT Pitch;
		FLOAT Priority;
		UBOOL Looping;
	} Voices[MAX_SOURCES];

	void UpdateVoice( INT Num );
	void StopVoice( INT Num );
	void PlayMusic();
	void StopMusic();
	void UpdateMusicBuffers();
	void ClearMusicBuffers();

	inline FLOAT GetVoicePriority( const FVector& Location, FLOAT Volume, FLOAT Radius )
	{
		if( Radius && Viewport->Actor )
			return Volume * ( 1.f - (Location - Viewport->Actor->Location).Size() / Radius );
		else
			return Volume;
	}

	// Convert 3D position to pan (0-255, 128 = center)
	BYTE LocationToPan( const FVector& Location, FLOAT Radius );
	
	// Calculate volume with distance attenuation
	BYTE CalculateVolume( FLOAT BaseVolume, const FVector& Location, FLOAT Radius );
};

