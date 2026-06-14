/*=============================================================================
	UnBits.h: Unreal bitstream manipulation classes.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "CorePrivate.h"

// Table.
static BYTE GShift[8]={0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80};
static BYTE GMask [8]={0x00,0x01,0x03,0x07,0x0f,0x1f,0x3f,0x7f};

// Optimized arbitrary bit range memory copy routine.
void appBitsCpy( BYTE* Dest, INT DestBit, BYTE* Src, INT SrcBit, INT BitCount )
{
	// Special-case maximum of 2 dwords to read, 2 to write.
	if( BitCount < 32 ) 
	{
		DWORD DestIndex		= DestBit/32;
		DWORD SrcIndex		= SrcBit/32;
		DWORD LastDest		= ( DestBit+BitCount )/32; 
		DWORD LastSrc		= ( SrcBit+BitCount )/32;  
		DWORD ShiftSrc      = SrcBit & 31; 
		DWORD ShiftDest     = DestBit & 31;
		DWORD FirstMask     = 0xFFFFFFFF << ShiftDest;  
		DWORD LastMask      = 0xFFFFFFFF << ((DestBit + BitCount) & 31) ; 		
		DWORD Accu;		

		if( ShiftSrc && (SrcIndex != LastSrc) ) // Avoid reading second word if not necessary.
			Accu = (((DWORD*)Src)[SrcIndex] >> ShiftSrc) | (((DWORD*)Src)[LastSrc ] << (32-ShiftSrc));
		else
			Accu = (((DWORD*)Src)[SrcIndex] >> ShiftSrc);

		if( DestIndex == LastDest )
		{
			DWORD MultiMask = FirstMask & ~LastMask;
			((DWORD*)Dest)[DestIndex] = (((DWORD*)Dest)[DestIndex] & ~MultiMask ) | ((Accu << ShiftDest) & MultiMask) ;
		}
		else
		{			
			((DWORD*)Dest)[DestIndex] = (((DWORD*)Dest)[DestIndex] & ~FirstMask ) | (((Accu << ShiftDest)) & FirstMask) ;
			if( LastMask ) // Avoid writing second word if not necessary.
				((DWORD*)Dest)[LastDest ] = (((DWORD*)Dest)[LastDest ] & LastMask  ) | (((Accu >> (32-ShiftDest)) & ~LastMask)) ;
		}
		return;
	}

	// Special case for doublewords.
	if( BitCount == 32 )
	{
		DWORD DestIndex		= DestBit/32;
		DWORD SrcIndex		= SrcBit/32;		
		DWORD ShiftSrc      = SrcBit & 31; 
		DWORD ShiftDest     = DestBit & 31;
		DWORD Accu;		

		if( ShiftSrc == 0 ) // Aligned read.
			Accu = ((DWORD*)Src)[SrcIndex];
		else
			Accu = (((DWORD*)Src)[SrcIndex] >> ShiftSrc) | (((DWORD*)Src)[SrcIndex+1] << (32-ShiftSrc));

		if( ShiftDest == 0 ) // Aligned write.
		{
			((DWORD*)Dest)[DestIndex] = Accu;
		}
		else
		{			
			DWORD FirstMask = 0xFFFFFFFF << ShiftDest;  
			((DWORD*)Dest)[DestIndex] = (((DWORD*)Dest)[DestIndex] & ~FirstMask ) | ((Accu << ShiftDest) ) ;
			((DWORD*)Dest)[DestIndex+1] = (((DWORD*)Dest)[DestIndex+1] &  FirstMask ) | ((Accu >> (32-ShiftDest)) ) ;
		}
		return;
	}

	// Main copier, uses byte sized shifting. Very fast inner loop.
	// Code below is general and works for all sequence sizes too when (*) is uncommented.
	DWORD DestIndex		= DestBit/8;
	DWORD FirstSrcMask  = 0xFF << ( DestBit & 7);  
	DWORD LastDest		= ( DestBit+BitCount )/8; 
	DWORD LastSrcMask   = 0xFF << ((DestBit + BitCount) & 7); 
	DWORD SrcIndex		= SrcBit/8;
	DWORD LastSrc		= ( SrcBit+BitCount )/8;  
	INT   ShiftCount    = (DestBit & 7) - (SrcBit & 7); 
	INT   WordDestLoop  = LastDest-DestIndex; 
	INT   WordSrcLoop   = LastSrc -SrcIndex;  
	DWORD FullLoop;
	DWORD BitAccu;

	// Lead-in needs to read 1 or 2 source bytes depending on alignment.
	if( ShiftCount>=0 )
	{
		FullLoop  = Max(WordDestLoop, WordSrcLoop);  
		BitAccu   = Src[SrcIndex] << ShiftCount; 
		ShiftCount += 8; //prepare for the inner loop.
	}
	else
	{
		ShiftCount +=8; // turn shifts -7..-1 into +1..+7
		FullLoop  = Max(WordDestLoop, WordSrcLoop-1);  
		BitAccu   = Src[SrcIndex] << ShiftCount; 
		SrcIndex++;
		ShiftCount += 8; //prepare for inner loop.
		BitAccu = ( ( (DWORD)Src[SrcIndex] << ShiftCount ) + (BitAccu)) >> 8; 
	}

	// (*)  This check is only needed if sizes 32 and smaller aren't handled separately.
	/*
	// Single byte destination -> combine all the masks.
	if (FullLoop == 0)
	{
		DWORD MultiMask = FirstSrcMask & LastSrcMask;
		Dest[DestIndex] = (BYTE) ( ( BitAccu & MultiMask ) | ( Dest[DestIndex] &  ~MultiMask ) );
		return;
	}
	*/

	Dest[DestIndex] = (BYTE) (( BitAccu & FirstSrcMask) | ( Dest[DestIndex] &  ~FirstSrcMask ) );
	SrcIndex++;
	DestIndex++;

	// Inner loop. 
	for(; FullLoop>1; FullLoop--) 
	{				
		BitAccu = (( (DWORD)Src[SrcIndex] << ShiftCount ) + (BitAccu)) >> 8; // copy in the new, discard the old.
		SrcIndex++;
		Dest[DestIndex] = (BYTE) BitAccu;  // copy low 8 bits
		DestIndex++;		
	}

	// Aviod unnecessary memory access.
	if( LastSrcMask != 0xFF) 
	{
		BitAccu = ( ( (DWORD)Src[SrcIndex] << ShiftCount ) + (BitAccu)) >> 8; 
		Dest[DestIndex] = (BYTE) ((BitAccu & ~LastSrcMask) | ( Dest[DestIndex] & LastSrcMask ) );  
	}
}

/*-----------------------------------------------------------------------------
	FBitWriter.
-----------------------------------------------------------------------------*/

FBitWriter::FBitWriter( INT InMaxBits )
:	Num			( 0 )
,	Max			( InMaxBits )
,	Buffer		( (InMaxBits+7)>>3 )
{
	guard(FBitWriter::FBitWriter);
	appMemzero( &Buffer(0), Buffer.Num() );
	ArIsPersistent = ArIsSaving = 1;
	ArNetVer |= 0x80000000;
	unguard;
}
void FBitWriter::SerializeBits( void* Src, INT LengthBits )
{
	guardSlow(FBitWriter::SerializeBits);
	if( Num+LengthBits<=Max )
	{
		for( INT i=0; i<LengthBits; i++,Num++ )
			if( ((BYTE*)Src)[i>>3] & GShift[i&7] )
				Buffer(Num>>3) |= GShift[Num&7];
	}
	else ArIsError = 1;
	unguardSlow;
}
void FBitWriter::Serialize( void* Src, INT LengthBytes )
{
	guardSlow(FBitWriter::Serialize);
	//warning: Copied and pasted from FBitWriter::SerializeBits
	INT LengthBits = LengthBytes*8;
	if( Num+LengthBits<=Max )
	{
		for( INT i=0; i<LengthBits; i++,Num++ )
			if( ((BYTE*)Src)[i>>3] & GShift[i&7] )
				Buffer(Num>>3) |= GShift[Num&7];
	}
	else ArIsError = 1;
	unguardSlow;
}
void FBitWriter::SerializeInt( DWORD& Value, DWORD ValueMax )
{
	guardSlow(FBitWriter::SerializeInt);
	checkSlow(Value<ValueMax);
#if ENGINE_VERSION<230 //oldver
	DWORD NewValue = INTEL_ORDER(Value);
	SerializeBits( &NewValue, appCeilLogTwo(ValueMax) );
#else
	if( Num+appCeilLogTwo(ValueMax)<=Max )
	{
		DWORD NewValue=0;
		for( DWORD Mask=1; NewValue+Mask<ValueMax && Mask; Mask*=2,Num++ )
		{
			if( Value&Mask )
			{
				Buffer(Num>>3) += GShift[Num&7];
				NewValue += Mask;
			}
		}
	} else ArIsError = 1;
#endif
	unguardSlow;
}
void FBitWriter::WriteInt( DWORD Value, DWORD ValueMax )
{
	guardSlow(FBitWriter::WriteInt);
	checkSlow(Value<ValueMax);
	//warning: Copied and pasted from FBitWriter::SerializeInt
#if ENGINE_VERSION<230 //oldver
	DWORD NewValue = INTEL_ORDER(Value);
	SerializeBits( &NewValue, appCeilLogTwo(ValueMax) );
#else
	if( Num+appCeilLogTwo(ValueMax)<=Max )
	{
		DWORD NewValue=0;
		for( DWORD Mask=1; NewValue+Mask<ValueMax && Mask; Mask*=2,Num++ )
		{
			if( Value&Mask )
			{
				Buffer(Num>>3) += GShift[Num&7];
				NewValue += Mask;
			}
		}
	} else ArIsError = 1;
#endif
	unguardSlow;
}
void FBitWriter::WriteBit( BYTE In )
{
	guardSlow(FBitWriter::WriteBit);
	if( Num+1<=Max )
	{
		if( In )
			Buffer(Num>>3) |= GShift[Num&7];
		Num++;
	}
	else ArIsError = 1;
	unguardSlow;
}
BYTE* FBitWriter::GetData()
{
	guardSlow(FBitWriter::GetData);
	return &Buffer(0);
	unguardSlow;
}
INT FBitWriter::GetNumBytes()
{
	return (Num+7)>>3;
}
INT FBitWriter::GetNumBits()
{
	return Num;
}
void FBitWriter::SetOverflowed()
{
	ArIsError = 1;
}

/*-----------------------------------------------------------------------------
	FBitWriterMark.
-----------------------------------------------------------------------------*/

void FBitWriterMark::Pop( FBitWriter& Writer )
{
	guardSlow(FBitWriterMark::Pop);
	checkSlow(Num<=Writer.Num);
	checkSlow(Num<=Writer.Max);

	if( Num&7 )
		Writer.Buffer(Num>>3) &= GMask[Num&7];
	INT Start = (Num       +7)>>3;
	INT End   = (Writer.Num+7)>>3;
	appMemzero( &Writer.Buffer(Start), End-Start );

	Writer.ArIsError = Overflowed;
	Writer.Num       = Num;

	unguardSlow;
}

/*-----------------------------------------------------------------------------
	FBitReader.
-----------------------------------------------------------------------------*/

//
// Reads bitstreams.
//
FBitReader::FBitReader( BYTE* Src, INT CountBits )
:	Num			( CountBits )
,	Buffer		( (CountBits+7)>>3 )
,	Pos			( 0 )
{
	guard(FBitReader::FBitReader);
	ArIsPersistent = ArIsLoading = 1;
	ArNetVer |= 0x80000000;
	if( Src )
		appMemcpy( &Buffer(0), Src, (CountBits+7)>>3 );
	unguard;
}
void FBitReader::SetData( FBitReader& Src, INT CountBits )
{
	guard(FBitReader::SetData);
	Num        = CountBits;
	Pos        = 0;
	ArIsError  = 0;
	Buffer.Empty();
	Buffer.Add( (CountBits+7)>>3 );
	Src.SerializeBits( &Buffer(0), CountBits );
	unguard;
}
void FBitReader::SerializeBits( void* Dest, INT LengthBits )
{
	guardSlow(FBitReader::SerializeBits);
	appMemzero( Dest, (LengthBits+7)>>3 );
	if( Pos+LengthBits<=Num )
	{
		for( INT i=0; i<LengthBits; i++,Pos++ )
			if( Buffer(Pos>>3) & GShift[Pos&7] )
				((BYTE*)Dest)[i>>3] |= GShift[i&7];
	}
	else SetOverflowed();
	unguardSlow;
}
void FBitReader::SerializeInt( DWORD& Value, DWORD ValueMax )
{
	guardSlow(FBitReader::SerializeInt);
#if ENGINE_VERSION<230 //oldver
	Value = 0;
	SerializeBits( &Value, appCeilLogTwo(ValueMax) );
	Value = INTEL_ORDER(Value);
#else
	Value=0;
	for( DWORD Mask=1; Value+Mask<ValueMax && Mask; Mask*=2,Pos++ )
	{
		if( Pos>=Num )
		{
			ArIsError = 1;
			break;
		}
		if( Buffer(Pos>>3) & GShift[Pos&7] )
		{
			Value |= Mask;
		}
	}
#endif
	unguardSlow;
}
DWORD FBitReader::ReadInt( DWORD Max )
{
	guardSlow(FBitReader::ReadInt);
	DWORD Value=0;
	SerializeInt( Value, Max );
	return Value;
	unguardSlow;
}
BYTE FBitReader::ReadBit()
{
	guardSlow(FBitReader::ReadBit);
	BYTE Bit=0;
	SerializeBits( &Bit, 1 );
	return Bit;
	unguardSlow;
}
void FBitReader::Serialize( void* Dest, INT LengthBytes )
{
	guardSlow(FBitReader::Serialize);
	SerializeBits( Dest, LengthBytes*8 );
	unguardSlow;
}
BYTE* FBitReader::GetData()
{
	guardSlow(FBitReader::GetData);
	return &Buffer(0);
	unguardSlow;
}
UBOOL FBitReader::AtEnd()
{
	return ArIsError || Pos==Num;
}
void FBitReader::SetOverflowed()
{
	ArIsError = 1;
}
INT FBitReader::GetNumBytes()
{
	return (Num+7)>>3;
}
INT FBitReader::GetNumBits()
{
	return Num;
}
INT FBitReader::GetPosBits()
{
	return Pos;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
