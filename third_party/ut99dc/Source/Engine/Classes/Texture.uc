//=============================================================================
// Texture: An Unreal texture map.
// This is a built-in Unreal class and it shouldn't be modified.
//=============================================================================
class Texture extends Bitmap
	safereplace
	native
	noexport;

// Subtextures.
var(Texture) texture BumpMap;		// Bump map to illuminate this texture with.
var(Texture) texture DetailTexture;	// Detail texture to apply.
var(Texture) texture MacroTexture;	// Macrotexture to apply, not currently used.

// Surface properties.
var(Texture) float Diffuse;			// Diffuse lighting coefficient.
var(Texture) float Specular;		// Specular lighting coefficient.
var(Texture) float Alpha;			// Alpha.
var(Texture) float DrawScale;       // Scaling relative to parent.
var(Texture) float Friction;		// Surface friction coefficient, 1.0=none, 0.95=some.
var(Texture) float MipMult;         // Mipmap multiplier.

// Sounds.
var() sound FootstepSound;			// Footstep sound.
var() sound HitSound;				// Sound when the texture is hit with a projectile.

// Surface flags. !!out of date
var          bool bInvisible;
var(Surface) editconst bool bMasked;
var(Surface) bool bTransparent;
var          bool bNotSolid;
var(Surface) bool bEnvironment;
var          bool bSemisolid;
var(Surface) bool bModulate;
var(Surface) bool bFakeBackdrop;
var(Surface) bool bTwoSided;
var(Surface) bool bAutoUPan;
var(Surface) bool bAutoVPan;
var(Surface) bool bNoSmooth;
var(Surface) bool bBigWavy;
var(Surface) bool bSmallWavy;
var(Surface) bool bWaterWavy;
var          bool bLowShadowDetail;
var          bool bNoMerge;
var(Surface) bool bCloudWavy;
var          bool bDirtyShadows;
var          bool bHighLedge;
var          bool bSpecialLit;
var          bool bGouraud;
var(Surface) bool bUnlit;
var          bool bHighShadowDetail;
var          bool bPortal;
var          const bool bMirrored, bX2, bX3;
var          const bool bX4, bX5, bX6, bX7;

// Texture flags.
var(Quality) private  bool bHighColorQuality;   // High color quality hint.
var(Quality) private  bool bHighTextureQuality; // High color quality hint.
var private           bool bRealtime;           // Texture changes in realtime.
var private           bool bParametric;         // Texture data need not be stored.
var private transient bool bRealtimeChanged;    // Changed since last render.
var private           bool bHasComp;			// Whether a compressed version exists.

// Level of detail set.
var(Quality) enum ELODSet
{
	LODSET_None,   // No level of detail mipmap tossing.
	LODSET_World,  // World level-of-detail set.
	LODSET_Skin,   // Skin level-of-detail set.
} LODSet;

// Animation.
var(Animation) texture AnimNext;
var transient  texture AnimCurrent;
var(Animation) byte    PrimeCount;
var transient  byte    PrimeCurrent;
var(Animation) float   MinFrameRate, MaxFrameRate;
var transient  float   Accumulator;

// Mipmaps.
var private native const array<int> Mips, CompMips;
var const ETextureFormat CompFormat;

defaultproperties
{
	MipMult=1
	Diffuse=1
	Specular=1
	DrawScale=1
	Friction=1
	LODSet=LODSET_World
}
