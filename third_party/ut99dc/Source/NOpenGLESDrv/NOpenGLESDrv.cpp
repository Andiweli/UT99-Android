/* UT99_ANDROID_V61_NATIVE_GLES_VIEWPORT removes old 960x540 logical viewport mutation */
#include "SDL2/SDL.h"
#include "glad.h"
#include "glm/matrix.hpp"
#include "glm/ext/matrix_clip_space.hpp"
#include "SDL.h"

#include "NOpenGLESDrvPrivate.h"

#ifdef PLATFORM_ANDROID
#include <android/log.h>
#define UT99_ANDROID_GLES_FORCEFULL_V28 1
#define UT99_ANDROID_GLES_LOGI_V28(...) __android_log_print(ANDROID_LOG_INFO, "UT99GLES", __VA_ARGS__)
#else
#define UT99_ANDROID_GLES_LOGI_V28(...)
#endif

#ifdef PLATFORM_ANDROID
#include <android/log.h>
#define UT99_ANDROID_MENU_SCALE_V27 1
#define UT99_ANDROID_GLES_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "UT99GLES", __VA_ARGS__)
#else
#define UT99_ANDROID_GLES_LOGI(...)
#endif

/*-----------------------------------------------------------------------------
	GLSL shaders.
-----------------------------------------------------------------------------*/

static const char *FragShaderGLSL {
#include "FragmentShader.glsl.inc"
};

static const char *VertShaderGLSL {
#include "VertexShader.glsl.inc"
};

/*-----------------------------------------------------------------------------
	Global implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_PACKAGE(NOpenGLESDrv);
IMPLEMENT_CLASS(UNOpenGLESRenderDevice);

/*-----------------------------------------------------------------------------
	UNOpenGLESRenderDevice implementation.
-----------------------------------------------------------------------------*/

// MV matrix that puts the coordinate system in order
static constexpr glm::mat4 MtxModelView {
	+1.f, +0.f, +0.f, +0.f,
	+0.f, -1.f, +0.f, +0.f,
	+0.f, +0.f, -1.f, +0.f,
	+0.f, +0.f, +0.f, +1.f,
};

// in floats
static constexpr DWORD AttribSizes[AT_Count] = {
	3, 2, 2, 2, 2, 4, 4
};

// from XOpenGLDrv:
// PF_Masked requires index 0 to be transparent, but is set on the polygon instead of the texture,
// so we potentially need two copies of any palettized texture in the cache
// unlike in newer unreal versions the low cache bits are actually used, so we have use one of the
// actually unused higher bits for this purpose, thereby breaking 64-bit compatibility for now
#define MASKED_TEXTURE_TAG (1ULL << 60ULL)

// FColor is adjusted for endianness
#define ALPHA_MASK 0xff000000

// lightmaps are 0-127
#define LIGHTMAP_SCALE 2

// and it also would be nice to overbright them
#define LIGHTMAP_OVERBRIGHT 1.4f

// max vertices in a single draw call
#define MAX_VERTS 32768


#ifdef PLATFORM_ANDROID
static FLOAT UT99AndroidCalcWidescreenFOVV159( FLOAT BaseFOV, FLOAT FrameX, FLOAT FrameY )
{
	if( FrameX <= 0.0f || FrameY <= 0.0f || BaseFOV <= 0.0f || BaseFOV >= 170.0f )
		return BaseFOV;

	const FLOAT CurrentAspect = FrameX / FrameY;
	const FLOAT ClassicAspect = 4.0f / 3.0f;
	if( CurrentAspect <= ClassicAspect + 0.01f )
		return BaseFOV;

	// UT99_ANDROID_V159_WIDESCREEN_FOV:
	// Keep the classic 4:3 vertical view and expand horizontally on wide screens.
	// 90 degrees at 4:3 becomes about 106.3 at 16:9 and about 118.1 at 20:9.
	FLOAT WideFOV = (FLOAT)( appAtan( appTan( BaseFOV * PI / 360.0 ) * ( CurrentAspect / ClassicAspect ) ) * 360.0 / PI );
	WideFOV = Max<FLOAT>( BaseFOV, WideFOV );
	WideFOV = Min<FLOAT>( 120.0f, WideFOV );
	return WideFOV;
}

static UBOOL UT99AndroidParseBoolTextV159( const TCHAR* Value, UBOOL& OutValue )
{
	if( !Value )
		return 0;
	if( !appStricmp(Value,TEXT("1")) || !appStricmp(Value,TEXT("TRUE")) || !appStricmp(Value,TEXT("ON")) || !appStricmp(Value,TEXT("YES")) )
	{
		OutValue = 1;
		return 1;
	}
	if( !appStricmp(Value,TEXT("0")) || !appStricmp(Value,TEXT("FALSE")) || !appStricmp(Value,TEXT("OFF")) || !appStricmp(Value,TEXT("NO")) )
	{
		OutValue = 0;
		return 1;
	}
	return 0;
}

static UBOOL UT99AndroidReadWidescreenFOVConfigV159( UBOOL DefaultValue )
{
	TCHAR Value[64];
	UBOOL ParsedValue = DefaultValue;
	if( GConfig && GConfig->GetString(TEXT("Engine.Engine.ViewportManager"), TEXT("AndroidWidescreenFOV"), Value, ARRAY_COUNT(Value)) )
	{
		if( UT99AndroidParseBoolTextV159( Value, ParsedValue ) )
			return ParsedValue;
	}
	if( GConfig && GConfig->GetString(TEXT("NOpenGLESDrv.NOpenGLESRenderDevice"), TEXT("WidescreenFOV"), Value, ARRAY_COUNT(Value)) )
	{
		if( UT99AndroidParseBoolTextV159( Value, ParsedValue ) )
			return ParsedValue;
	}
	return DefaultValue;
}
#endif

#ifdef PLATFORM_ANDROID
static void UT99AndroidForceDrawableViewportV28( UViewport* Viewport, const char* Where, INT FrameX, INT FrameY )
{
    INT W = FrameX;
    INT H = FrameY;
    if( Viewport )
    {
        SDL_Window* Window = (SDL_Window*)Viewport->GetWindow();
        if( Window )
        {
            int DW = 0;
            int DH = 0;
            SDL_GL_GetDrawableSize( Window, &DW, &DH );
            if( DW <= 0 || DH <= 0 )
                SDL_GetWindowSize( Window, &DW, &DH );
            if( DW > 0 && DH > 0 )
            {
                W = DW;
                H = DH;
            }
        }
        if( W <= 0 && Viewport->SizeX > 0 ) W = Viewport->SizeX;
        if( H <= 0 && Viewport->SizeY > 0 ) H = Viewport->SizeY;
    }
    if( W <= 0 ) W = FrameX;
    if( H <= 0 ) H = FrameY;
    if( W > 0 && H > 0 )
    {
        // UT99_ANDROID_V61_NATIVE_GLES_VIEWPORT:
        // v60 correctly restored the Android/SDL surface to native size, but this
        // GLES helper still mutated Unreal's UViewport back to the old logical
        // 960x540 mode.  That made the actual scene/HUD/touch viewport 960x540
        // even while the EGL drawable was native, so the user still saw a
        // stretched low-resolution frame.
        // Keep both the GL viewport and Unreal's UViewport in native drawable
        // coordinates.  The Java side still applies GUIScale=2 for heights > 540.
        if( Viewport && ( Viewport->SizeX != W || Viewport->SizeY != H ) )
        {
            UT99_ANDROID_GLES_LOGI_V28("v61 native viewport object from %dx%d to %dx%d", Viewport->SizeX, Viewport->SizeY, W, H);
            Viewport->SizeX = W;
            Viewport->SizeY = H;
        }
        glDisable( GL_SCISSOR_TEST );
        glViewport( 0, 0, W, H );
        // UT99_ANDROID_V163_LOADPROF_V1E:
        // Keep the useful startup proof, but do not emit this every frame on
        // slower Android 4.x devices.  The old 40-line burst was visible in
        // loadprof logs and adds avoidable logcat work.
        static INT LogCount = 0;
        if( LogCount < 4 )
        {
            UT99_ANDROID_GLES_LOGI_V28("%s forced viewport drawable=%dx%d frame=%dx%d vp=%dx%d", Where ? Where : "?", W, H, FrameX, FrameY, Viewport ? Viewport->SizeX : -1, Viewport ? Viewport->SizeY : -1);
            LogCount++;
        }
    }
}
#endif

#ifdef PLATFORM_ANDROID
static void UT99AndroidGetDrawableSizeV27( UViewport* Viewport, INT& OutX, INT& OutY )
{
    OutX = 0;
    OutY = 0;
    if( Viewport )
    {
        SDL_Window* Window = (SDL_Window*)Viewport->GetWindow();
        if( Window )
        {
            int W = 0;
            int H = 0;
            SDL_GL_GetDrawableSize( Window, &W, &H );
            if( W <= 0 || H <= 0 )
                SDL_GetWindowSize( Window, &W, &H );
            if( W > 0 && H > 0 )
            {
                OutX = W;
                OutY = H;
                return;
            }
        }
        if( Viewport->SizeX > 0 && Viewport->SizeY > 0 )
        {
            OutX = Viewport->SizeX;
            OutY = Viewport->SizeY;
        }
    }
}

static void UT99AndroidApplyFullViewportV27( UViewport* Viewport, INT FallbackX, INT FallbackY )
{
    INT W = 0;
    INT H = 0;
    UT99AndroidGetDrawableSizeV27( Viewport, W, H );
    if( W <= 0 ) W = FallbackX;
    if( H <= 0 ) H = FallbackY;
    if( W > 0 && H > 0 )
    {
        glViewport( 0, 0, W, H );
    }
}
#endif

#ifdef PLATFORM_ANDROID
#define UT99_ANDROID_GLES_FULLSCREEN_VIEWPORT_V26B 1
static void UT99AndroidGetDrawableSize( UViewport* Viewport, INT& OutX, INT& OutY )
{
    OutX = 0;
    OutY = 0;
    if( Viewport )
    {
        SDL_Window* Window = (SDL_Window*)Viewport->GetWindow();
        if( Window )
        {
            int W = 0;
            int H = 0;
            SDL_GL_GetDrawableSize( Window, &W, &H );
            if( W > 0 && H > 0 )
            {
                OutX = W;
                OutY = H;
                return;
            }
        }
        if( Viewport->SizeX > 0 && Viewport->SizeY > 0 )
        {
            OutX = Viewport->SizeX;
            OutY = Viewport->SizeY;
            return;
        }
    }
}

static void UT99AndroidApplyFullViewport( UViewport* Viewport, INT FallbackX, INT FallbackY )
{
    INT W = 0;
    INT H = 0;
    UT99AndroidGetDrawableSize( Viewport, W, H );
    if( W <= 0 ) W = FallbackX;
    if( H <= 0 ) H = FallbackY;
    if( W > 0 && H > 0 )
        glViewport( 0, 0, W, H );
}
#endif

void UNOpenGLESRenderDevice::InternalClassInitializer( UClass* Class )
{
	guardSlow(UNOpenGLESRenderDevice::InternalClassInitializer);
	new(Class, "NoFiltering",    RF_Public)UBoolProperty( CPP_PROPERTY(NoFiltering),    "Options", CPF_Config );
	new(Class, "Overbright",     RF_Public)UBoolProperty( CPP_PROPERTY(Overbright),     "Options", CPF_Config );
	new(Class, "DetailTextures", RF_Public)UBoolProperty( CPP_PROPERTY(DetailTextures), "Options", CPF_Config );
	new(Class, "UseVAO",         RF_Public)UBoolProperty( CPP_PROPERTY(UseVAO),         "Options", CPF_Config );
	new(Class, "UseBGRA",        RF_Public)UBoolProperty( CPP_PROPERTY(UseBGRA),        "Options", CPF_Config );
	new(Class, "WidescreenFOV",  RF_Public)UBoolProperty( CPP_PROPERTY(WidescreenFOV),  "Options", CPF_Config );
	unguardSlow;
}

//
// UT-era static construction hook. InternalClassInitializer belongs to a
// later UE1 initialization path and otherwise never registers these options.
//
void UNOpenGLESRenderDevice::StaticConstructor()
{
	guard(UNOpenGLESRenderDevice::StaticConstructor);
	InternalClassInitializer( StaticClass() );
	unguard;
}

UNOpenGLESRenderDevice::UNOpenGLESRenderDevice()
{
	// UT99_ANDROID_V138_VISUAL_GAMEPLAY_DEFAULTS:
	// Keep the renderer's inherited high-detail flags enabled by default.
	// The first-person weapon muzzle flash is guarded by Level.bHighDetailMode,
	// which is initialized from RenDev->HighDetailActors.  Missing INI keys on
	// Android therefore used to hide the muzzle flash completely.
	VolumetricLighting = true;
	ShinySurfaces = true;
	Coronas = true;
	HighDetailActors = true;

	DetailTextures = true;
	Overbright = true;
	NoFiltering = false;
	UseVAO = false;
	UseBGRA = true;
	WidescreenFOV = false;
	CurrentBrightness = -1.f;
}

UBOOL UNOpenGLESRenderDevice::Init( UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen )
{
	guard(UNOpenGLESRenderDevice::Init)

	debugf( NAME_Log, TEXT("NOpenGLESDrv::Init: NewX=%d, NewY=%d, ColorBytes=%d, Fullscreen=%d"), NewX, NewY, NewColorBytes, Fullscreen );

	if( !gladLoadGLES2Loader( &SDL_GL_GetProcAddress ) )
	{
		debugf( NAME_Warning, "Could not load GLES2: %s", SDL_GetError() );
		return false;
	}

	debugf( NAME_Log, "Got OpenGL %s", glGetString( GL_VERSION ) );

	SupportsFogMaps = true;
	SupportsDistanceFog = true;

	ComposeSize = 256 * 256 * 4;
	Compose = (BYTE*)appMalloc( ComposeSize, "GLComposeBuf" );
	verify( Compose );

	VtxDataSize = 18 * MAX_VERTS; // should be enough for all attributes
	VtxData = (FLOAT*)appMalloc( VtxDataSize * sizeof(FLOAT), "GLVtxDataBuf" );
	verify( VtxData );
	VtxDataEnd = VtxData + VtxDataSize;
	VtxDataPtr = VtxData;

	IdxDataSize = MAX_VERTS;
	IdxData = (GLushort*)appMalloc( IdxDataSize * sizeof(GLushort), "GLIdxDataBuf" );
	verify( IdxData );
	IdxDataEnd = IdxData + IdxDataSize;
	IdxDataPtr = IdxData;
	IdxCount = 0;

	if( UseVAO )
	{
		glGenBuffers( 1, &GLBuf );
		glBindBuffer( GL_ARRAY_BUFFER, GLBuf );
		glBufferData( GL_ARRAY_BUFFER, VtxDataSize, (void*)VtxData, GL_DYNAMIC_DRAW );
	}

	WidescreenFOV = UT99AndroidReadWidescreenFOVConfigV159( WidescreenFOV );
	UT99_ANDROID_GLES_LOGI_V28("UT99_ANDROID_V159_WIDESCREEN_FOV_INIT config=%d", WidescreenFOV ? 1 : 0);
	debugf( NAME_Log, "UT99_ANDROID_V159_WIDESCREEN_FOV_INIT config=%d", WidescreenFOV ? 1 : 0 );

	if( UseBGRA )
	{
		// check if BGRA is actually supported
		if( !( GLAD_GL_APPLE_texture_format_BGRA8888 || GLAD_GL_EXT_texture_format_BGRA8888 || GLAD_GL_MESA_bgra ) )
		{
			debugf( "GLES2: BGRA8888 enabled, but not supported; disabling" );
			UseBGRA = false;
		}
		else
		{
			debugf( "GLES2: BGRA8888 supported" );
		}
	}

	// Set permanent state.
	glEnable( GL_DEPTH_TEST );
	glDepthMask( GL_TRUE );
	glBlendFunc( GL_ONE, GL_ZERO );
	glEnable( GL_BLEND );
#ifdef PLATFORM_ANDROID
    #define UT99_ANDROID_V27_DISABLE_SCISSOR 1
    glDisable( GL_SCISSOR_TEST );
#endif
	glEnableVertexAttribArray( 0 );

	// Precache some common shaders
	static const DWORD PrecacheShaders[] = {
		SF_VtxColor,
		SF_Texture0,
		SF_Texture0 | SF_VtxColor,
		SF_Texture0 | SF_AlphaTest,
		SF_Texture0 | SF_AlphaTest | SF_ModulatedDecalMask, // UT99_ANDROID_V166G_MODULATED_DECAL_MASK
		SF_Texture0 | SF_VtxColor | SF_AlphaTest,
		SF_Texture0 | SF_VtxColor | SF_VtxFog,
		SF_Texture0 | SF_VtxColor | SF_VtxFog | SF_AlphaTest,
		SF_Texture0 | SF_Texture1 | SF_Lightmap,
		SF_Texture0 | SF_Texture1 | SF_Lightmap | SF_AlphaTest,
		SF_Texture0 | SF_Texture1 | SF_Texture2 | SF_Lightmap | SF_Fogmap,
		SF_Texture0 | SF_Texture1 | SF_Texture2 | SF_Lightmap | SF_Fogmap | SF_AlphaTest,
	};
	for( DWORD i = 0; i < ARRAY_COUNT( PrecacheShaders ); ++i )
		CreateShader( PrecacheShaders[i] );

	CurrentPolyFlags = PF_Occlude;
	CurrentShaderFlags = 0;
	CurrentBrightness = -1.f;
	Viewport = InViewport;

	// Set initial viewport size
	#ifdef PLATFORM_ANDROID
    UT99AndroidForceDrawableViewportV28( Viewport, "InitSetRes", NewX, NewY );
#else
    glViewport( 0, 0, NewX, NewY );
#endif
	debugf( NAME_Log, TEXT("NOpenGLESDrv::Init complete, viewport set to %dx%d"), NewX, NewY );

	return true;
	unguard;
}

UBOOL UNOpenGLESRenderDevice::SetRes( INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen )
{
	guard(UNOpenGLESRenderDevice::SetRes);
	debugf( NAME_Log, TEXT("NOpenGLESDrv::SetRes: NewX=%d, NewY=%d"), NewX, NewY );
	// Update GL viewport when resolution changes
	#ifdef PLATFORM_ANDROID
    UT99AndroidForceDrawableViewportV28( Viewport, "InitSetRes", NewX, NewY );
#else
    glViewport( 0, 0, NewX, NewY );
#endif
	return true;
	unguard;
}

void UNOpenGLESRenderDevice::Exit()
{
	guard(UNOpenGLESRenderDevice::Exit);

	debugf( NAME_Log, "Shutting down OpenGL ES2 renderer" );

	Flush( 0 );

	if( Compose )
	{
		appFree( Compose );
		Compose = NULL;
	}
	ComposeSize = 0;

	unguard;
}

void UNOpenGLESRenderDevice::PostEditChange()
{
	guard(UNOpenGLESRenderDevice::PostEditChange)

	Super::PostEditChange();

	unguard;
}

void UNOpenGLESRenderDevice::Flush( UBOOL AllowPrecache )
{
	guard(UNOpenGLESRenderDevice::Flush);

	if( TexAlloc.Num() )
	{
		debugf( NAME_Log, "Flushing %d textures", TexAlloc.Num() );
		ResetTexture( 0 );
		ResetTexture( 1 );
		ResetTexture( 2 );
		ResetTexture( 3 );
		glFinish();
		glDeleteTextures( TexAlloc.Num(), &TexAlloc(0) );
		TexAlloc.Empty();
		BindMap.Empty();
	}

	unguard;
}

UBOOL UNOpenGLESRenderDevice::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(UNOpenGLESRenderDevice::Exec);

	if( URenderDevice::Exec( Cmd, Ar ) )
		return 1;

#ifdef PLATFORM_ANDROID
	if( ParseCommand( &Cmd, TEXT("UTGETWIDESCREENFOV") ) )
	{
		Ar.Logf( TEXT("%i"), WidescreenFOV ? 1 : 0 );
		return 1;
	}
	else if( ParseCommand( &Cmd, TEXT("UTSETWIDESCREENFOV") ) )
	{
		TCHAR Token[64];
		UBOOL NewValue = WidescreenFOV;

		if( ParseToken( Cmd, Token, ARRAY_COUNT(Token), 0 ) )
		{
			if( !appStricmp(Token,TEXT("1")) || !appStricmp(Token,TEXT("TRUE")) || !appStricmp(Token,TEXT("ON")) || !appStricmp(Token,TEXT("YES")) )
				NewValue = 1;
			else if( !appStricmp(Token,TEXT("0")) || !appStricmp(Token,TEXT("FALSE")) || !appStricmp(Token,TEXT("OFF")) || !appStricmp(Token,TEXT("NO")) )
				NewValue = 0;
		}

		WidescreenFOV = NewValue;
		CurrentSceneNode.FX = -1.f;
		CurrentSceneNode.FY = -1.f;
		CurrentSceneNode.FovAngle = -1.f;
		if( GConfig )
		{
			GConfig->SetBool(TEXT("Engine.Engine.ViewportManager"), TEXT("AndroidWidescreenFOV"), WidescreenFOV);
			GConfig->SetBool(TEXT("NOpenGLESDrv.NOpenGLESRenderDevice"), TEXT("WidescreenFOV"), WidescreenFOV);
			GConfig->Flush(0);
		}
		SaveConfig();
		debugf( NAME_Log, "UT99_ANDROID_V159_WIDESCREEN_FOV_RUNTIME live=%d", WidescreenFOV ? 1 : 0 );
		Ar.Logf( TEXT("%i"), WidescreenFOV ? 1 : 0 );
		return 1;
	}
#endif

	return 0;

	unguard;
}

void UNOpenGLESRenderDevice::Lock( FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* InHitData, INT* InHitSize )
{
    guard(UNOpenGLESRenderDevice::Lock);
#ifdef PLATFORM_ANDROID
    UT99AndroidForceDrawableViewportV28( Viewport, "Lock", Viewport ? Viewport->SizeX : 0, Viewport ? Viewport->SizeY : 0 );
#endif


	glClearColor( ScreenClear.X, ScreenClear.Y, ScreenClear.Z, ScreenClear.W );
	glClearDepthf( 1.f );
	glDepthFunc( GL_LEQUAL );

	FLOAT TargetBrightness = CurrentBrightness;
	if( Viewport && Viewport->GetOuterUClient() )
	{
		// UT99_ANDROID_V155_PREFERENCES_CLEANUP:
		// The Preferences > Video brightness slider writes UClient::Brightness.
		// GLES previously ignored it and kept the shader at the neutral 0.5 value.
		TargetBrightness = Clamp<FLOAT>( Viewport->GetOuterUClient()->Brightness, 0.0f, 1.0f );
	}
	else if( CurrentBrightness < 0.f )
		TargetBrightness = 0.5f;

	if( CurrentBrightness != TargetBrightness )
	{
		CurrentBrightness = TargetBrightness;
		UniformsChanged[UF_Brightness] = true;
	}

	SetBlend( PF_Occlude );
	SetShader( CurrentShaderFlags );

	GLbitfield ClearBits = GL_DEPTH_BUFFER_BIT;
	if( RenderLockFlags & LOCKR_ClearScreen )
		ClearBits |= GL_COLOR_BUFFER_BIT;
	glClear( ClearBits );

	if( FlashScale != FPlane(0.5f, 0.5f, 0.5f, 0.0f) || FlashFog != FPlane(0.0f, 0.0f, 0.0f, 0.0f) )
		ColorMod = FPlane( FlashFog.X, FlashFog.Y, FlashFog.Z, 1.f - Min( FlashScale.X * 2.f, 1.f ) );
	else
		ColorMod = FPlane( 0.f, 0.f, 0.f, 0.f );

	unguard;
}

void UNOpenGLESRenderDevice::Unlock( UBOOL Blit )
{
	guard(UNOpenGLESRenderDevice::Unlock);

	static INT UnlockCount = 0;
	if( (++UnlockCount % 60) == 0 )
	{
		debugf( NAME_Log, TEXT("NOpenGLESDrv: Unlock %d (Blit=%d)"), UnlockCount, Blit );
	}

	FlushTriangles();

	glFlush();

	unguard;
}

void UNOpenGLESRenderDevice::DrawComplexSurface( FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet )
{
	guard(UNOpenGLESRenderDevice::DrawComplexSurface);

	check(Surface.Texture);

	SetSceneNode( Frame );
	SetBlend( Surface.PolyFlags );
	SetTexture( 0, *Surface.Texture, ( Surface.PolyFlags & PF_Masked ), 0.f );
	if( Surface.LightMap )
	{
		SetTexture( 1, *Surface.LightMap, 0, -0.5f );
		CurrentShaderFlags |= SF_Lightmap;
	}
	if( Surface.FogMap )
	{
		SetTexture( 2, *Surface.FogMap, 0, -0.5f );
		CurrentShaderFlags |= SF_Fogmap;
	}
	if( Surface.DetailTexture && DetailTextures )
	{
		SetTexture( 3, *Surface.DetailTexture, 0, 0.f );
		CurrentShaderFlags |= SF_Detail;
	}
	SetShader( CurrentShaderFlags );

	FLOAT UDot = Facet.MapCoords.XAxis | Facet.MapCoords.Origin;
	FLOAT VDot = Facet.MapCoords.YAxis | Facet.MapCoords.Origin;
	for( FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next )
	{
		BeginPoly();
		for( INT i = 0; i < Poly->NumPts; i++ )
		{
			FLOAT U = Facet.MapCoords.XAxis | Poly->Pts[i]->Point;
			FLOAT V = Facet.MapCoords.YAxis | Poly->Pts[i]->Point;
			AttribFloat3( &Poly->Pts[i]->Point.X );
			AttribFloat2( (U-UDot-TexInfo[0].UPan)*TexInfo[0].UMult, (V-VDot-TexInfo[0].VPan)*TexInfo[0].VMult );
			if( Surface.LightMap )
				AttribFloat2( (U-UDot-TexInfo[1].UPan)*TexInfo[1].UMult, (V-VDot-TexInfo[1].VPan)*TexInfo[1].VMult );
			if( Surface.FogMap )
				AttribFloat2( (U-UDot-TexInfo[2].UPan)*TexInfo[2].UMult, (V-VDot-TexInfo[2].VPan)*TexInfo[2].VMult );
			if( Surface.DetailTexture && DetailTextures )
				AttribFloat2( (U-UDot-TexInfo[3].UPan)*TexInfo[3].UMult, (V-VDot-TexInfo[3].VPan)*TexInfo[3].VMult );
			PolyVertex();
		}
		EndPoly();
	}

	CurrentShaderFlags &= ~( SF_Lightmap|SF_Fogmap|SF_Detail );

	ResetTexture( 1 );
	ResetTexture( 2 );
	ResetTexture( 3 );

	unguard;
}

void UNOpenGLESRenderDevice::DrawGouraudPolygon( FSceneNode* Frame, FTextureInfo& Texture, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* SpanBuffer )
{
	guard(UNOpenGLESRenderDevice::DrawGouraudPolygon);

	const UBOOL IsFog = ( ( PolyFlags & ( PF_RenderFog|PF_Translucent|PF_Modulated ) ) == PF_RenderFog );
	const UBOOL IsModulated = ( PolyFlags & PF_Modulated );
#ifdef PLATFORM_ANDROID
	// UT99_ANDROID_V166G_MODULATED_DECAL_MASK:
	// Keep the original UE1 modulated blob-shadow/decal blend, but discard the
	// light grey no-op border around paletted decal textures. This mirrors the
	// Unreal1 Android path: do not redraw the shadow; only alpha-test the bright
	// quad background so Show Decals can stay enabled without square borders.
	const UBOOL AndroidModulatedDecalMask = ( SpanBuffer != NULL ) && ( PolyFlags & PF_Modulated ) && ( PolyFlags & PF_Masked );
	if( AndroidModulatedDecalMask )
		CurrentShaderFlags |= SF_ModulatedDecalMask;
#else
	const UBOOL AndroidModulatedDecalMask = 0;
#endif
	if( !IsModulated )
		CurrentShaderFlags |= SF_VtxColor;
	if( IsFog )
		CurrentShaderFlags |= SF_VtxFog;

	SetSceneNode( Frame );
	SetBlend( PolyFlags );
	SetTexture( 0, Texture, ( PolyFlags & PF_Masked ), 0 );
	SetShader( CurrentShaderFlags );

	BeginPoly();
	for( INT i=0; i<NumPts; i++ )
	{
		FTransTexture* P = Pts[i];
		AttribFloat3( &P->Point.X );
		AttribFloat2( P->U*TexInfo[0].UMult, P->V*TexInfo[0].VMult );
		if( !IsModulated )
			AttribFloat4( P->Light.X, P->Light.Y, P->Light.Z, 1.f );
		if( IsFog )
			AttribFloat4( &P->Fog.X );
		PolyVertex();
	}
	EndPoly();

	CurrentShaderFlags &= ~( SF_VtxColor|SF_VtxFog|SF_ModulatedDecalMask );

	unguard;
}

void UNOpenGLESRenderDevice::DrawTile( FSceneNode* Frame, FTextureInfo& Texture, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, FSpanBuffer* Span, FLOAT Z, FPlane Light, FPlane Fog, DWORD PolyFlags )
{
	guard(UNOpenGLESRenderDevice::DrawTile);

	#ifdef PLATFORM_ANDROID
    /* UT99_ANDROID_DISABLE_LEGACY_2D_SCALE_V37: v29 native viewport makes old tile scaling harmful. */
#endif
    FPlane VtxColor;
	if( !( PolyFlags & PF_Modulated ) )
	{
		VtxColor.X = Light.X;
		VtxColor.Y = Light.Y;
		VtxColor.Z = Light.Z;
		VtxColor.W = 1.f;
	}
	else
	{
		VtxColor.X = 1.f;
		VtxColor.Y = 1.f;
		VtxColor.Z = 1.f;
		VtxColor.W = 1.f;
	}

	CurrentShaderFlags |= SF_VtxColor;
#ifdef PLATFORM_ANDROID
	// UT99_ANDROID_V167V_MODULATED_SPRITE_TILE_MASK:
	// Thrown rocket/grenade smoke puffs are actor sprites drawn through DrawTile
	// with a span buffer.  They can arrive as modulated/translucent effect quads
	// without PF_Masked even though the paletted sprite uses its border/neutral
	// pixels as cutout.  That makes the whole expanding smoke tile darken as a
	// square on GLES.  For world actor-effect tiles only (Span != NULL), force the
	// masked cache variant and, for modulated smoke-style sprites, also discard
	// the bright neutral modulation background like the existing decal fix.
	UBOOL AndroidV167VForcedEffectMask = 0;
	UBOOL AndroidV167VModulatedTileMask = 0;
	if( Span != NULL && Texture.Palette && ( PolyFlags & (PF_Modulated|PF_Translucent) ) )
	{
		if( !( PolyFlags & PF_Masked ) )
		{
			PolyFlags |= PF_Masked;
			AndroidV167VForcedEffectMask = 1;
		}
		if( PolyFlags & PF_Modulated )
		{
			CurrentShaderFlags |= SF_ModulatedDecalMask;
			AndroidV167VModulatedTileMask = 1;
		}
		static INT AndroidV167VEffectTileLogCount = 0;
		if( AndroidV167VEffectTileLogCount < 48 )
		{
			UT99_ANDROID_GLES_LOGI(
				"UT99_ANDROID_V167V_EFFECT_SPRITE_TILE_MASK tex=%s poly=0x%08x forced=%i modmask=%i size=%.1fx%.1f z=%.3f palette=%i",
				(Texture.Texture ? appToAnsi(Texture.Texture->GetName()) : "<null>"),
				(unsigned int)PolyFlags,
				AndroidV167VForcedEffectMask ? 1 : 0,
				AndroidV167VModulatedTileMask ? 1 : 0,
				XL, YL, Z, Texture.Palette ? 1 : 0 );
			AndroidV167VEffectTileLogCount++;
		}
	}
#endif

	SetSceneNode( Frame );
	SetBlend( PolyFlags );
	SetTexture( 0, Texture, ( PolyFlags & PF_Masked ), 0.f );
	SetShader( CurrentShaderFlags );

	BeginPoly();
		AttribFloat3( RFX2 * Z * (X - Frame->FX2), RFY2 * Z * (Y - Frame->FY2), Z );
		AttribFloat2( U * TexInfo[0].UMult, V * TexInfo[0].VMult );
		AttribFloat4( &VtxColor.X );
		PolyVertex();
		AttribFloat3( RFX2 * Z * (X + XL - Frame->FX2), RFY2 * Z * (Y - Frame->FY2), Z );
		AttribFloat2( (U + UL) * TexInfo[0].UMult, V * TexInfo[0].VMult );
		AttribFloat4( &VtxColor.X );
		PolyVertex();
		AttribFloat3( RFX2 * Z * (X + XL - Frame->FX2), RFY2 * Z * (Y + YL - Frame->FY2), Z );
		AttribFloat2( (U + UL) * TexInfo[0].UMult, (V + VL) *TexInfo[0].VMult );
		AttribFloat4( &VtxColor.X );
		PolyVertex();
		AttribFloat3( RFX2 * Z * (X - Frame->FX2), RFY2 * Z * (Y + YL - Frame->FY2), Z );
		AttribFloat2( U * TexInfo[0].UMult, (V + VL) * TexInfo[0].VMult );
		AttribFloat4( &VtxColor.X );
		PolyVertex();
	EndPoly();

	CurrentShaderFlags &= ~SF_VtxColor;
#ifdef PLATFORM_ANDROID
	if( AndroidV167VModulatedTileMask )
		CurrentShaderFlags &= ~SF_ModulatedDecalMask;
#endif

	unguard;
}

void UNOpenGLESRenderDevice::Draw2DLine( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2 )
{

}

void UNOpenGLESRenderDevice::Draw2DPoint( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z )
{

}

void UNOpenGLESRenderDevice::EndFlash( )
{
	guard(UNOpenGLESRenderDevice::EndFlash);

	if( ColorMod == FPlane( 0.f, 0.f, 0.f, 0.f ) )
		return;

	CurrentShaderFlags = SF_VtxColor;
	ResetTexture( 0 );
	ResetTexture( 1 );
	ResetTexture( 2 );
	ResetTexture( 3 );
	SetBlend( PF_Highlighted );
	SetShader( CurrentShaderFlags );

	const FLOAT Z = 1.f;
	const FLOAT RFX2 = RProjZ;
	const FLOAT RFY2 = RProjZ * Aspect;

	glDisable( GL_DEPTH_TEST );

	BeginPoly();
		AttribFloat3( RFX2 * -Z, RFY2 * -Z, Z );
		AttribFloat4( &ColorMod.X );
		PolyVertex();
		AttribFloat3( RFX2 * +Z, RFY2 * -Z, Z );
		AttribFloat4( &ColorMod.X );
		PolyVertex();
		AttribFloat3( RFX2 * +Z, RFY2 * +Z, Z );
		AttribFloat4( &ColorMod.X );
		PolyVertex();
		AttribFloat3( RFX2 * -Z, RFY2 * +Z, Z );
		AttribFloat4( &ColorMod.X );
		PolyVertex();
	EndPoly();

	glEnable( GL_DEPTH_TEST );

	CurrentShaderFlags &= ~SF_VtxColor;

	unguard;
}

void UNOpenGLESRenderDevice::PushHit( const BYTE* Data, INT Count )
{

}

void UNOpenGLESRenderDevice::PopHit( INT Count, UBOOL bForce )
{

}

void UNOpenGLESRenderDevice::GetStats( TCHAR* Result )
{
	guard(UNOpenGLESRenderDevice::GetStats)

	if( Result ) *Result = '\0';

	unguard;
}

void UNOpenGLESRenderDevice::ReadPixels( FColor* Pixels )
{
	guard(UNOpenGLESRenderDevice::ReadPixels);

	glPixelStorei( GL_UNPACK_ALIGNMENT, 0 );
	glReadPixels( 0, 0, Viewport->SizeX, Viewport->SizeY, GL_RGBA, GL_UNSIGNED_BYTE, (void*)Pixels );

	// Swap RGBA -> BGRA.
	FColor* Ptr = Pixels;
	for( INT Y = 0; Y < Viewport->SizeY; ++Y )
	{
		for( INT X = 0; X < Viewport->SizeX; ++X, ++Ptr )
		{
			FColor Old = *Ptr;
			Ptr->R = Old.B;
			Ptr->G = Old.G;
			Ptr->B = Old.R;
		}
	}

	unguard;
}

void UNOpenGLESRenderDevice::ClearZ( FSceneNode* Frame )
{
	guard(UNOpenGLESRenderDevice::ClearZ);

	FlushTriangles();
	SetBlend( PF_Occlude );
	SetShader( CurrentShaderFlags );

	glClear( GL_DEPTH_BUFFER_BIT );

	unguard;
}

void UNOpenGLESRenderDevice::UpdateUniforms()
{
	guard(UNOpenGLESRenderDevice::UpdateUniforms);

	if( UniformsChanged[UF_Mtx] )
	{
		FlushTriangles();
		glUniformMatrix4fv( ShaderInfo->Uniforms[UF_Mtx], 1, GL_FALSE, &MtxMVP[0][0] );
		UniformsChanged[UF_Mtx] = false;
	}

	if( UniformsChanged[UF_Brightness] )
	{
		FlushTriangles();
		glUniform1f( ShaderInfo->Uniforms[UF_Brightness], CurrentBrightness );
		UniformsChanged[UF_Brightness] = false;
	}

	for( INT i = UF_Texture0; i <= UF_Texture3; ++i )
	{
		if( UniformsChanged[i] && ShaderInfo->Uniforms[i] >= 0 )
		{
			glUniform1i( ShaderInfo->Uniforms[i], i - UF_Texture0 );
			UniformsChanged[i] = false;
		}
	}

	unguard;
}

GLuint UNOpenGLESRenderDevice::CompileShader( GLenum Type, const char* Text )
{
	guard(UNOpenGLESRenderDevice::CompileShader);

	GLuint Id = glCreateShader( Type );

	const char *Src[] = { Text, NULL };
	glShaderSource( Id, 1, Src, NULL );

	glCompileShader( Id );

	GLint Status = 0;
	glGetShaderiv( Id, GL_COMPILE_STATUS, &Status );
	if( !Status )
	{
		char Tmp[2048];
		glGetShaderInfoLog( Id, sizeof(Tmp), NULL, Tmp );
		appErrorf( "%s shader compilation failed:\n%s", ( Type == GL_FRAGMENT_SHADER ) ? "Fragment" : "Vertex", Tmp );
	}

	return Id;

	unguard;
}

UNOpenGLESRenderDevice::FCachedShader* UNOpenGLESRenderDevice::CreateShader( DWORD ShaderFlags )
{
	guard(UNOpenGLESRenderDevice::CreateShader);

	static const char* FlagNames[SF_Count] = {
		"SF_Texture0", "SF_Texture1", "SF_Texture2", "SF_Texture3",
		"SF_VtxColor", "SF_AlphaTest", "SF_Lightmap", "SF_Fogmap",
		"SF_Detail", "SF_VtxFog", "SF_ModulatedDecalMask"
	};

	static const char* UniformNames[UF_Count] = {
		"uMtx", "uBrightness", "uTexture0", "uTexture1", "uTexture2", "uTexture3"
	};

	static const char* AttribNames[AT_Count] = {
		"aPosition", "aTexCoord0", "aTexCoord1", "aTexCoord2",
		"aTexCoord3", "aVtxColor", "aVtxFog"
	};

	static const DWORD AttribFlags[AT_Count] = {
		0, SF_Texture0, SF_Texture1, SF_Texture2, SF_Texture3, SF_VtxColor, SF_VtxFog
	};

	static const char* ShaderVersion = "#version 100\n";

	FCachedShader NewShaderValue;
	NewShaderValue.Flags = ShaderFlags;
	FCachedShader& NewShaderRef = ShaderMap.Set( ShaderFlags, NewShaderValue );
	FCachedShader* NewShader = &NewShaderRef;

	FString VSText;
	FString FSText;

	VSText += ShaderVersion;
	FSText += ShaderVersion;

	for( DWORD Flag = 1, FlagNum = 0; Flag <= SF_Max; Flag <<= 1, ++FlagNum )
	{
		if( ShaderFlags & Flag )
		{
			TCHAR Buf[256];
			appSprintf( Buf, TEXT("#define %s %u\n"), FlagNames[FlagNum], Flag );
			VSText += Buf;
			FSText += Buf;
		}
	}

	if( Overbright )
	{
		TCHAR Buf[256];
		appSprintf( Buf, TEXT("#define LIGHTMAP_OVERBRIGHT %f\n"), LIGHTMAP_OVERBRIGHT );
		FSText += Buf;
	}

	VSText += VertShaderGLSL;
	FSText += FragShaderGLSL;

	GLuint VS = CompileShader( GL_VERTEX_SHADER, *VSText );
	GLuint FS = CompileShader( GL_FRAGMENT_SHADER, *FSText );

	GLuint Prog = glCreateProgram();
	glAttachShader( Prog, VS );
	glAttachShader( Prog, FS );

	NewShader->NumFloats = 0;
	for( INT i = 0; i < AT_Count; ++i )
	{
		if( i == 0 || ( ShaderFlags & AttribFlags[i] ) )
		{
			glBindAttribLocation( Prog, i, AttribNames[i] );
			NewShader->Attribs[i] = true;
			NewShader->NumFloats += AttribSizes[i];
		}
		else
		{
			NewShader->Attribs[i] = false;
		}
	}

	glLinkProgram( Prog );

	GLint Status = 0;
	glGetProgramiv( Prog, GL_LINK_STATUS, &Status );
	if( !Status )
	{
		char Tmp[2048];
		glGetProgramInfoLog( Prog, sizeof(Tmp), NULL, Tmp );
		appErrorf( "Failed to link shader %08x:\n%s", ShaderFlags, Tmp );
	}

	glDeleteShader( VS );
	glDeleteShader( FS );

	NewShader->Prog = Prog;

	for( INT i = 0; i < UF_Count; ++i )
		NewShader->Uniforms[i] = glGetUniformLocation( NewShader->Prog, UniformNames[i] );

	return NewShader;
	unguard;
}

void UNOpenGLESRenderDevice::SetShader( DWORD ShaderFlags )
{
	guard(UNOpenGLESRenderDevice::SetShader);

	if( !ShaderInfo || ShaderInfo->Flags != ShaderFlags )
	{
		FlushTriangles();

		ShaderInfo = ShaderMap.Find( ShaderFlags );
		if( !ShaderInfo )
			ShaderInfo = CreateShader( ShaderFlags );
		verify( ShaderInfo );

		// TODO: probably don't do this every program change
		for( INT i = 0; i < UF_Count; ++i )
			UniformsChanged[i] = true;

		BYTE* Ptr = UseVAO ? nullptr : (BYTE*)VtxData;
		for( INT i = 0; i < AT_Count; ++i )
		{
			if( ShaderInfo->Attribs[i] )
			{
				glEnableVertexAttribArray( i );
				glVertexAttribPointer( i, AttribSizes[i], GL_FLOAT, GL_FALSE, ShaderInfo->NumFloats * sizeof(FLOAT), (void*)Ptr );
				Ptr += AttribSizes[i] * sizeof(FLOAT);
			}
			else
			{
				glDisableVertexAttribArray( i );
			}
		}

		glUseProgram( ShaderInfo->Prog );
	}

	UpdateUniforms();

	unguard;
}

void UNOpenGLESRenderDevice::SetSceneNode( FSceneNode* Frame )
{
	guard(UNOpenGLESRenderDevice::SetSceneNode);

	check(Viewport);

	if( !Frame )
	{
		// invalidate current saved data
		CurrentSceneNode.X = -1;
		CurrentSceneNode.FX = -1.f;
		CurrentSceneNode.SizeX = -1;
		return;
	}

	if( Frame->X != CurrentSceneNode.X || Frame->Y != CurrentSceneNode.Y ||
			Frame->XB != CurrentSceneNode.XB || Frame->YB != CurrentSceneNode.YB ||
			Viewport->SizeX != CurrentSceneNode.SizeX || Viewport->SizeY != CurrentSceneNode.SizeY )
	{
		FlushTriangles();
#ifdef PLATFORM_ANDROID
		// UT99_ANDROID_V62_CLIPPED_PREVIEW_VIEWPORT:
		// Keep the root scene fullscreen, but honor non-root/clipped FSceneNode
		// rectangles such as UCanvas::DrawClippedActor used by the UT player preview.
		// Coordinates are scaled from Unreal's logical viewport to the actual EGL
		// drawable, so this also survives devices where drawable != logical size.
		UBOOL bClippedScene = (Frame->XB != 0 || Frame->YB != 0 || (Frame->X < Viewport->SizeX && Frame->Y < Viewport->SizeY));
			// UT99_ANDROID_V63_ROOT_SAFEAREA_GUARD:
			// Some root scene nodes use a shorter safe-area height.  Do not treat those as
			// UWindow preview rectangles; only real clipped sub-rectangles get scissored.
		if( bClippedScene && Viewport->SizeX > 0 && Viewport->SizeY > 0 )
		{
			INT DrawableW = 0;
			INT DrawableH = 0;
			UT99AndroidGetDrawableSizeV27( Viewport, DrawableW, DrawableH );
			if( DrawableW <= 0 ) DrawableW = Viewport->SizeX;
			if( DrawableH <= 0 ) DrawableH = Viewport->SizeY;

			const FLOAT ScaleX = (FLOAT)DrawableW / (FLOAT)Viewport->SizeX;
			const FLOAT ScaleY = (FLOAT)DrawableH / (FLOAT)Viewport->SizeY;
			INT GLX = appRound( Frame->XB * ScaleX );
			INT GLY = appRound( (Viewport->SizeY - Frame->Y - Frame->YB) * ScaleY );
			INT GLW = Max<INT>( 1, appRound( Frame->X * ScaleX ) );
			INT GLH = Max<INT>( 1, appRound( Frame->Y * ScaleY ) );

			GLX = Clamp<INT>( GLX, 0, Max<INT>(0, DrawableW-1) );
			GLY = Clamp<INT>( GLY, 0, Max<INT>(0, DrawableH-1) );
			GLW = Clamp<INT>( GLW, 1, Max<INT>(1, DrawableW-GLX) );
			GLH = Clamp<INT>( GLH, 1, Max<INT>(1, DrawableH-GLY) );

			glViewport( GLX, GLY, GLW, GLH );
			glEnable( GL_SCISSOR_TEST );
			glScissor( GLX, GLY, GLW, GLH );

			static INT ClippedLogCount = 0;
			if( ClippedLogCount < 24 )
			{
				UT99_ANDROID_GLES_LOGI_V28("v62 clipped preview viewport frame=%dx%d+%d,%d gl=%dx%d+%d,%d drawable=%dx%d logical=%dx%d",
					Frame->X, Frame->Y, Frame->XB, Frame->YB, GLW, GLH, GLX, GLY, DrawableW, DrawableH, Viewport->SizeX, Viewport->SizeY);
				ClippedLogCount++;
			}
		}
		else
		{
			UT99AndroidForceDrawableViewportV28( Viewport, "SetSceneNodeRoot", Frame->X, Frame->Y );
		}
#else
		glViewport( Frame->XB, Viewport->SizeY - Frame->Y - Frame->YB, Frame->X, Frame->Y );
#endif
		CurrentSceneNode.X = Frame->X;
		CurrentSceneNode.Y = Frame->Y;
		CurrentSceneNode.XB = Frame->XB;
		CurrentSceneNode.YB = Frame->YB;
		CurrentSceneNode.SizeX = Viewport->SizeX;
		CurrentSceneNode.SizeY = Viewport->SizeY;
	}

#ifdef PLATFORM_ANDROID
	{
		UBOOL ConfigWidescreenFOV = UT99AndroidReadWidescreenFOVConfigV159( WidescreenFOV );
		if( ConfigWidescreenFOV != WidescreenFOV )
		{
			WidescreenFOV = ConfigWidescreenFOV;
			CurrentSceneNode.FX = -1.f;
			CurrentSceneNode.FY = -1.f;
			CurrentSceneNode.FovAngle = -1.f;
			UT99_ANDROID_GLES_LOGI_V28("UT99_ANDROID_V159_WIDESCREEN_FOV_CONFIG live=%d frame=%.0fx%.0f", WidescreenFOV ? 1 : 0, Frame->FX, Frame->FY);
		}
	}
#endif

	FLOAT EffectiveFovAngle = Viewport->Actor->FovAngle;
	FLOAT EffectiveRProjZ = appTan( EffectiveFovAngle * PI / 360.0 );
	if( Frame->FX > 0.0f && Frame->Proj.Z > 0.0f )
	{
		// UT99_ANDROID_V161_WIDESCREEN_FOV_SCENENODE:
		// FSceneNode now owns the real FoV.  Consuming Frame->Proj keeps CPU culling,
		// mesh/sprite projection and GLES projection in sync instead of just shrinking
		// the already-built 90-degree scene in the render device.
		EffectiveRProjZ = (0.5f * Frame->FX) / Frame->Proj.Z;
		EffectiveFovAngle = (FLOAT)( appAtan( EffectiveRProjZ ) * 360.0 / PI );
	}

	if( Frame->FX != CurrentSceneNode.FX || Frame->FY != CurrentSceneNode.FY ||
			EffectiveFovAngle != CurrentSceneNode.FovAngle )
	{
		RProjZ = EffectiveRProjZ;
		Aspect = Frame->FY / Frame->FX;
		RFX2 = 2.0f * RProjZ / Frame->FX;
		RFY2 = 2.0f * RProjZ * Aspect / Frame->FY;
		MtxProj = glm::frustum( -RProjZ, +RProjZ, -Aspect * RProjZ, +Aspect * RProjZ, 1.f, 32768.f );
		MtxMVP = MtxProj * MtxModelView;
		CurrentSceneNode.FX = Frame->FX;
		CurrentSceneNode.FY = Frame->FY;
		CurrentSceneNode.FovAngle = EffectiveFovAngle;
#ifdef PLATFORM_ANDROID
		static INT FovLogCount = 0;
		if( WidescreenFOV && FovLogCount < 24 )
		{
			UT99_ANDROID_GLES_LOGI_V28("UT99_ANDROID_V161_WIDESCREEN_FOV_SCENENODE base=%.2f effective=%.2f frame=%.0fx%.0f projZ=%.2f",
				Viewport->Actor->FovAngle, EffectiveFovAngle, Frame->FX, Frame->FY, Frame->Proj.Z);
			FovLogCount++;
		}
#endif
		UniformsChanged[UF_Mtx] = true;
	}

	unguard;
}

void UNOpenGLESRenderDevice::SetBlend( DWORD PolyFlags, UBOOL InverseOrder )
{
	guard(UNOpenGLESRenderDevice::SetBlend);

	// Adjust PolyFlags according to Unreal's precedence rules.
	if( !(PolyFlags & (PF_Translucent|PF_Modulated)) )
		PolyFlags |= PF_Occlude;
	else if( PolyFlags & PF_Translucent )
	{
#ifdef PLATFORM_ANDROID
		// UT99_ANDROID_V167U_TRANSLUCENT_MASKED_SMOKE_ALPHA:
		// UT's thrown-rocket/grenade smoke path can arrive as a translucent
		// sprite while the smoke texture itself still relies on PF_Masked/index-0
		// cutout.  The old GLES precedence rule stripped PF_Masked for every
		// translucent draw.  That keeps additive/translucent blending but also
		// disables the alpha-test shader bit, so the transparent edge texels of
		// smoke sprites become a visible square frame.
		// Keep PF_Masked on Android when content explicitly asks for both flags:
		// SetTexture already uploads a masked cache variant, and the shader then
		// discards index-0 pixels while the normal translucent blend remains active.
		static INT AndroidTranslucentMaskedSmokeLogCountV167U = 0;
		if( ( PolyFlags & PF_Masked ) && AndroidTranslucentMaskedSmokeLogCountV167U < 12 )
		{
			UT99_ANDROID_GLES_LOGI("UT99_ANDROID_V167U_TRANSLUCENT_MASKED_SMOKE_ALPHA preserve translucent masked alpha poly=0x%08x", (unsigned int)PolyFlags);
			AndroidTranslucentMaskedSmokeLogCountV167U++;
		}
#else
		PolyFlags &= ~PF_Masked;
#endif
	}

	// Detect changes in the blending modes.
	DWORD Xor = CurrentPolyFlags ^ PolyFlags;
	if( Xor & (PF_Translucent|PF_Modulated|PF_Invisible|PF_Occlude|PF_Masked|PF_Highlighted) )
	{
		FlushTriangles();
		if( Xor & (PF_Translucent|PF_Modulated|PF_Highlighted) )
		{
			glEnable( GL_BLEND );
#ifdef PLATFORM_ANDROID
    #define UT99_ANDROID_V27_DISABLE_SCISSOR 1
    glDisable( GL_SCISSOR_TEST );
#endif
			if( PolyFlags & PF_Translucent )
			{
				glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_COLOR );
			}
			else if( PolyFlags & PF_Modulated )
			{
				glBlendFunc( GL_DST_COLOR, GL_SRC_COLOR );
			}
			else if( PolyFlags & PF_Highlighted )
			{
				glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
			}
			else
			{
				glDisable( GL_BLEND );
				glBlendFunc( GL_ONE, GL_ZERO );
			}
		}
		if( Xor & PF_Invisible )
		{
			UBOOL Show = !( PolyFlags & PF_Invisible );
			glColorMask( Show, Show, Show, Show );
		}
		if( Xor & PF_Occlude )
			glDepthMask( (PolyFlags & PF_Occlude) != 0 );
		if( Xor & PF_Masked )
		{
			if( PolyFlags & PF_Masked )
				CurrentShaderFlags |= SF_AlphaTest;
			else
				CurrentShaderFlags &= ~SF_AlphaTest;
		}
	}

	CurrentPolyFlags = PolyFlags;

	unguard;
}

void UNOpenGLESRenderDevice::UpdateTextureFilter( const FTextureInfo& Info, DWORD PolyFlags )
{
	guard(UNOpenGLESRenderDevice::UpdateTextureFilter);

	// Set mip filtering if there are mips.
	if( ( PolyFlags & PF_NoSmooth ) || ( NoFiltering && Info.Palette ) ) // TODO: This is set per poly, not per texture.
	{
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ( Info.NumMips > 1 ) ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	}
	else
	{
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ( Info.NumMips > 1 ) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	}

	// This is a light/fog map.
	if( !Info.Palette )
	{
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	}

	unguard;
}

void UNOpenGLESRenderDevice::ResetTexture( INT TMU )
{
	guard(UNOpenGLESRenderDevice::ResetTexture);

	CurrentShaderFlags &= ~(1 << TMU);

	if( TexInfo[TMU].CurrentCacheID != 0 )
	{
		FlushTriangles();
		glActiveTexture( GL_TEXTURE0 + TMU );
		glBindTexture( GL_TEXTURE_2D, 0 );
		TexInfo[TMU].CurrentCacheID = 0;
	}

	unguard;
}

void UNOpenGLESRenderDevice::SetTexture( INT TMU, FTextureInfo& Info, DWORD PolyFlags, FLOAT PanBias )
{
	guard(UNOpenGLESRenderDevice::SetTexture);

	CurrentShaderFlags |= 1 << TMU;

	// Set panning.
	FTexInfo& Tex = TexInfo[TMU];
	Tex.UPan      = Info.Pan.X + PanBias*Info.UScale;
	Tex.VPan      = Info.Pan.Y + PanBias*Info.VScale;

	// Account for all the impact on scale normalization.
	Tex.UMult = 1.f / (Info.UScale * static_cast<FLOAT>(Info.USize));
	Tex.VMult = 1.f / (Info.VScale * static_cast<FLOAT>(Info.VSize));

	// Find in cache.
	QWORD NewCacheID = Info.CacheID;
	if( ( PolyFlags & PF_Masked ) && Info.Palette )
		NewCacheID |= MASKED_TEXTURE_TAG;
	UBOOL RealtimeChanged = Info.bRealtimeChanged;
	if( NewCacheID == Tex.CurrentCacheID && !RealtimeChanged )
		return;

	FlushTriangles();

	// Make current.
	Tex.CurrentCacheID = NewCacheID;
	FCachedTexture* Bind = BindMap.Find( NewCacheID );
	FCachedTexture* OldBind = Bind;
	if( !Bind )
	{
		// New texture.
		FCachedTexture NewTexture;
		FCachedTexture& TexRef = BindMap.Set( NewCacheID, NewTexture );
		Bind = &TexRef;
		glGenTextures( 1, &Bind->Id );
		TexAlloc.AddItem( Bind->Id );
	}

	glActiveTexture( GL_TEXTURE0 + TMU );
	glBindTexture( GL_TEXTURE_2D, Bind->Id );

	if( !OldBind || RealtimeChanged )
	{
		// New texture or it has changed, upload it.
		Info.bRealtimeChanged = 0;
		UploadTexture( Info, ( PolyFlags & PF_Masked ), !OldBind );
		// TODO: This depends on PolyFlags, not Info.
		UpdateTextureFilter( Info, PolyFlags );
	}

	unguard;
}

void UNOpenGLESRenderDevice::UploadTexture( FTextureInfo& Info, UBOOL Masked, UBOOL NewTexture )
{
	guard(UNOpenGLESRenderDevice::UploadTexture);

	if( !Info.Mips[0] )
	{
		debugf( NAME_Warning, "Encountered texture with invalid mips!" );
		return;
	}

	// We're gonna be using the compose buffer, so expand it to fit.
	INT NewComposeSize = Info.Mips[0]->USize * Info.Mips[0]->VSize * 4;
	if( NewComposeSize > ComposeSize )
	{
		Compose = (BYTE*)appRealloc( Compose, NewComposeSize, "GLComposeBuf" );
		verify( Compose );
	}

	// Upload all mips.
	for( INT MipIndex = 0; MipIndex < Info.NumMips; ++MipIndex )
	{
		FMipmapBase* Mip = Info.Mips[MipIndex];
		if( !Mip || !Mip->DataPtr ) break;
		BYTE* UploadBuf;
		GLenum UploadFormat;
		// Convert texture if needed.
		if( Info.Palette )
		{
			// 8-bit indexed. We have to fix the alpha component since it's mostly garbage in non-detailmaps.
			UploadBuf = Compose;
			UploadFormat = GL_RGBA;
			DWORD* Dst = (DWORD*)Compose;
			const BYTE* Src = (const BYTE*)Mip->DataPtr;
			const DWORD* Pal = (const DWORD*)Info.Palette;
			const DWORD Count = Mip->USize * Mip->VSize;
			if( Masked )
			{
				// index 0 is transparent
				for( DWORD i = 0; i < Count; ++i, ++Src )
					*Dst++ = *Src ? ( Pal[*Src] | ALPHA_MASK ) : 0;
			}
			else
			{
				// index 0 is whatever
				for( DWORD i = 0; i < Count; ++i )
					*Dst++ = ( Pal[*Src++] | ALPHA_MASK );
			}
		}
		else if( UseBGRA )
		{
			// BGRA8888 (or 7777) and we can upload it as-is.
			UploadBuf = Mip->DataPtr;
			UploadFormat = GL_BGRA_EXT;
		}
		else
		{
			// BGRA8888 (or 7777), but we must swap it because it's not supported natively.
			UploadBuf = Compose;
			UploadFormat = GL_RGBA;
			BYTE* Dst = (BYTE*)Compose;
			const BYTE* Src = (const BYTE*)Mip->DataPtr;
			const DWORD Count = Mip->USize * Mip->VSize;
			for( DWORD i = 0; i < Count; ++i, Src += 4 )
			{
				*Dst++ = Src[2];
				*Dst++ = Src[1];
				*Dst++ = Src[0];
				*Dst++ = Src[3];
			}
		}
		// Upload to GL.
		if( NewTexture )
			glTexImage2D( GL_TEXTURE_2D, MipIndex, UploadFormat, Mip->USize, Mip->VSize, 0, UploadFormat, GL_UNSIGNED_BYTE, (void*)UploadBuf );
		else
			glTexSubImage2D( GL_TEXTURE_2D, MipIndex, 0, 0, Mip->USize, Mip->VSize, UploadFormat, GL_UNSIGNED_BYTE, (void*)UploadBuf );
	}

	unguard;
}
