/*=============================================================================
	TTFontImport.cpp: True-type Font Importing
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EditorPrivate.h"

// Windows includes.
#define STRICT
#undef TEXT
#undef HANDLE
#undef HINSTANCE
#include <windows.h>

/*------------------------------------------------------------------------------
	UTrueTypeFontFactory.
------------------------------------------------------------------------------*/

INT FromHex( TCHAR Ch )
{
	if( Ch>='0' && Ch<='9' )
		return Ch-'0';
	else if( Ch>='a' && Ch<='f' )
		return 10+Ch-'a';
	else if( Ch>='A' && Ch<='F' )
		return 10+Ch-'A';
	appErrorf(TEXT("Expecting digit, got character %i"),Ch);
	return 0;
}
void UTrueTypeFontFactory::StaticConstructor()
{
	guard(UTrueTypeFontFactory::StaticConstructor);

	SupportedClass		= UFont::StaticClass();
	bCreateNew			= 1;
	bShowPropertySheet	= 1;
	bShowCategories		= 0;
	AutoPriority        = -1;
	Description			= TEXT("Font Imported From TrueType");
	InContextCommand	= TEXT("Import TrueType Font");
	OutOfContextCommand	= TEXT("Import TrueType Font");
	FontName			= TEXT("MS Sans Serif");
	Height				= 16;
	USize				= 256;
	VSize				= 256;
	XPad				= 1;
	YPad				= 1;
	CharactersPerPage	= 64;
	Gamma				= 0.7;
	Count				= 256;
	AntiAlias			= 0;
	Chars				= TEXT("");
	new(GetClass(),TEXT("FontName"),          RF_Public)UStrProperty  (CPP_PROPERTY(FontName         ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Height"),            RF_Public)UIntProperty  (CPP_PROPERTY(Height           ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("USize"),             RF_Public)UIntProperty  (CPP_PROPERTY(USize            ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("VSize"),             RF_Public)UIntProperty  (CPP_PROPERTY(VSize            ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("XPad"),              RF_Public)UIntProperty  (CPP_PROPERTY(XPad             ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("YPad"),              RF_Public)UIntProperty  (CPP_PROPERTY(YPad             ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("CharactersPerPage"), RF_Public)UIntProperty  (CPP_PROPERTY(CharactersPerPage), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Count"),             RF_Public)UIntProperty  (CPP_PROPERTY(Count            ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Gamma"),             RF_Public)UFloatProperty(CPP_PROPERTY(Gamma            ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Chars"),             RF_Public)UStrProperty  (CPP_PROPERTY(Chars            ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("AntiAlias"),         RF_Public)UBoolProperty (CPP_PROPERTY(AntiAlias        ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("List"),              RF_Public)UStrProperty  (CPP_PROPERTY(List             ), TEXT(""), CPF_Edit );

	unguard;
}
UTrueTypeFontFactory::UTrueTypeFontFactory()
{
	guard(UTrueTypeFontFactory::UTrueTypeFontFactory);
	unguard;
}
UObject* UTrueTypeFontFactory::FactoryCreateNew
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	DWORD				Flags,
	UObject*			Context,
	FFeedbackContext*	Warn
)
{
	guard(UTrueTypeFontFactory::FactoryCreateNew);
	check(Class==UFont::StaticClass());
	INT i, j, n;

	// Create font and its texture.
	UFont* Font = new( InParent, Name, Flags )UFont;
	Font->CharactersPerPage = CharactersPerPage;

	TArray<BYTE> ChList;
	if( List!=TEXT("") )
	{
		Warn->Logf(TEXT("List <%s>:"),*List);
		const TCHAR* C=*List;
		while( *C )
		{
			INT Current = FromHex(C[0])*16 + FromHex(C[1]);
			C+=2;
			ChList.AddUniqueItem(Current);
			if( *C=='-' )
			{
				C++;
				INT Next=FromHex(C[0])*16 + FromHex(C[1]);
				C+=2;
				check(Next>Current);
				for( INT i=Current+1; i<=Next; i++ )
					ChList.AddUniqueItem(i);
			}
			if( *C==' ' )
				C++;
			else
				check(*C==0);
		}
		for( INT i=0; i<ChList.Num(); i++ )
			Warn->Logf(TEXT("   %02X"),ChList(i));
		Count=65536;
	}

	// Create the font.
	HFONT F = TCHAR_CALL_OS(
		CreateFont(-Height, 0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, AntiAlias ? ANTIALIASED_QUALITY : NONANTIALIASED_QUALITY, DEFAULT_PITCH, *FontName),
		CreateFontA(-Height, 0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, AntiAlias ? ANTIALIASED_QUALITY : NONANTIALIASED_QUALITY, DEFAULT_PITCH, TCHAR_TO_ANSI(*FontName)) );
	if( !F )
	{
		Warn->Logf( TEXT("CreateFont failed: %s"), appGetSystemErrorMessage() );
		return NULL;
	}

	// Create palette.
	UPalette* Palette = new( Font, NAME_None, RF_Public )UPalette;
	Palette->Colors.Empty();
	Palette->Colors.AddZeroed( NUM_PAL_COLORS );
	for( i=0; i<256; i++ )
	{
		Palette->Colors(i).R = AntiAlias ? i : i ? 255 : 0;
		Palette->Colors(i).G = AntiAlias ? i : i ? 255 : 0;
		Palette->Colors(i).B = AntiAlias ? i : i ? 255 : 0;
		Palette->Colors(i).A = AntiAlias ? i : i ? 255 : 0;
	}

	// Render all font characters into texture.
	INT      N         = 1+65536/Font->CharactersPerPage;
	INT*     X         =(INT    *)appAlloca(N*sizeof(INT    ));
	INT*     Y         =(INT    *)appAlloca(N*sizeof(INT    ));
	INT*     RowHeight =(INT    *)appAlloca(N*sizeof(INT    ));
	HBITMAP* B         =(HBITMAP*)appAlloca(N*sizeof(HBITMAP));
	HDC*     dc        =(HDC    *)appAlloca(N*sizeof(HDC    ));
	for( i=0; i<N; i++ )
	{
		X [i] = Y[i] = RowHeight[i] = 0;
		B [i] = NULL;
		dc[i] = NULL;
	}
	for( INT Ch=0; Ch<Count; Ch++ )
	{
		// Skip if this character isn't desired.
		if( Chars!=TEXT("") && (!Ch || !appStrchr(*Chars, Ch)) )
			continue;
		if( ChList.Num() && ChList.FindItemIndex(Ch/256)==INDEX_NONE )
			continue;

		// Create font page if needed.
		INT Page  = Ch / Font->CharactersPerPage;
		INT Index = Ch - Font->CharactersPerPage * Page;
		if( Page >= Font->Pages.Num() )
			Font->Pages.AddZeroed( 1 + Page - Font->Pages.Num() );
		FFontPage* PageInfo = &Font->Pages(Page);
		if( Index >= PageInfo->Characters.Num() )
			PageInfo->Characters.AddZeroed( 1 + Index - PageInfo->Characters.Num() );

		// Create Windows resources.
		if( !B[Page] )
		{
			dc[Page] = CreateCompatibleDC( NULL );
			if( !dc[Page] )
			{
				Warn->Logf( TEXT("CreateDC failed: %s"), appGetSystemErrorMessage() );
				return NULL;
			}
			if( AntiAlias )
			{
				BITMAPINFO* pBI = (BITMAPINFO*)appMalloc(sizeof(BITMAPINFO), TEXT("FontImport"));

				pBI->bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
				pBI->bmiHeader.biWidth         = USize;
				pBI->bmiHeader.biHeight        = VSize;
				pBI->bmiHeader.biPlanes        = 1;      //  Must be 1
				pBI->bmiHeader.biBitCount      = 24;
				pBI->bmiHeader.biCompression   = BI_RGB; 
				pBI->bmiHeader.biSizeImage     = 0;      
				pBI->bmiHeader.biXPelsPerMeter = 0;      
				pBI->bmiHeader.biYPelsPerMeter = 0;      
				pBI->bmiHeader.biClrUsed       = 0;      
				pBI->bmiHeader.biClrImportant  = 0;      

				void* pvBmp;
				B [Page] = CreateDIBSection((HDC)NULL, 
										pBI,
										DIB_RGB_COLORS,
										&pvBmp,
										NULL,
										0);  
				appFree( pBI );
			}
			else
			{
				B [Page] = CreateBitmap( USize, VSize, 1, 1, NULL);
			}

			if( !B[Page] )
			{
				Warn->Logf( TEXT("CreateBitmap failed: %s"), appGetSystemErrorMessage() );
				return NULL;
			}

			SelectObject( dc[Page], B[Page] );
			SelectObject( dc[Page], F );
			SetTextColor( dc[Page], 0x00000000 );
			SetBkColor( dc[Page], 0x00ffffff );
			HBRUSH White = CreateSolidBrush(0x00ffffff);
			RECT   r     = {0, 0, USize, VSize};
			FillRect( dc[Page], &r, White );
		}

		// Get text size.
		SIZE Size;
		TCHAR Tmp[5]={Ch,0};
		GetTextExtentPoint32( dc[Page], Tmp, 1, &Size );

		// If it doesn't fit right here, advance to next line.
		if( Size.cx + X[Page] + 2 > USize)
		{
			Y[Page]         = Y[Page] + RowHeight[Page] + YPad;
			RowHeight[Page] = 0;
			X[Page]         = 0;
		}

		// Set font character information.
		PageInfo->Characters(Index).StartU = X[Page];
		PageInfo->Characters(Index).StartV = Y[Page]+1;
		PageInfo->Characters(Index).USize  = Size.cx;
		PageInfo->Characters(Index).VSize  = Size.cy;

		// Draw character into font and advance.
		if( Size.cy > RowHeight[Page] )
			RowHeight[Page] = Size.cy;
		if( Y[Page]+RowHeight[Page]>VSize )
		{
			Warn->Logf( TEXT("Font vertical size exceeded maximum of %i at character %i"), VSize, Index );
			return NULL;
		}
		TextOut( dc[Page], X[Page], Y[Page], Tmp, 1 );
		X[Page] = X[Page] + Size.cx + XPad;
	}

	// Create textures.
	for( n=0; n<Font->Pages.Num(); n++ )
	{
		FFontPage* PageInfo = &Font->Pages(n);
		if( PageInfo->Characters.Num() )
		{
			// Create texture for page.
			PageInfo->Texture = new(Font)UTexture;
			PageInfo->Texture->Init( USize, 1<<appCeilLogTwo(Y[n]+RowHeight[n]) );
			PageInfo->Texture->PolyFlags = PF_Masked;
			PageInfo->Texture->PostLoad();
			PageInfo->Texture->Palette = Palette;

			// Copy bitmap data to texture page.
			for( i=0; i<PageInfo->Texture->USize; i++ )
			{
				for( j=0; j<PageInfo->Texture->VSize; j++ )
				{
					if( !AntiAlias )
					{
						PageInfo->Texture->Mips(0).DataArray(i + j * PageInfo->Texture->USize) = GetPixel( dc[n], i, j ) ? 0 : 1;
					}
					else
					{
						INT RValue = GetRValue(GetPixel( dc[n], i, j ));
						PageInfo->Texture->Mips(0).DataArray(i + j * PageInfo->Texture->USize) = 255 - RValue;
					}
				}
			}
		}
		if( dc[i] )
			DeleteDC( dc[i] );
		if( B[i] )
			DeleteObject( B[i] );
	}

	// Success.
	return Font;

	unguard;
}
IMPLEMENT_CLASS(UTrueTypeFontFactory);

/*------------------------------------------------------------------------------
	The end.
------------------------------------------------------------------------------*/
