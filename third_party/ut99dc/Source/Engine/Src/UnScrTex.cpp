/*=============================================================================
	UnScrTex.cpp: Unreal scripted texture class
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Jack Porter
============================================================================*/

#include "EnginePrivate.h"
#include "UnRender.h"


#include <stdlib.h>

/*-----------------------------------------------------------------------------
	Font Drawing Routines.
-----------------------------------------------------------------------------*/

//
// Draw a string of characters.
// - returns pixels drawn
//
static inline INT DrawString
(
	UScriptedTexture*	ScrTex, 
	UFont*			Font, 
	INT				DrawX, 
	INT				DrawY,
	const TCHAR*	Text, 
	UBOOL			bUseColor=0,
	BYTE			PaletteEntry=0 
)
{
	guardSlow(DrawString);

	FTextureInfo Info;

	INT LineX = 0;
	INT Page = -1;

	for( INT i=0; Text[i]; i++ )
	{
		INT Ch = (TCHARU)Text[i];

		INT NewPage = Ch / Font->CharactersPerPage;
		if( NewPage<Font->Pages.Num() && Font->Pages(NewPage).Texture )
		{
			INT        Index    = Ch - NewPage*Font->CharactersPerPage;
			FFontPage& PageInfo = Font->Pages(NewPage);
			if( Index<PageInfo.Characters.Num() )
			{
				if( NewPage!=Page )
				{
					if( Page!=-1 )
						Font->Pages(Page).Texture->Unlock( Info );
					Page = NewPage;
					PageInfo.Texture->Lock( Info, appSeconds(), 0, NULL );
				}
				FFontCharacter& Char = PageInfo.Characters( Index );

				if( bUseColor )
					ScrTex->DrawTile( DrawX+LineX, DrawY, Char.USize, Char.VSize, Char.StartU, Char.StartV, Char.USize, Char.VSize, PageInfo.Texture, &Info, 1, 1, PaletteEntry );
				else
					ScrTex->DrawTile( DrawX+LineX, DrawY, Char.USize, Char.VSize, Char.StartU, Char.StartV, Char.USize, Char.VSize, PageInfo.Texture, &Info, 1 );
				LineX += Char.USize;
			}
		}
	}

	if( Page!=-1 )
		Font->Pages(Page).Texture->Unlock( Info );

	return LineX;

	unguardSlow;
}

//
// Get a character's dimensions.
//
static inline void GetCharSize( UFont* Font, TCHAR InCh, INT& Width, INT& Height )
{
	guardSlow(GetCharSize);
	Width = 0;
	Height = 0;
	INT Ch    = (TCHARU)InCh;
	INT Page  = Ch / Font->CharactersPerPage;
	INT Index = Ch - Page * Font->CharactersPerPage;
	if( Page<Font->Pages.Num() && Index<Font->Pages(Page).Characters.Num() )
	{
		FFontCharacter& Char = Font->Pages(Page).Characters(Index);
		Width = Char.USize;
		Height = Char.VSize;
	}
	unguardSlow;
}


/*-----------------------------------------------------------------------------
	UScriptedTexture.
-----------------------------------------------------------------------------*/

UScriptedTexture::UScriptedTexture()
:	UTexture()
{	
	guard(UScriptedTexture::UScriptedTexture);

	// Set appropriate texture flags
	bParametric = 1;
	bRealtime   = 1;

	SourceTexture = NULL;
	OldSourceTex = NULL;
	LocalSourceBitmap = NULL;
	NotifyActor = NULL;
	PaletteMap = new TMap< UTexture*, TArray< BYTE > >;

	unguard;
}


void UScriptedTexture::Destroy()
{
	guard(UScriptedTexture::Destroy);
	delete PaletteMap;
	if( LocalSourceBitmap )
		delete LocalSourceBitmap;

	Super::Destroy();
	
	unguard;
}

void UScriptedTexture::Init( INT  InUSize, INT  InVSize )
{
	guard(UScriptedTexture::Init);

	// Init base class.
	UTexture::Init( InUSize, InVSize );

    unguard;
}

void UScriptedTexture::PostLoad()
{
	guard(UScriptedTexture::PostLoad);

	UTexture::PostLoad();

	if(SourceTexture)
	{
		INT UScaler;
		INT VScaler;

		FTextureInfo Info;
		SourceTexture->Lock( Info, LocalTime, 0, NULL );
		SourceTexture->Unlock( Info );

		if( LocalSourceBitmap ) delete LocalSourceBitmap;
		LocalSourceBitmap = new BYTE[ USize * VSize ]; 

		guard(CalcScaler);
		UScaler = UBits - SourceTexture->UBits;
		VScaler = VBits - SourceTexture->VBits;
		unguard;

		guard(CopyPalette);
		if( SourceTexture != OldSourceTex )
		{
			Palette = SourceTexture->Palette; // Make sure palette pointer gets updated.
		}
		OldSourceTex = SourceTexture;
		unguard;

		guard(CopyData);
		BYTE* SourceMapAddr = &SourceTexture->Mips(0).DataArray(0);

		// Copy bitmap
		for( INT V=0; V<VSize; V++)
		{
			for( INT U=0; U<USize; U++)
			{	
				LocalSourceBitmap[ U + ( V * USize )] = 
					SourceMapAddr[ ( U >> UScaler ) + ((V >> VScaler) << SourceTexture->UBits) ];
			}
		}					
		unguard;
	}

	unguard;
}

void UScriptedTexture::Tick(FLOAT DeltaSeconds) 
{
	BYTE* DestBitmap;

	guard(UScriptedTexture::Tick);

	LocalTime = appSeconds();

	Super::Tick(DeltaSeconds); 
	DestBitmap = &Mips(0).DataArray(0);

	check(DestBitmap);

	if(LocalSourceBitmap)
		appMemcpy(DestBitmap, LocalSourceBitmap, USize*VSize);

	if(NotifyActor)
		NotifyActor->eventRenderTexture(this);

	unguard;
}

void inline UScriptedTexture::DrawTile( FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, UTexture *Tex, FTextureInfo *Info, UBOOL bMasked, UBOOL bUseColor, BYTE ColorIndex )
{
	guardSlow(UScriptedTexture::execDrawTile);

	BYTE* SourceBitmap = Info->Mips[0]->DataPtr;
	BYTE* DestBitmap = &Mips(0).DataArray(0);

	if( X+XL < 0 || Y+YL < 0 || X > USize || Y > VSize)
		return;

	TArray<BYTE>* PalMap = NULL;
	
	if(!bUseColor)
	{
		PalMap = PaletteMap->Find(Tex);

		if(!PalMap)
		{
			TArray<BYTE> Temp(256);

			for(INT i=0;i<256;i++)
				Temp(i) = Palette->BestMatch(Tex->Palette->Colors(i), 0);

			PalMap = &PaletteMap->Set(Tex, Temp);
		}
	}

	INT DX1 = Min<INT>( Max<INT>( appRound(X), 0 ), USize-1 );
	U += ((FLOAT)DX1 - X) * (UL/XL);
	INT DX2 = Min( appRound(XL+X), USize-1 );
	INT DY1 = Min<INT>( Max<INT>( appRound(Y), 0 ), VSize-1 );
	V += ((FLOAT)DY1 - Y) * (VL/YL);
	INT DY2 = Min( appRound(YL+Y), VSize-1 );

	DWORD SrcUMask = Tex->USize-1; // Can assume power-of-two texture sizes.
	DWORD SrcVMask = Tex->VSize-1;

	INT DestBase = DY1 * USize;     

	INT XFixStep  = appRound( UL/XL * (FLOAT)(1 << 19) );
	INT FixU = appRound( U * (FLOAT)(1 << 19) ) - XFixStep;
	INT YFixStep  = appRound( VL/YL * (FLOAT)(1 << 19) );
	INT FixV = appRound( V * (FLOAT)(1 << 19) ) - YFixStep;

	INT YFixStart = FixV; 
	
	if ( bUseColor && bMasked )
	{       
		// Masked monochrome blitting using ColorIndex.
		for( INT DY=DY1; DY < DY2; DY++)
		{
			INT XFixStart = FixU;
			INT SrcYIndex = (SrcVMask & ((YFixStart+=YFixStep) >> 19)) * Tex->USize;
			for( INT DX=DX1; DX < DX2; DX++)
			{			       
				if( SourceBitmap[ SrcYIndex + (SrcUMask & ((XFixStart+=XFixStep) >> 19)) ] !=0 )
				{
					DestBitmap[ DestBase + DX ] = ColorIndex;
				}
			}
			DestBase += USize;
		}
	}
	else if ( !bMasked ) 
	{       
		// Unmasked textured blitting.
		for( INT DY=DY1; DY < DY2; DY++ )
		{
			INT XFixStart = FixU;
			INT SrcYIndex = (SrcVMask & ((YFixStart+=YFixStep) >> 19)) * Tex->USize;
			for( INT DX=DX1; DX < DX2; DX++ )
			{			       
				DestBitmap[ DestBase + DX ] = (*PalMap)(SourceBitmap[ SrcYIndex + (SrcUMask & ((XFixStart+=XFixStep) >> 19)) ]);
			}
			DestBase += USize;
		}	       
	}
	else 
	{       
		// Masked textured blitting. 
		for( INT DY=DY1; DY < DY2; DY++ )
		{
			INT XFixStart = FixU;
			INT SrcYIndex = (SrcVMask & ((YFixStart+=YFixStep) >> 19)) * Tex->USize;
			for( INT DX=DX1; DX < DX2; DX++ )
			{			       
				INT SourcePix = SourceBitmap[ SrcYIndex + (SrcUMask & ((XFixStart+=XFixStep) >> 19)) ];
				if( SourcePix )
				{
					DestBitmap[ DestBase + DX ] = (*PalMap)(SourcePix);
				}
			}
			DestBase += USize;
		}
	}

	unguardSlow;
}


void UScriptedTexture::execDrawText( FFrame& Stack, RESULT_DECL )
{
	guard(UScriptedTexture::execDrawText);
	P_GET_FLOAT(X);
	P_GET_FLOAT(Y);
	P_GET_STR(InText);
	P_GET_OBJECT(UFont, Font);
	P_FINISH;

	if( !Font )
	{
		Stack.Logf( TEXT("DrawText: No font") );
		return;
	}

	DrawString(this, Font, (INT) X, (INT) Y, *InText);
	
	unguardexec;
}
IMPLEMENT_FUNCTION( UScriptedTexture, 472, execDrawText );

void UScriptedTexture::execReplaceTexture( FFrame& Stack, RESULT_DECL )
{
	guard(UScriptedTexture::execReplaceTexture);
	P_GET_OBJECT(UTexture,Tex);
	P_FINISH;
	if( !Tex )
	{
		Stack.Logf( TEXT("ReplaceTexture: Missing Texture") );
		return;
	}

	if(USize != Tex->USize || VSize != Tex->VSize)
	{
		Stack.Logf( TEXT("ReplaceTexture: Dimensions are different") );
		return;
	}

	// Make sure the texture is loaded
	UTexture* T = Tex->Get(LocalTime);
	FTextureInfo Info;
	T->Lock( Info, LocalTime, 0, NULL );
	BYTE* SourceBitmap = Info.Mips[0]->DataPtr;
	BYTE* DestBitmap = &Mips(0).DataArray(0);
	appMemcpy(DestBitmap, SourceBitmap, USize*VSize);
	T->Unlock( Info );
	
	unguardexec;
}
IMPLEMENT_FUNCTION( UScriptedTexture, 475, execReplaceTexture );

void UScriptedTexture::execDrawTile( FFrame& Stack, RESULT_DECL )
{
	guard(UScriptedTexture::execDrawTile);
	P_GET_FLOAT(X);
	P_GET_FLOAT(Y);
	P_GET_FLOAT(XL);
	P_GET_FLOAT(YL);
	P_GET_FLOAT(U);
	P_GET_FLOAT(V);
	P_GET_FLOAT(UL);
	P_GET_FLOAT(VL);
	P_GET_OBJECT(UTexture,Tex);
	P_GET_UBOOL(bMasked);
	P_FINISH;
	if( !Tex )
	{
		Stack.Logf( TEXT("DrawTile: Missing Texture") );
		return;
	}

	// Make sure the texture is loaded
	UTexture* T = Tex->Get(LocalTime);
	FTextureInfo Info;
	T->Lock( Info, LocalTime, 0, NULL );
	DrawTile( X, Y, XL, YL, U, V, UL, VL, Tex->Get(LocalTime), &Info, bMasked );
	T->Unlock( Info );

	unguardexec;
}
IMPLEMENT_FUNCTION( UScriptedTexture, 473, execDrawTile );

void UScriptedTexture::execDrawColoredText( FFrame& Stack, RESULT_DECL )
{
	guard(UScriptedTexture::execDrawText);
	P_GET_FLOAT(X);
	P_GET_FLOAT(Y);
	P_GET_STR(InText);
	P_GET_OBJECT(UFont, Font);
	P_GET_STRUCT(FColor, Color);
	P_FINISH;
	if( !Font )
	{
		Stack.Logf( TEXT("DrawText: No font") );
		return;
	}

	BYTE PaletteEntry = Palette->BestMatch(Color, 0);
	DrawString(this, Font, (INT) X, (INT) Y, *InText, 1, PaletteEntry);
	unguardexec;
}
IMPLEMENT_FUNCTION( UScriptedTexture, 474, execDrawColoredText );

void UScriptedTexture::execTextSize( FFrame& Stack, RESULT_DECL )
{
	guard(UScriptedTexture::execTextSize);
	P_GET_STR(InText);
	P_GET_FLOAT_REF(XL);
	P_GET_FLOAT_REF(YL);
	P_GET_OBJECT(UFont, Font);
	P_FINISH;
	if( !Font )
	{
		Stack.Logf( TEXT("TextSize: No font") );
		return;
	}

	INT XLi = 0;
	INT YLi = 0;
	INT W, H;

	for( INT i=0; (*InText)[i]; i++)
	{
		GetCharSize( Font, (*InText)[i], W, H );
		
		XLi += W;
		if(YLi < H)
			YLi = H;	
	}
	
	*XL = XLi;
	*YL = YLi;

	unguardexec;
}
IMPLEMENT_FUNCTION( UScriptedTexture, 476, execTextSize );

IMPLEMENT_CLASS(UScriptedTexture);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

