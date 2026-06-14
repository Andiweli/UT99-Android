/*=============================================================================
	UnLight.cpp: Unreal global lighting subsystem implementation.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Description:
	Computes all point lighting information and builds surface light meshes 
	based on light actors and shadow maps.

Definitions:
	attenuation:
		The amount by which light diminishes as it travells from a point source
		outward through space.  Physically correct attenuation is propertional to
		1/(distance*distance), but for speed, Unreal uses a lookup table 
		approximation where all light ceases after a light's predefined radius.
	diffuse lighting:
		Viewpoint-invariant lighting on a surface that is the result of a light's
		brightness and a surface texture's diffuse lighting coefficient.
	dynamic light:
		A light that does not move, but has special effects.
	illumination map:
		A 2D array of floating point or MMX Red-Green-Blue-Unused values which 
		represent the illumination that a light applies to a surface. An illumination
		map is the result of combining a light's spatial effects, attenuation,
		incidence factors, and shadow map.
	incidence:
		The angle at which a ray of light hits a point on a surface. Resulting brightness
		is directly proportional to incidence.
    light:
		Any actor whose LightType member has a value other than LT_None.
	meshel:
		A mesh element; a single point in the rectangular NxM mesh containing lighting or
		shadowing values.
	moving light:
		A light that moves. Moving lights do not cast shadows.
	radiosity:
		The process of determining the surface lighting resulting from 
		propagation of light through an environment, accounting for interreflection
		as well as direct light propagation. Radiosity is a computationally
		expensive preprocessing step but generates physically correct lighting.
	raytracing:
		The process of tracing rays through a level between lights and map lattice points
		to precalculate shadow maps, which are later filtered to provide smoothing. 
		Raytracing generates cool looking though physically unrealistic lighting.
	resultant map:
		The final 2D array of floating point or MMX values which represent the total
		illumination resulting from all of the lights (and hence illumination maps) 
		which apply to a surface.
	shadow map:
		A 2D array of floating point values which represent the amount of shadow
		occlusion between a light and a map lattice point, from 0.0 (fully occluded)
		to 1.0 (fully visible).
	shadow hypervolume:
		The six-dimensional hypervolume of space which is not affected by a volume
		lightsource.
	shadow volume:
		The volume of space which is not affected by a point lightsource. The inverse of light
		volume.
	shadow z-buffer:
		A 2D z-buffer representing a perspective projection depth view of the world from a
		lightsource. Often used in dynamic shadowing computations.
	spatial lighting effect:
		A lighting effect that is a function of a location in space, usually relative to
		a light's location.
	specular lighting:
		Viewpoint-varient lighting on a shiny surface that is the result of a
		light's brightness and a surface texture's specular lighting
		coefficient.
	static illumination map:
		An illumination map that represents the total of all static light illumination
		maps that apply to a surface. Static illumination maps do not change in time
		and thus they can be cached.
	static light:
		A light that is constantly on, does not move, and has no special effects.
	surface map:
		Any 2D map that applies to a surface, such as a shadow map or illumination
		map.  Surface maps are always aligned to the surface's U and V texture
		coordinates and are bilinear filtered across the extent of the surface.
	volumetric lighting:
		Lighting that is visible as a result of light interacting with a volume in
		space due to an interacting media such as fog. Volumetric lighting is view
		variant and cannot be associated with a particular surface.

Design notes:
 *	Uses a multi-tiered system for generating the resultant map for a surface,
	where all known constant intermediate and resulting meshes that may be needed 
	in the future are cached, and all known variable intermediate and resulting
	meshes are allocated temporarily.

Notes:
	No radiosity.
	No dynamic shadows.
	No shadow hypervolumes.
	No shadow volumes.
	No shadow z-buffers.

Revision history:
    9-23-96, Tim: Rewritten from the ground up.
=============================================================================*/

#include "RenderPrivate.h"
#include <math.h>

#define SHADOW_SMOOTHING 1 /* Smooth shadows (should be 1) */
#define ZERO_FLOAT_LIGHT (FLOAT)((3<<22) + 0x10)

/*------------------------------------------------------------------------------------
	Approximate math implementation.
------------------------------------------------------------------------------------*/

FLOAT SqrtManTbl[2<<APPROX_MAN_BITS];
FLOAT DivSqrtManTbl[1<<APPROX_MAN_BITS],DivManTbl[1<<APPROX_MAN_BITS];
FLOAT DivSqrtExpTbl[1<<APPROX_EXP_BITS],DivExpTbl[1<<APPROX_EXP_BITS];

static INT SavedESP,SavedEBP; 

/*------------------------------------------------------------------------------------
	Subsystem definition
------------------------------------------------------------------------------------*/

// Function pointer types.
typedef void (*LIGHT_SPATIAL_FUNC)( FTextureInfo& Tex, class FLightInfo* Info, BYTE* Src, BYTE* Dest );

// Information about one special lighting effect.
struct FLocalEffectEntry
{
	LIGHT_SPATIAL_FUNC	SpatialFxFunc;		// Function to perform spatial lighting
	INT					IsSpatialDynamic;	// Indicates whether light spatiality changes over time.
	INT					IsMergeDynamic;		// Indicates whether merge function changes over time.
};

// Light classification.
enum ELightKind
{
	ALO_StaticLight		= 0,	// Actor is a non-moving, non-changing lightsource
	ALO_DynamicLight	= 1,	// Actor is a non-moving, changing lightsource
	ALO_MovingLight		= 2,	// Actor is a moving, changing lightsource
	ALO_NotLight		= 3,	// Not a surface light (probably volumetric only).
};

// Information about a lightsource.
class FLightInfo
{
public:
	// For all lights.
	AActor*		Actor;					// All actor drawing info.
	ELightKind	Opt;					// Light type.
	FVector		Location;				// Transformed screenspace location of light.
	FLOAT		Radius;					// Maximum effective radius.
	FLOAT		RRadius;				// 1.0 / Radius.
	FLOAT		RRadiusMult;			// 16383.0 / (Radius * Radius).
	FLOAT		Brightness;				// Center brightness at this instance, 1.0=max, 0.0=none.
	FLOAT		Diffuse;				// BaseNormalDelta * RRadius.
	BYTE*		IlluminationMap;		// Temporary illumination map pointer.
	BYTE*		ShadowBits;				// Temporary shadow map.
	UBOOL		IsVolumetric;			// Whether it's volumetric.

	// Clipping region.
	INT MinU, MaxU, MinV, MaxV;

	// For volumetric lights.
	FLOAT		VolRadius;				// Volumetric radius.
	FLOAT		VolRadiusSquared;		// VolRadius*VolRadius.
	FLOAT		VolBrightness;			// Volumetric lighting brightness.
	FLOAT		LocationSizeSquared;	// Location.SizeSqurated().
	FLOAT		RVolRadius;				// 1/Volumetric radius.
	FLOAT		RVolRadiusSquared;		// 1/VolRadius*VolRadius.
	UBOOL       VolInside;               // Viewpoint is inside the sphere.

	// Information about the lighting effect.
	FLocalEffectEntry Effect;

	// Coloring.
	FPlane		FloatColor;				// Incident lighting color.
	FPlane		VolumetricColor;		// Volumetric lighting color.
	FColor*		Palette;				// Brightness scaler.
	FColor*     VolPalette;             // Volumetric color scaler.
	
	// Functions.
	void ComputeFromActor( FTextureInfo* Map, FSceneNode* Frame );
};

//
// Lighting manager definition.
//
class FLightManager : public FLightManagerBase
{
public:
	// FLightManagerBase functions.
	void Init();
	void Exit();
	DWORD SetupForActor( FSceneNode* Frame, AActor* Actor, FVolActorLink* LeafLights, FActorLink* Volumetrics );
	void SetupForSurf( FSceneNode* Frame, FCoords& FacetCoords, FBspDrawList* Draw, FTextureInfo*& LightMap, FTextureInfo*& FogMap, UBOOL Merged );
	void FinishSurf();
	void FinishActor();
	FPlane Light( FTransSample& Point, DWORD PolyFlags );
	FPlane Fog( FTransSample& Point, DWORD PolyFlags );

	// Constants and types.
	enum {MAX_LIGHTS=256};
	typedef DWORD FILTER_TAB[4];

	// Spatial lighting functions.
	static void spatial_None		( FTextureInfo& Tex, FLightInfo* Info, BYTE* Src, BYTE* Dest );
	static void spatial_SearchLight	( FTextureInfo& Tex, FLightInfo* Info, BYTE* Src, BYTE* Dest );
	static void spatial_SlowWave	( FTextureInfo& Tex, FLightInfo* Info, BYTE* Src, BYTE* Dest );
	static void spatial_FastWave	( FTextureInfo& Tex, FLightInfo* Info, BYTE* Src, BYTE* Dest );
	static void spatial_CloudCast	( FTextureInfo& Tex, FLightInfo* Info, BYTE* Src, BYTE* Dest );
	static void spatial_Shock		( FTextureInfo& Tex, FLightInfo* Info, BYTE* Src, BYTE* Dest );
	static void spatial_Disco		( FTextureInfo& Tex, FLightInfo* Info, BYTE* Src, BYTE* Dest );
	static void spatial_Interference( FTextureInfo& Tex, FLightInfo* Info, BYTE* Src, BYTE* Dest );
	static void spatial_Cylinder	( FTextureInfo& Tex, FLightInfo* Info, BYTE* Src, BYTE* Dest );
	static void spatial_Rotor		( FTextureInfo& Tex, FLightInfo* Info, BYTE* Src, BYTE* Dest );
	static void spatial_Spotlight	( FTextureInfo& Tex, FLightInfo* Info, BYTE* Src, BYTE* Dest );
	static void spatial_NonIncidence( FTextureInfo& Tex, FLightInfo* Info, BYTE* Src, BYTE* Dest );
	static void spatial_Shell       ( FTextureInfo& Tex, FLightInfo* Info, BYTE* Src, BYTE* Dest );
	static void spatial_Test		( FTextureInfo& Tex, FLightInfo* Info, BYTE* Src, BYTE* Dest );

	// FLightManager functions.
	static void Merge( FTextureInfo& Tex, BYTE LightEffect, INT Key, FLightInfo* Light, DWORD* Stream, DWORD* Dest );
	static FLOAT Volumetric( FLightInfo* Info, FVector& Vertex );
	void ShadowMapGen( FTextureInfo& Tex, BYTE* SrcBits, BYTE* Dest1 );
	UBOOL AddLight( AActor* Actor, AActor* Other );

	// Variables.
	static FCoords			*MapCoords, MapUncoords;
	static FVector			VertexBase, VertexDU, VertexDV;
	static FSceneNode*		Frame;
	static ULevel*			Level;
	static FMemMark			Mark;
	static INT				ShadowMaskU, ShadowMaskSpace, ShadowSkip;
	static INT				StaticLights, DynamicLights, MovingLights, StaticLightingChanged;
	static FTextureInfo		LightMap, FogMap;
	static FMipmap			LightMip, FogMip;
	static FLightInfo*		LastLight, **LastVtric;
	static FLightInfo*const FinalLight;
	static FLightInfo		FirstLight[MAX_LIGHTS], *FirstVtric[MAX_LIGHTS];
	static ALevelInfo*		LevelInfo;
	static AZoneInfo*		Zone;
	static FPlane			AmbientVector;
	static FLOAT			Diffuse;
	static INT              TemporaryTablesBuilt;
	static FLOAT			BackdropBrightness;
	static FLOAT            LightSqrt[4096];
	static FILTER_TAB		FilterTab[128];
	static BYTE				ByteFog[0x4000];
	static AActor*			Actor;
	static const FLocalEffectEntry Effects[LE_MAX];

	// Memory cache info.
	enum {MAX_UNLOCKED_ITEMS=256};
	static FCacheItem* ItemsToUnlock[MAX_UNLOCKED_ITEMS];
	static FCacheItem** TopItemToUnlock;
};

FCoords*						FLightManager::MapCoords;
FCoords							FLightManager::MapUncoords;
FVector							FLightManager::VertexBase;
FVector							FLightManager::VertexDU;
FVector							FLightManager::VertexDV;
FSceneNode*						FLightManager::Frame;
ULevel*							FLightManager::Level;
FMemMark						FLightManager::Mark;
INT								FLightManager::ShadowMaskU;
INT								FLightManager::ShadowMaskSpace;
INT								FLightManager::ShadowSkip;
INT								FLightManager::StaticLights;
INT								FLightManager::DynamicLights;
INT								FLightManager::MovingLights;
INT								FLightManager::StaticLightingChanged;
FTextureInfo					FLightManager::LightMap;
FTextureInfo					FLightManager::FogMap;
FMipmap							FLightManager::LightMip;
FMipmap							FLightManager::FogMip;
FLightInfo*						FLightManager::LastLight;
FLightInfo**					FLightManager::LastVtric;
FLightInfo*const				FLightManager::FinalLight = &FirstLight[MAX_LIGHTS];
FLightInfo						FLightManager::FirstLight[MAX_LIGHTS];
FLightInfo*						FLightManager::FirstVtric[MAX_LIGHTS];
FLightManager::FILTER_TAB		FLightManager::FilterTab[128];
ALevelInfo*						FLightManager::LevelInfo;
AZoneInfo*						FLightManager::Zone;
FPlane							FLightManager::AmbientVector;
FLOAT							FLightManager::Diffuse;
BYTE							FLightManager::ByteFog[0x4000];
AActor*							FLightManager::Actor;
INT								FLightManager::TemporaryTablesBuilt;
FLOAT							FLightManager::BackdropBrightness;
FLOAT							FLightManager::LightSqrt[4096];
FCacheItem*						FLightManager::ItemsToUnlock[MAX_UNLOCKED_ITEMS];
FCacheItem**					FLightManager::TopItemToUnlock;

const FLocalEffectEntry FLightManager::Effects[LE_MAX] =
{
// LE_ tag			Spatial func			SpacDyn MergeDyn
// ----------------	-----------------------	------- --------
{/* None         */	spatial_None,			0,		0        },
{/* TorchWaver   */	spatial_None,			0,		1        },
{/* FireWaver    */	spatial_None,			0,		1        },
{/* WateryShimmer*/	spatial_None,			0,		1        },
{/* Searchlight  */	spatial_SearchLight,	1,		0        },
{/* SlowWave     */	spatial_SlowWave,		1,		0        },
{/* FastWave     */	spatial_FastWave,		1,		0        },
{/* CloudCast    */	spatial_CloudCast,		1,		0        },
{/* StaticSpot   */	spatial_Spotlight,		0,		0        },
{/* Shock        */	spatial_Shock,			1,		0        },
{/* Disco        */	spatial_Disco,			1,		0        },
{/* Warp         */	spatial_None,			0,		0        },
{/* Spotlight    */	spatial_Spotlight,		0,		0        },
{/* NonIncidence */	spatial_NonIncidence,	0,		0        },
{/* Shell        */	spatial_Shell,			0,		0        },
{/* Satellite    */	spatial_None,			0,		0        },
{/* Interference */	spatial_Interference,	1,		0        },
{/* Cylinder     */	spatial_Cylinder,		0,		0        },
{/* Rotor        */	spatial_Rotor,			1,		0        },
{/* Unused		 */	spatial_None,			0,		0        },
};

/*------------------------------------------------------------------------------------
	Init & Exit.
------------------------------------------------------------------------------------*/

//
// Set up the tables required for fast square root computation.
//
static void SetupTable( FLOAT* ManTbl, FLOAT* ExpTbl, FLOAT Power )
{
	union {FLOAT F; DWORD D;} Temp;

	Temp.F = 1.0;
	for( DWORD i=0; i<(1<<APPROX_EXP_BITS); i++ )
	{
		Temp.D = (Temp.D & 0x007fffff ) + (i << (32-APPROX_EXP_BITS));
		ExpTbl[ i ] = appPow( Abs(Temp.F), Power );
		if( appIsNan(ExpTbl[ i ]) )
			ExpTbl[ i ]=0.0;
		//debugf("exp [%f] %i = %f",Power,i,ExpTbl[i]);
	}

	Temp.F = 1.0;
	INT i;
	for( i=0; i<(1<<APPROX_MAN_BITS); i++ )
	{
		Temp.D = (Temp.D & 0xff800000 ) + (i << (32-APPROX_EXP_BITS-APPROX_MAN_BITS));
		ManTbl[ i ] = appPow( Abs(Temp.F), Power );
		if( appIsNan(ManTbl[ i ]) )
			ManTbl[ i ]=0.0;
		//debugf("man [%f] %i = %f",i,Power,ManTbl[i]);
	}
}

//
// Initialize the global lighting subsystem.
//
void FLightManager::Init()
{
	guard(FLightManager::Init);
	appMemset( &LightMap, 0, sizeof(LightMap) );
	appMemset( &FogMap,   0, sizeof(FogMap  ) );

	// Mutual occlusion blending.
	for( INT i=0; i<128; i++ )
		for( INT j=0; j<128; j++ )
			ByteFog[i*128+j] = (127-i)*j/127;

	// Filtering table.
	INT FilterWeight[8][8] = 
	{
		{ 0,24,40,24,0,0,0,0},
		{ 0,40,64,40,0,0,0,0},
		{ 0,24,40,24,0,0,0,0},
		{ 0, 0, 0, 0,0,0,0,0},
		{ 0, 0, 0, 0,0,0,0,0},
		{ 0, 0, 0, 0,0,0,0,0},
		{ 0, 0, 0, 0,0,0,0,0},
		{ 0, 0, 0, 0,0,0,0,0}
	};

	// Setup square root tables.
	for( DWORD D=0; D< (1<< APPROX_MAN_BITS ); D++ )
	{
		union {FLOAT F; DWORD D;} Temp;
		Temp.F = 1.0;
		Temp.D = (Temp.D & 0xff800000 ) + (D << (23 - APPROX_MAN_BITS));
		Temp.F = appSqrt(Temp.F);
		Temp.D = (Temp.D - ( 64 << 23 ) );   // exponent bias re-adjust
		SqrtManTbl[ D ] = (FLOAT)(Temp.F * appSqrt(2.0)); // for odd exponents
		SqrtManTbl[ D + (1 << APPROX_MAN_BITS) ] =  (FLOAT) (Temp.F * 2.0);
	}
	SetupTable(DivSqrtManTbl,DivSqrtExpTbl,-0.5);
	SetupTable(DivManTbl,    DivExpTbl,    -1.0);
	
	// Init square roots.
	INT i;
	for( i=0; i<ARRAY_COUNT(LightSqrt); i++ )
	{
		FLOAT S = appSqrt((FLOAT)(i+1) * (1.0/ARRAY_COUNT(LightSqrt)));

		// This function gives a more luminous, specular look to the lighting.
		FLOAT Temp = (2*S*S*S-3*S*S+1); // Or 1.0-S.

		// This function makes surfaces look more matte.
		//FLOAT Temp = (1.0-S);
		LightSqrt[i] = Temp/S;
	}

	// Generate filter lookup table
	INT FilterSum=0;
	for( i=0; i<8; i++ )
		for( int j=0; j<8; j++ )
			FilterSum += FilterWeight[i][j];

	// Iterate through all filter table indices 0x00-0x3f.
	for( i=0; i<128; i++ )
	{
		// Iterate through all vertical filter weights 0-3.
		for( int j=0; j<4; j++ )
		{
			// Handle all four packed values.
			FilterTab[i][j] = 0;
			for( INT Pack=0; Pack<4; Pack++ )
			{
				// Accumulate filter weights in FilterTab[i][j] according to which bits are set in i.
				INT Acc = 0;
				for( INT Bit=0; Bit<8; Bit++ )
					if( i & (1<<(Pack + Bit)) )
						Acc += FilterWeight[j][Bit];

				// Add to sum.
				DWORD Result = (Acc * 255) / FilterSum;
				check(Result>=0 && Result<=255);
				FilterTab[i][j] += (Result << (Pack*8));
			}
		}
	}

	// Cache items.
	TopItemToUnlock = &ItemsToUnlock[0];

	// Success.
	debugf( NAME_Init, TEXT("Lighting subsystem initialized") );
	unguard;
}

//
// Shut down the global lighting system.
//
void FLightManager::Exit()
{
	guard(FLightManager::Exit);

	debugf( NAME_Exit, TEXT("Lighting subsystem shut down") );
	unguard;
}

/*------------------------------------------------------------------------------------
	Intermediate map generation code
------------------------------------------------------------------------------------*/

//
// Generate the shadow map for one lightsource that applies to a Bsp surface.
//
void FLightManager::ShadowMapGen( FTextureInfo& Tex, BYTE* SrcBits, BYTE* Dest1 )
{
	guardSlow(FLightManager::ShadowMapGen);
	checkSlow(((INT)Dest1 & 3)==0);

	// If no source, fill it.
	if( !SrcBits )
	{
		appMemset( Dest1, 127, ShadowMaskSpace*8 );
		return;
	}

	// Generate smooth shadow map by convolving the shadow bitmask with a smoothing filter.
	INT Size4 = (ShadowMaskU*8)/4;
	appMemzero( Dest1, ShadowMaskSpace*8 );
	DWORD* Dests[3] = { (DWORD*)Dest1, (DWORD*)Dest1, (DWORD*)Dest1 + Size4 };
	for( INT V=0; V<Tex.VClamp; V++ )
	{
		// Get initial bits, with low bit shifted in.
		BYTE* Src = SrcBits;

		// Offset of shadow map relative to convolution filter left edge.
		DWORD D = (DWORD)*Src++ << (8+2);
		if( D & 0x400 ) D |= 0x300;

		// Filter everything.
		for( INT U=0; U<ShadowMaskU; U++ )
		{
			D = D >> 8;
			D += (U<ShadowMaskU-1) ? (((DWORD)*Src++) << (8+2)) : (D&0x200) ? 0xC00 : 0;

			FILTER_TAB& Tab1 = FilterTab[D & 0x7f];
			*Dests[0]++     += Tab1[0];
			*Dests[1]++     += Tab1[1];
			*Dests[2]++     += Tab1[2];

			FILTER_TAB& Tab2 = FilterTab[(D>>4) & 0x7f];
			*Dests[0]++     += Tab2[0];
			*Dests[1]++     += Tab2[1];
			*Dests[2]++     += Tab2[2];
		}
		SrcBits += ShadowMaskU;
		if( V == 0            ) Dests[0] -= Size4;
		if( V == Tex.VClamp-2 ) Dests[2] -= Size4;
	}
	unguardSlow;
}

/*------------------------------------------------------------------------------------
	Vertex lighting and fogging.
------------------------------------------------------------------------------------*/

FPlane FLightManager::Light( FTransSample& Vert, DWORD PolyFlags )
{
	guard(FLightManager::Light);
	STAT(clock(GStat.MeshLightTime));

	FPlane Color(0,0,0,0);
	if( !(PolyFlags & PF_Unlit) )
	{
		// Lit.
		STAT(GStat.MeshVertLightCount += LastLight-FirstLight);
		FLOAT PointSquared(Vert.Point.SizeSquared());
		for( FLightInfo* Light=FirstLight; Light<LastLight; Light++ )
		{
			if( Light->Opt != ALO_NotLight )
			{
				// Diffuse lighting.
				FVector LightVector  = Light->Location - Vert.Point;
				FLOAT   LightSquared = LightVector.SizeSquared();
				FLOAT   LightSize    = SqrtApprox( LightSquared );
				FLOAT   G            = Square(1.0 + (LightVector | Vert.Normal) / LightSize) - 1.5;
				if( G < 0.0)
					G = 0.0;

				// Specular lighting.
				FLOAT Specular = (Light->Location.MirrorByPlane(Vert.Normal) | Vert.Point) - PointSquared;
				if( Specular > 0.0 )
					G += 6.0 * Square(Specular)/(LightSquared*PointSquared);

				// Radial falloff.
				G *= 1.0 - LightSize * Light->RRadius;

				// Update result color.
				if( G > 0.0 )
					Color += Light->FloatColor * G;
			}
		}
	}
	else Color = FPlane(0.5,0.5,0.5,0);

	// Add ambient light.
	FLOAT D = Diffuse * 1.4;
	Color.X = Min( D * Color.X + AmbientVector.X, 1.f );
	Color.Y = Min( D * Color.Y + AmbientVector.Y, 1.f );
	Color.Z = Min( D * Color.Z + AmbientVector.Z, 1.f );

	// Editor highlighting.
	if( (PolyFlags & PF_Selected) && GIsEditor )
		Color = Color*0.5 + FVector(0.5,0.5,0.5);

	STAT(unclock(GStat.MeshLightTime));
	return Color;
	unguard;
}
#pragma warning (default:4799)

FPlane FLightManager::Fog(FTransSample& Vert, DWORD PolyFlags )
{
	guard(FLightManager::Fog);
	if( PolyFlags & PF_RenderFog )
	{
		STAT(clock(GStat.MeshLightTime));
		FPlane Fog(0,0,0,0);
		for( FLightInfo** Ptr=FirstVtric; Ptr<LastVtric; Ptr++ )
		{
			FLightInfo* LightInfo = *Ptr;
			FLOAT VolumeValue = 2.0f * Volumetric( LightInfo, Vert.Point);
			if( *(DWORD*)&VolumeValue )
			{
				FLOAT A = MinPositiveFloat( LightInfo->VolumetricColor.W * VolumeValue, 1.0 );
				Fog.X   = MinPositiveFloat( LightInfo->VolumetricColor.X * VolumeValue + Fog.X*(1-A), 1.0f );
				Fog.Y   = MinPositiveFloat( LightInfo->VolumetricColor.Y * VolumeValue + Fog.Y*(1-A), 1.0f );
				Fog.Z   = MinPositiveFloat( LightInfo->VolumetricColor.Z * VolumeValue + Fog.Z*(1-A), 1.0f );
				Fog.W   = MinPositiveFloat( A + Fog.W, 1.0f );
			}
		}		
		STAT(unclock(GStat.MeshLightTime));
		return Fog; 
	}
	else return FPlane(0,0,0,0);
	unguard;
}

/*------------------------------------------------------------------------------------
	Light merging.
------------------------------------------------------------------------------------*/

void FLightManager::Merge( FTextureInfo& Tex, BYTE Effect, INT Key, FLightInfo* Info, DWORD* Stream, DWORD* Dest )
{
	guardSlow(FLightManager::Merge);

	static INT Count; 
	FColor* Palette;
    INT Skip;

	// Merge the two streams of light.
	BYTE* Src = Info->IlluminationMap;
	Palette   = Info->Palette;
	Skip      = Info->MinU;
	Count     = Info->MaxU - Info->MinU;

	if( Count<=0 ) return;

	UBOOL FXDetect = ( (Effect==LE_TorchWaver) || (Effect==LE_FireWaver) || (Effect==LE_WateryShimmer) );

	Src    += Info->MinV * Tex.UClamp;
	Dest   += Info->MinV * Tex.USize;
	Stream += Info->MinV * Tex.USize;

	for( INT i=Info->MinV; i<Info->MaxV; i++ )
	{
		BYTE* NewSrc = Src;

		// Execute merge-time effects.
		if( FXDetect )
		{
			BYTE Temp[1024];
			NewSrc = Temp;
			if( Effect==LE_TorchWaver )
			{
				for( INT i=Info->MinU; i<Info->MaxU; i++ )
					Temp[i] = appFloor((FLOAT)Src[i] * (0.95 + 0.05 * GRandoms->RandomBase(Key++)));
			}
			else if( Effect==LE_FireWaver )
			{
				for( INT i=Info->MinU; i<Info->MaxU; i++ )
					Temp[i] = appFloor((FLOAT)Src[i] * (0.80 + 0.20 * GRandoms->RandomBase(Key++)));
			}
			else if( Effect==LE_WateryShimmer )
			{
				for( INT i=Info->MinU; i<Info->MaxU; i++ )
					Temp[i] = appFloor((FLOAT)Src[i] * (0.60 + 0.40 * GRandoms->Random(Key++)));
			}
		}


		// Scale and merge the lighting.
	#if ASM 
		__asm
		{
			// esi = Stream
			// edi = Dest
			// eax = Temp
			// ebx = Palette
			// ecx = Loop counter
			// edx = Temp
			// ebp = Light element
			// esp = NewSrc

			// Setup.
			mov		[SavedESP], esp
			mov		[SavedEBP], ebp
			mov		edx, [Skip]
			mov		esi, [Stream]
			mov		edi, [Dest]
			mov		ebx, [Palette]
			mov		esp, [NewSrc]
			xor		ecx, ecx
			mov     ebp, [Count]
			lea     esi, [esi+edx*4]
			lea     edi, [edi+edx*4]
			lea     esp, [esp+edx]

			// lead-in
			xor		edx, edx
			dec     ebp   // Compensate for leadin/out plus test for 1.
			mov     dl, [esp+ecx]
			jz      LeadOut 
			
			align 16
			// Get scaled light element - 6 cycles
			LightLoop:
			mov		eax, [esi+ecx*4] // Get stream
			mov		edx, [ebx+edx*4] // Get color from palette
			add		eax, edx         // Add stream and color		
			xor     edx, edx
			test	eax, 0x80808080  // Check for saturation
			jnz		LightSaturate    // Fix up after saturation
			mov		dl,[esp+ecx+1]
			mov		[edi+ecx*4], eax // Store result
			inc		ecx
			cmp		ecx, ebp
			jb		LightLoop

			// lead-out
			LeadOut:
			mov		eax, [esi+ecx*4] // Get stream
			mov		edx, [ebx+edx*4] // Get color from palette
			add		eax, edx         // Add stream and color		
			test	eax, 0x80808080  // Check for saturation
			jnz		LastLightSaturate    // Fix up after saturation
			mov		[edi+ecx*4], eax     // Store result
			jmp		LightOut

			align 16
			// Handle saturation - about 9 cycles
			LightSaturate:
			mov		edx,eax
			and		edx,0x80808080
			mov		ebp,edx
			shr		edx,7
			and		eax,0x7F7F7F7F	// mask out all overflowed bits
			sub		ebp,edx			// Creates 7f in each overflowing channel
			xor     edx,edx
			or		eax,ebp			// Set saturated channels
			mov     ebp,[Count]     // reload end indicator
			mov		[edi+ecx*4],eax // Store result
			mov		dl,[esp+ecx+1]
			dec     ebp             // compensate for leadin/out
			inc		ecx
			cmp		ecx, ebp
			jb		LightLoop
			jmp		LeadOut


			align 16
			LastLightSaturate:
			mov		edx,eax
			and		edx,0x80808080
			mov		ebp,edx
			shr     edx,7
			and     eax,0x7f7f7f7f
			sub     ebp,edx
			or      eax,ebp
			mov     [edi+ecx*4],eax

			///
			align 16
			LightOut:
			mov ebp, [SavedEBP]
			mov esp, [SavedESP]
		}		

	#else

		for( INT j=Info->MinU; j<Info->MaxU; j++ )
		{
			Dest[j] = Stream[j] + GET_COLOR_DWORD(Palette[NewSrc[j]]);
			if( Dest[j] & 0x80808080 )
			{
				// Handle saturation.
				DWORD SatMask = Dest[j] & 0x80808080;
				SatMask -= (SatMask >>7);
				Dest[j] = (Dest[j] & 0x7f7f7f7f) | SatMask;
			}
		}

	#endif

		Src    += Tex.UClamp;
		Stream += Tex.USize;
		Dest   += Tex.USize;
	}

	unguardSlow;
}

/*------------------------------------------------------------------------------------
	Spatial effect functions.
------------------------------------------------------------------------------------*/

//
// Convenience macros that give you access to the following parameters easily:
// Info			= FLightInfo pointer
// Vertex		= This point in space
// Location		= Location of light in space
// RRadiusMult	= Inverse radius multiplier
//
#define SPATIAL_PRE \
	STAT(GStat.MeshPtsGen+=Map.UClamp*Map.VClamp); \
	STAT(GStat.MeshesGen++); \
	/* Compute values for stepping through mesh points */ \
	FVector Vertex1 = VertexBase + VertexDV*Info->MinV + VertexDU*Info->MinU; \
	Src  += (ShadowMaskU*8)*Info->MinV + Info->MinU; \
	Dest += Map.UClamp*Info->MinV + Info->MinU; \
	INT USkip = Map.UClamp - (Info->MaxU - Info->MinU); \
	for( INT VCounter=Info->MinV; VCounter<Info->MaxV; VCounter++,Src+=USkip+ShadowSkip,Dest+=USkip ) {

#define SPATIAL_POST \
		Vertex1 += VertexDV; }

#define SPATIAL_BEGIN \
	SPATIAL_PRE \
	FVector Vertex      = Vertex1; \
	FVector Location    = Info->Actor->Location; \
	FLOAT	RRadiusMult = Info->RRadiusMult; \
	FLOAT   Diffuse     = Info->Diffuse; \
	(void)Diffuse; /* Shut up compiler warning */ \
	for( INT UCounter=Info->MinU; UCounter<Info->MaxU; UCounter++,Vertex+=VertexDU,Src++,Dest++ ) { \
		if( *Src ) { \
			DWORD SqrtOfs = appRound( FDistSquared(Vertex,Location) * RRadiusMult ); \
			if( SqrtOfs<4096 ) {

#define SPATIAL_BEGIN1 \
	SPATIAL_PRE \
	FVector Vertex = Vertex1 - Info->Actor->Location; \
	FLOAT	RRadiusMult = Info->RRadiusMult; \
	FLOAT   Diffuse     = Info->Diffuse; \
	(void)Diffuse; /* Shut up compiler warning */ \
	for( INT UCounter=Info->MinU; UCounter<Info->MaxU; UCounter++,Vertex+=VertexDU,Src++,Dest++ ) { \
		if( *Src ) { \
			DWORD SqrtOfs = appRound( Vertex.SizeSquared() * RRadiusMult ); \
			if( SqrtOfs<4096 ) {

#define SPATIAL_END } else *Dest=0; } else *Dest=0; } SPATIAL_POST

// No effects.
void FLightManager::spatial_None( FTextureInfo& Map, FLightInfo* Info, BYTE* Src, BYTE* Dest )
{
	guardSlow(FLightManager::spatial_None);
	STAT(GStat.MeshPtsGen+=Map.UClamp*Map.VClamp);
	STAT(GStat.MeshesGen++);

	// Variables.
	static FVector Vertex;
	static FLOAT   Scale, Diffuse;
	static INT     Dist, DistU, DistV, DistUU, DistVV, DistUV;
	static INT     Interp00, Interp10, Interp20, Interp01, Interp11, Interp02;
	static DWORD   Inner0, Inner1;
	static INT     Hecker;

	// Compute values for stepping through mesh points.
#if 0
	FVector Vertex1 = VertexBase - Info->Actor->Location;
	Scale           = Info->RRadiusMult;
	Diffuse         = Info->Diffuse;
	for( INT V=0; V<Map.VClamp; V++,Vertex1+=VertexDV )
	{
		FVector Vertex = Vertex1;
		for( INT U=0; U<Map.UClamp; U++,Vertex+=VertexDU,Src++,Dest++ )
		{
			if( *Src )
			{
				DWORD SqrtOfs = appRound( Vertex.SizeSquared() * Scale );
				if( SqrtOfs<4096 )
				{
					*(FLOAT*)&Hecker = *Src * Diffuse * LightSqrt[SqrtOfs] + (2<<22);
					*Dest = Hecker;
				}
				else *Dest = 0;
			}
			else *Dest = 0;
		}
	}
#else
	Vertex   = VertexBase - Info->Actor->Location + VertexDU*Info->MinU + VertexDV*Info->MinV;
	Diffuse  = Info->Diffuse;
	Scale    = Info->RRadiusMult * 4096.0;
	Dist     = appRound((Vertex   | Vertex  ) * Scale);
	DistU    = appRound((Vertex   | VertexDU) * Scale);
	DistV    = appRound((Vertex   | VertexDV) * Scale);
	DistUU   = appRound((VertexDU | VertexDU) * Scale);
	DistVV   = appRound((VertexDV | VertexDV) * Scale);
	DistUV   = appRound((VertexDU | VertexDV) * Scale);

	Interp00 = Dist;
	Interp10 = 2 * DistV + DistVV;
	Interp20 = 2 * DistVV;
	Interp01 = 2 * DistU + DistUU;
	Interp11 = 2 * DistUV;
	Interp02 = 2 * DistUU;

	Src  += (ShadowMaskU*8) * Info->MinV + Info->MinU;
	Dest += Map.UClamp * Info->MinV + Info->MinU;
	INT USkip = Map.UClamp - (Info->MaxU-Info->MinU);
	for( INT VCounter=Info->MinV; VCounter<Info->MaxV; VCounter++ )
	{
		// Forward difference the square of the distance between the points.
		Inner0 = Interp00;
		Inner1 = Interp01;
		for( INT U=Info->MinU; U<Info->MaxU; U++ )
		{
			if( *Src!=0 && Inner0<4096*4096 ) 
			{
				*(FLOAT*)&Hecker = *Src * Diffuse * LightSqrt[Inner0>>12] + (2<<22);
				*Dest = Hecker;
			}
			else *Dest = 0;
			Src++;
			Dest++;
			Inner0 += Inner1;
			Inner1 += Interp02;
		}
		Interp00 += Interp10;
		Interp10 += Interp20;
		Interp01 += Interp11;
		Src  += USkip+ShadowSkip;
		Dest += USkip;
	}
#endif
	unguardSlow;
}

inline FLOAT FLightManager::Volumetric( FLightInfo* Info, FVector &Vertex )
{
	// Optimize: there's 2 sqrtapproxes and 2 divides -   too many?
	//
    // d  = (square of) distance of shortest line from viewer-to-surface-line to light location.
	// c1 = distance along viewer-to-surface line to surface, relative to nearest point to light location.
	// c2 = distance along viewer-to-surface line to viewer, relative to nearest point to light location.
	// F  = fog line-integral.
	//
	// FLOAT VertexSize = SqrtApprox(Vertex.SizeSquared());	// Distance eye-to-vertex.
	//
	static int FogRejectionMethod = 0;

	FLOAT c1, c2, d, F, h, c0, S, S2; 
	
	S  = ( Info->Location | Vertex ); // 3 fmuls 2 fadds 
	
	if (! Info->VolInside )
	{
		// No fog if negative dotproduct Light*Vertex AND we're not inside the sphere.
		if (IsNegativeFloat(S)) return 0.f;
	}

	// Determine distance to the vertex.
	FLOAT VertexSizeSquared = Vertex.SizeSquared(); // 3 fmuls
	S2  = S*S; 

	// Whenever the stricter 'd'-check rejected a vertex, we'll try to revert to this one next time.
	// 2 fmuls.
	if ( FogRejectionMethod )
		// equivalent to    if( Info->VolRadiusSquared < d )
		if ( (VertexSizeSquared * Info->VolRadiusSquared) < (VertexSizeSquared * Info->LocationSizeSquared - S2) )	
		{
			return 0.0f;
		}

	// d>=0, sqrt(fabs(d))= Distance of shortest line from viewer-to-surface-line to light location.
	d  = Info->LocationSizeSquared - S2/VertexSizeSquared;   

	// Viewer-to-surface line does not intersect light's sphere.
	if( Info->VolRadiusSquared < d )	// Out distance squared < d 
	{
		FogRejectionMethod = 1; // primed to do cheaper radius checks.
		return 0.0f;
	}

	FogRejectionMethod = 0; //radius checks failed, probably next time also.

	// ERIK+: Maybe it's worth having a lookup table which simultaneously
	// returns sqrt(x) and 1.0/sqrt(x), to avoid the S/VertexSize divide. -Tim

	FLOAT VertexSize = SqrtApprox(VertexSizeSquared);	
	c0 = S/VertexSize;		// c2 <= Info.Location.Size().

	// Compute c1 and c2 from line clipped to interior of sphere.
	h  = SqrtApprox( Info->VolRadiusSquared - d );

	int FullSphere = 0;

	c1 = c0 - VertexSize;   

	if (c0 > h)
	{ 
		c2 = h;
		if (c1 < -h)
		{
			c1 = -h;        // special-case: c1==-h, c2==h, only one integral needed.
			FullSphere = 1;
		}
	}
	else
	{
		c2 = c0;
		if (c1 < -h)
		{
			c1 = -h;
		}	
	}
	
	if (c1 >= c2) return 0.0f; // point totally outside sphere

	FLOAT D2 = d  * Info->RVolRadiusSquared; // real distance from center -> normalized.
	c2 = c2 * Info->RVolRadius;	// scaled relative to volume radius.

	if (FullSphere) // c1== -c2
	{
		FLOAT I = (3-3*D2);
		F =  2.0f * Info->VolBrightness * ( c2*(I - c2*c2) );

		// F = 2.0f * Bri * ( c2 * Rad  ( (3-3*d*rad*rad)  - c2 c2 rad rad ) );
	}
	else
	{
		c1 = c1  * Info->RVolRadius;  
		FLOAT I = (3-3*D2);
		F =  Info->VolBrightness *( (c2*(I - c2*c2 ) ) - ( c1*(I - c1*c1 ) ) );
	}

	if (F < 0.f) return 0.f; //#debug superfluous check, with the new lighting code ?!!
	return MinPositiveFloat( F, 1.f );       	
}

// Yawing searchlight effect.
void FLightManager::spatial_SearchLight( FTextureInfo& Map, FLightInfo* Info, BYTE* Src, BYTE* Dest )
{
	guardSlow(FLightManager::spatial_SearchLight);
	FLOAT Offset
	=	(2.0 * PI)
	+	(Info->Actor->LightPhase * (8.0 * PI / 256.0))
	+	(Info->Actor->LightPeriod ? 35.0 * LevelInfo->TimeSeconds / Info->Actor->LightPeriod : 0);
	SPATIAL_BEGIN1
		FLOAT Angle = appFmod( Offset + 4.0 * appAtan2( Vertex.X, Vertex .Y), 8.*PI );
		if( Angle<PI || Angle>PI*3.0 )
		{
			*Dest = (BYTE) 0.0;
		}
		else
		{
			FLOAT Scale = 0.5 + 0.5 * GMath.CosFloat(Angle);
			FLOAT D     = 0.00006 * (Square(Vertex.X) + Square(Vertex.Y));
			if( D < 1.0 )
				Scale *= D;
			*Dest = appFloor(*Src * Scale * Diffuse * LightSqrt[SqrtOfs]);
		}
	SPATIAL_END
	unguardSlow;
}

// Yawing rotor effect.
void FLightManager::spatial_Rotor( FTextureInfo& Map, FLightInfo* Info, BYTE* Src, BYTE* Dest )
{
	guardSlow(FLightManager::spatial_Rotor);
	SPATIAL_BEGIN1
		FLOAT Angle = 6.0 * appAtan2(Vertex.X,Vertex.Y);
		FLOAT Scale = 0.5 + 0.5 * GMath.CosFloat(Angle + LevelInfo->TimeSeconds*3.5);
		FLOAT D     = 0.0001 * (Square(Vertex.X) + Square(Vertex.Y));
		if (D<1.0) Scale = 1.0 - D + Scale * D;
		*Dest		= appFloor(*Src * Scale * Diffuse * LightSqrt[SqrtOfs]);
	SPATIAL_END
	unguardSlow;
}

// Slow radial waves.
void FLightManager::spatial_SlowWave( FTextureInfo& Map, FLightInfo* Info, BYTE* Src, BYTE* Dest )
{
	guardSlow(FLightManager::spatial_SlowWave);
	SPATIAL_BEGIN1
		FLOAT Scale	= 0.7 + 0.3 * GMath.SinTab(((int)SqrtApprox(Vertex.SizeSquared()) - LevelInfo->TimeSeconds*35.0) * 1024.0);
		*Dest		= appFloor(*Src * Scale * Diffuse * LightSqrt[SqrtOfs]);
	SPATIAL_END
	unguardSlow;
}

// Fast radial waves.
void FLightManager::spatial_FastWave( FTextureInfo& Map, FLightInfo* Info, BYTE* Src, BYTE* Dest )
{
	guardSlow(FLightManager::spatial_FastWave);
	SPATIAL_BEGIN1
		FLOAT Scale	= 0.7 + 0.3 * GMath.SinTab((((int)SqrtApprox(Vertex.SizeSquared())>>2) - LevelInfo->TimeSeconds*35.0) * 2048.0);
		*Dest		= appFloor(*Src * Scale * Diffuse * LightSqrt[SqrtOfs]);
	SPATIAL_END
	unguardSlow;
}

// Scrolling clouds.
void FLightManager::spatial_CloudCast( FTextureInfo& Map, FLightInfo* Info, BYTE* Src, BYTE* Dest )
{
	guardSlow(FLightManager::spatial_CloudCast);
	//if( !LevelInfo->CloudcastTexture )
	{
		spatial_None( Map, Info, Src, Dest );
		return;
	}
	/*
	BYTE*	Data	= LevelInfo->CloudcastTexture->GetMip(0)->DataPtr;
	BYTE	VShift	= LevelInfo->CloudcastTexture->UBits;
	int		UMask	= LevelInfo->CloudcastTexture->USize-1;
	int		VMask	= LevelInfo->CloudcastTexture->VSize-1;
	int		UPan	= 256.0 * 40.0 * LevelInfo->TimeSeconds;
	int		VPan	= 256.0 * 40.0 * LevelInfo->TimeSeconds;

	// optimize: Convert to assembly and optimize reasonably well. This routine is often used
	// for large outdoors areas.
	SPATIAL_PRE
		FVector Vertex = Vertex1;
		for( int i=0; i<Map.UClamp; i++,Vertex+=VertexDU,Src++,Dest++ )
		{
			*Dest = 0.0;
			if( *Src != 0.0 )
			{
				DWORD SqrtOfs = appRound( FDistSquared(Vertex,Info->Actor->Location) * Info->RRadiusMult );
				if( SqrtOfs<4096 )
				{
					int		FixU	= appRound((Vertex.X+Vertex.Z) * (256.0 / 12.0)) + UPan;
					int		FixV	= appRound((Vertex.Y+Vertex.Z) * (256.0 / 12.0)) + VPan;
					int		U0		= (FixU >> 8) & UMask;
					int		U1		= (U0    + 1) & UMask;
					int		V0		= (FixV >> 8) & VMask;
					int		V1		= (V0    + 1) & VMask;

					FLOAT	Alpha1	= FixU & 255;
					FLOAT	Beta1	= FixV & 255;
					FLOAT	Alpha2	= 256.0 - Alpha1;
					FLOAT	Beta2	= 256.0 - Beta1;

					*Dest = Clamp(appFloor(*Src * 3.0 * LightSqrt[SqrtOfs] * Info->Diffuse *
					(
						Data[U0 + (V0<<VShift)] * Alpha2 * Beta2 +
						Data[U1 + (V0<<VShift)] * Alpha1 * Beta2 +
						Data[U0 + (V1<<VShift)] * Alpha2 * Beta1 +
						Data[U1 + (V1<<VShift)] * Alpha1 * Beta1
					) / (256.0 * 256.0 * 256.0)), 0, 255 );
				}
			}
		}
	SPATIAL_POST*/
	unguardSlow;
}

// Shock wave.
void FLightManager::spatial_Shock( FTextureInfo& Map, FLightInfo* Info, BYTE* Src, BYTE* Dest )
{
	guardSlow(FLightManager::spatial_Shock);
	SPATIAL_BEGIN1
		int Dist = INT (8.0 * SqrtApprox(Vertex.SizeSquared()));
		FLOAT Brightness  = 0.9 + 0.1 * GMath.SinTab(((Dist<<1) - (LevelInfo->TimeSeconds * 4000.0))*16.0);
		Brightness       *= 0.9 + 0.1 * GMath.CosTab(((Dist   ) + (LevelInfo->TimeSeconds * 4000.0))*16.0);
		Brightness       *= 0.9 + 0.1 * GMath.SinTab(((Dist>>1) - (LevelInfo->TimeSeconds * 4000.0))*16.0);
		*Dest = appFloor(*Src * Diffuse * LightSqrt[SqrtOfs] * Brightness);
	SPATIAL_END
	unguardSlow;
}

// Disco ball.
void FLightManager::spatial_Disco( FTextureInfo& Map, FLightInfo* Info, BYTE* Src, BYTE* Dest )
{
	guardSlow(FLightManager::spatial_Disco);
	SPATIAL_BEGIN1
		FLOAT Yaw	= 11.0 * appAtan2(Vertex.X,Vertex.Y);
		FLOAT Pitch = 11.0 * appAtan2(SqrtApprox(Square(Vertex.X)+Square(Vertex.Y)),Vertex.Z);

		FLOAT Scale1 = 0.50 + 0.50 * GMath.CosFloat(Yaw   + LevelInfo->TimeSeconds*5.0);
		FLOAT Scale2 = 0.50 + 0.50 * GMath.CosFloat(Pitch + LevelInfo->TimeSeconds*5.0);

		FLOAT Scale  = Scale1 + Scale2 - Scale1 * Scale2;

		FLOAT D = 0.00005 * (Square(Vertex.X) + Square(Vertex.Y));
		if (D<1.0) Scale *= D;

		*Dest = appFloor(*Src * (1.0-Scale) * Diffuse * LightSqrt[SqrtOfs]);
	SPATIAL_END
	unguardSlow;
}

// Cylinder lightsource.
void FLightManager::spatial_Cylinder( FTextureInfo& Map, FLightInfo* Info, BYTE* Src, BYTE* Dest )
{
	guardSlow(FLightManager::spatial_Cylinder);
	SPATIAL_PRE
		FVector Vertex = Vertex1 - Info->Actor->Location;
		for( INT i=Info->MinU; i<Info->MaxU; i++,Vertex+=VertexDU,Src++,Dest++ )
			*Dest = Max<INT>( 0, appFloor(*Src * (1.0 - ( Square(Vertex.X) + Square(Vertex.Y) ) * Square(Info->RRadius)) ) );
	SPATIAL_POST
	unguardSlow;
}

// Interference pattern.
void FLightManager::spatial_Interference( FTextureInfo& Map, FLightInfo* Info, BYTE* Src, BYTE* Dest )
{
	guardSlow(FLightManager::spatial_Interference);
	SPATIAL_BEGIN1
		FLOAT Pitch = 11.0 * appAtan2(SqrtApprox(Square(Vertex.X)+Square(Vertex.Y)),Vertex.Z);
		FLOAT Scale = 0.50 + 0.50 * GMath.CosFloat(Pitch + LevelInfo->TimeSeconds*5.0);
		*Dest = appFloor(*Src * Scale * Diffuse * LightSqrt[SqrtOfs]);
	SPATIAL_END
	unguardSlow;
}

// Spotlight lighting.
void FLightManager::spatial_Spotlight( FTextureInfo& Map, FLightInfo* Info, BYTE* Src, BYTE* Dest )
{
	guardSlow(FLightManager::spatial_Spotlight);
	FVector View      = Info->Actor->GetViewRotation().Vector();
	FLOAT   Sine      = 1.0 - Info->Actor->LightCone / 256.0;
	FLOAT   RSine     = 1.0 / (1.0 - Sine);
	FLOAT   SineRSine = Sine * RSine;
	FLOAT   SineSq    = Sine * Sine;
	SPATIAL_BEGIN1
		FLOAT SizeSq = Vertex | Vertex;
		FLOAT VDotV  = Vertex | View;
		if( VDotV > 0.0 && Square(VDotV) > SineSq * SizeSq )
		{
			FLOAT Dot = Square( VDotV * RSine * DivSqrtApprox(SizeSq) - SineRSine );
			*Dest = appFloor(Dot * *Src * Diffuse * LightSqrt[SqrtOfs]);
		}
		else *Dest = 0;
	SPATIAL_END
	unguardSlow;
}

// Spatial routine for testing.
void FLightManager::spatial_Test( FTextureInfo& Map, FLightInfo* Info, BYTE* Src, BYTE* Dest )
{
	guardSlow(FLightManager::spatial_Test);
	SPATIAL_BEGIN1
		*Dest = 0;
	SPATIAL_END
	unguardSlow;
}

// Amorphous lighting.
void FLightManager::spatial_NonIncidence( FTextureInfo& Map, FLightInfo* Info, BYTE* Src, BYTE* Dest )
{
	guardSlow(FLightManager::spatial_NonIncidence);
	SPATIAL_BEGIN1
		*Dest = appFloor( *Src * (1.02 - SqrtApprox(Vertex.SizeSquared()) * Info->RRadius) );
	SPATIAL_END
	unguardSlow;
}

// Shell lighting.
void FLightManager::spatial_Shell( FTextureInfo& Map, FLightInfo* Info, BYTE* Src, BYTE* Dest )
{
	guardSlow(FLightManager::spatial_Shell);
	SPATIAL_BEGIN1
		FLOAT Dist = SqrtApprox(Vertex.SizeSquared()) * Info->RRadius;
		if( Dist >= 1.0 || Dist <= 0.8 )
			*Dest = 0;
		else
			*Dest = appFloor( *Src * (1.0 - 10.0*Abs(Dist-0.9)) );
	SPATIAL_END
	unguardSlow;
}

/*------------------------------------------------------------------------------------
	Global light effects.
------------------------------------------------------------------------------------*/

// No global lighting
static void global_None( AActor* Owner, FLOAT& Brightness, FVector& Color )
{
	guardSlow(global_None);
	Brightness=0.0;
	unguardSlow;
}

// Steady global lighting
static void global_Steady( AActor* Owner, FLOAT& Brightness, FVector& Color )
{
	guardSlow(global_Steady);
	unguardSlow;
}

// Global light pulsing effect
static void global_Pulse( AActor* Owner, FLOAT& Brightness, FVector& Color )
{
	guardSlow(global_Pulse);
	Brightness *= 0.6 + 0.39 * GMath.SinTab
	(
		(Owner->Level->TimeSeconds * 35.0 * 65536.0) / Max((int)Owner->LightPeriod,1) + (Owner->LightPhase << 8)
	);
	unguardSlow;
}

// Global blinking effect
static void global_Blink( AActor* Owner, FLOAT& Brightness, FVector& Color )
{
	guardSlow(global_Blink);
	if( (int)((Owner->Level->TimeSeconds * 35.0 * 65536.0)/(Owner->LightPeriod+1) + (Owner->LightPhase << 8)) & 1 )
		Brightness = 0.0;
	unguardSlow;
}

// Global flicker effect
static void global_Flicker( AActor* Owner, FLOAT& Brightness, FVector& Color )
{
	guardSlow(global_Flicker);

	FLOAT Random = GRandoms->RandomBase((int)Owner);
	if( Random < 0.5 )	Brightness = 0.0;
	else				Brightness *= Random;
	unguardSlow;
}

// Strobe light.
static void global_Strobe( AActor* Owner, FLOAT& Brightness, FVector& Color )
{
	guardSlow(global_Strobe);
	static float LastUpdateTime=0; static int Toggle=0;
	if( LastUpdateTime != Owner->Level->TimeSeconds )
	{
		LastUpdateTime = Owner->Level->TimeSeconds;
		Toggle ^= 1;
	}
	if( Toggle ) Brightness = 0.0;
	unguardSlow;
}

// Simulated light emmanating from the backdrop.
static void global_BackdropLight( AActor* Owner, FLOAT& Brightness, FVector& Color )
{
	guardSlow(global_BackdropLight);
	unguardSlow;
}

// Global subtle light pulsing effect
static void global_SubtlePulse( AActor* Owner, FLOAT& Brightness, FVector& Color )
{
	guardSlow(global_Pulse);
	Brightness *= 0.9 + 0.09 * GMath.SinTab
	(
		(Owner->Level->TimeSeconds * 35.0 * 65536.0) / Max((int)Owner->LightPeriod,1) + (Owner->LightPhase << 8)
	);
	unguardSlow;
}

// Use texture palette with LifeSpan to indicate index.
static void global_TexturePaletteOnce( AActor* Owner, FLOAT& Brightness, FVector& Color )
{
	guardSlow(global_TexturePaletteOnce);
	if( Owner->Skin && Owner->Skin->Palette )
	{
		FColor C = Owner->Skin->Palette->Colors(appFloor(255.0 * Owner->LifeFraction()));
		Color = FVector( C.R, C.G, C.B ).SafeNormal();
		Brightness *= C.FBrightness() * 2.8;
	}
	unguardSlow;
}

// Use texture palette, looping over and over.
static void global_TexturePaletteLoop( AActor* Owner, FLOAT& Brightness, FVector& Color )
{
	guardSlow(global_TexturePaletteLoop);
	if( Owner->Skin && Owner->Skin->Palette )
	{
		FLOAT Time        = Owner->Level->TimeSeconds * 35 / Max((int)Owner->LightPeriod,1) + Owner->LightPhase;
		FColor C          = Owner->Skin->Palette->Colors(((int)(Time*256) & 255) % 255);
		Color             = FVector( C.R, C.G, C.B ).UnsafeNormal();
		Brightness       *= C.FBrightness() * 2.8;
	}
	unguardSlow;
}

// Table of global lighting functions.
typedef void (*LIGHT_TYPE_FUNC)( AActor* Owner, FLOAT& Brightness, FVector& Color );
static const LIGHT_TYPE_FUNC GLightTypeFuncs[LT_MAX] =
{
	global_None,
	global_Steady,
	global_Pulse,
	global_Blink,
	global_Flicker,
	global_Strobe,
	global_BackdropLight,
	global_SubtlePulse,
	global_TexturePaletteOnce,
	global_TexturePaletteLoop
};

// Compute global lighting for an actor.
void URender::GlobalLighting( UBOOL Realtime, AActor* Owner, FLOAT& Brightness, FPlane& Color )
{
	guard(URender::GlobalLighting);

	// Figure out global dynamic lighting effect.
	ELightType Type = (ELightType)Owner->LightType;
	if( !Realtime )
		Type = LT_Steady;

	// Coloring.
	Color = FGetHSV( Owner->LightHue, Owner->LightSaturation, 255 );

	// Only compute global lighting effect once per actor per frame, so that
	// lights with random functions produce consistent lighting on all surfaces they hit!!
	if( Type<LT_MAX )
		GLightTypeFuncs[Type]( Owner, Brightness, Color );
	Brightness = Clamp( Brightness, 0.f, 1.f );

	unguard;
}

/*------------------------------------------------------------------------------------
	Implementation of FLightInfo class
------------------------------------------------------------------------------------*/

//
// Compute lighting information based on an actor lightsource.
//
void FLightInfo::ComputeFromActor( FTextureInfo* Map, FSceneNode* Frame )
{
	guard(FLightInfo::ComputeFromActor);

	// General setup.
	Radius			= Actor->WorldLightRadius();
	RRadius			= 1.0/Max((FLOAT)1.0,Radius);
	RRadiusMult		= 4093.0 * RRadius * RRadius;
	Location		= Actor->Location.TransformPointBy( Frame->Coords );
	Brightness      = Actor->LightBrightness/255.f;
	Effect          = FLightManager::Effects[(Actor->LightEffect<LE_MAX) ? Actor->LightEffect : 0];
	GRender->GlobalLighting( (Frame->Viewport->Actor->ShowFlags&SHOW_PlayerCtrl)!=0, Actor, Brightness, FloatColor );
	FloatColor     *= Brightness * Actor->Level->Brightness;

	// Surface setup.
	if( Map )
	{
		// Compute coords.
		if( !FLightManager::TemporaryTablesBuilt )
		{
			FLightManager::TemporaryTablesBuilt = 1;
			FLightManager::MapUncoords = FLightManager::MapCoords->Inverse().Transpose();
			FLightManager::VertexBase  = FLightManager::MapCoords->Origin + FLightManager::MapUncoords.XAxis*Map->Pan.X + FLightManager::MapUncoords.YAxis*Map->Pan.Y;
			FLightManager::VertexDU    = FLightManager::MapUncoords.XAxis * Map->UScale;
			FLightManager::VertexDV    = FLightManager::MapUncoords.YAxis * Map->VScale;
		}

		// Surface lighting.
		Diffuse = Abs(((Actor->Location-FLightManager::MapCoords->Origin) | FLightManager::MapCoords->ZAxis) * RRadius);

		// Cache the scaler palette.
		STAT(clock(GStat.ExtraTime));
		QWORD CacheID = MakeCacheID( CID_LightPalette, Actor );
		FVector* Color = (FVector*)GCache.Get(CacheID,*FLightManager::TopItemToUnlock++);
		if( !Color || *Color!=FloatColor || Actor->bLightChanged )
		{
			// Create or replace the palette.
			if( !Color )
				Color = (FVector*)GCache.Create(CacheID,FLightManager::TopItemToUnlock[-1],sizeof(FVector)+256*sizeof(FColor));
			*Color = FloatColor;
			Palette = (FColor*)(Color+1);
			INT FixR = 0; INT FixDR=appFloor(FloatColor.X*65536.0);
			INT FixG = 0; INT FixDG=appFloor(FloatColor.Y*65536.0);
			INT FixB = 0; INT FixDB=appFloor(FloatColor.Z*65536.0);
			for( INT i=0; i<256; i++ )
			{
				Palette[i].B = Min<INT>( Unfix(FixR), 127 ); FixR+=FixDR;
				Palette[i].G = Min<INT>( Unfix(FixG), 127 ); FixG+=FixDG;
				Palette[i].R = Min<INT>( Unfix(FixB), 127 ); FixB+=FixDB;
				Palette[i].A = 255;
			}
		}
		Palette = (FColor*)(Color+1);
		STAT(unclock(GStat.ExtraTime));

		// Compute clipping region.
		FLOAT   PlaneDot    = (Actor->Location - FLightManager::MapCoords->Origin) | FLightManager::MapCoords->ZAxis;
		FLOAT   Radius      = Actor->WorldLightRadius();
		FLOAT   PlaneRadius = SqrtApprox( Max( Radius*Radius*1.05 - PlaneDot*PlaneDot, 0.0 ) );
		FVector Center      = Actor->Location - FLightManager::MapCoords->ZAxis*PlaneDot;
		FLOAT   CenterU     = ((Center - FLightManager::MapCoords->Origin) | FLightManager::MapCoords->XAxis) - FLightManager::LightMap.Pan.X;
		FLOAT   CenterV     = ((Center - FLightManager::MapCoords->Origin) | FLightManager::MapCoords->YAxis) - FLightManager::LightMap.Pan.Y;
		FLOAT   RadiusU     = PlaneRadius * SqrtApprox(FLightManager::MapCoords->XAxis.SizeSquared());
		FLOAT   RadiusV     = PlaneRadius * SqrtApprox(FLightManager::MapCoords->YAxis.SizeSquared());

		// Save clipping region.
		MinU = Max<INT>( appRound( (CenterU - RadiusU)/FLightManager::LightMap.UScale), 0 );
		MinV = Max<INT>( appRound( (CenterV - RadiusV)/FLightManager::LightMap.VScale), 0 );
		MaxU = Min<INT>( appRound( (CenterU + RadiusU)/FLightManager::LightMap.UScale), FLightManager::LightMap.UClamp );
		MaxV = Min<INT>( appRound( (CenterV + RadiusV)/FLightManager::LightMap.VScale), FLightManager::LightMap.VClamp );
	}

	// Init volumetric lighting.
	if( IsVolumetric )
	{		
		VolumetricColor   = FloatColor;   
		VolumetricColor.W = (FLOAT)Actor->VolumeFog * (1.f/255.f);

		// Cache the volumetric color scaler palette
		STAT(clock(GStat.ExtraTime));
		QWORD CacheID = MakeCacheID( CID_VolumetricScaler, Actor );
		FPlane* Color = (FPlane*)GCache.Get(CacheID,*FLightManager::TopItemToUnlock++);
		if( !Color || *Color!=VolumetricColor || Actor->bLightChanged )
		{
			// Create or replace the palette.
			if( !Color )
				Color = (FPlane*)GCache.Create(CacheID,FLightManager::TopItemToUnlock[-1],sizeof(FPlane)+256*sizeof(FColor));
			*Color = VolumetricColor;
			VolPalette = (FColor*)(Color+1);
			INT FixR = 0; INT FixDR=appFloor(VolumetricColor.X*65536.0);
			INT FixG = 0; INT FixDG=appFloor(VolumetricColor.Y*65536.0);
			INT FixB = 0; INT FixDB=appFloor(VolumetricColor.Z*65536.0);
			INT FixA = 0; INT FixDA=appFloor(VolumetricColor.W*65536.0);
			for( INT i=0; i<256; i++ )
			{
				VolPalette[i].B = Min<INT>( Unfix(FixR), 127 ); FixR+=FixDR;
				VolPalette[i].G = Min<INT>( Unfix(FixG), 127 ); FixG+=FixDG;
				VolPalette[i].R = Min<INT>( Unfix(FixB), 127 ); FixB+=FixDB;
				VolPalette[i].A = Min<INT>( Unfix(FixA), 127 ); FixA+=FixDA;
			}
		}
		VolPalette = (FColor*)(Color+1);
		STAT(unclock(GStat.ExtraTime));

		VolRadius			= Actor->WorldVolumetricRadius();
		VolRadiusSquared	= VolRadius * VolRadius;
		RVolRadius          = 1.0f/VolRadius;
		RVolRadiusSquared   = RVolRadius * RVolRadius;
		LocationSizeSquared = Location.SizeSquared();
		VolBrightness		= Brightness * Actor->VolumeBrightness / 64.0f;
		VolInside           = (LocationSizeSquared < VolRadiusSquared);
	}
	unguard;
}

/*------------------------------------------------------------------------------------
	Implementation of FLightList class
------------------------------------------------------------------------------------*/

//
// For sorting volumetric lights by distance.
//
INT CDECL LightInfoDistCompare( const void* A, const void* B )
{
	return 2*((*(FLightInfo**)A)->Location.Z<(*(FLightInfo**)B)->Location.Z)-1;
}

//
// Compute fast light list for a surface.
//
#pragma warning (disable : 4799)
void FLightManager::SetupForSurf
(
	FSceneNode*		InFrame,
	FCoords&		InMapCoords,
	FBspDrawList*	Draw,
	FTextureInfo*&	OutLightMap,
	FTextureInfo*&	OutFogMap,
	UBOOL			Merged
)
{
	guard(FLightManager::SetupForSurf);
	STAT(clock(GStat.IllumTime));
	INT Key=0;

#if 0
	// To regenerate all lighting every frame, uncomment this.
	guard(FlushAll);
	GCache.Flush(MakeCacheID(CID_StaticMap      ,0,0),MakeCacheID(CID_MAX,0,0,NULL));
	GCache.Flush(MakeCacheID(CID_DynamicMap     ,0,0),MakeCacheID(CID_MAX,0,0,NULL));
	GCache.Flush(MakeCacheID(CID_ShadowMap      ,0,0),MakeCacheID(CID_MAX,0,0,NULL));
	GCache.Flush(MakeCacheID(CID_IlluminationMap,0,0),MakeCacheID(CID_MAX,0,0,NULL));
	unguard;
#endif

	// Init mip pointer.
	LightMap.bRealtimeChanged	= 0;
	LightMap.NumMips			= 1;
	LightMap.Mips[0]			= &LightMip;
	LightMap.Format				= TEXF_RGBA7;
	LightMap.Palette			= NULL;
	LightMap.Mips[0]->DataPtr	= NULL;

	// Fog.
	FogMap.bRealtimeChanged	    = 0;
	FogMap.NumMips				= 1;
	FogMap.Mips[0]				= &FogMip;
	FogMap.Format				= TEXF_RGBA7;
	FogMap.Palette				= NULL;
	FogMap.Mips[0]->DataPtr		= NULL;

	// Set up variables.
	Mark						= FMemMark(GMem);
	Frame						= InFrame;
	Level						= Frame->Level;
	INT iLightMap				= Level->Model->Surfs(Draw->iSurf).iLightMap;
	LevelInfo					= Level->GetLevelInfo();
	Zone						= NULL;
	TemporaryTablesBuilt		= 0;	
	FBspSurf& Surf				= Level->Model->Surfs(Draw->iSurf);
	AMover* Mover				= (Frame->Level->BrushTracker && Frame->Level->BrushTracker->SurfIsDynamic(Draw->iSurf)) ?  (AMover*)Surf.Actor : NULL;
	UModel* Model				= Mover ? Mover->Brush : Level->Model;
	FLightMapIndex* Index		= &Model->LightMap(iLightMap);
	Zone						= Draw->Zone;
	BYTE ZoneID					= Zone ? Zone->Region.ZoneNumber : 255;
	LastLight					= FirstLight;
	StaticLights				= 0;
	DynamicLights				= 0;
	MovingLights				= 0;
	StaticLightingChanged		= 0;
	MapCoords					= &InMapCoords;
	ShadowMaskU					= (Index->UClamp+7) >> 3;
	ShadowMaskSpace				= ShadowMaskU * Index->VClamp;
	ShadowSkip					= ShadowMaskU*8 - Index->UClamp;
	OutLightMap					= &LightMap;

	// Handle lighting.
	if( !Mover || !Mover->bDynamicLightMover )
	{
		guard(SetupNormalSurface);
		Mover = NULL;

		// Static lights.
		BYTE* ShadowBase = &Model->LightBits(Index->DataOffset);
		if( Index->iLightActors != INDEX_NONE )
			for( INT i=0; Model->Lights(i+Index->iLightActors); i++,ShadowBase+=ShadowMaskSpace )
				if( AddLight( Mover, Model->Lights(i+Index->iLightActors) ) )
					LastLight[-1].ShadowBits = ShadowBase;

		// Dynamic lights.
		for( FActorLink* Link=Draw->SurfLights; Link; Link=Link->Next )
			AddLight( Mover, Link->Actor );

		unguard;
	}
	else if( Mover->Region.iLeaf!=INDEX_NONE && Level->Model->Leaves.Num() )
	{
		guard(SetupMoverSurface);

		// Static mover lights.
		FLeaf& Leaf = Level->Model->Leaves(Mover->Region.iLeaf);
		if( Leaf.iPermeating!=INDEX_NONE )
			for( INT i=Leaf.iPermeating; Level->Model->Lights(i); i++ )
				if( ((Level->Model->Lights(i)->Location - MapCoords->Origin)|MapCoords->ZAxis) > 0.0 && Level->Model->Lights(i)->bSpecialLit==Mover->bSpecialLit )
					AddLight( NULL, Level->Model->Lights(i) );

		// Dynamic mover lights.
		for( FActorLink* Link=Draw->SurfLights; Link; Link=Link->Next )
			if( Link->Actor->bSpecialLit==Mover->bSpecialLit )
				AddLight( Mover, Link->Actor );

		// Volumetric mover lights.
		for( FActorLink* Volumetrics=Draw->Volumetrics; Volumetrics && LastLight<FinalLight; Volumetrics=Volumetrics->Next )
		{
			FLightInfo* Find;
			for( Find=FirstLight; Find<LastLight && Find->Actor!=Volumetrics->Actor; Find++ );
			Find->IsVolumetric = 1;
			if( Find==LastLight )
			{
				// New volumetric light.
				LastLight->Actor = Volumetrics->Actor;
				LastLight->Opt   = ALO_NotLight;
				LastLight++;
			}
		}
		unguard;
	}

	// Volumetric lights.
	if( Zone && Zone->bFogZone && Draw->Volumetrics && !(Draw->PolyFlags&PF_Translucent) )
	{
		guard(SetupVolumetrics);
		OutFogMap = &FogMap;
		for( FActorLink* Link=Draw->Volumetrics; Link && LastLight<FinalLight; Link=Link->Next )
		{
			// See if volumetric light is already on the list as a regular light.
			FLightInfo* Find;
			for( Find=FirstLight; Find<LastLight && Find->Actor!=Link->Actor; Find++ );
			Find->IsVolumetric = 1;
			if( Find==LastLight )
			{
				// New volumetric light.
				LastLight->Actor     = Link->Actor;
				LastLight->Opt       = ALO_NotLight;
				LastLight++;
			}
		}

		// Setup FogMip and FogMap.
		FogMip.UBits			= appCeilLogTwo(Index->UClamp);
		FogMip.VBits			= appCeilLogTwo(Index->VClamp);
		FogMip.USize			= 1 << FogMip.UBits;
		FogMip.VSize			= 1 << FogMip.VBits;
		FogMap.Pan				= Index->Pan;
		FogMap.UScale			= Index->UScale;
		FogMap.VScale			= Index->VScale;
		FogMap.UClamp			= Index->UClamp;
		FogMap.VClamp			= Index->VClamp;
		FogMap.USize			= FogMip.USize;
		FogMap.VSize			= FogMip.VSize;
		FogMap.bRealtimeChanged = 1;
		FogMap.CacheID			= MakeCacheID( CID_RenderFogMap, iLightMap, ZoneID, Model );

		// Setup the volumetrics.
		FogMip.DataPtr = New<BYTE>(GMem,FogMap.USize*FogMap.VSize*sizeof(DWORD)+sizeof(FColor));
		FogMap.MaxColor = (FColor*)FogMip.DataPtr; FogMip.DataPtr += sizeof(FColor);
		*FogMap.MaxColor = FColor(255,255,255,255);
		unguard;

		// Merge the volumetrics.
		guard(MergeVolumetrics);
		UBOOL VirginFog = 1;
		LastVtric = FirstVtric;
		for( FLightInfo* Info=FirstLight; Info<LastLight; Info++ )
		{
			if( Info->IsVolumetric )
			{
				Info->ComputeFromActor( &FogMap, Frame );
				*LastVtric++ = Info;
			}
		}
		if( LastVtric != FirstVtric )
			appQsort( FirstVtric, LastVtric-FirstVtric, sizeof(FLightInfo*), LightInfoDistCompare );
		for( FLightInfo** Ptr=FirstVtric; Ptr<LastVtric; Ptr++ )
		{
			// Compute the volumetric.
			FLightInfo* Info = *Ptr;
			FVector	Vertex1  = VertexBase.TransformPointBy ( Frame->Coords );
			FVector VertDU   = VertexDU  .TransformVectorBy( Frame->Coords );
			FVector VertDV   = VertexDV  .TransformVectorBy( Frame->Coords );
			if( VirginFog )
			{					
				// First-time fog calculation, no merging required.
				FColor* Dest = (FColor*)FogMip.DataPtr;
				for( INT i=0; i<FogMap.VClamp; i++ )
				{
					FVector Vertex = Vertex1;
					for( INT j=0; j<FogMap.UClamp; j++,Vertex += VertDU )
						Dest[j] = Info->VolPalette[appRound( Volumetric( Info, Vertex ) * 255.0f )];
					Vertex1 += VertDV;
					Dest    += FogMap.USize; 
				}
				VirginFog = 0;
			}
			else
			{
				// Merge in more fog. 
				FColor* Dest = (FColor*)FogMip.DataPtr;
				for( INT i=0; i<FogMap.VClamp; i++ )
				{
					FVector Vertex = Vertex1;
					for( INT j=0; j<FogMap.UClamp; j++,Vertex+=VertDU )
					{
						FLOAT FogAdd = Volumetric( Info, Vertex );
						if( *(DWORD*)&FogAdd )
						{
							DWORD Light = appRound( FogAdd  * 255.0f );
							BYTE* Table = &ByteFog[128*(INT)Info->VolPalette[Light].A];
							Dest[j].R = Min(Table[Dest[j].R]+Info->VolPalette[Light].R,127); 
							Dest[j].G = Min(Table[Dest[j].G]+Info->VolPalette[Light].G,127); 
							Dest[j].B = Min(Table[Dest[j].B]+Info->VolPalette[Light].B,127); 
							Dest[j].A = Min(Dest[j].A+Info->VolPalette[Light].A,127); 
						}
					}
					Vertex1 += VertDV;
					Dest += FogMap.USize;
				}

			}
		}
		unguard;
	}

	// Set up LightMip and LightMap.
	guard(SetupLightMap);
	LightMip.UBits			= appCeilLogTwo(Index->UClamp);
	LightMip.VBits			= appCeilLogTwo(Index->VClamp);
	LightMip.USize			= 1 << LightMip.UBits;
	LightMip.VSize			= 1 << LightMip.VBits;
	LightMap.Pan			= Index->Pan;
	LightMap.UScale			= Index->UScale;
	LightMap.VScale			= Index->VScale;
	LightMap.UClamp			= Index->UClamp;
	LightMap.VClamp			= Index->VClamp;
	LightMap.USize			= LightMip.USize;
	LightMap.VSize			= LightMip.VSize;
	LightMap.CacheID		= MakeCacheID( CID_StaticMap, iLightMap, ZoneID, Model );
	unguard;

	// Handle static lighting.
	DWORD* Stream = (DWORD*)GCache.Get(LightMap.CacheID,*TopItemToUnlock++);
	struct FMoverStamp{ INT iLeaf; FVector Location; FRotator Rotation; };
	if( Mover && Stream )
	{
		StaticLightingChanged |= ((FMoverStamp*)Stream)->iLeaf    != Mover->Region.iLeaf;
		StaticLightingChanged |= ((FMoverStamp*)Stream)->Location != Mover->Location;
		StaticLightingChanged |= ((FMoverStamp*)Stream)->Rotation != Mover->Rotation;
	}
	if( !Stream || StaticLightingChanged )
	{
		// Setup caching.
		guard(StaticLighting);
		StaticLightingChanged=1;
		if( !Stream )
			Stream = (DWORD*)GCache.Create( LightMap.CacheID, TopItemToUnlock[-1], (LightMap.USize*LightMap.VClamp) * sizeof(DWORD) + sizeof(FColor) + sizeof(FMoverStamp), DEFAULT_ALIGNMENT, LightMap.USize*(LightMap.VSize-LightMap.VClamp) );
		if( Mover )
		{
			((FMoverStamp*)Stream)->iLeaf    = Mover->Region.iLeaf;
			((FMoverStamp*)Stream)->Location = Mover->Location;
			((FMoverStamp*)Stream)->Rotation = Mover->Rotation;
		}
		Stream += sizeof(FMoverStamp)/sizeof(DWORD);
		LightMap.MaxColor = (FColor*)Stream++;
		*LightMap.MaxColor = FColor(255,255,255,255);

		// Generate ambient color.
		AmbientVector = FGetHSV( Zone->AmbientHue, Zone->AmbientSaturation, Zone->AmbientBrightness );
		FColor AmbientColor( AmbientVector*0.25 );
		Exchange( AmbientColor.R, AmbientColor.B );
		AmbientColor.A = 127;
		DWORD* Temp = Stream;
		for( INT i=0; i<LightMap.VClamp; i++ )
		{
			for( INT j=0; j<LightMap.UClamp; j++ )
				Temp[j] = GET_COLOR_DWORD(AmbientColor);
			Temp += LightMap.USize;
		}

		// Add in all static lights.
		FMemMark Mark(GMem);
		for( FLightInfo* Info = FirstLight; Info < LastLight; Info++ )
		{
			if( Info->Opt == ALO_StaticLight )
			{
				// Static lighting.
				Info->ComputeFromActor( &LightMap, Frame );
				BYTE* ShadowMap = New<BYTE>(GMem,ShadowMaskSpace*8);
				ShadowMapGen( LightMap, Info->ShadowBits, ShadowMap );
				Info->IlluminationMap = New<BYTE>(GMem,LightMap.UClamp*LightMap.VClamp);
				Info->Effect.SpatialFxFunc( LightMap, Info, ShadowMap, Info->IlluminationMap );
				Merge( LightMap, Info->Actor->LightEffect, 0, Info, Stream, Stream );
				Mark.Pop();
			}
		}
		unguard;
	}
	else
	{
		Stream += sizeof(FMoverStamp)/sizeof(DWORD);
		LightMap.MaxColor = (FColor*)Stream++;
	}

	// Merge in the dynamic lights.
	if( DynamicLights || MovingLights )
	{
		guard(DynamicLight);
		DWORD* Static = Stream;
		LightMap.CacheID = MakeCacheID( CID_DynamicMap, iLightMap, ZoneID, Model );
		if( Merged )
		{
			// Allocate in temporary memory.
			Stream = New<DWORD>(GMem,LightMap.USize*LightMap.VSize+1);
			LightMap.MaxColor = (FColor*)Stream++;
		}
		else
		{
			// Cache it.
			Stream = (DWORD*)GCache.Get( LightMap.CacheID, *TopItemToUnlock++ );
			if( !Stream || *(DOUBLE*)Stream!=Frame->Viewport->CurrentTime )
			{
				if( !Stream )
					Stream = (DWORD*)GCache.Create( LightMap.CacheID, TopItemToUnlock[-1], (LightMap.USize*LightMap.VClamp + 3) * sizeof(DWORD), DEFAULT_ALIGNMENT, LightMap.USize*(LightMap.VSize-LightMap.VClamp) );
				*(DOUBLE*)Stream = Frame->Viewport->CurrentTime;
				Stream += 2;
				LightMap.MaxColor = (FColor*)Stream++;
			}
			else
			{
				*(DOUBLE*)Stream = Frame->Viewport->CurrentTime;
				Stream += 2;
				LightMap.MaxColor = (FColor*)Stream++;
				goto SkipDynamicLight;
			}
		}
		*LightMap.MaxColor = FColor(255,255,255,255);

		// Copy the static lighting.
		DWORD *TmpStatic=Static, *TmpStream=Stream;
		for( INT i=0; i<LightMap.VClamp; i++ )
		{
			appMemcpy( TmpStream, TmpStatic, LightMap.UClamp*sizeof(DWORD) );
			TmpStatic += LightMap.USize;
			TmpStream += LightMap.USize;
		}

		// Merge in the dynamic lights.
		FMemMark Mark(GMem);
		LightMap.bRealtimeChanged = 1;
		for( FLightInfo* Info=FirstLight; Info<LastLight; Info++ )
		{
			if( Info->Opt==ALO_DynamicLight || Info->Opt==ALO_MovingLight )
			{	
				// Set up.
				BYTE* ShadowMap;
				Info->ComputeFromActor( &LightMap, Frame );
				if( Info->Opt==ALO_MovingLight )
				{
					// Build a temporary shadow map and fill it with max.
					ShadowMap = New<BYTE>(GMem,ShadowMaskSpace*8);
					ShadowMapGen( LightMap, NULL, ShadowMap );

					// Build a temporary illumination map.
					Info->IlluminationMap = New<BYTE>(GMem,LightMap.UClamp*LightMap.VClamp);
					Info->Effect.SpatialFxFunc( LightMap, Info, ShadowMap, Info->IlluminationMap );
				}
				else if( Info->Effect.IsSpatialDynamic )
				{
					// This light has spatial effects, so we must cache its shadow map since
					// we will be generating its illumination map per frame.
					//note: we use iSurf because only (iLightMap,Actor,Mover) is unique.
					QWORD CacheID = MakeCacheID( CID_ShadowMap, Draw->iSurf/*iLightMap*/, 0, Info->Actor );
					ShadowMap = (BYTE *)GCache.Get( CacheID, *TopItemToUnlock++ );
					if( !ShadowMap  )
					{
						// Create and generate its shadow map.
						ShadowMap = (BYTE *)GCache.Create( CacheID, TopItemToUnlock[-1], ShadowMaskSpace*8 );
						ShadowMapGen( LightMap, Info->ShadowBits, ShadowMap );
					}

					// Build a temporary illumination map:
					Info->IlluminationMap = New<BYTE>(GMem,LightMap.UClamp*LightMap.VClamp);
					Info->Effect.SpatialFxFunc( LightMap, Info, ShadowMap, Info->IlluminationMap );
				}
				else
				{
					// No spatial lighting. We use a cached illumination map generated from a temporary
					// shadow map. See if the illumination map is already cached:
					//note: we use iSurf because only (iLightMap,Actor,Mover) is unique.
					QWORD CacheID = MakeCacheID( CID_IlluminationMap, Draw->iSurf/*iLightMap*/, 0, Info->Actor );
					Info->IlluminationMap = (BYTE *)GCache.Get( CacheID, *TopItemToUnlock++ );
					if( !Info->IlluminationMap || Info->Actor->bLightChanged )
					{
						// Build a temporary shadow map.
						ShadowMap = New<BYTE>(GMem,ShadowMaskSpace*8);
						ShadowMapGen( LightMap, Info->ShadowBits, ShadowMap );

						// Build and cache an illumination map
						if( !Info->IlluminationMap )
							Info->IlluminationMap = (BYTE *)GCache.Create( CacheID, TopItemToUnlock[-1], (LightMap.UClamp*(LightMap.VClamp+1)+1) * sizeof(BYTE) );
						Info->Effect.SpatialFxFunc( LightMap, Info, ShadowMap, Info->IlluminationMap );
					}
				}

				// Merge the illumination map in.
				Merge( LightMap, Info->Actor->LightEffect, Key, Info, Stream, Stream );
				Mark.Pop();
			}
		}
		unguard;
	}
	SkipDynamicLight:;

	// Set pointers.
	LightMip.DataPtr = (BYTE*)Stream;

	STAT(unclock(GStat.IllumTime));
	unguard;
}

//
// Finish surface lighting.
//
void FLightManager::FinishSurf()
{
	guard(FLightManager::FinishSurf);

	// Release working memory.
	Mark.Pop();

	// Unlock any locked cache items.
	while( TopItemToUnlock > &ItemsToUnlock[0] )
		(*--TopItemToUnlock)->Unlock();

	// Update stats.
	STAT(GStat.Lightage += LightMap.UClamp * LightMap.VClamp);
	STAT(GStat.LightMem += LightMap.UClamp * LightMap.VClamp * sizeof(FLOAT));

	unguard;
}

/*------------------------------------------------------------------------------------
	Light setup.
------------------------------------------------------------------------------------*/

//
// Add a light to the list.
//
UBOOL FLightManager::AddLight( AActor* Actor, AActor* Light )
{
	guardSlow(FLightManager::AddLight);

	// Reject.
	if
	(	LastLight>=FinalLight
	||	Light->LightType==LT_None
	||	Light->LightBrightness==0
	||	Light==Actor )
		return 0;

	// Set actor and optimization flags.
	if( Actor )
	{
		// Distance-reject will reject lights that partially hit the mesh
		// because it doesn't take the collision size into account.
		LastLight->Opt = ALO_MovingLight;
		MovingLights++;
	}
	else if( Light->LightEffect == LE_OmniBumpMap )
	{
		LastLight->Opt = ALO_NotLight;
	}
	else if( Actor || Light->bDynamicLight || !(Light->bStatic || Light->bNoDelete) )
	{
		if( Frame->Viewport->GetOuterUClient()->NoDynamicLights )
			return 0;
		LastLight->Opt = ALO_MovingLight;
		MovingLights++;
	}
	else if
	((	Light->bStatic 
	&&	Light->LightType==LT_Steady
	&&	!Effects[Light->LightEffect].IsSpatialDynamic
	&&	!Effects[Light->LightEffect].IsMergeDynamic) || Frame->Viewport->GetOuterUClient()->NoDynamicLights)
	{
		LastLight->Opt = ALO_StaticLight;
		StaticLights++;
	}
	else
	{
		LastLight->Opt = ALO_DynamicLight;
		DynamicLights++;
	}

	// Info.
	LastLight->Actor        = Light;
	LastLight->IsVolumetric = 0;
	LastLight->ShadowBits   = NULL;
	if( Light->bLightChanged )
		StaticLightingChanged = 1;
	LastLight++;
	return 1;
	unguardSlow;
}

/*------------------------------------------------------------------------------------
	Actor lighting.
------------------------------------------------------------------------------------*/

//
// Compute fast light list for an actor.
// Returns 1 if the actor should be lit, 0 if it should be fake-lit.
//
enum {MaxActorLights=16};
#if defined(LEGEND) //LEGEND
	extern int GLODActorLights;
#else
	enum {DesiredActorLights=3};
#endif
AActor* Consider[120];
INT NumConsider=0, ConsiderTag=0;
inline void AddConsider( AActor* Actor, AActor* Light, BYTE Factor )
{
	if( Light->ExtraTag!=ConsiderTag )
	{
		// Note that distance reject neglects actor bounding sphere.
		FLOAT DistSquared = FDistSquared(Actor->Location,Light->Location);
		FLOAT Radius      = Light->WorldLightRadius();
		if( Light->bSpecialLit==Actor->bSpecialLit && DistSquared<Square(Radius) )
		{
			Light->LightingTag = appRound((1.0-SqrtApprox(DistSquared)/Radius) * Light->LightBrightness * 1024);
			Light->ExtraTag    = ConsiderTag;
			Light->SpecialTag  = Factor;
			Consider[NumConsider++] = Light;
		}
	}
}
static INT Compare( const AActor* A1, const AActor* A2 )
{
	return 1 - 2*(A1->LightingTag>A2->LightingTag);
}
DWORD FLightManager::SetupForActor( FSceneNode* InFrame, AActor* InActor, FVolActorLink* LeafLights, FActorLink* Volumetrics )
{
	guard(FLightManager::SetupForActor);
	DWORD Result=0;

	// Init per actor variables.
	Mark      = FMemMark(GMem);
	Frame     = InFrame;
	Level	  = Frame->Level;
	LevelInfo = Level->GetLevelInfo();
	Actor     = InActor;
	Zone	  = Actor->Region.Zone ? Actor->Region.Zone : LevelInfo;
	MapCoords = NULL;
	LastLight = FirstLight;
	Diffuse   = Actor->ScaleGlow;

	// If not zoned, try now.
	if( Actor->Region.ZoneNumber==0 )
		Level->SetActorZone( Actor, 1, 0 );

	// Ambient lighting.
	FLOAT C       = Actor->AmbientGlow!=255 ? Actor->AmbientGlow/255.0 : 0.25+0.2*sin(8*Frame->Viewport->CurrentTime);
	AmbientVector = FVector(C,C,C) + FGetHSV( Zone->AmbientHue, Zone->AmbientSaturation, Zone->AmbientBrightness );

	// Reject if the proper data structures aren't in place.
	if
	(	!Actor->bUnlit
	&&	Actor->Region.iLeaf!=INDEX_NONE
	&&	Level->Model->Leaves.Num()
	&&	Frame->Viewport->Actor->RendMap==REN_DynLight 
	&&	!Frame->Viewport->GetOuterUClient()->NoLighting )
	{
		// Get actor light cache.
		INT Delta      = Clamp<INT>( (INT)((Frame->Viewport->CurrentTime-Frame->Viewport->LastUpdateTime)*768), 0, 255 );
		INT FrameCount = Frame->Viewport->FrameCount;
		INT* Num       = NULL;
		struct FInfo
		{
			AActor* Actor;
			BYTE Factor;
			INT Index;
		} *Senders;
		UBOOL FirstSeeActor = 0;
		QWORD CacheID  = MakeCacheID( CID_ActorLightCache, Actor );
		Senders        = (FInfo*)GCache.Get( CacheID, *TopItemToUnlock++ );
		if( Senders )
		{
			// Look up actors from cache.
			guardSlow(CacheLook);
			Num = (INT*)Senders++;
			INT NewNum = 0;
			for( INT i=0; i<*Num; i++ )
			{
				if( NewNum!=i )
					Senders[NewNum] = Senders[i];
				if( UObject::GetIndexedObject(Senders[i].Index)==Senders[i].Actor && !Senders[i].Actor->bDeleteMe )
					NewNum++;
			}
			*Num = NewNum;
			unguardSlow;
		}
		else
		{
			// Create cache item.
			guardSlow(CacheCreate);
			FirstSeeActor = 1;
			Senders  = (FInfo*)GCache.Create( CacheID, TopItemToUnlock[-1], (MaxActorLights+1) * sizeof(FInfo) );
			Num      = (INT*)Senders++;
			*Num     = 0;
			unguardSlow;
		}

		// Make list of all lights to consider.
		NumConsider=0;
		ConsiderTag++;

		// Cached lights.
		guardSlow(ConsiderLights);
		for( INT i=0; i<*Num; i++ )
			if( Senders[i].Actor )
				AddConsider( Actor, Senders[i].Actor, Senders[i].Factor );
		unguardSlow;

		// Static leaf lights.
		guardSlow(StaticLeafLights);
		FLeaf& Leaf = Level->Model->Leaves(Actor->Region.iLeaf);
		if( Leaf.iPermeating!=INDEX_NONE )
			for( INT i=Leaf.iPermeating; Level->Model->Lights(i) && NumConsider<ARRAY_COUNT(Consider); i++ )
				AddConsider( Actor, Level->Model->Lights(i), 0 );
		unguardSlow;

		// Dynamic leaf lights.
		guardSlow(DynamicLeafLights);
		for( FVolActorLink* Link=LeafLights; Link && NumConsider<ARRAY_COUNT(Consider); Link=Link->Next )
			AddConsider( Actor, Link->Actor, 255 );
		unguardSlow;

		// Sort considered lights by priority.
		Sort( Consider, NumConsider );

		// Amortized trace visibility.
		INT NumStaticVisible=0, NumRealVisible=0, Threshold=-1;
		guardSlow(TraceVis);
		for( INT i=0; i<NumConsider; i++ )
		{
			// Determine effective visibility.
			UBOOL IsVisible;
#if defined(LEGEND) //LEGEND
			if( Consider[i]->LightingTag<Threshold || NumRealVisible>=GLODActorLights )
#else
			if( Consider[i]->LightingTag<Threshold || (Consider[i]->bStatic ? NumStaticVisible : NumRealVisible)>=DesiredActorLights )
#endif
			{
				IsVisible = 0;
			}
			else if( !Consider[i]->bStatic && Consider[i]->bMovable )
			{
				IsVisible = 1;
			}
			else if( FirstSeeActor || ((FrameCount ^ Consider[i]->GetIndex())&15)==0 )
			{
				FCheckResult Hit(0);
				IsVisible = Level->Model->LineCheck( Hit, NULL, Consider[i]->Location, Actor->Location, FVector(0,0,0), 0 );
				if( IsVisible && FirstSeeActor )
					Consider[i]->SpecialTag = 255;
			}
			else
			{
				IsVisible = (Consider[i]->SpecialTag&1);
			}

			// Handle light.
			if( IsVisible )
			{
				// This light is considered visible.
				NumRealVisible++;
				if( Consider[i]->bStatic )
					NumStaticVisible++;
				if( Threshold==-1 )
					Threshold = Consider[i]->LightingTag/8;
				AddLight( Actor, Consider[i] );
				Consider[i]->SpecialTag = Min<INT>( (INT)Consider[i]->SpecialTag+Delta, 255 ) | 1;
			}
			else
			{
				// This light is not considered visible.
				Consider[i]->SpecialTag = Max<INT>( (INT)Consider[i]->SpecialTag-Delta, 0 ) & ~1;
				if( Consider[i]->SpecialTag>0 )
					AddLight( Actor, Consider[i] );
			}
		}
		unguardSlow;

		// Recache all accepted lights.
		guardSlow(Recache);
		*Num=0;
		for( INT i=0; i<NumConsider && *Num<MaxActorLights; i++ )
		{
			if( Consider[i]->SpecialTag > 1 )
			{
				Senders[*Num].Actor  = Consider[i];
				Senders[*Num].Index  = Consider[i]->GetIndex();
				Senders[*Num].Factor = Consider[i]->SpecialTag;
				++*Num;
			}
		}
		unguardSlow;

		// Volumetric occluding lights.
		guardSlow(Vtrics);
		for( Volumetrics; Volumetrics && LastLight<FinalLight; Volumetrics=Volumetrics->Next )
		{
			Result |= PF_RenderFog;
			FLightInfo* Find;
			for( Find=FirstLight; Find<LastLight && Find->Actor!=Volumetrics->Actor; Find++ );
			Find->IsVolumetric = 1;
			if( Find==LastLight )
			{
				// New volumetric light.
				LastLight->Actor = Volumetrics->Actor;
				LastLight->Opt   = ALO_NotLight;
				STAT(GStat.MeshVtricCount++);
				LastLight++;
			}
		}
		unguardSlow;

		// Set up the lights.
		guardSlow(SetupLights);
		LastVtric = FirstVtric;
		for( FLightInfo* Light=FirstLight; Light<LastLight; Light++ )
		{
			Light->ComputeFromActor( NULL, Frame );
			Light->FloatColor *= Light->Actor->SpecialTag/255.0;
			if( Light->IsVolumetric )
				*LastVtric++ = Light;
		}
		if( LastVtric != FirstVtric )
			appQsort( FirstVtric, LastVtric-FirstVtric, sizeof(FLightInfo*), LightInfoDistCompare );
		unguardSlow;
	}
	STAT(GStat.MeshLightCount+=(LastLight-FirstLight));
	return Result;
	unguard;
}

//
// Finish actor lighting.
//
void FLightManager::FinishActor()
{
	guard(FLightManager::FinishActor);

	// Release working memory.
	Mark.Pop();

	// Unlock any locked cache items.
	while( TopItemToUnlock > &ItemsToUnlock[0])
		(*--TopItemToUnlock)->Unlock();

	unguard;
}

/*------------------------------------------------------------------------------------
	Light subsystem instantiation
------------------------------------------------------------------------------------*/

static FLightManager GLightManagerInstance;
FLightManagerBase* GLightManager = &GLightManagerInstance;

/*------------------------------------------------------------------------------------
	The End
------------------------------------------------------------------------------------*/
