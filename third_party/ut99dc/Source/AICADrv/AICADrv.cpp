#include "AICADrvPrivate.h"
#include "UnRender.h"
#include <kos.h>
#include <string.h>
// Redefine debugf after AudioEngine.h undef'd it
#ifndef debugf
#define debugf GLog->Logf
#endif

/*-----------------------------------------------------------------------------
	Global implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_PACKAGE(AICADrv);
IMPLEMENT_CLASS(UAICAAudioSubsystem);

/*-----------------------------------------------------------------------------
	UAICAAudioSubsystem implementation.
-----------------------------------------------------------------------------*/

void UAICAAudioSubsystem::InternalClassInitializer( UClass* Class )
{
	guardSlow(UAICAAudioSubsystem::InternalClassInitializer);
	new(Class, "OutputRate",         RF_Public)UIntProperty   ( CPP_PROPERTY( OutputRate         ), "Audio", CPF_Config );
	new(Class, "MusicVolume",        RF_Public)UByteProperty  ( CPP_PROPERTY( MusicVolume        ), "Audio", CPF_Config );
	new(Class, "SoundVolume",        RF_Public)UByteProperty  ( CPP_PROPERTY( SoundVolume        ), "Audio", CPF_Config );
	new(Class, "MasterVolume",       RF_Public)UByteProperty  ( CPP_PROPERTY( MasterVolume       ), "Audio", CPF_Config );
	new(Class, "AmbientFactor",      RF_Public)UFloatProperty ( CPP_PROPERTY( AmbientFactor      ), "Audio", CPF_Config );
	new(Class, "DopplerFactor",      RF_Public)UFloatProperty ( CPP_PROPERTY( DopplerFactor      ), "Audio", CPF_Config );
	unguardSlow;
}

UAICAAudioSubsystem::UAICAAudioSubsystem()
{
	OutputRate = DEFAULT_OUTPUT_RATE;
	MasterVolume = 255;
	SoundVolume = 127;
	MusicVolume = 63;
	AmbientFactor = 0.6f;
	DopplerFactor = 0.01f;
	Initialized = false;
	Music = NULL;
	MusicSection = 255;
	MusicIsPlaying = false;
}

UBOOL UAICAAudioSubsystem::Init()
{
	guard(UAICAAudioSubsystem::Init)

	debugf( NAME_Init, "Initializing AICA Audio Subsystem" );

	Viewport = NULL;
	NextId = 0;
	
	if( OutputRate <= 0 )
		OutputRate = DEFAULT_OUTPUT_RATE;
	
	if( DopplerFactor < 0.f )
		DopplerFactor = 0.f;

	AmbientFactor = Clamp( AmbientFactor, 0.f, 1.f );

	// Initialize AudioEngine
	if( !AudioEngine_Initialise() )
	{
		debugf( NAME_Warning, "Could not initialize AICA audio engine" );
		return false;
	}

	for( INT i = 0; i < MAX_SOURCES; ++i )
		Voices[i].StreamId = INVALID_STREAM_ID;

	// Set ourselves up as the audio subsystem.
	USound::Audio = this;
	UMusic::Audio = this;

	Initialized = true;
	debugf( NAME_Init, "AICA audio subsystem initialized: rate=%d, master_vol=%d", 
		OutputRate, MasterVolume );

	return true;

	unguard;
}

void UAICAAudioSubsystem::Destroy()
{
	guard(UAICAAudioSubsystem::Destroy)

	USound::Audio = NULL;
	UMusic::Audio = NULL;

	// Stop all voices
	SetViewport( NULL );

	// Unload all sounds
	// Note: AudioEngine doesn't have a global cleanup, individual streams are cleaned up

	Initialized = false;

	Super::Destroy();

	unguard;
}

void UAICAAudioSubsystem::ShutdownAfterError()
{
	guard(UAICAAudioSubsystem::ShutdownAfterError)

	USound::Audio = NULL;
	UMusic::Audio = NULL;

	Initialized = false;

	Super::ShutdownAfterError();

	unguard;
}

void UAICAAudioSubsystem::PostEditChange()
{
	guard(UAICAAudioSubsystem::PostEditChange)

	Super::PostEditChange();

	if( DopplerFactor < 0.f )
		DopplerFactor = 0.f;
	AmbientFactor = Clamp( AmbientFactor, 0.f, 1.f );

	unguard;
}

void UAICAAudioSubsystem::SetViewport( UViewport* InViewport )
{
	guard(UAICAAudioSubsystem::SetViewport)

	// Stop all sounds before viewport change.
	for( INT i = 0; i < MAX_SOURCES; ++i )
		StopVoice( i );

	// Stop and clear music
	ClearMusicBuffers();

	Viewport = InViewport;

	for( TObjectIterator<USound> It; It; ++It )
			RegisterSound(*It);

	unguard;
}

void UAICAAudioSubsystem::RegisterMusic( UMusic* Music )
{
	guard(UAICAAudioSubsystem::RegisterMusic)

	return;

	unguard;
}

void UAICAAudioSubsystem::UnregisterMusic( UMusic* Music )
{
	guard(UAICAAudioSubsystem::UnregisterMusic)

	if( !Music->Handle )
		return;

	unguard;
}


void UAICAAudioSubsystem::RegisterSound( USound* Sound )
{
	guard(UAICAAudioSubsystem::RegisterSound)


	if( !Sound || !Initialized )
	{
		return;
	}

	if( Sound->Handle )
	{
		return;
	}

	// Set the handle to avoid reentrance.
	Sound->Handle = (void*)-1;

	// Load the data.
	Sound->Data.Load();
	debugf( NAME_Warning, TEXT("Register sound: %s (%i)"), Sound->GetPathName(), Sound->Data.Num() );
	check(Sound->Data.Num()>0);

	FWaveModInfo WaveInfo;
	if( !WaveInfo.ReadWaveInfo( Sound->Data ) )
	{
		debugf( NAME_Warning, "Sound %s is not a valid WAV file", Sound->GetName() );
		Sound->Data.Unload();
		Sound->Handle = NULL;
		return;
	}

	// Calculate total samples to determine if we need streaming
	int total_samples;
	if( *WaveInfo.pBitsPerSample == 4 )
		total_samples = WaveInfo.SampleDataSize * 2;
	else if( *WaveInfo.pBitsPerSample == 8 )
		total_samples = WaveInfo.SampleDataSize;
	else if( *WaveInfo.pBitsPerSample == 16 )
		total_samples = WaveInfo.SampleDataSize / 2;
	else
		total_samples = (int)((float)WaveInfo.SampleDataSize / (((float)*WaveInfo.pBitsPerSample / 8) * (float)*WaveInfo.pChannels));

	// For large sounds, pass full WAV data for in-memory streaming
	const uint8_t* full_wav_data = nullptr;
	uint32_t full_wav_size = 0;
	if( total_samples > 65534 ) // AICA_MAX_SAMPLES
	{
		full_wav_data = &Sound->Data(0);
		full_wav_size = Sound->Data.Num();
	}

	debugf( NAME_DevSound, "AudioEngine_LoadFromWaveInfo params: SampleDataStart=%p, SampleDataSize=%d, SamplesPerSec=%d, Channels=%d, BitsPerSample=%d, Name=%s",
		WaveInfo.SampleDataStart,
		WaveInfo.SampleDataSize,
		*WaveInfo.pSamplesPerSec,
		*WaveInfo.pChannels,
		*WaveInfo.pBitsPerSample,
		Sound->GetName()
	);

	INT StreamId = AudioEngine_LoadFromWaveInfo(
		WaveInfo.SampleDataStart,
		WaveInfo.SampleDataSize,
		*WaveInfo.pSamplesPerSec,
		*WaveInfo.pChannels,
		*WaveInfo.pBitsPerSample,
		Sound->GetName(),
		full_wav_data,
		full_wav_size
	);

	if( StreamId < 0 )
	{
		Sound->Handle = NULL;
		return;
	}

	Sound->Handle = (void*)(DWORD)StreamId;

	// Scrap the source data we no longer need.
	Sound->Data.Unload();

	unguard;
}

void UAICAAudioSubsystem::UnregisterSound( USound* Sound )
{
	guard(UAICAAudioSubsystem::UnregisterSound)

	if( !Sound->Handle )
		return;

	INT StreamId = (INT)(DWORD)Sound->Handle;

	// Stop any voices using this sound
	for( INT i = 0; i < MAX_SOURCES; ++i )
	{
		if( Voices[i].Sound == Sound )
			StopVoice( i );
	}

	// Unload from AudioEngine
	AudioEngine_Unload( StreamId );

	Sound->Handle = NULL;

	unguard;
}

BYTE UAICAAudioSubsystem::LocationToPan( const FVector& Location, FLOAT Radius )
{
	guard(UAICAAudioSubsystem::LocationToPan)

	if( !Viewport || !Viewport->Actor || Radius <= 0.f )
		return AICA_PAN_CENTER;

	FVector ToSound = Location - Viewport->Actor->Location;
	FLOAT Distance = ToSound.Size();
	if( Distance < 1.f )
		return AICA_PAN_CENTER;

	ToSound /= Distance; // Normalize
	
	// Get right vector from view rotation
	FRotator ViewRot = Viewport->Actor->ViewRotation;
	FVector Forward = ViewRot.Vector();
	FVector Up(0,0,1);
	FVector Right = Forward ^ Up;  // Cross product
	Right.Normalize();

	FLOAT Dot = ToSound | Right;  // Dot product
	
	// Convert -1..1 to 0..255
	// Left = 0, Center = 128, Right = 255
	INT Pan = (INT)(128.f + Dot * 127.f);
	return (BYTE)Clamp( Pan, (INT)0, (INT)255 );

	unguard;
}

BYTE UAICAAudioSubsystem::CalculateVolume( FLOAT BaseVolume, const FVector& Location, FLOAT Radius )
{
	guard(UAICAAudioSubsystem::CalculateVolume)

	FLOAT Attenuation = 1.f;
	
	if( Viewport && Viewport->Actor && Radius > 0.f )
	{
		FLOAT Distance = (Location - Viewport->Actor->Location).Size();
		Attenuation = 1.f - (Distance / Radius);
		Attenuation = Clamp( Attenuation, 0.f, 1.f );
	}

	// Always apply MasterVolume and SoundVolume
	FLOAT FinalVolume = BaseVolume * Attenuation * (MasterVolume / 255.f) * (SoundVolume / 255.f);
	return (BYTE)Clamp( FinalVolume * 255.f, 0.f, 255.f );

	unguard;
}

void UAICAAudioSubsystem::UpdateVoice( INT Num )
{
	guard(UAICAAudioSubsystem::UpdateVoice)

	FNVoice& Voice = Voices[Num];
	if( Voice.StreamId == INVALID_STREAM_ID )
		return;

	// Update volume and pan based on 3D position
	BYTE Volume = CalculateVolume( Voice.Volume, Voice.Location, Voice.Radius );
	BYTE Pan = LocationToPan( Voice.Location, Voice.Radius );

	// Get stream info to update
	if( Voice.StreamId < AUDIO_ENGINE_MAX_STREAMS )
	{
		// It's a stream - AudioEngine handles updates internally
		// We could check if it's still playing here if needed
		struct stream_info* StreamInfo = AudioEngine_getStreamInfo( Voice.StreamId );
		if( StreamInfo && !StreamInfo->playing )
		{
			// Stream finished, stop the voice
			StopVoice( Num );
		}
	}
	else
	{
		// It's an SFX - try to update volume/pan if we can find the channel
		int aica_channel = AudioEngine_GetSfxChannel( Voice.StreamId );
		if( aica_channel >= 0 )
		{
			// Update volume and pan on the AICA channel
			aica_volpan_chn( aica_channel, Volume, Pan );
		}
	}

	unguard;
}

void UAICAAudioSubsystem::StopVoice( INT Num )
{
	guard(UAICAAudioSubsystem::StopVoice)

	FNVoice& Voice = Voices[Num];
	if( Voice.StreamId == INVALID_STREAM_ID )
		return;

	AudioEngine_Stop( Voice.StreamId );

	Voice.StreamId = INVALID_STREAM_ID;
	Voice.Id = 0;
	Voice.Sound = NULL;
	Voice.Actor = NULL;

	unguard;
}

UBOOL UAICAAudioSubsystem::PlaySound( AActor* Actor, INT Id, USound* Sound, FVector Location, FLOAT Volume, FLOAT Radius, FLOAT Pitch )
{
	guard(UAICAAudioSubsystem::PlaySound)

	if( !Viewport || !Initialized )
		return false;

	// Allocate a new slot if requested.
	if( SOUND_SLOT_IS( Id, SLOT_None ) )
		Id = 16 * --NextId;

	FLOAT Priority = GetVoicePriority( Location, Volume, Radius );
	FLOAT MaxPriority = Priority;
	FNVoice* Voice = NULL;
	for( INT i = 0; i < MAX_SOURCES; ++i )
	{
		FNVoice* V = &Voices[i];
		if( ( V->Id & ~1 ) == ( Id & ~1 ) )
		{
			// Skip if not interruptable.
			if( Id & 1 )
				return false;
			StopVoice( i );
			Voice = V;
			break;
		}
		else if( V->Priority <= MaxPriority )
		{
			MaxPriority = V->Priority;
			Voice = V;
		}
	}

	// If we ran out of voices or the sound is too low priority, bail.
	if( !Voice || !Sound || !Sound->Handle )
		return false;

	INT StreamId = (INT)(DWORD)Sound->Handle;

	Voice->Id = Id;
	Voice->StreamId = StreamId;
	Voice->Location = Location;
	Voice->Velocity = Actor ? Actor->Velocity : FVector();
	Voice->Volume = Clamp( Volume, 0.f, 1.f );
	Voice->Radius = Radius;
	Voice->Pitch = Clamp( Pitch, 0.25f, 4.f ); // AudioEngine pitch limits
	Voice->Priority = Priority;
	Voice->Actor = Actor;
	Voice->Looping = SOUND_SLOT_IS( Id, SLOT_Ambient ); // Ambient sounds loop, others don't
	Voice->Sound = Sound;

	// Calculate initial volume and pan
	BYTE VolumeByte = CalculateVolume( Voice->Volume, Voice->Location, Voice->Radius );
	BYTE Pan = LocationToPan( Voice->Location, Voice->Radius );

	// Play the sound
	// For mono sounds, AICA uses a single pan value: 0=left, 128=center, 255=right
	// Pass the same pan value for both left and right (AudioEngine will average it for mono)
	AudioEngine_Play( StreamId, VolumeByte, Pan, Pan, Voice->Looping, 0 );

	return true;

	unguard;
}

void UAICAAudioSubsystem::NoteDestroy( AActor* Actor )
{
	guard(UAICAAudioSubsystem::NoteDestroy)

	check(Actor);
	check(Actor->IsValid());
	for( INT i = 0; i < MAX_SOURCES; ++i)
	{
		if( Voices[i].Actor == Actor )
		{
			if( SOUND_SLOT_IS( Voices[i].Id, SLOT_Ambient ) )
			{
				// Stop ambient sound when actor dies.
				StopVoice( i );
			}
			else
			{
				// Unbind regular sounds from actors.
				Voices[i].Actor = NULL;
			}
		}
	}

	unguard;
}

void UAICAAudioSubsystem::Update( FPointRegion Region, FCoords& Listener )
{
	guard(UAICAAudioSubsystem::Update)

	if( !Viewport || !Viewport->IsRealtime() || !Initialized )
		return;

	// Start new ambient sounds if needed.
	if( Viewport->Actor && Viewport->Actor->XLevel )
	{
		for( INT i = 0; i < Viewport->Actor->XLevel->NumPV; i++ )
		{
			AActor* Actor = Viewport->Actor->XLevel->Actors(i);
			if( !Actor || !Actor->IsValid() )
				continue;

			const FLOAT DistSq = FDistSquared( Viewport->Actor->Location, Actor->Location );
			const FLOAT AmbRad = Square( Actor->WorldSoundRadius() );
			if( !Actor->AmbientSound || DistSq > AmbRad )
				continue;

			// See if it's already playing.
			INT Id = AMBIENT_SOUND_ID( Actor->GetIndex() );
			INT AmbientNum;
			for( AmbientNum = 0; AmbientNum < MAX_SOURCES; ++AmbientNum )
			{
				if( Voices[AmbientNum].Id == Id )
					break;
			}

			// If not, start it.
			if( AmbientNum == MAX_SOURCES )
			{
				FLOAT Vol = AmbientFactor * Actor->SoundVolume / 255.f;
				FLOAT Rad = Actor->WorldSoundRadius();
				FLOAT Pitch = Actor->SoundPitch / 64.f;
				PlaySound( Actor, Id, Actor->AmbientSound, Actor->Location, Vol, Rad, Pitch );
			}
		}
	}

	// Update active ambient sounds.
	for( INT VoiceNum = 0; VoiceNum < MAX_SOURCES; ++VoiceNum )
	{
		FNVoice& Voice = Voices[VoiceNum];
		if( !Voice.Id || !Voice.Sound || Voice.StreamId == INVALID_STREAM_ID || !SOUND_SLOT_IS( Voice.Id, SLOT_Ambient ) )
			continue;

		check( Voice.Actor );

		const FLOAT DistSq = FDistSquared( Viewport->Actor->Location, Voice.Actor->Location );
		const FLOAT AmbRad = Square( Voice.Actor->WorldSoundRadius() );
		if( Voice.Sound != Voice.Actor->AmbientSound || DistSq > AmbRad )
		{
			// Sound changed or went out of range.
			StopVoice( VoiceNum );
		}
		else
		{
			// Update parameters.
			Voice.Radius = Voice.Actor->WorldSoundRadius();
			Voice.Pitch = Voice.Actor->SoundPitch / 64.f;
			Voice.Volume = AmbientFactor * Voice.Actor->SoundVolume / 255.f;
			if( Voice.Actor->LightType != LT_None )
				Voice.Volume *= Voice.Actor->LightBrightness / 255.f;
		}
	}

	// Update all active voices.
	for( INT VoiceNum = 0; VoiceNum < MAX_SOURCES; ++VoiceNum )
	{
		FNVoice& Voice = Voices[VoiceNum];
		if( !Voice.Id || !Voice.Sound || Voice.StreamId == INVALID_STREAM_ID )
			continue;

		// Update location if attached to actor
		if( Voice.Actor && Voice.Actor->IsValid() )
		{
			Voice.Location = Voice.Actor->Location;
			Voice.Velocity = Voice.Actor->Velocity;
		}
		Voice.Priority = GetVoicePriority( Voice.Location, Voice.Volume, Voice.Radius );
		
		// Update voice parameters
		UpdateVoice( VoiceNum );
	}

	// Handle music playback
	if( Viewport && Viewport->Actor )
	{
		UBOOL MusicChanged = ( Music != Viewport->Actor->Song );
		
		if( MusicChanged )
		{
			if( Music )
				StopMusic();
			Music = Viewport->Actor->Song;
			MusicSection = Viewport->Actor->SongSection;
			if( Music )
			{
				// RegisterMusic will set Handle if registration succeeds
				// It returns early if Handle is already set (already registered)
				RegisterMusic( Music );
			}
		}
		else if( Music )
		{
			MusicSection = Viewport->Actor->SongSection;
		}

		if( Music && Music->Handle )
		{
			if( MusicSection != 255 )
			{
				if( !MusicIsPlaying )
				{
					debugf( NAME_Log, "Calling PlayMusic for %s, section=%d, handle=%p", 
						Music->GetName(), MusicSection, Music->Handle );
					PlayMusic();
				}
			}
			else
			{
				if( MusicIsPlaying )
				{
					debugf( NAME_Log, "Stopping music %s (section=255)", Music->GetName() );
					StopMusic();
				}
			}
		}
		else
		{
			if( MusicIsPlaying )
				StopMusic();
			if( Music && !Music->Handle )
			{
				return;
			}
		}

		// Update music buffers (check if still playing)
		UpdateMusicBuffers();
	}

	unguard;
}

UBOOL UAICAAudioSubsystem::Exec( const char* Cmd, FOutputDevice& Out )
{
	guard(UAICAAudioSubsystem::Exec)

	// No special commands for now
	return false;

	unguard;
}

void UAICAAudioSubsystem::PlayMusic()
{
	guard(UAICAAudioSubsystem::PlayMusic)

	if( !Music || !Music->Handle )
		return;

	unguard;
}

void UAICAAudioSubsystem::StopMusic()
{
	guard(UAICAAudioSubsystem::StopMusic)

	if( !MusicIsPlaying || !Music || !Music->Handle )
		return;


	unguard;
}

void UAICAAudioSubsystem::UpdateMusicBuffers()
{
	guard(UAICAAudioSubsystem::UpdateMusicBuffers)

	if( !Music || !Music->Handle )
		return;


	unguard;
}

void UAICAAudioSubsystem::ClearMusicBuffers()
{
	guard(UAICAAudioSubsystem::ClearMusicBuffers)

	if( MusicIsPlaying )
		StopMusic();

	// Clear music state
	Music = NULL;
	MusicSection = 255;
	MusicIsPlaying = false;

	unguard;
}

//
// GetViewport
//
UViewport* UAICAAudioSubsystem::GetViewport()
{
	guard(UAICAAudioSubsystem::GetViewport);
	return Viewport;
	unguard;
}

//
// RenderAudioGeometry
//
void UAICAAudioSubsystem::RenderAudioGeometry( FSceneNode* Frame )
{
	guard(UAICAAudioSubsystem::RenderAudioGeometry);
	// AICA doesn't use audio geometry
	unguard;
}

//
// PostRender
//
void UAICAAudioSubsystem::PostRender( FSceneNode* Frame )
{
	guard(UAICAAudioSubsystem::PostRender);
	// AICA doesn't need post-render audio processing
	unguard;
}

