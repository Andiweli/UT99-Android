/*=============================================================================
	AudioLibrary.h: Unreal general audio management functions.
	Copyright 1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Brandon Reinhart.
=============================================================================*/

/*------------------------------------------------------------------------------------
	Helpers
------------------------------------------------------------------------------------*/

#define CheckAudioLib(f)	if( !AudioInitialized ) return f;

#define ALock				AudioLock(&Mutex);
#define AUnlock				AudioUnlock(&Mutex);

/*------------------------------------------------------------------------------------
	Constants
------------------------------------------------------------------------------------*/

#define AUDIO_TOTALCHANNELS	32
#define AUDIO_TOTALVOICES	256

#define AUDIO_MINPAN		0
#define AUDIO_MIDPAN		16384
#define AUDIO_MAXPAN		32767
#define AUDIO_SURPAN		32768

#define AUDIO_MINVOLUME		0
#define AUDIO_MAXVOLUME		256

#define AUDIO_DISABLED		0
#define AUDIO_ENABLED		1

#define AUDIO_MINSAMPLE8	-128
#define AUDIO_MAXSAMPLE8	127

#define AUDIO_MINSAMPLE16	-32768
#define AUDIO_MAXSAMPLE16	32767

/*------------------------------------------------------------------------------------
	Flags
------------------------------------------------------------------------------------*/

#define AUDIO_MONO			0
#define AUDIO_STEREO		1

#define AUDIO_8BIT			0
#define AUDIO_16BIT			2

#define AUDIO_SHOLD			0
#define AUDIO_COSINE		4

#define AUDIO_2DAUDIO		0
#define AUDIO_3DAUDIO		8

#define SAMPLE_8BIT 		1
#define SAMPLE_16BIT		2
#define SAMPLE_BIDILOOP		4
#define SAMPLE_LOOPED		8
#define SAMPLE_MONO			16
#define SAMPLE_STEREO		32

#define MEM_SEEK_CUR		0
#define MEM_SEEK_ABS		1

#define VOICE_AUTO			0
#define VOICE_DISABLED		0
#define VOICE_ENABLED		1
#define VOICE_ACTIVE		2
#define VOICE_FINISHED		4

/*------------------------------------------------------------------------------------
	Structs
------------------------------------------------------------------------------------*/

struct AudioVector
{
	FLOAT X, Y, Z, W;
};

struct MemChunk
{
	BYTE*	Data;
	DWORD	DataLength;
	DWORD	Position;
};

struct Sample
{
	DWORD	Size;			// Size of the rest of structure.
	_WORD	Panning;		// Panning position.
	_WORD	Volume;			// Volume level.
	_WORD	Type;			// Loop or sample type.
	DWORD	Length;			// Length in samples.
	DWORD	LoopStart;		// Start of loop in samples.
	DWORD	LoopEnd;		// End of loop in samples.
	DWORD	SamplesPerSec;	// Samples per second.
	void*	Data;			// Data.
};

struct Voice
{
	INT		State;
	Sample*	pSample;
	INT		PlayPosition;
	DOUBLE	StartTime;
	DOUBLE	LastTime;
	Voice*  StereoVoice;
	INT		Panning;
	INT		BasePanning;
	INT		Volume;
};

struct VoiceStats
{
	FString	CompletionString;
	FLOAT	Completion;
};

struct AudioThread
{
	void*	Thread;
	INT		Valid;
	INT		Exited;
};

struct AudioMutex
{
	void*	Mutex;
};

/*------------------------------------------------------------------------------------
	Globals
------------------------------------------------------------------------------------*/

// Raw Buffer
extern void* 	AudioBuffer;
extern void*	MixBuffer;
extern INT   	BufferSize;
extern INT		AudioFormat;
extern INT		AudioRate;

// State
extern INT   	AudioInitialized;
extern INT		AudioPaused;

// Audio
extern INT		MusicVoices;
extern INT		SampleVoices;
extern Voice 	Voices[AUDIO_TOTALVOICES];

// Volume
extern INT		SampleVolume;
extern INT		MusicVolume;

// Threads
extern AudioThread		MixingThread;
extern AudioMutex		Mutex;

/*------------------------------------------------------------------------------------
	Interface
------------------------------------------------------------------------------------*/

// Library control.
UBOOL AudioInit( DWORD Rate, INT OutputMode, INT Latency );
UBOOL AudioReinit( DWORD Rate, INT OutputMode, INT Latency );
UBOOL AudioShutdown();
UBOOL AllocateVoices( INT Channels );

// Master control.
UBOOL AudioStartOutput( DWORD Rate, INT OutputMode, INT Latency );
UBOOL AudioStopOutput();

// Sample control.
Sample* LoadSample( MemChunk* LoadChunk, const TCHAR* SampleName );
UBOOL UnloadSample( Sample* UnloadSample );
Voice* StartSample( INT Voice, Sample* Sample, INT Freq, INT Volume, INT Panning );
UBOOL StopSample( Voice* StopVoice );
UBOOL SampleFinished( Voice* Sample );
void UpdateSample( Voice* InVoice, INT Freq, INT Volume, INT Panning );

// Voice control.
Voice* AcquireVoice();
void GetVoiceStats( VoiceStats* InStats, Voice* InVoice );

// Volume control.
UBOOL SetSampleVolume( FLOAT Volume );
UBOOL SetMusicVolume( FLOAT Volume );
UBOOL SetCDAudioVolume( FLOAT Volume );

// Digital tracker/UMX music control.
UBOOL LoadMusic( MemChunk* LoadChunk, const TCHAR* MusicName );
UBOOL UnloadMusic();
UBOOL StartMusic( INT Section );
UBOOL StopMusic();
UBOOL SetMusicPosition( INT Section );
UBOOL IsMusicPlaying();
void  MixMusicIntoBuffer();

// CD Audio control.
UBOOL StartCDAudio( INT Track );
UBOOL StopCDAudio();