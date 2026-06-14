/*=============================================================================
	Model.cpp: DCUtil model processing functions
	
	This tool compresses LightBits in .unr map packages using RLE compression
	to reduce memory usage on Dreamcast. The compressed format stores:
	- 4-byte header with original uncompressed size
	- RLE-encoded byte sequences with run-length encoding
	
	Note: The engine's UModel::Serialize method needs to be modified to
	decompress LightBits when loading packages. See UnModel.cpp for the
	decompression implementation.
=============================================================================*/

#include "DCUtilPrivate.h"

/*-----------------------------------------------------------------------------
	RLE Compression for LightBits
-----------------------------------------------------------------------------*/

//
// Emit a run-length encoded byte sequence.
// Format: 
//   - If run length <= 63: single byte = (Code & 0xC0) | (RunLength & 0x3F)
//   - If run length > 63: two bytes = (Code & 0xC0) | 0x40 | (RunLength>>8), then RunLength & 0xFF
//   Code bits:
//     0x00 = normal run
//     0x80 = single byte exception (temporary change)
//
static void EmitRLEByteRun( TArray<BYTE>& Out, BYTE Code, INT RunLength, BYTE Value )
{
	guard(EmitRLEByteRun);
	
	if( RunLength <= 63 )
	{
		// Single byte encoding: Code[2 bits] + RunLength[6 bits] + Value[8 bits]
		Out.AddItem( (Code & 0xC0) | (RunLength & 0x3F) );
		Out.AddItem( Value );
	}
	else
	{
		// Two byte encoding: Code[2 bits] + 0x40 flag + RunLength high byte, then RunLength low byte + Value
		Out.AddItem( (Code & 0xC0) | 0x40 | ((RunLength >> 8) & 0x3F) );
		Out.AddItem( RunLength & 0xFF );
		Out.AddItem( Value );
	}
	
	unguard;
}

//
// Compress LightBits using RLE compression.
// Similar to UBitArray compression but for BYTE values instead of bits.
//
static void CompressLightBitsRLE( TArray<BYTE>& LightBits )
{
	guard(CompressLightBitsRLE);
	
	if( LightBits.Num() == 0 )
		return;
	
	// Create compressed output array
	TArray<BYTE> Compressed;
	Compressed.Empty();
	
	// First, write the original size (needed for decompression)
	INT OriginalSize = LightBits.Num();
	Compressed.AddItem( (OriginalSize >> 24) & 0xFF );
	Compressed.AddItem( (OriginalSize >> 16) & 0xFF );
	Compressed.AddItem( (OriginalSize >> 8) & 0xFF );
	Compressed.AddItem( OriginalSize & 0xFF );
	
	// RLE compression: find runs of identical bytes
	// Algorithm similar to UBitArray but for BYTE values
	BYTE CurrentValue = LightBits(0);
	INT RunLength = 1;
	
	for( INT i = 1; i < LightBits.Num(); i++ )
	{
		if( LightBits(i) == CurrentValue )
		{
			// Continue the run
			RunLength++;
			
			// Max run length is 16383 (14 bits)
			if( RunLength >= 16383 )
			{
				// Emit the run and start a new one
				EmitRLEByteRun( Compressed, 0x00, RunLength, CurrentValue );
				RunLength = 0;
			}
		}
		else
		{
			// Value changed - check if it's a temporary single-byte change
			if( i + 1 < LightBits.Num() && LightBits(i + 1) == CurrentValue )
			{
				// Temporary single-byte change - emit current run, then exception
				if( RunLength > 0 )
				{
					EmitRLEByteRun( Compressed, 0x00, RunLength, CurrentValue );
				}
				EmitRLEByteRun( Compressed, 0x80, 1, LightBits(i) );
				// Next iteration will process i+1 which equals CurrentValue
				RunLength = 0; // Will be set to 1 in next iteration when we see CurrentValue
			}
			else
			{
				// Permanent change - emit current run and start new one
				if( RunLength > 0 )
				{
					EmitRLEByteRun( Compressed, 0x00, RunLength, CurrentValue );
				}
				CurrentValue = LightBits(i);
				RunLength = 1;
			}
		}
	}
	
	// Emit final run
	if( RunLength > 0 )
	{
		EmitRLEByteRun( Compressed, 0x00, RunLength, CurrentValue );
	}
	
	// Replace original with compressed data
	LightBits = Compressed;
	
	unguard;
}

/*-----------------------------------------------------------------------------
	ConvertMapPkg: Process .unr map packages to compress LightBits
-----------------------------------------------------------------------------*/

UBOOL FDCUtil::ConvertMapPkg( const FString& PkgPath, UPackage* Pkg )
{
	guard(ConvertMapPkg);

	printf( "Processing map package '%s'\n", Pkg->GetName() );

	// Record package size before processing for comparison
	INT OldSize = GFileManager->FileSize( *PkgPath );
	// Explicitly load the "MyLevel" ULevel object from the package
	// This ensures all level data is loaded, especially on Dreamcast where lazy loading is used
	ULevel* Level = LoadObject<ULevel>( Pkg, "MyLevel", nullptr, LOAD_NoFail, nullptr );
	if( !Level )
	{
		printf( "  WARNING: Could not load MyLevel from package '%s'\n", Pkg->GetName() );
		return false;
	}
	
	// Force load the Level's Model to ensure it's fully loaded
	// This is critical for proper saving later
	if( Level->Model )
	{
		// Access the Model to force it to load if it's lazy-loaded
		Level->Model->GetFullName(); // Force load by accessing
		// Ensure Model is marked as public so it gets saved
		if( !(Level->Model->GetFlags() & RF_Public) )
			Level->Model->SetFlags( RF_Public );
	}
	
	// Force load all objects in the package to ensure we process everything
	// On Dreamcast, LoadPackage doesn't eagerly load, so we need to explicitly load
	// Loading MyLevel should trigger loading of related objects, but we'll also
	// iterate through all models to make sure we catch everything
	
	UBOOL Changed = false;
	DWORD TotalOriginalSize = 0;
	DWORD TotalCompressedSize = 0;
	
	// Process the main level model first
	if( Level->Model && Level->Model->IsIn( Pkg ) )
	{
		UModel* Model = Level->Model;
		
		if( Model->LightBits.Num() > 0 )
		{
			const DWORD OriginalSize = Model->LightBits.Num();
			TotalOriginalSize += OriginalSize;
			
		TArray<BYTE> OriginalLightBits = Model->LightBits;
		CompressLightBitsRLE( Model->LightBits );
		const DWORD CompressedSize = Model->LightBits.Num();
		TotalCompressedSize += CompressedSize;
		
		if( CompressedSize < OriginalSize )
		{
			Changed = true;
			// Mark model as modified so it gets saved
			Model->Modify();
			printf( "  - Compressed LightBits in '%s': %u -> %u bytes (%.1f%% reduction)\n", 
				Model->GetName(), OriginalSize, CompressedSize, 
				100.0f * (1.0f - (FLOAT)CompressedSize / (FLOAT)OriginalSize) );
		}
		else
		{
			Model->LightBits = OriginalLightBits;
			printf( "  - LightBits in '%s': %u bytes (compression not beneficial)\n", 
				Model->GetName(), OriginalSize );
		}
		}
	}
	
	// Iterate through all other UModel objects in the package (brush models, etc.)
	for( TObjectIterator<UModel> It; It; ++It )
	{
		UModel* Model = *It;
		
		if( !Model->IsIn( Pkg ) )
			continue;
		
		// Skip the level model, we already processed it
		if( Model == Level->Model )
			continue;
		
		// Check if this model has LightBits to compress
		if( Model->LightBits.Num() == 0 )
			continue;
		
		const DWORD OriginalSize = Model->LightBits.Num();
		TotalOriginalSize += OriginalSize;
		
		// Make a copy of original for compression
		TArray<BYTE> OriginalLightBits = Model->LightBits;
		
		// Compress the LightBits
		CompressLightBitsRLE( Model->LightBits );
		
		const DWORD CompressedSize = Model->LightBits.Num();
		TotalCompressedSize += CompressedSize;
		
		if( CompressedSize < OriginalSize )
		{
			Changed = true;
			Model->Modify();
			printf( "  - Compressed LightBits in '%s': %u -> %u bytes (%.1f%% reduction)\n", 
				Model->GetName(), OriginalSize, CompressedSize, 
				100.0f * (1.0f - (FLOAT)CompressedSize / (FLOAT)OriginalSize) );
		}
		else
		{
			// Compression didn't help, restore original
			Model->LightBits = OriginalLightBits;
			printf( "  - LightBits in '%s': %u bytes (compression not beneficial)\n", 
				Model->GetName(), OriginalSize );
		}
	}
	
	if( Changed )
	{
		printf( "Total LightBits compression: %u -> %u bytes (%.1f%% reduction)\n",
			TotalOriginalSize, TotalCompressedSize,
			100.0f * (1.0f - (FLOAT)TotalCompressedSize / (FLOAT)TotalOriginalSize) );
		return true;
	}
		else if( TotalOriginalSize > 0 )
	{
		printf( "No beneficial compression found for LightBits in '%s'\n", Pkg->GetName() );
	}

	unguard;
	return false;
}

