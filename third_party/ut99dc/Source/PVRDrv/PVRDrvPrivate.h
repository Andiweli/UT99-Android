/*------------------------------------------------------------------------------------
	Dependencies.
------------------------------------------------------------------------------------*/

#include <dc/pvr.h>
#include "RenderPrivate.h"

/*------------------------------------------------------------------------------------
	PVR rendering private definitions.
------------------------------------------------------------------------------------*/

//
// Fixed function PVR renderer for the Dreamcast.
//
class DLL_EXPORT UPVRRenderDevice : public URenderDevice
{
	DECLARE_CLASS(UPVRRenderDevice, URenderDevice, CLASS_Config)

	static constexpr INT MaxMipLevel = 0;
	static constexpr INT MinTexSize = 8;

	// Options.
	UBOOL NoFiltering;

    // All currently cached textures (CacheID -> VRAM ptr + last type).
	struct FTexBind
	{
        pvr_ptr_t Tex;
		BYTE LastType;
		INT  SizeBytes;
	};
	TMap<QWORD, FTexBind> BindMap;

	struct FTexInfo
	{
		QWORD CurrentCacheID;
		FTexBind* CurrentBind;
		FLOAT UMult;
		FLOAT VMult;
		FLOAT UPan;
		FLOAT VPan;
		UBOOL bIsTile;
	} TexInfo;

	// Texture upload buffer;
	BYTE* Compose;
	DWORD ComposeSize;

	DWORD CurrentPolyFlags;
	FLOAT RProjZ, Aspect;
	FLOAT RFX2, RFY2;
	FPlane ColorMod;
	DWORD VRAMUsed;

	struct FCachedSceneNode
	{
		FLOAT FovAngle;
		FLOAT FX, FY;
		INT X, Y;
		INT XB, YB;
		INT SizeX, SizeY;
		UBOOL bIsSky;
	} CurrentSceneNode;

	// Constructors.
	UPVRRenderDevice();
	static void InternalClassInitializer( UClass* Class );

	// URenderDevice interface.
	virtual UBOOL Init( UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen ) override;
	virtual UBOOL SetRes( INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen ) override;
	virtual void Exit() override;
	virtual void Flush( UBOOL AllowPrecache ) override;
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar ) override;
	virtual void Lock( FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* HitData, INT* HitSize ) override;
	virtual void Unlock( UBOOL Blit ) override;
	virtual void DrawComplexSurface( FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet ) override;
	virtual void DrawGouraudPolygon( FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, int NumPts, DWORD PolyFlags, FSpanBuffer* Span ) override;
	virtual void DrawTile( FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags ) override;
	virtual void EndFlash() override;
	virtual void GetStats( TCHAR* Result ) override;
	virtual void Draw2DLine( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2 ) override;
	virtual void Draw2DPoint( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z ) override;
	virtual void PushHit( const BYTE* Data, INT Count ) override;
	virtual void PopHit( INT Count, UBOOL bForce ) override;
	virtual void ReadPixels( FColor* Pixels ) override;
	virtual void ClearZ( FSceneNode* Frame ) override;

	// UGLDCRenderDevice interface.
	void SetSceneNode( FSceneNode* Frame );
	void SetBlend( DWORD PolyFlags, UBOOL InverseOrder = false );
	void SetTexture( FTextureInfo& Info, DWORD PolyFlags, FLOAT PanBias );
	void ResetTexture( );
	void UploadTexture( FTextureInfo& Info, const UBOOL NewTexture );
	void EnsureComposeSize( const DWORD NewSize );
	void* ConvertTextureMipI8( const FMipmap* Mip, const FColor* Palette );
	void* ConvertTextureMipBGRA7777( const FMipmap* Mip );
	void* VerticalUpscale( const INT USize, const INT VSize, const INT VTimes );
	void PrintMemStats() const;

public:
	// Queryors
	DWORD GetVRAMUsed() const { return VRAMUsed; }
};
