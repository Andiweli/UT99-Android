#include "Texture.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <stdlib.h>

// Log two function.
static BYTE FLogTwo( INT V ) { BYTE R=0; while(V>1) { V>>=1; R++; } return R; }

// Color bytes function.
static INT GColorBytes( ETextureFormat Fmt )
{
	switch( Fmt )
	{
	case TEXF_P8: return 1;
	case TEXF_RGBA7: return 2;
	case TEXF_RGB16: return 2;
	case TEXF_DXT1: return 4; 
	case TEXF_RGB8: return 3;
	case TEXF_RGBA8: return 4;
	case TEXF_EXT_ARGB1555: return 2;
	case TEXF_EXT_ARGB1555_TWID: return 2;
	case TEXF_EXT_RGB565_TWID: return 2;
	default: return 1;
	}
}

// HACK: It isn't possible to reliably determine which textures are referenced by fx
// without loading all packages, which I can't be arsed to do, so
const char* FTextureConverter::Blacklist[] =
{
	"DecayedS.Deco.Window",
	"DecayedS.Light.dlaser1",
	"DecayedS.Light.dlaser2",
	"DecayedS.Floor.Aisle",
	"Detail.paint2",
	"genfluid.Water.*",
	"genfluid.lava",
	"genfluid.Lava1",
	"genfluid.Lava2",
	"genfluid.Lava2a",
	"genfluid.Lava2b",
	"genfluid.lava.*",
	"genfluid.dnitro7",
	"genfluid.Sky.gmist2",
	"genfluid.Sky.gmist3",
	"genfluid.Sky.BlueSky",
	"GenFX.Gradient.L2",
	"GenIn.Crystals.Cryst2",
	"GenWarp.Sun128a",
	"GenWarp.hullgry5",
	"GenWarp.Disp11",
	"GenWarp.glob128c",
	"GenWarp.gw_rusty",
	"GenWarp.gw_calcium",
	"GenWarp.gw_algae",
	"HubEffects.Lavab",
	"UnrealI.MenuGfx.*"
};

namespace
{

static const FName FlattenPackages[] =
{
	FName("Female1Skins"),
	FName("Male1Skins")
};

static UPackage* GetOuterPackage( UObject* Obj )
{
	while( Obj && !Obj->IsA( UPackage::StaticClass() ) )
	{
		Obj = Obj->GetOuter();
	}
	return Cast<UPackage>( Obj );
}

static UBOOL ShouldFlattenTextureInternal( UTexture* Texture )
{
	if( Texture == nullptr )
	{
		return 0;
	}

	UPackage* OuterPkg = GetOuterPackage( Texture );
	if( OuterPkg == nullptr )
	{
		return 0;
	}

	const FName PackageName = OuterPkg->GetFName();
	for( INT i = 0; i < ARRAY_COUNT( FlattenPackages ); ++i )
	{
		if( PackageName == FlattenPackages[i] )
		{
			return 1;
		}
	}

	return 0;
}

static void FlattenTextureToSolidWhiteInternal( UTexture* Texture )
{
	if( Texture == nullptr )
	{
		return;
	}

	Texture->Format = TEXF_EXT_RGB565_TWID;
	Texture->Palette = nullptr;
	Texture->USize = Texture->VSize = 8;
	Texture->UBits = 3;
	Texture->VBits = 3;

	if( Texture->Mips.Num() == 0 )
	{
		new( Texture->Mips ) FMipmap;
	}
	else if( Texture->Mips.Num() > 1 )
	{
		Texture->Mips.Remove( 1, Texture->Mips.Num() - 1 );
	}

	FMipmap& Mip = Texture->Mips(0);
	Mip.USize = 8;
	Mip.VSize = 8;
	Mip.UBits = 3;
	Mip.VBits = 3;
	Mip.DataArray.AddZeroed( 8 * 8 * sizeof(_WORD) );

	const _WORD Pixel = 0xFFFF;
	_WORD* Dest = (_WORD*)&Mip.DataArray(0);
	for( INT i = 0; i < 64; ++i )
	{
		Dest[i] = Pixel;
	}
}

}

UBOOL FTextureConverter::AutoConvertTexture( UTexture* InTexture )
{
	verify( InTexture );

	// Don't touch realtime textures
	if( InTexture->bRealtime || InTexture->bParametric )
		return false;

	// Don't touch textures that are very small
	if( GColorBytes( (ETextureFormat)InTexture->Format ) * InTexture->USize * InTexture->VSize < 2300 )
		return false;

	ETextureFormat TargetFormat;
	if( InTexture->Format == TEXF_P8 && InTexture->Palette )
		TargetFormat = TEXF_EXT_ARGB1555_VQ;
	else
		TargetFormat = (ETextureFormat)InTexture->Format; // TODO: figure out what to do with lightmaps (they are combined at runtime)

	FTextureConverter TexCvt( InTexture, TargetFormat );
	TexCvt.Convert();

	return true;
}

FTextureConverter::FTextureConverter( UTexture* InTexture, const ETextureFormat InFormat )
{
	verify( InTexture );
	verify( InTexture->USize );
	verify( InTexture->VSize );
	verify( InTexture->Mips.Num() );

	Texture = InTexture;
	DstFormat = InFormat;
	DstColorBytes = GColorBytes( InFormat );
	SrcColorBytes = GColorBytes( (ETextureFormat)Texture->Format );
	USize = Max( MinTexSize, Texture->USize );
	VSize = Max( MinTexSize, Texture->VSize );
}

void FTextureConverter::Convert()
{
	// First, cut off the first N mip levels if needed
	INT RealDropMips = Min( DropMips, Texture->Mips.Num() - 1 );
	if( RealDropMips > 0 )
	{
		INT DroppedSize = Max( USize >> DropMips, VSize >> DropMips );
		while( DroppedSize > 1 && DroppedSize > MaxTexSize && RealDropMips < Texture->Mips.Num() - 1 )
		{
			RealDropMips++;
			DroppedSize >>= 1;
		}
		if( Texture->Mips.Num() > RealDropMips )
		{
			Texture->Mips.Remove( 0, RealDropMips );
			Texture->Scale *= (FLOAT)( 1 << RealDropMips );
			Texture->USize = Texture->Mips(0).USize;
			Texture->VSize = Texture->Mips(0).VSize;
			Texture->UBits = FLogTwo(Texture->USize);
			Texture->VBits = FLogTwo(Texture->VSize);
			USize = Max( MinTexSize, Texture->USize );
			VSize = Max( MinTexSize, Texture->VSize );
		}
	}

	// Cut off extra mip levels
	if( Texture->Mips.Num() > MaxMipLevel + 1 )
		Texture->Mips.Remove( MaxMipLevel + 1, Texture->Mips.Num() - MaxMipLevel - 1 );

	// Strip off bump and detail
	Texture->BumpMap = nullptr;
	Texture->DetailTexture = nullptr;

	// If no conversion was requested, bail
	if( DstFormat == Texture->Format )
		return;

	// Do not convert format if texture is blacklisted
	for( INT i = 0; i < ARRAY_COUNT( Blacklist ); ++i )
	{
		char TempStr[1024];
		appStrncpy( TempStr, Blacklist[i], sizeof( TempStr ) );
		char* Glob = appStrchr( TempStr, '*' );
		if( Glob )
		{
			*Glob = 0;
			if( appStrstr( Texture->GetPathName(), TempStr ) )
				return;
		}
		else if ( !appStricmp( Texture->GetPathName(), TempStr ) )
		{
			return;
		}
	}

	// Convert and scale if needed
	for( INT i = 0; i < Texture->Mips.Num(); ++i )
	{
		FMipmap& Mip = Texture->Mips(i);
		ExportMip( Mip, TempPngFile );
		ConvertMip( Mip, TempPngFile );
	}

	// Strip off palette, if there was any and it was from the same package
	if( Texture->Palette && Texture->GetOuter() == Texture->Palette->GetOuter() )
		Texture->Palette = nullptr;

	// Update texture size in case mip 0 was upscaled
	const FMipmap& Mip = Texture->Mips(0);
	if( Texture->USize != Mip.USize )
	{
		Texture->USize = Mip.USize;
		Texture->UBits = Mip.UBits;
		USize = Mip.USize;
	}
	if( Texture->VSize != Mip.VSize )
	{
		Texture->VSize = Mip.VSize;
		Texture->VBits = Mip.VBits;
		VSize = Mip.VSize;
	}

	Texture->Format = DstFormat;
}

void FTextureConverter::ExportMip( const FMipmap& Mip, const char *Filename )
{
	// Convert to RGBA8888
	FColor* OutData = new FColor[ Mip.USize * Mip.VSize ];
	verify( OutData );

	if( Texture->Format == TEXF_P8 )
	{
		const FColor* Palette = &Texture->Palette->Colors(0);
		const DWORD SrcCount = Mip.USize * Mip.VSize;
		const BYTE* Src = &Mip.DataArray(0);
		FColor* Dst = OutData;
		for( DWORD i = 0; i < SrcCount; ++i, ++Src, ++Dst )
		{
			*Dst = Palette[*Src];
			Dst->A = *Src ? 0xFF : 0x00;
		}
	}
	else if( Texture->Format == TEXF_RGBA8 )
	{
		const DWORD SrcCount = Mip.USize * Mip.VSize;
		const FColor* Src = (const FColor*)&Mip.DataArray(0);
		FColor* Dst = OutData;
		for( DWORD i = 0; i < SrcCount; ++i, ++Src, ++Dst )
		{
			Dst->R = Src->R << 1;
			Dst->G = Src->G << 1;
			Dst->B = Src->B << 1;
			Dst->A = Src->A << 1;
		}
	}
	else
	{
		appErrorf( "Can't export format %d", Texture->Format );
	}

	stbi_write_png( Filename, Mip.USize, Mip.VSize, 4, (const void*)OutData, Mip.USize * 4 );

	delete OutData;
}

void FTextureConverter::ConvertMip( FMipmap &Mip, const char* Filename )
{
	// get dest format name
	const char* FormatArgs;
	switch( DstFormat )
	{
		case TEXF_EXT_ARGB1555_TWID: FormatArgs = "-f ARGB1555"; break;
		case TEXF_EXT_ARGB1555_VQ: FormatArgs = "-f ARGB1555 -c"; break;
		case TEXF_EXT_RGB565_TWID: FormatArgs = "-f RGB565"; break;
		case TEXF_EXT_RGB565_VQ: FormatArgs = "-f RGB565 -c"; break;
		default:
			appErrorf( "Can't use pvrtex for format %d", DstFormat );
	}

	// pvrtex is only available in executable form, so fuck it
	const char* PvrtexPath = "pvrtex";

	char Command[2048];
#ifdef PLATFORM_WIN32
	snprintf( Command, sizeof( Command ), "%s -i %s -o %s -r NEAR %s > NUL 2>&1", PvrtexPath, Filename, TempPvrFile, FormatArgs );
#else
	if( const char* KosBase = getenv( "KOS_BASE" ) )
	{
		snprintf( Command, sizeof( Command ), "%s/utils/pvrtex/pvrtex -i %s -o %s -r NEAR %s > /dev/null 2>&1", KosBase, Filename, TempPvrFile, FormatArgs );
	}
	else
	{
		snprintf( Command, sizeof( Command ), "%s -i %s -o %s -r NEAR %s > /dev/null 2>&1", PvrtexPath, Filename, TempPvrFile, FormatArgs );
	}
#endif
	const INT Ret = system( Command );
	if( Ret != 0 )
		appErrorf( "pvrtex returned %d maybe KOS_BASE isn't set, source environ.sh", Ret );

	// load dt file
	FILE* f = fopen( TempPvrFile, "rb" );
	verify( f );

	DWORD Skip = 0;
	DWORD Size = 0;

	fread( &Skip, 4, 1, f ); // FourCC

	fread( &Size, 4, 1, f ); // Size
	verify( Size > 32 )
;
	fread( &Skip, 1, 1, f ); // Version
	fread( &Skip, 1, 1, f ); // Header size
	fread( &Skip, 1, 1, f ); // Codebook size
	fread( &Skip, 1, 1, f ); // Palette size

	_WORD NewUSize = 0;
	_WORD NewVSize = 0;
	DWORD TexType = 0;
	fread( &NewUSize, 2, 1, f );
	fread( &NewVSize, 2, 1, f );
	fread( &TexType, 4, 1, f );

	fread( &Skip, 4, 1, f );
	fread( &Skip, 4, 1, f );
	fread( &Skip, 4, 1, f );

	Size -= 32;
	Mip.DataArray.Add( Size );
	fread( &Mip.DataArray(0), 1, Size, f );

	fclose( f );

	if( NewUSize != Mip.USize )
	{
		Mip.USize = NewUSize;
		Mip.UBits = FLogTwo(NewUSize);
	}

	if( NewVSize != Mip.VSize )
	{
		Mip.VSize = NewVSize;
		Mip.VBits = FLogTwo(NewVSize);
	}
}

UBOOL FTextureConverter::ShouldFlattenTexture( UTexture* Tex )
{
	return ShouldFlattenTextureInternal( Tex );
}

void FTextureConverter::FlattenToSolidWhite( UTexture* Tex )
{
	FlattenTextureToSolidWhiteInternal( Tex );
}
