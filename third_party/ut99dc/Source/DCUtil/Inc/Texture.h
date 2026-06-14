#pragma once

#include "Engine.h"

class FTextureConverter
{
public:
	static constexpr INT MinTexSize = 8;
	static constexpr INT MaxTexSize = 512;
	static constexpr INT MaxMipLevel = 0;
	static constexpr INT DropMips = 1;
	static const char* Blacklist[];

	static UBOOL AutoConvertTexture( UTexture* Tex );
	static UBOOL ShouldFlattenTexture( UTexture* Tex );
	static void FlattenToSolidWhite( UTexture* Tex );

protected:
	FTextureConverter( UTexture* InTexture, const ETextureFormat InFormat );
	void Convert();

protected:
	void ExportMip( const FMipmap& Mip, const char* Filename );
	void ConvertMip( FMipmap& Mip, const char* Filename );

protected:
	static constexpr const char* TempPngFile = "Temp.png";
	static constexpr const char* TempPvrFile = "Temp.dt";
	UTexture* Texture;
	ETextureFormat DstFormat;
	INT DstColorBytes;
	INT SrcColorBytes;
	INT USize;
	INT VSize;
};
