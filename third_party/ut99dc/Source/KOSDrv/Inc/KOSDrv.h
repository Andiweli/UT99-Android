#include <kos.h>
#include "Engine.h"

/*-----------------------------------------------------------------------------
	Defines.
-----------------------------------------------------------------------------*/

#ifdef KOSDRV_EXPORTS
#define KOSDRV_API DLL_EXPORT
#else
#define KOSDRV_API DLL_IMPORT
#endif

#define MAX_JOY_BTNS 16


/*-----------------------------------------------------------------------------
	UKOSViewport.
-----------------------------------------------------------------------------*/

extern KOSDRV_API UBOOL GTickDue;

//
// A SDL2 viewport.
//
class KOSDRV_API UKOSViewport : public UViewport
{
	DECLARE_CLASS( UKOSViewport, UViewport, CLASS_Transient )
	NO_DEFAULT_CONSTRUCTOR( UKOSViewport )

	// Constructors.
	UKOSViewport( ULevel* InLevel, class UKOSClient* InClient );
	static void InternalClassInitializer( UClass* Class );

	// UObject interface.
	virtual void Destroy();

	// UViewport interface.
	virtual UBOOL Lock( FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* HitData=NULL, INT* HitSize=NULL );
	virtual void Unlock( UBOOL Blit=0 );
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog );
	virtual void Repaint( UBOOL Blit );
	virtual void SetModeCursor();
	virtual void UpdateWindowFrame();
	virtual void OpenWindow( DWORD ParentWindow, UBOOL Temporary, INT NewX, INT NewY, INT OpenX, INT OpenY );
	virtual void CloseWindow();
	virtual void UpdateInput( UBOOL Reset );
	virtual void* GetWindow();
	virtual void SetMouseCapture( UBOOL Capture, UBOOL Clip, UBOOL FocusOnly=0 );
	virtual UBOOL IsFullscreen();
	virtual UBOOL ResizeViewport( DWORD BlitType, INT X=INDEX_NONE, INT Y=INDEX_NONE, INT ColorBytes=INDEX_NONE );

	// UKOSViewport interface.
	void SetClientSize( INT NewX, INT NewY, UBOOL UpdateProfile );
	void MakeFullscreen( INT NewX, INT NewY, UBOOL UpdateProfile );
	void EndFullscreen();
	void TickJoystick( maple_device_t* Dev, const FLOAT DeltaTime );
	void TickKeyboard( maple_device_t* Dev, const FLOAT DeltaTime );
	void TickMouse( maple_device_t* Dev, const FLOAT DeltaTime );
	UBOOL TickInput(); // returns true if the viewport has requested death

private:
	// Static variables.
	static BYTE KeyMap[KBD_MAX_KEYS]; // DC keycode -> EInputKey map
	static const BYTE JoyBtnMapGame[MAX_JOY_BTNS]; // DC controller button bit -> EInputKey (game) map
	static const BYTE JoyBtnMapUI[MAX_JOY_BTNS];   // DC controller button bit -> EInputKey (UI) map

	// Variables.
	BYTE KeyState[KBD_MAX_KEYS]; // Current keys held
	BYTE KeyStatePrev[KBD_MAX_KEYS]; // Previous keys held
	BYTE KbdModsPrev;  // Previous modifier state (kbd_mods_t::raw)
	DWORD JoyState;      // Current buttons bitmask
	DWORD MouseButtons;  // Previous mouse buttons bitmask (for edge events)
	FLOAT MenuNavTimerX; // Menu repeat timers
	FLOAT MenuNavTimerY;
	INT MenuNavDirX;     // -1,0,1
	INT MenuNavDirY;     // -1,0,1
	UBOOL bWasInMenu;    // Track menu entry/exit for cursor centering and timer reset
	UBOOL bWasInUWindow; // Track UWindow frontend state (for UI-specific hacks like B->Space exec)
	FName LastConsoleState; // Track Console state changes (UWindow/Menuing/etc)
	class UKOSClient* Client;
	UBOOL Destroyed;
	UBOOL QuitRequested;
	FLOAT InputUpdateTime;
public:
	INT HoldCount;
private:

	// Info saved during captures and fullscreen sessions.
	INT SavedX, SavedY;

	// UKOSViewport private methods.
	UBOOL CauseInputEvent( INT iKey, EInputAction Action, FLOAT Delta=0.0 );
	UBOOL JoystickAxisEvent( FLOAT RawValue, FLOAT Center, FLOAT Range, EInputKey Key, FLOAT Scale, UBOOL DeadZone );
	static void InitKeyMap();
};

/*-----------------------------------------------------------------------------
	UKOSClient.
-----------------------------------------------------------------------------*/

//
// SDL2 implementation of the client.
//
class KOSDRV_API UKOSClient : public UClient, public FNotifyHook
{
	DECLARE_CLASS( UKOSClient, UClient, CLASS_Transient|CLASS_Config )
	friend class UKOSViewport;

	// Configuration.
	INT DefaultDisplay;
	UBOOL StartupFullscreen;
	UBOOL UseJoystick;
	UBOOL InvertY;
	UBOOL InvertV;
	FLOAT ScaleXYZ;
	FLOAT ScaleRUV;
	FLOAT DeadZoneXYZ;
	FLOAT DeadZoneRUV;

	// Constructors.
	UKOSClient();
	static void InternalClassInitializer( UClass* Class );

	// UObject interface.
	virtual void Destroy();
	virtual void PostEditChange();
	virtual void ShutdownAfterError();

	// UClient interface.
	virtual void Init( UEngine* InEngine );
	virtual void ShowViewportWindows( DWORD ShowFlags, int DoShow );
	virtual void EnableViewportWindows( DWORD ShowFlags, int DoEnable );
	virtual void Tick();
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog );
	virtual UViewport* NewViewport( const FName Name );
	virtual void MakeCurrent( UViewport* InViewport );

	// Additional client helpers.
	virtual void Poll();
	virtual UViewport* CurrentViewport();
	virtual void EndFullscreen();

	// UKOSClient interface.
	void TryRenderDevice( UViewport* Viewport, const char* ClassName, UBOOL Fullscreen );

protected:
	// Variables.
	UViewport* FullscreenViewport;
};
