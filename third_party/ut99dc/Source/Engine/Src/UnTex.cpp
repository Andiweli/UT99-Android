/*=============================================================================
	UnTex.cpp: Unreal texture loading/saving/processing functions.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnRender.h"

#ifdef WIN32
#include "UnDDraw.h"
#include "Palette.h"
#include "S3tc.h"
#endif

/*-----------------------------------------------------------------------------
	UBitmap.
-----------------------------------------------------------------------------*/

UBitmap::UBitmap()
{
	guard(UBitmap::UBitmap);

	Format			= TEXF_P8;
	Palette			= NULL;
	UBits			= 0;
	VBits			= 0;
	USize			= 0;
	VSize			= 0;
	MipZero			= FColor(64,128,64,0);
	MaxColor		= FColor(255,255,255,255);
	LastUpdateTime  = appSeconds();

	unguard;
}
UClient* UBitmap::__Client=NULL;
IMPLEMENT_CLASS(UBitmap);

/*-----------------------------------------------------------------------------
	FColor functions.
-----------------------------------------------------------------------------*/

//
// Convert byte hue-saturation-brightness to floating point red-green-blue.
//
FPlane ENGINE_API FGetHSV( BYTE H, BYTE S, BYTE V )
{
	FLOAT Brightness = V * 1.4 / 255.0;
	Brightness *= 0.70/(0.01 + appSqrt(Brightness));
	Brightness  = Clamp(Brightness,(FLOAT)0.0,(FLOAT)1.0);
	FVector Hue = (H<86) ? FVector((85-H)/85.f,(H-0)/85.f,0) : (H<171) ? FVector(0,(170-H)/85.f,(H-85)/85.f) : FVector((H-170)/85.f,0,(255-H)/84.f);
	return FPlane( (Hue + S/255.0 * (FVector(1,1,1) - Hue)) * Brightness, 0 );
}

/*-----------------------------------------------------------------------------
	Texture locking and unlocking.
-----------------------------------------------------------------------------*/

//
// Update texture.
//
void UTexture::Update( DOUBLE CurrentTime )
{
	guard(UTexture::Update);
	if( CurrentTime != LastUpdateTime )
	{
		if( bRealtime )
			bRealtimeChanged = 1;
		Tick( CurrentTime - LastUpdateTime);
		LastUpdateTime = CurrentTime;
	}
	unguard;
}

//
// Lock a texture for rendering.
//
void UTexture::Lock( FTextureInfo& TextureInfo, DOUBLE CurrentTime, INT LOD, URenderDevice* RenDev )
{
	guard(UTexture::Lock);

	// See if format needs translating.
	UBOOL UseComp = bHasComp && RenDev && RenDev->SupportsTC && CompFormat==TEXF_DXT1;
	TArray<FMipmap>& WhichMips = UseComp ? CompMips : Mips;

	// Adjust LOD.
	if( LOD==-1 )
	{
		LOD = 0;
		if( __Client )
			LOD = __Client->TextureLODSet[LODSet];
		if( RenDev )
			LOD = Max(LOD,RenDev->RecommendedLOD);
	}
	LOD = TextureInfo.LOD = Min<INT>( LOD, Min<INT>(WhichMips.Num()-1,MAX_TEXTURE_LOD-1) );

	// Set locked texture info.
	TextureInfo.Texture         = this;
	TextureInfo.Pan				= FVector( 0, 0, 0 );
	TextureInfo.MaxColor		= &MaxColor;
	FLOAT ScaleFactor           = Mips(0).USize / (FLOAT)WhichMips(LOD).USize;
	TextureInfo.UScale          = Scale * ScaleFactor;
	TextureInfo.VScale          = Scale * ScaleFactor;
	TextureInfo.PaletteCacheID	= MakeCacheID( CID_RenderPalette, Palette );
	TextureInfo.Palette			= GetColors();
	TextureInfo.CacheID			= MakeCacheID( (ECacheIDBase)(CID_RenderTexture+LOD), this );
	TextureInfo.USize			= TextureInfo.UClamp			= WhichMips(LOD).USize;
	TextureInfo.VSize			= TextureInfo.VClamp			= WhichMips(LOD).VSize;
	TextureInfo.NumMips			= WhichMips.Num() - LOD;
	TextureInfo.Format          = (ETextureFormat)(UseComp ? CompFormat : Format);
	if( !bParametric && (!RenDev || !RenDev->PrefersDeferredLoad) )
		for( INT i=LOD; i<WhichMips.Num(); i++ )
			WhichMips(i).DataArray.Load();
	for( INT i=LOD; i<WhichMips.Num(); i++ )
	{
		WhichMips(i).DataPtr    = &WhichMips(i).DataArray(0);
		TextureInfo.Mips[i-LOD] = &WhichMips(i);
	}

	// Update the texture if time has passed.
	if( CurrentTime != 0.0 )
		Update( CurrentTime );

	// Reset the texture flags.
	TextureInfo.bHighColorQuality   = bHighColorQuality;
	TextureInfo.bHighTextureQuality = bHighTextureQuality;
	TextureInfo.bRealtime           = bRealtime;
	TextureInfo.bParametric         = bParametric;
	TextureInfo.bRealtimeChanged    = bRealtimeChanged;
	bRealtimeChanged = 0;

	// Success.
	unguardobj;
}
void UTexture::Unlock( FTextureInfo& TextureInfo )
{}
INT UTexture::DefaultLOD()
{
	return __Client ? Min(__Client->TextureLODSet[LODSet],Mips.Num()-1) : 0;
}

/*---------------------------------------------------------------------------------------
	Texture animation.
---------------------------------------------------------------------------------------*/

//
// Continuous time update.
//
// To provide your own continuous-time update, override this and do not call this
// base class function in your child class.
//
void UTexture::Tick( FLOAT DeltaSeconds )
{
	guard(UTexture::Tick);

	// Prime the texture.
	while( PrimeCurrent < PrimeCount )
	{
		PrimeCurrent++;
		ConstantTimeTick();
	}

	// Update.
	if( MaxFrameRate == 0.0 )
	{
		// Constant update.
		ConstantTimeTick();
	}
	else
	{
		// Catch up.
		FLOAT MinTime  = 1.f/Clamp(MaxFrameRate,0.01f,100.0f);
		FLOAT MaxTime  = 1.f/Clamp(MinFrameRate,0.01f,100.0f);
		Accumulator   += DeltaSeconds;
		if( Accumulator < MinTime )
		{
			// Skip update.
		}
		else if( Accumulator < MaxTime )
		{
			// Normal update.
			ConstantTimeTick();
			Accumulator = 0;
		}
		else
		{
			ConstantTimeTick();
			Accumulator -= MaxTime;
			if( Accumulator > MaxTime )
				Accumulator = MaxTime;
		}
	}
	unguardobj;
}

//
// Discrete time update.
//
// To provide your own discrete-time update, override this and do not call this
// base class function in your child class.
//
void UTexture::ConstantTimeTick()
{
	guard(UTexture::ConstantTimeTick);

	// Simple cyclic animation.
	if( !AnimCur ) AnimCur = this;
	AnimCur = AnimCur->AnimNext;
	if( !AnimCur ) AnimCur = this;

	unguardobj;
}

/*---------------------------------------------------------------------------------------
	UTexture object implementation.
---------------------------------------------------------------------------------------*/

//
// PCX file header.
//
class FPCXFileHeader
{
public:
	BYTE	Manufacturer;		// Always 10.
	BYTE	Version;			// PCX file version.
	BYTE	Encoding;			// 1=run-length, 0=none.
	BYTE	BitsPerPixel;		// 1,2,4, or 8.
	_WORD	XMin;				// Dimensions of the image.
	_WORD	YMin;				// Dimensions of the image.
	_WORD	XMax;				// Dimensions of the image.
	_WORD	YMax;				// Dimensions of the image.
	_WORD	hdpi;				// Horizontal printer resolution.
	_WORD	vdpi;				// Vertical printer resolution.
	BYTE	OldColorMap[48];	// Old colormap info data.
	BYTE	Reserved1;			// Must be 0.
	BYTE	NumPlanes;			// Number of color planes (1, 3, 4, etc).
	_WORD	BytesPerLine;		// Number of bytes per scanline.
	_WORD	PaletteType;		// How to interpret palette: 1=color, 2=gray.
	_WORD	HScreenSize;		// Horizontal monitor size.
	_WORD	VScreenSize;		// Vertical monitor size.
	BYTE	Reserved2[54];		// Must be 0.
};

// Headers found at the beginning of a ".BMP" file.
#if _MSC_VER
#pragma pack(push,1)
#endif
struct FBitmapFileHeader
{
    _WORD  bfType;          // Always "BM".
    DWORD bfSize;          // Size of file in bytes.
    _WORD  bfReserved1;     // Ignored.
    _WORD  bfReserved2;     // Ignored.
    DWORD bfOffBits;       // Offset of bitmap in file.
};
#if _MSC_VER
#pragma pack(pop)
#endif

#if _MSC_VER
#pragma pack(push,1)
#endif
struct FBitmapInfoHeader
{
    DWORD biSize;          // Size of header in bytes.
    DWORD biWidth;         // Width of bitmap in pixels.
    DWORD biHeight;        // Height of bitmap in pixels.
    _WORD  biPlanes;        // Number of bit planes (always 1).
    _WORD  biBitCount;      // Number of bits per pixel.
    DWORD biCompression;   // Type of compression (ingored).
    DWORD biSizeImage;     // Size of pixel array (usually 0).
    DWORD biXPelsPerMeter; // Ignored.
    DWORD biYPelsPerMeter; // Ignored.
    DWORD biClrUsed;       // Number of colors (usually 0).
    DWORD biClrImportant;  // Important colors (usually 0).
};
#if _MSC_VER
#pragma pack(pop)
#endif

UTexture::UTexture()
{
	guard(UTexture::UTexture);

	Diffuse			= 1.0;
	Specular		= 1.0;
	Alpha           = 0.0;
	Scale			= 1.0;
	Friction		= 1.0;
	MipMult			= 1.0;

	unguardobj;
}
static void SerializeMips( UTexture* Texture, FArchive& Ar, TArray<FMipmap>& Mips )
{
	guard(SerializeMips);
	Mips.CountBytes( Ar );
	if( Ar.IsLoading() )
	{
		// Load array.
		INT NewNum;
		Ar << AR_INDEX(NewNum);
		Mips.Empty( NewNum );
		INT LOD = Min<INT>( UTexture::__Client ? UTexture::__Client->TextureLODSet[Texture->LODSet] : 0, NewNum-1 );
		for( INT i=0; i<NewNum; i++ )
		{
			UBOOL SavedLazyLoad = GLazyLoad;
#ifdef PLATFORM_LOW_MEMORY
			// Force lazy load for all mips on low memory platforms
			GLazyLoad = 1;
#else
			if( i<LOD )
				GLazyLoad = 1;
#endif
			FMipmap* NewMip = new(Mips)FMipmap;
			Ar << *NewMip;

			// On Dreamcast, empty mip data after loading to save RAM
			// Skip scripted textures which need their data for dynamic updates
#ifdef PLATFORM_DREAMCAST
			if( !GLazyLoad && NewMip->DataArray.Num() > 0 && Texture && !Texture->IsA(UScriptedTexture::StaticClass()) )
			{
				NewMip->DataArray.Empty();
				NewMip->DataPtr = NULL;
			}
#endif

			GLazyLoad = SavedLazyLoad;
		}
	}
	else
	{
		// Save array.
		INT Num = Mips.Num();
		Ar << AR_INDEX(Num);
		for( INT i=0; i<Mips.Num(); i++ )
			Ar << Mips( i );
	}
	unguard;
}
void UTexture::Serialize( FArchive& Ar )
{
	guard(UTexture::Serialize);
	Super::Serialize( Ar );

	// Empty algorithmic textures.
	if( (Ar.IsSaving() || Ar.IsLoading()) && bParametric )
		for( INT i=0; i<Mips.Num(); i++ )
			Mips(i).DataArray.Empty();

	// Serialize the mipmaps.
	SerializeMips( this, Ar, Mips );
	if( bHasComp )
		SerializeMips( this, Ar, CompMips );

	// Refill algorithmic textures.
	if( (Ar.IsSaving() || Ar.IsLoading()) && bParametric )
	{
		for( INT i=0; i<Mips.Num(); i++ )
		{
			INT Size = Mips(i).USize * Mips(i).VSize;
			Mips(i).DataArray.Empty    ( Size );
			Mips(i).DataArray.AddZeroed( Size );
		}
	}

	unguard;
}
void UTexture::PostLoad()
{
	guard(UTexture::PostLoad);
	Super::PostLoad();

	// Handle post editing.
	if( !Palette )
	{
		// Make sure the palette is valid.
		Palette = new(GetOuter())UPalette;
		for( INT i=0; i<256; i++ )
			new(Palette->Colors)FColor(i,i,i,0);
	}
	UClamp = Clamp<INT>( UClamp, 0, USize );
	VClamp = Clamp<INT>( VClamp, 0, VSize );

	// Init animation.
	Accumulator = 0;
	LastUpdateTime = appSeconds();

	unguardobj;
}
void UTexture::Destroy()
{
	guard(UTexture::Destroy);
	Super::Destroy();
	unguard;
}
IMPLEMENT_CLASS(UTexture);

/*---------------------------------------------------------------------------------------
	UTexture mipmap generation.
---------------------------------------------------------------------------------------*/

//
// Initialize the texture for a given resolution, single mipmap.
//
void UTexture::Init( INT InUSize, INT InVSize )
{
	guard(UTexture::Init);
	check(!(USize & (USize-1)));
	check(!(VSize & (VSize-1)));

	// Allocate space.
	USize = UClamp = InUSize;
	VSize = VClamp = InVSize;
    UBits = appCeilLogTwo(USize);
    VBits = appCeilLogTwo(VSize);

	// Allocate first mipmap.
	Mips.Empty();
	if( Format==TEXF_RGBA8 )
		new(Mips)FMipmap(UBits,VBits, USize * VSize * 4);
	else
		new(Mips)FMipmap(UBits,VBits);
	Mips(0).Clear();

	unguardobj;
}

//
// Generate all mipmaps for a texture.  Call this after setting the texture's palette.
// Erik changed: converted to simpler 2x2 box filter with 24-bit color intermediates.
//
void UTexture::CreateMips( UBOOL FullMips, UBOOL Downsample )
{
#ifndef PLATFORM_LOW_MEMORY

	guard(UTexture::CreateMips);

	check(Palette!=NULL);
	FColor* Colors = GetColors(); 

	// Create average color.
	FPlane C(0,0,0,0);
	if( Format == TEXF_P8 )
	{
		for( INT i=0; i<Mips(0).DataArray.Num(); i++ )
			C += Colors[Mips(0).DataArray(i)].Plane();
	}
	MipZero = FColor(C/Mips(0).DataArray.Num());

	// Empty any mipmaps.
	if( Mips.Num() > 1 )
		Mips.Remove( 1, Mips.Num()-1 );

	// Allocate mipmaps.
	if( Format == TEXF_RGBA8 )
	{
		// Decompress the texture to create the decompressed mip set.
		Compress( TEXF_DXT1, FullMips );
		bHasComp = 1;
		Decompress( TEXF_P8 );
	}
	else if( FullMips )
	{
		// Format == TEXF_P8.
		while( UBits-Mips.Num()>=0 || VBits-Mips.Num()>=0 )
		{
			INT Num = Mips.Num();
			new(Mips)FMipmap( Max(UBits-Num,0), Max(VBits-Num,0) );
		}
		if( Downsample )
		{
			// Build each mip from the next-larger mip.
			FColor* TrueSource = NULL;
			FColor* TrueDest   = NULL;		
			for( INT MipLevel=1; MipLevel<Mips.Num(); MipLevel++ )
			{
				FMipmap&  Src        = Mips(MipLevel-1);
				FMipmap&  Dest       = Mips(MipLevel  );
				INT       ThisUTile  = Src.USize;
				INT       ThisVTile  = Src.VSize;

				// Cascade down the mip sequence with truecolor source and destination textures.			
				TrueSource = TrueDest; // Last destination is current source..
				TrueDest = new(TEXT("FColor"))FColor[Src.USize * Src.VSize];
				if( !(PolyFlags & PF_Masked) )
				{
					// Source coordinate masking important for degenerate mipmap sizes.
					DWORD MaskU = (ThisUTile-1);
					DWORD MaskV = (ThisVTile-1);
					INT   UD    = (1 & MaskU);
					INT   VD    = (1 & MaskV)*ThisUTile;

					// Non-masked mipmap.
					for( INT V=0; V<Dest.VSize; V++ )
					{
						for( INT U=0; U<Dest.USize; U++)
						{
							// Get 4 pixels from a one-higher-level mipmap.
							INT TexCoord = U*2 + V*2*ThisUTile;
							FVector C(0,0,0);
							if( TrueSource )
							{	
								C += TrueSource[ TexCoord +  0 +  0 ].Plane();
								C += TrueSource[ TexCoord + UD +  0 ].Plane();
								C += TrueSource[ TexCoord +  0 + VD ].Plane();
								C += TrueSource[ TexCoord + UD + VD ].Plane();
							}
							else
							{
								C += Colors[ Src.DataArray( TexCoord +  0 +  0 ) ].Plane();
								C += Colors[ Src.DataArray( TexCoord + UD +  0 ) ].Plane();
								C += Colors[ Src.DataArray( TexCoord +  0 + VD ) ].Plane(); 
								C += Colors[ Src.DataArray( TexCoord + UD + VD ) ].Plane();
							}
							FColor MipTexel;
							TrueDest[V*Dest.USize+U] = MipTexel = FColor( C/4.0f );
							Dest.DataArray(V*Dest.USize+U) = Palette->BestMatch( MipTexel, 0 );
						}
					}
				}
				else
				{
					// Masked mipmap.
					DWORD MaskU = (ThisUTile-1);
					DWORD MaskV = (ThisVTile-1);
					for( INT V=0; V<Dest.VSize; V++ )
					{
						for( INT U=0; U<Dest.USize; U++) 
						{
							INT F = 0;
							BYTE B;
							FPlane C(0,0,0,0);
							INT TexCoord = V*2*ThisUTile + U*2;
							for( INT I=0; I<2; I++ )
							{
								for( INT J=0; J<2; J++ )
								{
									B = Src.DataArray(TexCoord + (I&MaskU) + (J&MaskV)*ThisUTile);
									if( B )
									{
										F++;
										if( TrueSource )
											C += TrueSource[TexCoord + (I&MaskU) + (J&MaskV)*ThisUTile].Plane();
										else
											C += Colors[B].Plane();
									}
								}
							}						

							// One masked texel or less becomes a non-masked texel.
							if( F >= 2 )
							{
								FColor MipTexel = TrueDest[V*Dest.USize+U] = FColor( C / F );
								Dest.DataArray(V*Dest.USize+U) = Palette->BestMatch( MipTexel, 1 );
							}
							else
							{
								TrueDest[V*Dest.USize+U] = FColor(0,0,0);
								Dest.DataArray(V*Dest.USize+U) = 0;
							}
						}
					}
				}
				if( TrueSource )
					delete TrueSource; 
			}
			if( TrueDest )
				delete TrueDest;
		}
	}
	unguardobj;
#endif
}

//
// Set the texture's MaxColor and MinColor so that the texture can be normalized
// when converting to lower color resolutions like RGB 5-5-5 for hardware
// rendering.
//
void UTexture::CreateColorRange()
{
	guard(UTexture::CreateColorRange);
	if( Palette )
	{
		MaxColor = FColor(0,0,0,0);
		FColor* Colors = GetColors();
		for( INT i=0; i<Mips.Num(); i++ )
		{
			for( INT j=0; j<Mips(i).DataArray.Num(); j++ )
			{
				FColor& Color = Colors[Mips(i).DataArray(j)];
				MaxColor.R    = ::Max(MaxColor.R, Color.R);
				MaxColor.G    = ::Max(MaxColor.G, Color.G);
				MaxColor.B    = ::Max(MaxColor.B, Color.B);
				MaxColor.A    = ::Max(MaxColor.A, Color.A);
			}
		}
	}
	else MaxColor = FColor(255,255,255,255);
	unguardobj;
}

/*---------------------------------------------------------------------------------------
	UTexture functions.
---------------------------------------------------------------------------------------*/

//
// Clear the texture.
//
void UTexture::Clear( DWORD ClearFlags )
{
	guard(UTexture::Clear);
	if( ClearFlags & TCLEAR_Bitmap )
		for( INT i=0; i<Mips.Num(); i++ )	
			Mips(i).Clear();
	unguardobj;
}

/*---------------------------------------------------------------------------------------
	UPalette implementation.
---------------------------------------------------------------------------------------*/

UPalette::UPalette()
{
	guard(UPalette::UPalette);
	unguard;
}
void UPalette::Serialize( FArchive& Ar )
{
	guard(UPalette::Serialize);
	Super::Serialize( Ar );

	Ar << Colors;
	if( Ar.Ver()<66 )
		for( INT i=0; i<Colors.Num(); i++ )
			Colors(i).A = 255;

	unguard;
}
IMPLEMENT_CLASS(UPalette);

/*-----------------------------------------------------------------------------
	UPalette general functions.
-----------------------------------------------------------------------------*/

//
// Adjust a regular (imported) palette.
//
void UPalette::FixPalette()
{
	guard(UPalette::FixPalette);

	FColor TempColors[256];
	for( int i=0; i<256; i++ )
		TempColors[i] = Colors(0);

	for( int iColor=0; iColor<8; iColor++ )
	{
		int iStart = (iColor==0) ? 1 : 32*iColor;
		for( int iShade=0; iShade<28; iShade++ )
			TempColors[16 + iColor + (iShade<<3)] = Colors(iStart + iShade);

	}
	INT i;
	for( i=0; i<256; i++ )
	{
		Colors(i) = TempColors[i];
		Colors(i).A = i+0x10;
	}
	Colors(0).A=0;

	unguardobj;
}

//
// Find closest palette color matching a given RGB value.
//
BYTE UPalette::BestMatch( FColor Color, INT First )
{
	guard(UPalette::BestMatch);
	INT BestDelta         = MAXINT;
	INT BestUnscaledDelta = MAXINT;
	INT BestColor         = First;
	INT TexRed            = Color.R;
	INT TexBlue           = Color.B;
	INT TexGreen          = Color.G;
	for( INT TestColor=First; TestColor<NUM_PAL_COLORS; TestColor++ )
	{
		// By comparing unscaled green, saves about 4 out of every 5 full comparisons.
		FColor* ColorPtr   = &Colors(TestColor);
		INT     GreenDelta = Square(ColorPtr->G - TexGreen);

		// Same as comparing 8*GreenDelta with BestDelta.
		if( GreenDelta < BestUnscaledDelta )
		{
			INT Delta = 
			(
				8 * GreenDelta                     +
				4 * Square(ColorPtr->R - TexRed  ) +
				1 * Square(ColorPtr->B - TexBlue )
			);
			if( Delta < BestDelta ) 
			{
				BestColor         = TestColor;
				BestDelta         = Delta;
				BestUnscaledDelta = (Delta + 7) >> 3; 
			}
		}
	}
	return BestColor;
	unguardobj;
}

//
// Sees if this palette is a duplicate of an existing palette.
// If it is, deletes this palette and returns the existing one.
// If not, returns this palette.
//
UPalette* UPalette::ReplaceWithExisting()
{
	guard(UPalette::ReplaceWithExisting);
	for( TObjectIterator<UPalette>It; It; ++It )
	{
		if( *It!=this && It->GetOuter()==GetOuter() )
		{
			FColor* C1 = &Colors(0);
			FColor* C2 = &It->Colors(0);
			INT i;
			for( i=0; i<NUM_PAL_COLORS; i++ )
				if( *C1++ != *C2++ ) break;
			if( i == NUM_PAL_COLORS )
			{
				debugf( NAME_Log, TEXT("Replaced palette %s with %s"), GetName(), It->GetName() );
				delete this;
				return *It;
			}
		}
	}
	return this;
	unguardobj;
}

/*-----------------------------------------------------------------------------
	DXT functions.
-----------------------------------------------------------------------------*/

typedef	struct {BYTE rgba[4];} COLOR;

#ifdef WIN32

//
// Compress the texture, with mip-levels if required
//
UBOOL UTexture::Compress( ETextureFormat InFormat, UBOOL Mipmap )
{
	guard(UTexture::Compress);

	INT i;
	FLOAT weight[3]={309,609,82}, inputweight[3];
	for( i=0; i<3; i++ )
		inputweight[i] = weight[i]/1000;

	DDSURFACEDESC descIn;
	descIn.dwSize=sizeof(descIn);
	descIn.dwFlags=DDSD_WIDTH|DDSD_HEIGHT|DDSD_LPSURFACE|DDSD_PITCH|DDSD_PIXELFORMAT;
	descIn.ddpfPixelFormat.dwFlags=DDPF_RGB;
	descIn.ddpfPixelFormat.dwRGBBitCount=32;
	descIn.ddpfPixelFormat.dwRBitMask=0xff0000;
	descIn.ddpfPixelFormat.dwGBitMask=0x00ff00;
	descIn.ddpfPixelFormat.dwBBitMask=0x0000ff;

	// Make 32-bit RGBA.
	DDSURFACEDESC descOut;
	descOut.dwSize=sizeof(descOut);
	TArray<COLOR> Buffer(USize*VSize);
	descIn.lpSurface = &Buffer(0);
	guard(CopyData);
	if( Format==TEXF_RGBA8 )
		appMemcpy( &Buffer(0), &Mips(0).DataArray(0), USize*VSize*4 );
	else
		for( INT i=0; i<USize*VSize; i++ )
			*(DWORD*)&Buffer(i) = Palette->Colors(Mips(0).DataArray(i)).TrueColor();
	unguard;

	// Copy to compressed mips.
	descIn.dwWidth=USize;
	descIn.dwHeight=VSize;
	descIn.lPitch=USize*4;
	DWORD dwSize = S3TCgetEncodeSize(&descIn, S3TC_ENCODE_RGB_FULL|S3TC_ENCODE_ALPHA_NONE);
	CompMips.Empty();
	new(CompMips)FMipmap( UBits, VBits, dwSize );

	guard(S3TCencode1);
	S3TCencode( &descIn, NULL, &descOut, &CompMips(0).DataArray(0), S3TC_ENCODE_RGB_FULL|S3TC_ENCODE_ALPHA_NONE, inputweight );
	unguard;

	// Now filter down image if possible for mipmap levels. We are reducing size in place.
	guard(DoMips);
	INT Width = USize;
	INT Height = VSize;
	if( Mipmap )
	{
		while( Width>1 || Height>1 )
		{
			COLOR* pDst = &Buffer(0);
			for( INT y=0; y<Height; y+=2 )
			{
				COLOR* pSrc = &Buffer(y*Width),
				*pSrc2 = (y+1==Height)?pSrc:pSrc+Width;
				for( INT x=0; x<Width; x+=2,pSrc+=2,pSrc2+=2 )
				{
					COLOR c1, c2, c3, c4;
					c1 = *pSrc;
					c2 = (x+1==Width)?*pSrc:pSrc[1];
					c3 = *pSrc2;
					c4 = (x+1==Width)?*pSrc2:pSrc2[1];
					for( INT i=0;i<4;i++ )
						c1.rgba[i]=(c1.rgba[i]+c2.rgba[i]+c3.rgba[i]+c4.rgba[i]+2)/4;
					*pDst++= c1;
				}
			}

			Width  = (Width+1)/2;
			Height = (Height+1)/2;

			descIn.dwWidth  = Width;
			descIn.dwHeight = Height;
			descIn.lPitch   = Width*4;

			guard(S3TCgetEncodeSize);
			dwSize = S3TCgetEncodeSize( &descIn, S3TC_ENCODE_RGB_FULL|S3TC_ENCODE_ALPHA_NONE );
			unguard;

			INT Num = CompMips.Num();
			new(CompMips)FMipmap( Max(UBits-Num,0), Max(VBits-Num,0), dwSize );

			// Compress.
			guard(S3TCencode2);
			S3TCencode( &descIn, NULL, &descOut, &CompMips(Num).DataArray(0), S3TC_ENCODE_RGB_FULL|S3TC_ENCODE_ALPHA_NONE, inputweight );
			unguard;
		}
	}
	unguard;
	CompFormat = TEXF_DXT1;
	return 1;
	unguardobj;
}

//
// Decompress the texture, creating mip-levels if required
//
UBOOL UTexture::Decompress( ETextureFormat InFormat )
{
	guard(UTexture::Decompress);
	if( CompFormat==TEXF_DXT1 )
	{
		// Make sure decompressed format is supported.
		if( InFormat!=TEXF_P8 && InFormat!=TEXF_RGBA8 )
			return 0;

		// Get top mipmap.
		FMemMark Mark(GMem);
		CompMips(0).DataArray.Load();

		// Create decompressed mipmaps.
		INT DecompColorBytes = InFormat==TEXF_P8 ? 1 : 4;
		Mips.Empty();
		for( INT i=0; i<CompMips.Num(); i++ )
			new(Mips)FMipmap( CompMips(i).UBits, CompMips(i).VBits, CompMips(i).USize*CompMips(i).VSize*DecompColorBytes );

		// Create outgoing descriptor.
		DDSURFACEDESC descOut;
		descOut.dwSize=sizeof(descOut);
		appMemzero( &descOut, sizeof(descOut) );

		// Create incoming descriptor.
		DDSURFACEDESC descIn;
		appMemzero( &descIn, sizeof(descIn) );
		descIn.dwSize                        = sizeof(descIn);
		descIn.dwFlags                       = DDSD_WIDTH|DDSD_HEIGHT|DDSD_LPSURFACE|DDSD_PIXELFORMAT;
		descIn.dwWidth                       = USize;
		descIn.dwHeight                      = VSize;
		descIn.ddpfPixelFormat.dwFlags       = DDPF_FOURCC;
		descIn.ddpfPixelFormat.dwRGBBitCount = 4;
		descIn.lpSurface                     = &CompMips(0).DataArray(0);
		INT   OutSize                        = S3TCgetDecodeSize( &descIn );
		BYTE* Decoded32                      = InFormat==TEXF_P8 ? NewZeroed<BYTE>(GMem,OutSize) : &Mips(0).DataArray(0);

		// Decode top S3TC mipmap.
		S3TCdecode( &descIn, &descOut, Decoded32 );

		// Convert to desired format and mipmap.
		if( InFormat==TEXF_P8 )
		{
			// And now palettise it.
			_WORD* PaletteData = New<_WORD>(GMem,256);
			INT PalCount = 0;
			if( CompMips.Num() != 1 )
			{
				// Rather than decompress each level of mipmap, decompress the top level
				// and create news mips from that. This gets around the fact the each mip
				// level must share the same palette.
				BYTE* pMipmaps = New<BYTE>(GMem,(USize*VSize * 22)/16);
				PalCount       = HeckbertQuantize( Decoded32, pMipmaps, PaletteData, XRGB_8888, 256, USize, VSize, 0, 0, 1 );
				INT   XSize    = USize;
				INT   YSize    = VSize;
				BYTE* pCurrMip = pMipmaps;
				for( INT i=0; i<CompMips.Num(); i++ )
				{
					appMemcpy( &Mips(i).DataArray(0), pCurrMip, XSize*YSize );
					pCurrMip += XSize * YSize;
					XSize /= 2;
					YSize /= 2;
				}
			}
			else
			{
				// No mip levels. Just palettize the texture.
				PalCount = HeckbertQuantize( Decoded32, &Mips(0).DataArray(0), PaletteData, XRGB_8888, 256, USize, VSize, 0, 0xff0000, 0 );
			}

			// Create palette.
			if( !Palette )
				Palette = new(GetOuter())UPalette;
			Palette->Colors.Empty( NUM_PAL_COLORS );
			Palette->Colors.Add  ( NUM_PAL_COLORS );
			for( INT i=0; i<PalCount; i++ )
				Palette->Colors(i) = FColor
				(
					(PaletteData[i] & EHiColor565_R) >> 8,
					(PaletteData[i] & EHiColor565_G) >> 3,
					(PaletteData[i] & EHiColor565_B) << 3,
					255
				);
			for( ; i<256; i++ )
				Palette->Colors(i) = FColor(0,0,0,255);
		}
		else
		{
			// Generate mips.
			for( INT i=1; i<Mips.Num(); i++ )
			{
				FMipmap& This = Mips(i);
				FMipmap& Prev = Mips(i-1);
				for( INT j=0; j<This.VSize; j++ )
					for( INT k=0; k<This.USize; k++ )
						This.DataArray(k + j*This.USize) = Prev.DataArray(k*2 + j*2*Prev.USize);
			}
		}
		Mark.Pop();
		Format = InFormat;
		return 1;
	}
	else return 0;
	unguardobj;
}

#else

UBOOL UTexture::Compress( ETextureFormat InFormat, UBOOL Mipmap )
{
	return 0;
}
UBOOL UTexture::Decompress( ETextureFormat InFormat )
{
	return 0;
}

#endif

/*-----------------------------------------------------------------------------
	FTextureInfo.
-----------------------------------------------------------------------------*/

void FTextureInfo::Load()
{
	if( Texture && !bParametric )
	{
		guard(FTextureInfo::Load);
		for( INT i=0; i<NumMips; i++ )
		{
			FMipmap* Mip = (FMipmap*)Mips[i];
			check(Mip);
			Mip->DataArray.Load();
			Mip->DataPtr = &Mip->DataArray(0);
		}
		unguard;
	}
}
void FTextureInfo::Unload()
{
	if( Texture && !bParametric )
	{
		guard(FTextureInfo::Unload);
		for( INT i=0; i<NumMips; i++ )
		{
			FMipmap* Mip = (FMipmap*)Mips[i];
			Mip->DataArray.Unload();
		}
		unguard;
	}
}
void FTextureInfo::CacheMaxColor()
{
	if( Format==TEXF_RGBA7 && GET_COLOR_DWORD(*MaxColor)==0xffffffff )
	{
		DWORD* Tmp = (DWORD*)Mips[0]->DataPtr;
		DWORD  Max = 0x01010101;
		for( INT i=0; i<VClamp; i++ )
		{
			for( INT j=0; j<UClamp; j++ )
			{
				DWORD Flow = (Max - *Tmp) & 0x80808080;
				if( Flow )
				{
					DWORD MaxMask = Flow - (Flow >> 7);
					Max = (*Tmp & MaxMask) | (Max & (0x7f7f7f7f - MaxMask)) ;
				}
				Tmp++;
			}
			Tmp += USize - UClamp;
		}
		check(!(Max&0x80808080));
		GET_COLOR_DWORD(*MaxColor) = Max;
		*MaxColor = FColor(MaxColor->B*2,MaxColor->G*2,MaxColor->R*2,255);
	}
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
