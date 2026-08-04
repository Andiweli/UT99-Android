/*=============================================================================
	UnAudio.cpp: Unreal base audio.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney
	* Wave modification code by Erik de Neve
=============================================================================*/

#include "EnginePrivate.h" 

/*-----------------------------------------------------------------------------
	USound implementation.
-----------------------------------------------------------------------------*/

#pragma DISABLE_OPTIMIZATION
void FSoundData::Load()
{
	guard(FSoundData::Load);
	UBOOL Loaded = SavedPos>0;

	guard(0);
	TLazyArray<BYTE>::Load();
	unguard;
	if( Loaded )
	{
		// Calculate our duration.
		guard(1);
		guard(callingGetPerioid);
		Owner->Duration = GetPeriod();
		unguard;
		unguard;

		// Derive these from the exposed 'low quality' preference setting.
		INT Force8Bit = 0;
		INT ForceHalve = 0;
		guard(3);
		if( Owner->Audio && Owner->Audio->GetLowQualitySetting() && !GIsEditor )
		{
			Force8Bit = 1;
			ForceHalve = 1;
		}
		unguard;

		// Frequencies below this sample rate will NOT be downsampled.
		DWORD FreqThreshold = 22050;

		// Reduce sound frequency and/or bit depth if required.
		if( Force8Bit || ForceHalve )
		{		
			// If ReadWaveInfo returns true, all relevant Wave chunks were found and 
			// all pointers in the WaveInfo structure have been successfully initialized.			
			guard(4);
			FWaveModInfo WaveInfo;
			if( WaveInfo.ReadWaveInfo(*this) && WaveInfo.SampleDataSize>4  ) 
			{				
				// Three main conversions:
				// * Halving the frequency -> simple 0.25, 0.50, 0.25 kernel. 
				// * Reducing bit-depth 8->16  
				// * Both in one sweep.  
				//
				// Important: Wave data ALWAYS padded to use 16-bit alignment even
				// though the number of bytes in pWaveDataSize may be odd.	
				UBOOL ReduceBits = ((Force8Bit) && (*WaveInfo.pBitsPerSample == 16));
				UBOOL ReduceFreq = ((ForceHalve) && (*WaveInfo.pSamplesPerSec >= FreqThreshold));
				if( ReduceBits && ReduceFreq )
				{
					// Convert 16-bit sample to 8 bit and halve the frequency too.
					guard(5);
					WaveInfo.HalveReduce16to8();
					unguard;
				}
				else if (ReduceBits && (!ReduceFreq))
				{	
					// Convert 16-bit sample down to 8-bit.
					guard(6);
					WaveInfo.Reduce16to8();
					unguard;
				}
				else if( ReduceFreq )
				{
					// Just halve the frequency. Separate cases for 16 and 8 bits.
					guard(7);
					WaveInfo.HalveData();
					unguard;
				}
				guard(8);
				WaveInfo.UpdateWaveData( *this );
				unguard;
			}
			unguard;
		}

		// Register it.
		guard(2);
		Owner->OriginalSize = Num();

		if( Owner->Audio && !GIsEditor )
		{
			Owner->Audio->RegisterSound( Owner );
#if defined(PLATFORM_DREAMCAST)
			// On Dreamcast, once the audio backend has uploaded the sample
			// into AICA RAM we do not need to keep another copy in system RAM.
			//Empty();
#endif
		}
		unguard;
	}
	unguard;
}

FLOAT FSoundData::GetPeriod()
{
	// Ensure the data is present.
	guard(a);
	TLazyArray<BYTE>::Load();
	unguard;
	// Calculate the sound's duration.
	FLOAT Period = 0.0f;
	FWaveModInfo WaveInfo;
	if( WaveInfo.ReadWaveInfo(*this) )
	{
		#define DEFAULT_FREQUENCY (22050)
#ifdef PLATFORM_DREAMCAST
		INT SPS, Channels, BitsPerSample, WaveDataSize;
		__builtin_memcpy( &SPS, WaveInfo.pSamplesPerSec, sizeof(INT) );
		__builtin_memcpy( &Channels, WaveInfo.pChannels, sizeof(INT) );
		__builtin_memcpy( &BitsPerSample, WaveInfo.pBitsPerSample, sizeof(INT) );
		__builtin_memcpy( &WaveDataSize, WaveInfo.pWaveDataSize, sizeof(INT) );
#else
		INT SPS = *WaveInfo.pSamplesPerSec;
		INT Channels = *WaveInfo.pChannels;
		INT BitsPerSample = *WaveInfo.pBitsPerSample;
		INT WaveDataSize = *WaveInfo.pWaveDataSize;
#endif
		if (! SPS) SPS = DEFAULT_FREQUENCY;
		INT DurDiv = Channels * BitsPerSample * SPS;
		if ( DurDiv ) Period = WaveDataSize * 8.f / (FLOAT)DurDiv;
		//debugf(TEXT("Name %19s pChannels %9i pBitsps %9i pSampPs %9i pWaveDataSize %9i Time %9f"),Owner->GetName(),Channels,BitsPerSample,SPS,WaveDataSize, Period );
	}	
	return Period;
}

void USound::Serialize( FArchive& Ar )
{
	guard(USound::Serialize);
	Super::Serialize( Ar );
	Ar << FileType;
	if( Ar.IsLoading() || Ar.IsSaving() )
	{
		Ar << Data;
		if( Ar.IsLoading() )
		{
			OriginalSize = Data.Num();
		}
	}
	else
	{
		Ar.CountBytes( OriginalSize, OriginalSize );
	}
	unguard;
}

void USound::Destroy()
{
	guard(USound::Destroy);
	if( Audio )
		Audio->UnregisterSound( this );
	Super::Destroy();
	unguard;
}

void USound::PostLoad()
{
	guard(USound::PostLoad);
	Super::PostLoad();
	unguard;
};
UAudioSubsystem* USound::Audio;
IMPLEMENT_CLASS(USound);

/*-----------------------------------------------------------------------------
	WaveModInfo implementation - downsampling of wave files.
-----------------------------------------------------------------------------*/

//
//	Figure out the WAVE file layout.
//
UBOOL FWaveModInfo::ReadWaveInfo( TArray<BYTE>& WavData )
{
#ifdef PLATFORM_DREAMCAST
	guard(FWaveModInfo::ReadWaveInfo);

	if( WavData.Num() < (INT)sizeof(FRiffWaveHeader) )
		return 0;

	BYTE* WaveStart = &WavData(0);
	WaveDataEnd = WaveStart + WavData.Num();

	FRiffWaveHeader RiffHeader;
	appMemcpy( &RiffHeader, WaveStart, sizeof(FRiffWaveHeader) );

	// Verify we've got a real 'WAVE' header.
	if( RiffHeader.wID != mmioFOURCC('W','A','V','E') )
		return 0;

	pMasterSize = (DWORD*)( WaveStart + STRUCT_OFFSET( FRiffWaveHeader, ChunkLen ) );

	// Helper lambda replacement to safely walk chunks.
	BYTE* ChunkCursor = WaveStart + sizeof(FRiffWaveHeader);
	FRiffChunkOld ChunkHeader;

	// Look for the 'fmt ' chunk.
	while( ChunkCursor + sizeof(FRiffChunkOld) <= WaveDataEnd )
	{
		appMemcpy( &ChunkHeader, ChunkCursor, sizeof(FRiffChunkOld) );
		if( ChunkHeader.ChunkID == mmioFOURCC('f','m','t',' ') )
			break;

		ChunkCursor += Pad16Bit( ChunkHeader.ChunkLen ) + sizeof(FRiffChunkOld);
	}

	if( ChunkCursor + sizeof(FRiffChunkOld) > WaveDataEnd || ChunkHeader.ChunkID != mmioFOURCC('f','m','t',' ') )
		return 0;

	BYTE* FmtData = ChunkCursor + sizeof(FRiffChunkOld);
	if( FmtData + sizeof(FFormatChunk) > WaveDataEnd )
		return 0;

	FFormatChunk FmtCopy;
	appMemcpy( &FmtCopy, FmtData, sizeof(FFormatChunk) );

	pBitsPerSample  = (_WORD*)( FmtData + STRUCT_OFFSET( FFormatChunk, wBitsPerSample ) );
	pSamplesPerSec  = (DWORD*)( FmtData + STRUCT_OFFSET( FFormatChunk, nSamplesPerSec ) );
	pAvgBytesPerSec = (DWORD*)( FmtData + STRUCT_OFFSET( FFormatChunk, nAvgBytesPerSec ) );
	pBlockAlign		= (_WORD*)( FmtData + STRUCT_OFFSET( FFormatChunk, nBlockAlign ) );
	pChannels       = (_WORD*)( FmtData + STRUCT_OFFSET( FFormatChunk, nChannels ) );

	// Look for the 'data' chunk.
	ChunkCursor = WaveStart + sizeof(FRiffWaveHeader);
	while( ChunkCursor + sizeof(FRiffChunkOld) <= WaveDataEnd )
	{
		appMemcpy( &ChunkHeader, ChunkCursor, sizeof(FRiffChunkOld) );
		if( ChunkHeader.ChunkID == mmioFOURCC('d','a','t','a') )
			break;

		ChunkCursor += Pad16Bit( ChunkHeader.ChunkLen ) + sizeof(FRiffChunkOld);
	}

	if( ChunkCursor + sizeof(FRiffChunkOld) > WaveDataEnd || ChunkHeader.ChunkID != mmioFOURCC('d','a','t','a') )
		return 0;

	SampleDataStart = ChunkCursor + sizeof(FRiffChunkOld);
	pWaveDataSize   = (DWORD*)( ChunkCursor + STRUCT_OFFSET( FRiffChunkOld, ChunkLen ) );
	SampleDataSize  = ChunkHeader.ChunkLen;
	OldBitsPerSample = FmtCopy.wBitsPerSample;
	SampleDataEnd   = SampleDataStart + SampleDataSize;

	if( SampleDataEnd > WaveDataEnd )
		return 0;

	NewDataSize	= SampleDataSize;

	// Look for a 'smpl' chunk (optional).
	ChunkCursor = WaveStart + sizeof(FRiffWaveHeader);
	while( ChunkCursor + sizeof(FRiffChunkOld) <= WaveDataEnd )
	{
		appMemcpy( &ChunkHeader, ChunkCursor, sizeof(FRiffChunkOld) );
		if( ChunkHeader.ChunkID == mmioFOURCC('s','m','p','l') )
			break;

		ChunkCursor += Pad16Bit( ChunkHeader.ChunkLen ) + sizeof(FRiffChunkOld);
	}

	// Chunk found ? smpl chunk is optional.
	// Find the first sample-loop structure, and the total number of them.
	if( ChunkCursor + sizeof(FRiffChunkOld) <= WaveDataEnd && ChunkHeader.ChunkID == mmioFOURCC('s','m','p','l') )
	{
		BYTE* SampleChunkData = ChunkCursor + sizeof(FRiffChunkOld);
		if( SampleChunkData + sizeof(FSampleChunk) <= WaveDataEnd )
		{
			FSampleChunk SampleChunkCopy;
			appMemcpy( &SampleChunkCopy, SampleChunkData, sizeof(FSampleChunk) );
			SampleLoopsNum  = SampleChunkCopy.cSampleLoops; // Number of tSampleLoop structures.
			// First tSampleLoop structure starts right after the tSampleChunk.
			pSampleLoop = (FSampleLoop*) ( SampleChunkData + sizeof(FSampleChunk) ); 
		}
	}
		
	return 1;
	unguard;
#else
	guard(FWaveModInfo::ReadWaveInfo);
	FFormatChunk* FmtChunk;
	FRiffWaveHeader* RiffHdr = (FRiffWaveHeader*)&WavData(0);
	WaveDataEnd = &WavData(0) + WavData.Num();	
	
	// Verify we've got a real 'WAVE' header.
	if( RiffHdr->wID != ( mmioFOURCC('W','A','V','E') )  )
		return 0;

	pMasterSize = &RiffHdr->ChunkLen;

	FRiffChunkOld* RiffChunk = (FRiffChunkOld*)&WavData(3*4);
	// Look for the 'fmt ' chunk.
	while( ( ((BYTE*)RiffChunk + 8) < WaveDataEnd)  && ( RiffChunk->ChunkID != mmioFOURCC('f','m','t',' ') ) )
	{
		// Go to next chunk.
		RiffChunk = (FRiffChunkOld*) ( (BYTE*)RiffChunk + Pad16Bit(RiffChunk->ChunkLen) + 8); 
	}
	// Chunk found ?
	if( RiffChunk->ChunkID != mmioFOURCC('f','m','t',' ') )
		return 0;

	FmtChunk = (FFormatChunk*)((BYTE*)RiffChunk + 8);
	pBitsPerSample  = &FmtChunk->wBitsPerSample;
	pSamplesPerSec  = &FmtChunk->nSamplesPerSec;
	pAvgBytesPerSec = &FmtChunk->nAvgBytesPerSec;
	pBlockAlign		= &FmtChunk->nBlockAlign;
	pChannels       = &FmtChunk->nChannels;

	// re-initalize the RiffChunk pointer
	RiffChunk = (FRiffChunkOld*)&WavData(3*4);
	// Look for the 'data' chunk.
	while( ( ((BYTE*)RiffChunk + 8) < WaveDataEnd) && ( RiffChunk->ChunkID != mmioFOURCC('d','a','t','a') ) )
	{
		// Go to next chunk.
		RiffChunk = (FRiffChunkOld*) ( (BYTE*)RiffChunk + Pad16Bit(RiffChunk->ChunkLen) + 8); 
	} 
	// Chunk found ?
	if( RiffChunk->ChunkID != mmioFOURCC('d','a','t','a') )
		return 0;

	SampleDataStart = (BYTE*)RiffChunk + 8;
	pWaveDataSize   = &RiffChunk->ChunkLen;
	SampleDataSize  =  RiffChunk->ChunkLen;
	OldBitsPerSample = FmtChunk->wBitsPerSample;
	SampleDataEnd   =  SampleDataStart+SampleDataSize;

	NewDataSize	= SampleDataSize;

	// Re-initalize the RiffChunk pointer
	RiffChunk = (FRiffChunkOld*)&WavData(3*4);
	// Look for a 'smpl' chunk.
	while( ( (((BYTE*)RiffChunk) + 8) < WaveDataEnd) && ( RiffChunk->ChunkID != mmioFOURCC('s','m','p','l') ) )
	{
		// Go to next chunk.
		RiffChunk = (FRiffChunkOld*) ( (BYTE*)RiffChunk + Pad16Bit(RiffChunk->ChunkLen) + 8); 
	} 

	// Chunk found ? smpl chunk is optional.
	// Find the first sample-loop structure, and the total number of them.

	if( (UPTRINT)RiffChunk+4<(UPTRINT)WaveDataEnd && RiffChunk->ChunkID == mmioFOURCC('s','m','p','l') )
	{
		FSampleChunk* pSampleChunk =  (FSampleChunk*)( (BYTE*)RiffChunk + 8);
		SampleLoopsNum  = pSampleChunk->cSampleLoops; // Number of tSampleLoop structures.
		// First tSampleLoop structure starts right after the tSampleChunk.
		pSampleLoop = (FSampleLoop*) ((BYTE*)pSampleChunk + sizeof(FSampleChunk)); 
	}

	return 1;
	unguard;
#endif
}


//
// Update internal variables and shrink the data fields.
//
UBOOL FWaveModInfo::UpdateWaveData( TArray<BYTE>& WavData )
{
#ifdef PLATFORM_DREAMCAST
	guard(FWaveModInfo::UpdateWaveData);
	if( NewDataSize < SampleDataSize )
	{		
		// Shrinkage of data chunk in bytes -> chunk data must always remain 16-bit-padded.
		INT ChunkShrinkage = Pad16Bit(SampleDataSize)  - Pad16Bit(NewDataSize);

		// Update sizes.
		*pWaveDataSize  = NewDataSize;
		*pMasterSize   -= ChunkShrinkage;

		// Refresh all wave parameters depending on bit depth and/or sample rate.
		*pBlockAlign    =  *pChannels * (*pBitsPerSample >> 3); // channels * Bytes per sample
		*pAvgBytesPerSec = *pBlockAlign * *pSamplesPerSec; //sample rate * Block align

		// Update 'smpl' chunk data also, if present.
		if (SampleLoopsNum && pSampleLoop)
		{
			BYTE* SampleLoopPtr = (BYTE*)pSampleLoop;
			INT SampleDivisor = ( (SampleDataSize *  *pBitsPerSample) / (NewDataSize ) );
			for (INT SL = 0; SL<SampleLoopsNum; SL++)
			{
				FSampleLoop LoopCopy;
				appMemcpy( &LoopCopy, SampleLoopPtr, sizeof(FSampleLoop) );
				LoopCopy.dwStart = LoopCopy.dwStart  * OldBitsPerSample / SampleDivisor;
				LoopCopy.dwEnd   = LoopCopy.dwEnd  * OldBitsPerSample / SampleDivisor;
				appMemcpy( SampleLoopPtr, &LoopCopy, sizeof(FSampleLoop) );
				SampleLoopPtr += sizeof(FSampleLoop); // Next TempSampleLoop structure.
			}	
		}		
			
		// Now shuffle back all data after wave data by SampleDataSize/2 (+ padding) bytes 
		// INT SampleShrinkage = ( SampleDataSize/4) * 2;
		BYTE* NewWaveDataEnd = SampleDataEnd - ChunkShrinkage;

		for ( INT pt = 0; pt< ( WaveDataEnd -  SampleDataEnd); pt++ )
		{ 
			NewWaveDataEnd[pt] =  SampleDataEnd[pt];
		}			

		// Resize the dynamic array.
		WavData.Remove( WavData.Num() - ChunkShrinkage, ChunkShrinkage );
		
		/*
		static INT SavedBytes = 0;
		SavedBytes += ChunkShrinkage;
		debugf(NAME_Log," Audio shrunk by: %i bytes, total savings %i bytes.",ChunkShrinkage,SavedBytes);
		debugf(NAME_Log," New BitsPerSample: %i ", *pBitsPerSample);
		debugf(NAME_Log," New SamplesPerSec: %i ", *pSamplesPerSec);
		debugf(NAME_Log," Olddata/NEW*wav* sizes: %i %i ", SampleDataSize, *pMasterSize);
		*/
	}

	// Noise gate filtering assumes 8-bit sound.
	// Warning: While very useful on SOME sounds, it erased too many low-volume sound fx
	// in its current form - even when 'noise' level scaled to average sound amplitude.
	// if (NoiseGate) NoiseGateFilter();

	return 1;
	unguard;
#else
	guard(FWaveModInfo::UpdateWaveData);
	if( NewDataSize < SampleDataSize )
	{		
		// Shrinkage of data chunk in bytes -> chunk data must always remain 16-bit-padded.
		INT ChunkShrinkage = Pad16Bit(SampleDataSize)  - Pad16Bit(NewDataSize);

		// Update sizes.
		*pWaveDataSize  = NewDataSize;
		*pMasterSize   -= ChunkShrinkage;

		// Refresh all wave parameters depending on bit depth and/or sample rate.
		*pBlockAlign    =  *pChannels * (*pBitsPerSample >> 3); // channels * Bytes per sample
		*pAvgBytesPerSec = *pBlockAlign * *pSamplesPerSec; //sample rate * Block align

		// Update 'smpl' chunk data also, if present.
		if (SampleLoopsNum)
		{
			FSampleLoop* pTempSampleLoop = pSampleLoop;
			INT SampleDivisor = ( (SampleDataSize *  *pBitsPerSample) / (NewDataSize ) );
			for (INT SL = 0; SL<SampleLoopsNum; SL++)
			{
				pTempSampleLoop->dwStart = pTempSampleLoop->dwStart  * OldBitsPerSample / SampleDivisor;
				pTempSampleLoop->dwEnd   = pTempSampleLoop->dwEnd  * OldBitsPerSample / SampleDivisor;
				pTempSampleLoop++; // Next TempSampleLoop structure.
			}	
		}		
			
		// Now shuffle back all data after wave data by SampleDataSize/2 (+ padding) bytes 
		// INT SampleShrinkage = ( SampleDataSize/4) * 2;
		BYTE* NewWaveDataEnd = SampleDataEnd - ChunkShrinkage;

		for ( INT pt = 0; pt< ( WaveDataEnd -  SampleDataEnd); pt++ )
		{ 
			NewWaveDataEnd[pt] =  SampleDataEnd[pt];
		}			

		// Resize the dynamic array.
		WavData.Remove( WavData.Num() - ChunkShrinkage, ChunkShrinkage );
		
		/*
		static INT SavedBytes = 0;
		SavedBytes += ChunkShrinkage;
		debugf(NAME_Log," Audio shrunk by: %i bytes, total savings %i bytes.",ChunkShrinkage,SavedBytes);
		debugf(NAME_Log," New BitsPerSample: %i ", *pBitsPerSample);
		debugf(NAME_Log," New SamplesPerSec: %i ", *pSamplesPerSec);
		debugf(NAME_Log," Olddata/NEW*wav* sizes: %i %i ", SampleDataSize, *pMasterSize);
		*/
	}

	// Noise gate filtering assumes 8-bit sound.
	// Warning: While very useful on SOME sounds, it erased too many low-volume sound fx
	// in its current form - even when 'noise' level scaled to average sound amplitude.
	// if (NoiseGate) NoiseGateFilter();

	return 1;
	unguard;
#endif
}

//
// Reduce bit depth and halve the number of samples simultaneously.
//
void FWaveModInfo::HalveReduce16to8()
{
	guard(FWaveModInfo::HalveReduce16to8);

	DWORD SampleWords =  SampleDataSize >> 1;
	INT OrigValue,NewValue;
	INT ErrorDiff = 0;

	DWORD SampleBytes = SampleWords >> 1;

	INT NextSample0 = (INT)((SWORD*) SampleDataStart)[0];
	INT NextSample1,NextSample2;

	BYTE* SampleData =  SampleDataStart;
	for (DWORD T=0; T<SampleBytes; T++)
	{
		NextSample1 = (INT)((SWORD*)SampleData)[T*2];
		NextSample2 = (INT)((SWORD*)SampleData)[T*2+1];
		INT Filtered16BitSample = 32768*4 + NextSample0 + NextSample1 + NextSample1 + NextSample2;
		NextSample0 = NextSample2;

		// Error diffusion works in '18 bit' resolution.
		OrigValue = ErrorDiff + Filtered16BitSample;
		// Rounding: "+0.5"
		NewValue  = (OrigValue + 512) & 0xfffffc00;
		if (NewValue > 0x3fc00) NewValue = 0x3fc00;

		INT NewSample = NewValue >> (8+2);
		SampleData[T] = (BYTE)NewSample;	// Output byte.

		ErrorDiff = OrigValue - NewValue;   // Error cycles back into input.
	}

	NewDataSize = SampleBytes;

	*pBitsPerSample  = 8;
	*pSamplesPerSec  = *pSamplesPerSec >> 1;

	NoiseGate = true;
	unguard;
}

//
// Reduce bit depth.
//
void FWaveModInfo::Reduce16to8()
{
	guard(FWaveModInfo::Reduce16to8);

	DWORD SampleBytes =  SampleDataSize >> 1;
	INT OrigValue,NewValue;
	INT ErrorDiff = 0;
	BYTE* SampleData = SampleDataStart;

	for (DWORD T=0; T<SampleBytes; T++)
	{
		// Error diffusion works in 16 bit resolution.
		OrigValue = ErrorDiff + 32768 + (INT)((SWORD*)SampleData)[T];
		// Rounding: '+0.5', then mask off low 2 bits.
		NewValue  = (OrigValue + 127 ) & 0xffffff00;  // + 128
		if (NewValue > 0xff00) NewValue = 0xff00;

		INT NewSample = NewValue >> 8;
		SampleData[T] = NewSample;

		ErrorDiff = OrigValue - NewValue;  // Error cycles back into input.
	}				

	NewDataSize = SampleBytes;
	*pBitsPerSample  = 8;
	NoiseGate = true;

	unguard;
}

//
// Halve the number of samples.
//
void FWaveModInfo::HalveData()
{
	guard(FWaveModInfo::HalveData);
	if( *pBitsPerSample == 16)
	{						
		DWORD SampleWords =  SampleDataSize >> 1;
		INT OrigValue,NewValue;
		INT ErrorDiff = 0;

		DWORD ScaledSampleWords = SampleWords >> 1; 

		INT NextSample0 = (INT)((SWORD*) SampleDataStart)[0];
		INT NextSample1, NextSample2;

		BYTE* SampleData =  SampleDataStart;
		for (DWORD T=0; T<ScaledSampleWords; T++)
		{
			NextSample1 = (INT)((SWORD*)SampleData)[T*2];
			NextSample2 = (INT)((SWORD*)SampleData)[T*2+1];
			INT Filtered18BitSample = 32768*4 + NextSample0 + NextSample1 + NextSample1 + NextSample2;
			NextSample0 = NextSample2;

			// Error diffusion works with '18 bit' resolution.
			OrigValue = ErrorDiff + Filtered18BitSample;
			// Rounding: '+0.5', then mask off low 2 bits.
			NewValue  = (OrigValue + 2) & 0x3fffc;
			if (NewValue > 0x3fffc) NewValue = 0x3fffc;
			((SWORD*)SampleData)[T] = (NewValue >> 2) - 32768;  // Output SWORD.
			ErrorDiff = OrigValue - NewValue; // Error cycles back into input.
		}				
		NewDataSize = (ScaledSampleWords * 2);
		*pSamplesPerSec  = *pSamplesPerSec >> 1;
	}
	else if( *pBitsPerSample == 8 )
	{									
		INT OrigValue,NewValue;
		INT ErrorDiff = 0;
	
		DWORD SampleBytes = SampleDataSize >> 1;  
		BYTE* SampleData = SampleDataStart;

		INT NextSample0 = SampleData[0];
		INT NextSample1, NextSample2;

		for (DWORD T=0; T<SampleBytes; T++)
		{
			NextSample1 =  SampleData[T*2];
			NextSample2 =  SampleData[T*2+1];
			INT Filtered10BitSample =  NextSample0 + NextSample1 + NextSample1 + NextSample2;
			NextSample0 =  NextSample2;

			// Error diffusion works with '10 bit' resolution.
			OrigValue = ErrorDiff + Filtered10BitSample;
			// Rounding: '+0.5', then mask off low 2 bits.
			NewValue  = (OrigValue + 2) & 0x3fc;
			if (NewValue > 0x3fc) NewValue = 0x3fc;
			SampleData[T] = (BYTE)(NewValue >> 2);	// Output BYTE.
			ErrorDiff = OrigValue - NewValue;		// Error cycles back into input.
		}				
		NewDataSize = SampleBytes; 
		*pSamplesPerSec  = *pSamplesPerSec >> 1;
	}
	unguard;
}

//
//	Noise gate filter. Hard to make general-purpose without hacking up some (soft) sounds.
//
void FWaveModInfo::NoiseGateFilter()
{
	guard(FWaveModInfo::NoiseGateFilter);

	BYTE* SampleData  =  SampleDataStart;
	INT   SampleBytes = *pWaveDataSize; 
	INT	  MinBlankSize = 860 * ((*pSamplesPerSec)/11025); // 600-800...

	// Threshold sound amplitude. About 18 seems OK.
	INT Amplitude	 = 18;

	// Ignore any over-threshold signals if under this size. ( 32 ?)
	INT GlitchSize	 = 32 * ((*pSamplesPerSec)/11025);
	INT StartSilence =  0;
	INT EndSilence	 =  0;
	INT LastErased   = -1;

	for( INT T = 0; T< SampleBytes; T++ )
	{
		UBOOL Loud;
		if	( Abs(SampleData[T]-128) >= Amplitude )
		{
			Loud = true;							
			if (StartSilence > 0)
			{
				if ( (T-StartSilence) < GlitchSize ) Loud = false;
			}							
		}
		else Loud = false;

		if( StartSilence == 0 )
		{
			if( !Loud )
				StartSilence = T;
		}
		else
		{													
			if( ((EndSilence == 0) && (Loud)) || (T ==(SampleBytes-1) ) )
			{
				EndSilence = T;
				//
				// Erase an area of low-amplitude sound ( noise... ) if size >= MinBlankSize.
				//
				// Todo: try erasing smoothly - decay, create some attack, 
				// proportional to the size of the area. ?
				//
				if	(( EndSilence - StartSilence) >= MinBlankSize )
				{					
					for ( INT Er = StartSilence; Er< EndSilence; Er++ )
					{
						SampleData[Er] = 128;
					}
				}
				LastErased = EndSilence-1;
				StartSilence = 0;
				EndSilence   = 0;
			}
		}										
	}
	unguard;
}
/*-----------------------------------------------------------------------------
	UMusic implementation.
-----------------------------------------------------------------------------*/

void UMusic::Serialize( FArchive& Ar )
{
	guard(UMusic::Serialize);
	Super::Serialize( Ar );
	Ar << FileType;
	if( Ar.IsLoading() || Ar.IsSaving() )
	{
#ifdef PLATFORM_LOW_MEMORY
		// UT99_ANDROID_UMX_LIBXMP_V5_BEGIN
		// The Android/LOW_MEMORY build used to skip UMusic payloads entirely by
		// reading only the seek position and then seeking past the data.  That kept
		// textures/sounds small, but it also made every UMX object permanently empty:
		// Music->Data.Load() later had nothing to load, so the libxmp music backend
		// could never start playback even though the UMX files and libxmp.so were
		// present in the APK.
		//
		// Use TLazyArray serialization here instead.  With GLazyLoad enabled this
		// still does NOT force all songs into RAM at package load time; it only
		// attaches the lazy loader so the selected song can be loaded on demand by
		// UGenericAudioSubsystem::RegisterMusic().
		Ar << Data;
		if( Ar.IsLoading() && !GLazyLoad )
			OriginalSize = Data.Num();
		else if( Ar.IsLoading() )
			OriginalSize = 0;
		// UT99_ANDROID_UMX_LIBXMP_V5_END
#else
		Ar << Data;
		if( Ar.IsLoading() )
			OriginalSize = Data.Num();
#endif
	}
	else
	{
		Ar.CountBytes( OriginalSize, OriginalSize );
	}
	unguard;
}
void UMusic::Destroy()
{
	guard(UMusic::Destroy);
	if( Audio )
		Audio->UnregisterMusic( this );
	Super::Destroy();
	unguard;
}
void UMusic::PostLoad()
{
	guard(UMusic::PostLoad);
	Super::PostLoad();
	unguard;
}
UAudioSubsystem* UMusic::Audio;
IMPLEMENT_CLASS(UMusic);

/*-----------------------------------------------------------------------------
	UAudioSubsystem implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UAudioSubsystem);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
