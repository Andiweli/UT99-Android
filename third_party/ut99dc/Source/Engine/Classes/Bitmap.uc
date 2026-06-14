//=============================================================================
// Bitmap: An abstract bitmap.
// This is a built-in Unreal class and it shouldn't be modified.
//=============================================================================
class Bitmap extends Object
	native
	noexport;

// Texture format.
var const enum ETextureFormat
{
	TEXF_P8,
	TEXF_RGB32,
	TEXF_RGB64,
	TEXF_DXT1,
	TEXF_RGB24
} Format;

// Palette.
var(Texture) palette Palette;

// Internal info.
var const byte  UBits, VBits;
var const int   USize, VSize;
var(Texture) const int UClamp, VClamp;
var const color MipZero;
var const color MaxColor;
var const int   InternalTime[2];

defaultproperties
{
	MipZero=(R=64,G=128,B=64,A=0)
	MaxColor=(R=255,G=255,B=255,A=255)
}
