/* UT99_ANDROID_V169L_CHECKBOX_TOUCH_ROOT_PATH */
/* UT99_ANDROID_V81_CURSOR_MODE_SWITCH */
/* UT99_ANDROID_V73_RESOLUTION_SCALE_RESTORED_START_TOGGLE */
/* UT99_ANDROID_V60_MENU_BACK_PREVIEW_LOOKSCALE */
/* UT99_ANDROID_V60_NATIVE_SURFACE_AUTOSCALE */

#ifdef PLATFORM_ANDROID
#ifndef UT99_ANDROID_V47B_ANDROID_HEADERS
#define UT99_ANDROID_V47B_ANDROID_HEADERS 1
#include <jni.h>
#include <android/log.h>
#include "SDL_system.h"
static void UT99AndroidSetJavaImeWantedV76( int Wanted )
{
    JNIEnv* Env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    if( !Env )
        return;

    jclass ActivityClass = Env->FindClass( "com/ast/ut99/GameActivity" );
    if( !ActivityClass )
    {
        Env->ExceptionClear();
        return;
    }

    jmethodID SetWanted = Env->GetStaticMethodID( ActivityClass, "ut99SetImeWanted", "(Z)V" );
    if( SetWanted )
    {
        Env->CallStaticVoidMethod( ActivityClass, SetWanted, Wanted ? JNI_TRUE : JNI_FALSE );
        if( Env->ExceptionCheck() )
            Env->ExceptionClear();
    }
    else
    {
        Env->ExceptionClear();
    }
    Env->DeleteLocalRef( ActivityClass );
}

#ifndef UT99_ANDROID_SDL_LOGI
#define UT99_ANDROID_SDL_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "UT99SDL", __VA_ARGS__)
#endif
static volatile int GUT99AndroidImeOpenV79 = 0;
#endif
#endif
// UT99_ANDROID_V44_MARKER safearea look keyboard
/* UT99_ANDROID_MENU_INPUT_V43_PATCHED */
/* UT99_ANDROID_MENU_TOUCH_ONLY_V42_PATCHED */
#include <string.h>
#include <ctype.h>

#include "NSDLDrv.h"

// UT99_ANDROID_V111_OUYA_KEYBOARD_BUILDFIX: used by early keyboard helpers and later input paths.
static UBOOL GUT99V79OuyaLikeDevice = 0;
#if defined(__ANDROID__)
#include "SDL_system.h"
#endif

#if defined(__ANDROID__)
#include <android/log.h>
static void UT99AndroidForceJavaKeyboardV74( int X, int Y )
{
    // SDL_StartTextInput() is normally enough, but some Android 4.x / OUYA
    // builds do not reliably raise the IME unless SDLActivity.showTextInput()
    // is called directly on the Java side.
    JNIEnv* Env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    if( !Env )
        return;

    jclass ActivityClass = Env->FindClass( "org/libsdl/app/SDLActivity" );
    if( !ActivityClass )
    {
        Env->ExceptionClear();
        return;
    }

    jmethodID ShowTextInput = Env->GetStaticMethodID( ActivityClass, "showTextInput", "(IIII)Z" );
    if( !ShowTextInput )
    {
        Env->ExceptionClear();
        Env->DeleteLocalRef( ActivityClass );
        return;
    }

    Env->CallStaticBooleanMethod( ActivityClass, ShowTextInput, X, Y, 300, 48 );
    if( Env->ExceptionCheck() )
        Env->ExceptionClear();
    Env->DeleteLocalRef( ActivityClass );
}

static void UT99AndroidShowSoftKeyboardV44(SDL_Window* Window, int X, int Y)
{
    (void)Window;

    // UT99_ANDROID_V74_UI_EDIT_FOCUS_KEYBOARD:
    // UE1/UWindow does not call SDL_StartTextInput() when an edit field gets
    // focus, so Android never opens the soft keyboard.  Keep this gated by a
    // menu/edit-field heuristic, but use SDL + direct Java showTextInput for
    // Android 4.1/OUYA reliability.
    SDL_Rect TextRect;
    TextRect.x = ( X > 8 ) ? ( X - 8 ) : 0;
    TextRect.y = ( Y > 8 ) ? ( Y - 8 ) : 0;
    TextRect.w = 300;
    TextRect.h = 48;

    GUT99AndroidImeOpenV79 = 1;
    UT99AndroidSetJavaImeWantedV76( 1 );
    SDL_SetTextInputRect( &TextRect );
    SDL_EventState( SDL_TEXTINPUT, SDL_ENABLE );
    SDL_StartTextInput();
    UT99AndroidForceJavaKeyboardV74( TextRect.x, TextRect.y );

    UT99_ANDROID_SDL_LOGI( "v74 show Android keyboard for UWindow edit candidate x=%d y=%d", X, Y );
}

static void UT99AndroidHideSoftKeyboardV72()
{
    GUT99AndroidImeOpenV79 = 0;
    UT99AndroidSetJavaImeWantedV76( 0 );
    SDL_StopTextInput();
    SDL_EventState( SDL_TEXTINPUT, SDL_IGNORE );
}
#endif /* UT99_ANDROID_SAFEAREA_LOOK_KEYBOARD_V44 */
#include "UnRender.h"

#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
// UT99_ANDROID_V166_REAL_RENDER_RESOLUTIONS
static void UT99AndroidGetNativeDisplaySizeV166( SDL_Window* Window, INT& OutW, INT& OutH )
{
    OutW = 0;
    OutH = 0;
    INT DisplayIndex = 0;
    if( Window )
    {
        INT Found = SDL_GetWindowDisplayIndex( Window );
        if( Found >= 0 )
            DisplayIndex = Found;
    }
    SDL_DisplayMode Mode;
    if( SDL_GetDesktopDisplayMode( DisplayIndex, &Mode ) == 0 && Mode.w > 0 && Mode.h > 0 )
    {
        OutW = Max( Mode.w, Mode.h );
        OutH = Min( Mode.w, Mode.h );
        return;
    }
    if( Window )
    {
        INT W = 0;
        INT H = 0;
        SDL_GetWindowSize( Window, &W, &H );
        if( W > 0 && H > 0 )
        {
            OutW = Max( W, H );
            OutH = Min( W, H );
        }
    }
}

static FString UT99AndroidNormalizeResolutionModeV166( const TCHAR* Raw )
{
    if( !Raw )
        return FString( TEXT("Native") );
    while( *Raw==' ' || *Raw=='\t' ) Raw++;
    if( appStrnicmp( Raw, TEXT("Native"), 6 ) == 0 )
        return FString( TEXT("Native") );
    if( appStrnicmp( Raw, TEXT("100"), 3 ) == 0 )
        return FString( TEXT("Native") );
    // Retire the old percentage modes instead of preserving stale 75/50 config.
    if( appStrnicmp( Raw, TEXT("75"), 2 ) == 0 || appStrnicmp( Raw, TEXT("50"), 2 ) == 0 )
        return FString( TEXT("Native") );

    const TCHAR* XChar = appStrchr( Raw, TEXT('x') );
    if( !XChar )
        XChar = appStrchr( Raw, TEXT('X') );
    if( XChar )
    {
        INT W = appAtoi( Raw );
        INT H = appAtoi( XChar + 1 );
        if( W >= 320 && H >= 240 && W <= 4096 && H <= 2160 )
        {
            W &= ~1;
            H &= ~1;
            return FString::Printf( TEXT("%dx%d"), W, H );
        }
    }
    return FString( TEXT("Native") );
}

static FString UT99AndroidGetConfiguredResolutionModeV166()
{
    FString Mode = TEXT("Native");
    if( GConfig )
    {
        FString Value;
        if( GConfig->GetString( TEXT("NSDLDrv.NSDLClient"), TEXT("AndroidResolutionMode"), Value ) )
            Mode = UT99AndroidNormalizeResolutionModeV166( *Value );
    }
    return Mode;
}

static void UT99AndroidSaveResolutionModeV166( const FString& Mode )
{
    FString Normalized = UT99AndroidNormalizeResolutionModeV166( *Mode );
    if( GConfig )
    {
        // UT99_ANDROID_V166C_RESOLUTION_PERSIST:
        // Mirror to both the active system config and the explicit Android INI.
        // This keeps UMenu's GetCurrentRes correct immediately and makes the
        // Java-side startup reader see the same value on the next launch.
        GConfig->SetString( TEXT("NSDLDrv.NSDLClient"), TEXT("AndroidResolutionMode"), *Normalized );
        GConfig->SetString( TEXT("NSDLDrv.NSDLClient"), TEXT("AndroidResolutionMode"), *Normalized, TEXT("AndroidUT99.ini") );
        GConfig->Flush( 0 );
        GConfig->Flush( 0, TEXT("AndroidUT99.ini") );
    }
}

static UBOOL UT99AndroidResolutionModeToXYV166( const FString& Mode, INT& OutX, INT& OutY, SDL_Window* Window )
{
    FString Normalized = UT99AndroidNormalizeResolutionModeV166( *Mode );
    if( appStricmp( *Normalized, TEXT("Native") ) == 0 )
    {
        UT99AndroidGetNativeDisplaySizeV166( Window, OutX, OutY );
        return ( OutX > 0 && OutY > 0 ) ? 1 : 0;
    }
    const TCHAR* Text = *Normalized;
    const TCHAR* XChar = appStrchr( Text, TEXT('x') );
    if( !XChar )
        XChar = appStrchr( Text, TEXT('X') );
    if( XChar )
    {
        OutX = appAtoi( Text );
        OutY = appAtoi( XChar + 1 );
        return ( OutX >= 320 && OutY >= 240 ) ? 1 : 0;
    }
    return 0;
}

static UBOOL UT99AndroidParseResolutionModeTokenV166( const TCHAR* Cmd, FString& OutMode, INT& OutX, INT& OutY, SDL_Window* Window )
{
    OutX = 0;
    OutY = 0;
    if( !Cmd )
        return 0;
    while( *Cmd==' ' || *Cmd=='\t' ) Cmd++;
    if( appStrnicmp( Cmd, TEXT("Native"), 6 ) == 0 )
    {
        OutMode = TEXT("Native");
        UT99AndroidResolutionModeToXYV166( OutMode, OutX, OutY, Window );
        return 1;
    }
    const TCHAR* XChar = appStrchr( Cmd, TEXT('x') );
    if( !XChar )
        XChar = appStrchr( Cmd, TEXT('X') );
    if( XChar )
    {
        INT W = appAtoi( Cmd );
        INT H = appAtoi( XChar + 1 );
        if( W >= 320 && H >= 240 && W <= 4096 && H <= 2160 )
        {
            W &= ~1;
            H &= ~1;
            OutX = W;
            OutY = H;
            OutMode = FString::Printf( TEXT("%dx%d"), W, H );
            return 1;
        }
    }
    return 0;
}

static void UT99AndroidCallJavaResolutionModeV166( const FString& Mode )
{
    FString Normalized = UT99AndroidNormalizeResolutionModeV166( *Mode );
    JNIEnv* Env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    if( !Env )
        return;

    jclass ActivityClass = Env->FindClass( "com/ast/ut99/GameActivity" );
    if( !ActivityClass )
    {
        Env->ExceptionClear();
        return;
    }

    jmethodID ApplyMode = Env->GetStaticMethodID( ActivityClass, "ut99ApplyResolutionModeV166", "(Ljava/lang/String;)V" );
    if( ApplyMode )
    {
        const char* ModeAnsi = TCHAR_TO_ANSI( *Normalized );
        jstring JMode = Env->NewStringUTF( ModeAnsi );
        Env->CallStaticVoidMethod( ActivityClass, ApplyMode, JMode );
        if( Env->ExceptionCheck() )
            Env->ExceptionClear();
        if( JMode )
            Env->DeleteLocalRef( JMode );
        UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V166_REAL_RENDER_RESOLUTIONS requested Java render mode=%s", ModeAnsi );
    }
    else
    {
        Env->ExceptionClear();
    }
    Env->DeleteLocalRef( ActivityClass );
}

static UBOOL UT99AndroidUseAsyncResolutionCommitV219()
{
    JNIEnv* Env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    if( !Env )
        return 0;

    jclass ActivityClass = Env->FindClass( "com/ast/ut99/GameActivity" );
    if( !ActivityClass )
    {
        Env->ExceptionClear();
        return 0;
    }

    UBOOL Result = 0;
    jmethodID Method = Env->GetStaticMethodID( ActivityClass,
        "ut99UseAsyncResolutionCommitV219", "()Z" );
    if( Method )
    {
        Result = Env->CallStaticBooleanMethod( ActivityClass, Method ) ? 1 : 0;
        if( Env->ExceptionCheck() )
        {
            Env->ExceptionClear();
            Result = 0;
        }
    }
    else
    {
        Env->ExceptionClear();
    }
    Env->DeleteLocalRef( ActivityClass );
    return Result;
}

static void UT99AndroidResolutionModeLabelV166( const FString& Mode, FOutputDevice& Ar )
{
    FString Normalized = UT99AndroidNormalizeResolutionModeV166( *Mode );
    Ar.Log( *Normalized );
}

// UT99_ANDROID_ASYNC_RESOLUTION_COMMIT_V219:
// SurfaceHolder.setFixedSize()/setSizeFromLayout() is asynchronous. The old
// SetRes path called MakeFullscreen immediately after posting the Java change,
// so handhelds frequently kept the previous Unreal viewport size while Android
// had already switched to the new render buffer. Commit only after SDL reports
// the requested drawable size on the game thread.
static INT    GUT99AndroidPendingResolutionXV219 = 0;
static INT    GUT99AndroidPendingResolutionYV219 = 0;
static DOUBLE GUT99AndroidPendingResolutionSinceV219 = 0.0;
static DOUBLE GUT99AndroidPendingResolutionLastLogV219 = 0.0;

static void UT99AndroidQueueResolutionCommitV219( INT X, INT Y )
{
    GUT99AndroidPendingResolutionXV219 = X;
    GUT99AndroidPendingResolutionYV219 = Y;
    GUT99AndroidPendingResolutionSinceV219 = appSeconds();
    GUT99AndroidPendingResolutionLastLogV219 = 0.0;
    UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_ASYNC_RESOLUTION_COMMIT_V219 queued target=%dx%d", X, Y );
}
#endif

#ifdef PLATFORM_ANDROID
// UT99_ANDROID_V77_OUYA_AUTOAIM_DIRECT_USERINI:
// v76 saved the first observed OUYA PlayerPawn value.  On legacy OUYA this can
// be the class default (1.000000), so it may overwrite the user's previous Auto
// Aim value before the Preferences menu can change it.  v77 instead loads the
// User.ini value on pawn creation, applies it to the live pawn/default object,
// and only writes when the observed value really changes.  It also writes the
// canonical [Engine.PlayerPawn] entry directly to the active USERINI file.
static APlayerPawn* GUT99AndroidAutoAimActorV78 = NULL;
static FLOAT GUT99AndroidAutoAimLastV78 = -9999.0f;
static UBOOL GUT99AndroidAutoAimInitializedV78 = 0;
static UBOOL GUT99AndroidAutoAimGameMinLoggedV78 = 0;

static UBOOL UT99AndroidLoadAutoAimUserIniV78( FLOAT& OutValue )
{
    if( !GConfig )
        return 0;

    TCHAR ValueText[64];
    ValueText[0] = 0;
    if( !GConfig->GetString( TEXT("Engine.PlayerPawn"), TEXT("MyAutoAim"), ValueText, ARRAY_COUNT(ValueText), TEXT("User") ) )
        return 0;

    FLOAT Loaded = appAtof( ValueText );
    if( Loaded < 0.0f ) Loaded = 0.0f;
    if( Loaded > 1.0f ) Loaded = 1.0f;
    OutValue = Loaded;
    return 1;
}

static void UT99AndroidApplyAutoAimToPawnV78( APlayerPawn* Player, FLOAT Value )
{
    if( !Player )
        return;

    Player->MyAutoAim = Value;

    APlayerPawn* DefaultPlayer = Cast<APlayerPawn>( Player->GetClass()->GetDefaultObject() );
    if( DefaultPlayer )
        DefaultPlayer->MyAutoAim = Value;
}

static void UT99AndroidSaveAutoAimV78( APlayerPawn* Player, FLOAT Current, const char* Reason )
{
    if( !Player )
        return;

    if( Current < 0.0f ) Current = 0.0f;
    if( Current > 1.0f ) Current = 1.0f;

    UT99AndroidApplyAutoAimToPawnV78( Player, Current );

    TCHAR ValueText[64];
    appSprintf( ValueText, TEXT("%.6f"), Current );

    if( GConfig )
    {
        // Canonical PlayerPawn globalconfig location used by AndroidUser.ini.
        GConfig->SetString( TEXT("Engine.PlayerPawn"), TEXT("MyAutoAim"), ValueText, TEXT("User") );

        // Also mirror to the concrete player class section in case a mod/menu
        // resolves the preference against TournamentPlayer or another subclass.
        if( Player->GetClass() )
        {
            FString PlayerClassName = Player->GetClass()->GetPathName();
            GConfig->SetString( *PlayerClassName, TEXT("MyAutoAim"), ValueText, TEXT("User") );
        }

        GConfig->Flush( 0 );
    }

    Player->SaveConfig();
    APlayerPawn* DefaultPlayer = Cast<APlayerPawn>( Player->GetClass()->GetDefaultObject() );
    if( DefaultPlayer )
        DefaultPlayer->SaveConfig();

    UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V78_AUTOAIM_GAMEINFO_MIN_FIX saved MyAutoAim=%.6f reason=%s", Current, Reason ? Reason : "unknown" );
}

static void UT99AndroidEnsureGameAutoAimMinimumV78( APlayerPawn* Player )
{
    if( !Player || !Player->Level || !Player->Level->Game )
        return;

    // OUYA legacy config can preserve GameInfo.AutoAim=1.000000 from older
    // Android INIs.  PlayerPawn.ChangeAutoAim() clamps with FMax(Level.Game.AutoAim, F),
    // so a game-level value of 1.0 makes the Preferences Auto Aim control
    // impossible to change: every requested value becomes 1.0 again.
    // Use the stock UT lower bound 0.93 so the menu can toggle 0.93 <-> 1.0.
    if( Player->Level->Game->AutoAim > 0.930001f )
    {
        Player->Level->Game->AutoAim = 0.93f;
        if( !GUT99AndroidAutoAimGameMinLoggedV78 )
        {
            GUT99AndroidAutoAimGameMinLoggedV78 = 1;
            UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V78_AUTOAIM_GAMEINFO_MIN_FIX lowered GameInfo.AutoAim to 0.930000" );
        }
    }
}

static void UT99AndroidPersistAutoAimV78( UNSDLViewport* Viewport )
{
    if( !Viewport || !Viewport->Actor )
        return;

    APlayerPawn* Player = Viewport->Actor;
    UT99AndroidEnsureGameAutoAimMinimumV78( Player );

    APlayerPawn* DefaultPlayer = Cast<APlayerPawn>( Player->GetClass()->GetDefaultObject() );
    FLOAT Current = Player->MyAutoAim;

    // Some Preferences paths update the default object first.  Watch that too.
    if( DefaultPlayer && Abs( DefaultPlayer->MyAutoAim - Current ) > 0.0001f
        && Abs( DefaultPlayer->MyAutoAim - GUT99AndroidAutoAimLastV78 ) > 0.0001f )
    {
        Current = DefaultPlayer->MyAutoAim;
        Player->MyAutoAim = Current;
    }

    if( Player != GUT99AndroidAutoAimActorV78 )
    {
        GUT99AndroidAutoAimActorV78 = Player;
        FLOAT IniValue = 0.0f;
        if( UT99AndroidLoadAutoAimUserIniV78( IniValue ) )
        {
            UT99AndroidApplyAutoAimToPawnV78( Player, IniValue );
            GUT99AndroidAutoAimLastV78 = IniValue;
            GUT99AndroidAutoAimInitializedV78 = 1;
            UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V78_AUTOAIM_GAMEINFO_MIN_FIX applied existing MyAutoAim=%.6f reason=actor-init", IniValue );
        }
        else
        {
            GUT99AndroidAutoAimLastV78 = Current;
            GUT99AndroidAutoAimInitializedV78 = 1;
            UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V78_AUTOAIM_GAMEINFO_MIN_FIX baseline MyAutoAim=%.6f reason=actor-init-no-save", Current );
        }
        return;
    }

    if( !GUT99AndroidAutoAimInitializedV78 )
    {
        GUT99AndroidAutoAimLastV78 = Current;
        GUT99AndroidAutoAimInitializedV78 = 1;
        UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V78_AUTOAIM_GAMEINFO_MIN_FIX baseline MyAutoAim=%.6f reason=first-init-no-save", Current );
        return;
    }

    if( Abs( Current - GUT99AndroidAutoAimLastV78 ) > 0.0001f )
    {
        GUT99AndroidAutoAimLastV78 = Current;
        UT99AndroidSaveAutoAimV78( Player, Current, "changed" );
    }
}
#endif

#define UT99_ANDROID_V51D_SDL2_SOFTKEYBOARD_COMPILEFIX_BUILT 1
IMPLEMENT_CLASS( UNSDLViewport );

#ifdef PLATFORM_ANDROID
// UT99_ANDROID_V79_EDIT_FOCUS_DETECT:
// Do not use broad coordinate guessing for the Android IME.  Inspect the live
// UWindow object graph and only raise the keyboard when an editable edit box
// really has keyboard focus. This avoids dropdown/main-menu/gameplay taps
// summoning the IME and stealing controller focus.
static UBoolProperty* UT99AndroidFindBoolPropertyV79( UObject* Obj, const TCHAR* Name )
{
    if( !Obj || !Obj->GetClass() || !Name )
        return NULL;
    for( TFieldIterator<UProperty> It( Obj->GetClass() ); It; ++It )
    {
        UProperty* Prop = *It;
        if( Prop && appStricmp( Prop->GetName(), Name ) == 0 && Prop->IsA( UBoolProperty::StaticClass() ) )
            return (UBoolProperty*)Prop;
    }
    return NULL;
}

static UBOOL UT99AndroidGetBoolPropertyV79( UObject* Obj, const TCHAR* Name, UBOOL DefaultValue )
{
    UBoolProperty* BoolProp = UT99AndroidFindBoolPropertyV79( Obj, Name );
    if( !BoolProp )
        return DefaultValue;
    DWORD* Value = (DWORD*)((BYTE*)Obj + BoolProp->Offset);
    return ((*Value & BoolProp->BitMask) != 0) ? 1 : 0;
}


static INT GUT99V81NativePointerActive = 0;

// UT99_ANDROID_V91_SOFTWARE_MENU_CURSOR:
// UWindow's DrawMouse() intentionally does NOT draw the software cursor while
// bWindowsMouseAvailable is true; on PC the OS cursor is used instead. Android
// has no visible OS cursor here, so analog-driven menu mouse must keep
// bWindowsMouseAvailable false and update WindowConsole.MouseX/MouseY directly.
static UFloatProperty* UT99AndroidFindFloatPropertyV91( UObject* Obj, const TCHAR* Name )
{
    if( !Obj || !Obj->GetClass() || !Name )
        return NULL;
    for( TFieldIterator<UProperty> It( Obj->GetClass() ); It; ++It )
    {
        UProperty* Prop = *It;
        if( Prop && appStricmp( Prop->GetName(), Name ) == 0 && Prop->IsA( UFloatProperty::StaticClass() ) )
            return (UFloatProperty*)Prop;
    }
    return NULL;
}

static UBOOL UT99AndroidSetFloatPropertyV91( UObject* Obj, const TCHAR* Name, FLOAT Value )
{
    UFloatProperty* FloatProp = UT99AndroidFindFloatPropertyV91( Obj, Name );
    if( !FloatProp )
        return 0;
    FLOAT* Ptr = (FLOAT*)((BYTE*)Obj + FloatProp->Offset);
    *Ptr = Value;
    return 1;
}

static UBOOL UT99AndroidSetWindowConsoleMouseV91( FLOAT X, FLOAT Y )
{
    UBOOL Updated = 0;
    for( TObjectIterator<UObject> It; It; ++It )
    {
        UObject* Obj = *It;
        if( !Obj || !Obj->GetClass() )
            continue;
        const TCHAR* ClassName = Obj->GetClass()->GetName();
        if( !ClassName || !appStrstr( ClassName, TEXT("WindowConsole") ) )
            continue;
        UBOOL A = UT99AndroidSetFloatPropertyV91( Obj, TEXT("MouseX"), X );
        UBOOL B = UT99AndroidSetFloatPropertyV91( Obj, TEXT("MouseY"), Y );
        Updated = Updated || (A && B);
    }
    return Updated;
}

static UBOOL UT99AndroidIsNativeMouseEventV110( Uint32 Which )
{
    // UT99_ANDROID_NATIVE_MOUSE_UWINDOW_V110
    // SDL can synthesize mouse events from touch. Do not route those through the
    // native mouse path because SDL_FINGER* is already handled below. Real USB,
    // Bluetooth, Android-TV and OUYA mouse/touchpad events have their own mouse id.
#ifdef SDL_TOUCH_MOUSEID
    return Which != SDL_TOUCH_MOUSEID;
#else
    return 1;
#endif
}

static UBOOL GUT99V111NativeMouseHadLast = 0; // UT99_ANDROID_NATIVE_MOUSE_GAMEPLAY_V111
static INT GUT99V111NativeMouseLastX = 0; // UT99_ANDROID_NATIVE_MOUSE_GAMEPLAY_V111
static INT GUT99V111NativeMouseLastY = 0; // UT99_ANDROID_NATIVE_MOUSE_GAMEPLAY_V111
static UBOOL GUT99V111NativeMouseLastMenuMode = 0; // UT99_ANDROID_NATIVE_MOUSE_GAMEPLAY_V111
static DOUBLE GUT99V111SuppressSyntheticTouchUntil = 0.0; // UT99_ANDROID_NATIVE_MOUSE_GAMEPLAY_V111
static DOUBLE GUT99V111LastGameplayMotionLog = 0.0; // UT99_ANDROID_NATIVE_MOUSE_GAMEPLAY_V111
static DOUBLE GUT99V111LastSuppressTouchLog = 0.0; // UT99_ANDROID_NATIVE_MOUSE_GAMEPLAY_V111
static UBOOL GUT99V113NativeMouseSeen = 0; // UT99_ANDROID_NATIVE_MOUSE_RELATIVE_CAPTURE_V113
static UBOOL GUT99V113NativeMouseCaptureActive = 0; // UT99_ANDROID_NATIVE_MOUSE_RELATIVE_CAPTURE_V113
static DOUBLE GUT99V113LastRelativeMotionLog = 0.0; // UT99_ANDROID_NATIVE_MOUSE_RELATIVE_CAPTURE_V113
// UT99_ANDROID_CHROMEOS_MOUSE_FRAMEPACED_FLOAT_V210
// Parallel SDL Android event carrying 20.12 fixed-point relative mouse motion.
static const INT GUT99AndroidHighResMouseMagicV210 = (INT)0x554D3231;
static const FLOAT GUT99AndroidHighResMouseInvScaleV210 = 1.0f / 4096.0f;
static DOUBLE GUT99V115LastNativeMouseActivity = 0.0; // UT99_ANDROID_NATIVE_MOUSE_MENU_CLICK_STABLE_V115
static UBOOL GUT99V115MenuButtonDownValid[8] = {0,0,0,0,0,0,0,0}; // UT99_ANDROID_NATIVE_MOUSE_MENU_CLICK_STABLE_V115
static INT GUT99V115MenuButtonDownX[8] = {0,0,0,0,0,0,0,0}; // UT99_ANDROID_NATIVE_MOUSE_MENU_CLICK_STABLE_V115
static INT GUT99V115MenuButtonDownY[8] = {0,0,0,0,0,0,0,0}; // UT99_ANDROID_NATIVE_MOUSE_MENU_CLICK_STABLE_V115
static DOUBLE GUT99V115LastSuppressTouchLog = 0.0; // UT99_ANDROID_NATIVE_MOUSE_MENU_CLICK_STABLE_V115
static DOUBLE GUT99V169KMenuPointerVisibleUntil = 0.0; // UT99_ANDROID_V169K_TOUCH_IDLE_CURSOR_HIDE
static UBOOL  GUT99V169KMenuPointerNative = 0; // UT99_ANDROID_V169K_TOUCH_IDLE_CURSOR_HIDE
static DOUBLE GUT99V169KMenuPointerLastLog = 0.0; // UT99_ANDROID_V169K_TOUCH_IDLE_CURSOR_HIDE

static void UT99AndroidMenuPointerActivityV169K( UBOOL bNativePointer, const char* Source )
{
    // UT99_ANDROID_V169K_TOUCH_IDLE_CURSOR_HIDE:
    // Menu touch must not inherit the old visible mouse cursor/hover target.
    // Only a real HID/BT mouse or the virtual left-stick mouse makes a cursor
    // visible/active, and that visibility expires after a short idle window.
    GUT99V169KMenuPointerVisibleUntil = appSeconds() + 3.00;
    GUT99V169KMenuPointerNative = bNativePointer ? 1 : 0;
    if( bNativePointer )
        SDL_ShowCursor( SDL_ENABLE );

    DOUBLE Now = appSeconds();
    if( Now - GUT99V169KMenuPointerLastLog > 0.80 )
    {
        GUT99V169KMenuPointerLastLog = Now;
        UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V169K_TOUCH_IDLE_CURSOR_HIDE pointer active source=%s native=%d", Source ? Source : "?", bNativePointer ? 1 : 0 );
    }
}

static UBOOL UT99AndroidMenuPointerVisibleV169K()
{
    return appSeconds() <= GUT99V169KMenuPointerVisibleUntil ? 1 : 0;
}

static void UT99AndroidMenuPointerTouchModeV169K()
{
    GUT99V169KMenuPointerVisibleUntil = 0.0;
    GUT99V169KMenuPointerNative = 0;
    SDL_ShowCursor( SDL_DISABLE );
}

static DWORD UT99AndroidMouseButtonFlagsV110( Uint8 Button )
{
    // UT99_ANDROID_NATIVE_MOUSE_UWINDOW_V110
    switch( Button )
    {
        case SDL_BUTTON_LEFT:   return MOUSE_Left;
        case SDL_BUTTON_RIGHT:  return MOUSE_Right;
        case SDL_BUTTON_MIDDLE: return MOUSE_Middle;
        default:                return 0;
    }
}

static DWORD UT99AndroidMouseStateFlagsV110( Uint32 State )
{
    // UT99_ANDROID_NATIVE_MOUSE_UWINDOW_V110
    DWORD Flags = 0;
    if( State & SDL_BUTTON_LMASK ) Flags |= MOUSE_Left;
    if( State & SDL_BUTTON_RMASK ) Flags |= MOUSE_Right;
    if( State & SDL_BUTTON_MMASK ) Flags |= MOUSE_Middle;
    return Flags;
}

static void UT99AndroidScaleNativeMouseMotionV114( UNSDLViewport* Viewport, INT& MouseX, INT& MouseY )
{
    // UT99_ANDROID_V166_REAL_RENDER_RESOLUTIONS:
    // Java/SDLSurface usually maps OUYA/native mouse coordinates from fullscreen
    // view space into the fixed render surface already.  If an event still arrives
    // outside the current viewport, map native-display coordinates to the active
    // render buffer instead of using the retired 75/50 percentage scale.
    if( !Viewport || Viewport->SizeX <= 0 || Viewport->SizeY <= 0 )
        return;
    const INT MaxX = Max( 1, Viewport->SizeX ) - 1;
    const INT MaxY = Max( 1, Viewport->SizeY ) - 1;
    const UBOOL bOutOfViewport = ( MouseX > MaxX || MouseY > MaxY || MouseX < 0 || MouseY < 0 ) ? 1 : 0;
    if( bOutOfViewport )
    {
        INT NativeW = 0;
        INT NativeH = 0;
        UT99AndroidGetNativeDisplaySizeV166( (SDL_Window*)Viewport->GetWindow(), NativeW, NativeH ); // UT99_ANDROID_V166B_BUILD_FIX private hWnd access removed
        if( NativeW > 0 && NativeH > 0 )
        {
            MouseX = ( MouseX * Viewport->SizeX + NativeW / 2 ) / NativeW;
            MouseY = ( MouseY * Viewport->SizeY + NativeH / 2 ) / NativeH;
        }
    }
    if( MouseX < 0 ) MouseX = 0;
    if( MouseY < 0 ) MouseY = 0;
    if( MouseX > MaxX ) MouseX = MaxX;
    if( MouseY > MaxY ) MouseY = MaxY;
}

static void UT99AndroidApplyNativeMouseToUWindowV110( UNSDLViewport* Viewport, INT MouseX, INT MouseY )
{
    // UT99_ANDROID_NATIVE_MOUSE_UWINDOW_V110
    // A real Android/USB/Bluetooth mouse must update the UWindow software state
    // even when bShowWindowsMouse is false. Older UT99 Android controller-menu
    // fixes only did this in the Windows-mouse branch, so some devices could move
    // Android's pointer while UT99's menu hit-test stayed at the old position.
    if( !Viewport )
        return;
    UT99AndroidMenuPointerActivityV169K( 1, "native-mouse" );
    Viewport->WindowsMouseX = MouseX;
    Viewport->WindowsMouseY = MouseY;
    Viewport->bWindowsMouseAvailable = 1;
    Viewport->SelectedCursor = 0;
    GUT99V81NativePointerActive = 1;
    UT99AndroidSetWindowConsoleMouseV91( MouseX, MouseY );
}

static void UT99AndroidMarkNativeMouseActivityV111()
{
    // UT99_ANDROID_NATIVE_MOUSE_MENU_CLICK_STABLE_V115
    // OUYA/Android often reports one physical mouse/touchpad action twice:
    // once as SDL mouse and once as SDL finger/touch.  v114 suppressed only a
    // short tail; if relative capture was active for more than that, a delayed
    // finger-down could still launch UWindow while the user was just moving or
    // clicking the mouse in gameplay.  Keep a slightly longer mouse-activity
    // window and also suppress while relative capture is active.
    GUT99V115LastNativeMouseActivity = appSeconds();
    GUT99V111SuppressSyntheticTouchUntil = GUT99V115LastNativeMouseActivity + 2.50;
    GUT99V113NativeMouseSeen = 1; // UT99_ANDROID_NATIVE_MOUSE_RELATIVE_CAPTURE_V113
}

static UBOOL UT99AndroidShouldSuppressSyntheticTouchV111()
{
    // UT99_ANDROID_NATIVE_MOUSE_MENU_CLICK_STABLE_V115
    const DOUBLE Now = appSeconds();
    if( Now < GUT99V111SuppressSyntheticTouchUntil )
        return 1;
    if( GUT99V113NativeMouseCaptureActive && GUT99V113NativeMouseSeen )
        return 1;
    if( GUT99V115LastNativeMouseActivity > 0.0 && ( Now - GUT99V115LastNativeMouseActivity ) < 2.50 )
        return 1;
    return 0;
}


static void UT99AndroidLogSuppressSyntheticTouchV115( const char* Phase, INT MouseX, INT MouseY, UBOOL bShowWindowsMouse )
{
    // UT99_ANDROID_NATIVE_MOUSE_MENU_CLICK_STABLE_V115
    DOUBLE Now = appSeconds();
    if( Now - GUT99V115LastSuppressTouchLog > 0.50 )
    {
        GUT99V115LastSuppressTouchLog = Now;
        UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_NATIVE_MOUSE_MENU_CLICK_STABLE_V115_SUPPRESS_%s x=%d y=%d show=%d capture=%d", Phase, MouseX, MouseY, bShowWindowsMouse ? 1 : 0, GUT99V113NativeMouseCaptureActive ? 1 : 0 );
    }
}

static void UT99AndroidNativeMouseGameplayMotionV111( UNSDLViewport* Viewport, INT MouseX, INT MouseY, DWORD MouseFlags )
{
    // UT99_ANDROID_NATIVE_MOUSE_GAMEPLAY_V111
    // v110 fixed UWindow/menu hit-testing, but in gameplay the non-relative SDL
    // mouse path only updated MousePosition.  A real mouse therefore moved the
    // Android pointer but never generated MouseX/MouseY look axes.  Convert native
    // absolute mouse motion into deltas whenever the UT menu cursor is not active.
    if( !Viewport )
        return;

    const UBOOL bMenuMode = Viewport->bShowWindowsMouse ? 1 : 0;
    if( !GUT99V111NativeMouseHadLast || GUT99V111NativeMouseLastMenuMode != bMenuMode )
    {
        GUT99V111NativeMouseHadLast = 1;
        GUT99V111NativeMouseLastX = MouseX;
        GUT99V111NativeMouseLastY = MouseY;
        GUT99V111NativeMouseLastMenuMode = bMenuMode;
        return;
    }

    INT DX = MouseX - GUT99V111NativeMouseLastX;
    INT DY = MouseY - GUT99V111NativeMouseLastY;
    GUT99V111NativeMouseLastX = MouseX;
    GUT99V111NativeMouseLastY = MouseY;
    GUT99V111NativeMouseLastMenuMode = bMenuMode;

    if( bMenuMode || (DX == 0 && DY == 0) )
        return;

    DX = Clamp( DX, -180, 180 );
    DY = Clamp( DY, -140, 140 );

    Viewport->AndroidNativeMouseDeltaV112( MouseFlags, DX, DY ); // UT99_ANDROID_NATIVE_MOUSE_GAMEPLAY_BUILD_FIX_V112

    DOUBLE Now = appSeconds();
    if( Now - GUT99V111LastGameplayMotionLog > 0.80 )
    {
        GUT99V111LastGameplayMotionLog = Now;
        UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_NATIVE_MOUSE_GAMEPLAY_V112_LOOK dx=%d dy=%d pos=%d,%d flags=%lu show=%d", DX, DY, MouseX, MouseY, (unsigned long)MouseFlags, Viewport->bShowWindowsMouse ? 1 : 0 );
    }
}

static UObject* UT99AndroidFocusedEditBoxObjectV83()
{
    for( TObjectIterator<UObject> It; It; ++It )
    {
        UObject* Obj = *It;
        if( !Obj || !Obj->GetClass() )
            continue;
        const TCHAR* ClassName = Obj->GetClass()->GetName();
        if( !ClassName )
            continue;
        // UWindowEditBox and small subclasses such as NameEditBox are valid.
        if( !appStrstr( ClassName, TEXT("EditBox") ) )
            continue;
        if( !UT99AndroidGetBoolPropertyV79( Obj, TEXT("bHasKeyboardFocus"), 0 ) )
            continue;
        if( !UT99AndroidGetBoolPropertyV79( Obj, TEXT("bCanEdit"), 1 ) )
            continue;

        // UT99_ANDROID_CHROMEOS_NUMERIC_EDIT_V218:
        // Closed UWindow pages can retain bHasKeyboardFocus. The old global
        // iterator could therefore select a hidden NameEditBox while the live
        // Mouse Sensitivity UWindowEditBox was focused on another page.
        UFunction* VisibleFn = Obj->FindFunction( FName(TEXT("WindowIsVisible"), FNAME_Find) );
        if( VisibleFn )
        {
            struct FVisibleParms
            {
                DWORD ReturnValue;
            } VisibleParms;
            VisibleParms.ReturnValue = 0;
            Obj->ProcessEvent( VisibleFn, &VisibleParms );
            if( !VisibleParms.ReturnValue )
                continue;
        }
        return Obj;
    }
    return NULL;
}

static UBOOL UT99AndroidFocusedEditBoxClassContainsV169K( const TCHAR* Needle )
{
    UObject* Obj = UT99AndroidFocusedEditBoxObjectV83();
    if( !Obj || !Obj->GetClass() || !Needle )
        return 0;
    const TCHAR* ClassName = Obj->GetClass()->GetName();
    return ( ClassName && appStrstr( ClassName, Needle ) ) ? 1 : 0;
}

static UBOOL UT99AndroidFocusedEditBoxWantsKeyboardV79()
{
    UObject* Obj = UT99AndroidFocusedEditBoxObjectV83();
    if( Obj && Obj->GetClass() )
    {
        UT99_ANDROID_SDL_LOGI( "v79 focused editable UWindow editbox=%s", TCHAR_TO_ANSI(Obj->GetClass()->GetName()) );
        return 1;
    }
    return 0;
}

// UT99_ANDROID_V84_EDIT_FIELD_CLICK_GUARD:
// v83 correctly inserted text, but a stale NameEditBox focus could survive when
// leaving the player-name screen.  The next non-text screen then opened the
// Android IME again.  Keep the real edit-focus check, but additionally require
// the last click to be in the plausible edit-field area for known stale-prone
// controls.
static UBOOL UT99AndroidFocusedEditBoxWantsKeyboardAtV84( INT X, INT Y )
{
    UObject* Obj = UT99AndroidFocusedEditBoxObjectV83();
    if( !Obj || !Obj->GetClass() )
        return 0;

    const TCHAR* ClassName = Obj->GetClass()->GetName();
    if( !ClassName )
        return 0;

    if( appStrstr( ClassName, TEXT("NameEditBox") ) )
    {
        if( X < 110 || X > 650 || Y < 145 || Y > 305 )
        {
            UT99_ANDROID_SDL_LOGI( "v84 stale NameEditBox focus ignored x=%d y=%d", X, Y );
            return 0;
        }
    }
    else if( Y > 500 )
    {
        UT99_ANDROID_SDL_LOGI( "v84 editbox focus ignored on bottom action area class=%s x=%d y=%d", TCHAR_TO_ANSI(ClassName), X, Y );
        return 0;
    }

    UT99_ANDROID_SDL_LOGI( "v84 focused editable UWindow editbox=%s x=%d y=%d", TCHAR_TO_ANSI(ClassName), X, Y );
    return 1;
}

static UBOOL UT99AndroidLikelyEditFieldClickV112( INT X, INT Y )
{
    // UT99_ANDROID_V112_IME_EXACT_EDIT_ONLY:
    // v110 was intentionally broad to rescue OUYA text fields, but it also
    // raised the keyboard on checkboxes.  Keep only real text-entry bands and
    // never treat the left-side checkbox/list area as text input.
    if( Y < 36 || Y > 520 || X < 90 || X > 980 )
        return 0;

    if( X < 360 && Y > 135 )
        return 0;

    if( X >= 115 && X <= 720 && Y >= 45 && Y <= 185 )
        return 1;

    if( X >= 500 && X <= 960 && Y >= 70 && Y <= 470 )
        return 1;

    return 0;
}

static DOUBLE GUT99V112PendingKeyboardUntil = 0.0;
static INT    GUT99V112PendingKeyboardX = 0;
static INT    GUT99V112PendingKeyboardY = 0;
// UT99_ANDROID_V170B_EXACT_EDIT_TARGET_IME:
// Preserve whether the UWindow hit-test itself identified the finger target as
// an EditBox. This is more reliable than fixed screen-coordinate bands and
// still cannot trigger the IME for checkboxes, sliders, tabs or scrollbars.
static UBOOL  GUT99V170BLastTouchWasEditTarget = 0;
static UBOOL  GUT99V170BPendingKeyboardFromEditTarget = 0;

static void UT99AndroidShowKeyboardForClickedEditV110( SDL_Window* Window, INT X, INT Y, const char* Source )
{
    // UT99_ANDROID_V170B_EXACT_EDIT_TARGET_IME:
    // The old coordinate-only heuristic excluded valid right-side edit controls
    // such as the Mouse Sensitivity value at 1920x1080 (X > 980). When direct
    // UWindow touch dispatch says the tapped object is an EditBox, trust that
    // exact hit-test and only use the legacy bands as a fallback for old paths.
    const UBOOL bExactEditTargetV170B = GUT99V170BLastTouchWasEditTarget;
    GUT99V170BLastTouchWasEditTarget = 0;

    const UBOOL bFocusedEditV170B = bExactEditTargetV170B
        ? UT99AndroidFocusedEditBoxWantsKeyboardV79()
        : UT99AndroidFocusedEditBoxWantsKeyboardAtV84( X, Y );
    const UBOOL bLikelyEditClickV169K = UT99AndroidLikelyEditFieldClickV112( X, Y );
    const UBOOL bStartUTNameEditV169K = UT99AndroidFocusedEditBoxClassContainsV169K( TEXT("NameEditBox") )
        && X >= 90 && X <= 760 && Y >= 135 && Y <= 345;
    // UT99_ANDROID_CHROMEOS_NUMERIC_EDIT_V218:
    // A native mouse already went through UWindow's own hit-test before this
    // function is called. A visible focused EditBox is therefore authoritative
    // and avoids the old fixed X<=980 heuristic, which excluded the right-side
    // Mouse Sensitivity control on wide Chromebook displays.
    const UBOOL bNativeMouseFocusedEditV218 = Source && strstr( Source, "native-mouse" )
        && bFocusedEditV170B;
    const UBOOL bEditClickV170B = bExactEditTargetV170B || bLikelyEditClickV169K
        || bStartUTNameEditV169K || bNativeMouseFocusedEditV218;

    if( bFocusedEditV170B && bEditClickV170B )
    {
        GUT99V112PendingKeyboardUntil = 0.0;
        GUT99V170BPendingKeyboardFromEditTarget = 0;
        UT99AndroidShowSoftKeyboardV44( Window, X, Y );
        UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V218_EXACT_EDIT_TARGET_IME showed keyboard source=%s x=%d y=%d exact=%d nativeFocused=%d namebox=%d ouya=%d", Source ? Source : "menu", X, Y, bExactEditTargetV170B ? 1 : 0, bNativeMouseFocusedEditV218 ? 1 : 0, bStartUTNameEditV169K ? 1 : 0, GUT99V79OuyaLikeDevice ? 1 : 0 );
    }
    else if( bEditClickV170B )
    {
        // UWindow sometimes transfers edit focus one frame after Click(), most
        // notably on OUYA. Preserve whether this came from an exact EditBox hit
        // so the delayed check also avoids the old coordinate restrictions.
        GUT99V112PendingKeyboardX = X;
        GUT99V112PendingKeyboardY = Y;
        GUT99V112PendingKeyboardUntil = appSeconds() + 0.60;
        GUT99V170BPendingKeyboardFromEditTarget = bExactEditTargetV170B;
        UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V170B_EXACT_EDIT_TARGET_IME pending source=%s x=%d y=%d exact=%d ouya=%d", Source ? Source : "menu", X, Y, bExactEditTargetV170B ? 1 : 0, GUT99V79OuyaLikeDevice ? 1 : 0 );
    }
    else
    {
        // UT99_ANDROID_V113_OUYA_CHECKBOX_TOUCHPAD_FIX:
        // A normal menu click, especially OUYA native touchpad clicks on checkboxes,
        // must not be followed by SDL_StopTextInput() unless the IME is actually open.
        // Calling StopTextInput after every non-edit click can eat/defocus the UWindow
        // control on older OUYA/Android 4.x builds.
        GUT99V112PendingKeyboardUntil = 0.0;
        GUT99V170BPendingKeyboardFromEditTarget = 0;
        if( GUT99AndroidImeOpenV79 )
            UT99AndroidHideSoftKeyboardV72();
        UT99_ANDROID_SDL_LOGI( "v113 no keyboard for non-edit %s click x=%d y=%d ime=%d", Source ? Source : "menu", X, Y, GUT99AndroidImeOpenV79 ? 1 : 0 );
    }
}

static UBOOL UT99AndroidInsertFocusedEditBoxCharV83( BYTE C )
{
    if( C < 32 || C >= 128 )
        return 0;

    UObject* EditObj = UT99AndroidFocusedEditBoxObjectV83();
    if( !EditObj )
        return 0;

    // UT99_ANDROID_CHROMEOS_NUMERIC_EDIT_V218:
    // UWindowEditControl selects its whole value on focus. Calling Insert()
    // directly skipped UWindowEditBox.KeyType(), so a four-character value
    // such as "3.00" remained selected and every replacement digit exceeded
    // MaxLength=4. Mirror the relevant KeyType rules before direct insertion.
    const UBOOL bNumericOnly = UT99AndroidGetBoolPropertyV79( EditObj, TEXT("bNumericOnly"), 0 );
    const UBOOL bNumericFloat = UT99AndroidGetBoolPropertyV79( EditObj, TEXT("bNumericFloat"), 0 );
    if( bNumericOnly && !( (C >= '0' && C <= '9') || (C == '.' && bNumericFloat) ) )
        return 1;

    if( UT99AndroidGetBoolPropertyV79( EditObj, TEXT("bAllSelected"), 0 ) )
    {
        UFunction* ClearFn = EditObj->FindFunction( FName(TEXT("Clear"), FNAME_Find) );
        if( ClearFn )
            EditObj->ProcessEvent( ClearFn, NULL );
    }

    UFunction* InsertFn = EditObj->FindFunction( TEXT("Insert") );
    if( !InsertFn )
        return 0;

    struct FInsertParms
    {
        BYTE C;
        DWORD ReturnValue;
    } Parms;
    Parms.C = C;
    Parms.ReturnValue = 0;

    EditObj->ProcessEvent( InsertFn, &Parms );
    // A focused edit box owns the printable key even when MaxLength rejects it;
    // never fall through and route that key as a menu/game action as well.
    UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_CHROMEOS_NUMERIC_EDIT_V218 class=%s char=%d inserted=%d numeric=%d float=%d",
        EditObj->GetClass() ? TCHAR_TO_ANSI(EditObj->GetClass()->GetName()) : "?",
        (INT)C, Parms.ReturnValue ? 1 : 0, bNumericOnly ? 1 : 0, bNumericFloat ? 1 : 0 );
    return 1;
}
#endif


#ifdef PLATFORM_ANDROID
#ifndef UT99_ANDROID_V50_MENU_CONTROLS
#define UT99_ANDROID_V50_MENU_CONTROLS UT99_ANDROID_V58_MENU_A_ENTER_HARDENING /* UT99_ANDROID_V52_INPUT_MARKER */ 1
#define UT99_ANDROID_V51H_DUPLICATE_STATIC_COMPILEFIX 1
#define UT99_ANDROID_V51C_RESUME_V50_STATE_COMPILEFIX 1
#define UT99_ANDROID_V50_SELF_CONTAINED_STATE_V51C 1
static UBOOL GUT99V50BtnA = 0;
static UBOOL GUT99V50BtnB = 0;
static UBOOL GUT99V50BtnX = 0;
static UBOOL GUT99V50BtnY = 0;
static UBOOL GUT99V50BtnL1 = 0;
static UBOOL GUT99V50BtnR1 = 0;
static UBOOL GUT99V50BtnL2 = 0;
static UBOOL GUT99V50BtnR2 = 0;
static UBOOL GUT99V50BtnThumbL = 0;
static UBOOL GUT99V50BtnThumbR = 0;
static UBOOL GUT99V50BtnDPadUp = 0; // UT99_ANDROID_CONTROLLER_KEY_NAMES_BUILD_FIX_V119
static UBOOL GUT99V50BtnDPadDown = 0; // UT99_ANDROID_CONTROLLER_KEY_NAMES_BUILD_FIX_V119
static UBOOL GUT99V50BtnDPadLeft = 0; // UT99_ANDROID_CONTROLLER_KEY_NAMES_BUILD_FIX_V119
static UBOOL GUT99V50BtnDPadRight = 0; // UT99_ANDROID_CONTROLLER_KEY_NAMES_BUILD_FIX_V119
static UBOOL GUT99V50BtnStart = 0;
static UBOOL GUT99V50BtnSelect = 0;
static FLOAT GUT99V50LX = 0.0f;
static FLOAT GUT99V50LY = 0.0f;
static FLOAT GUT99V50RX = 0.0f;
static FLOAT GUT99V50RY = 0.0f;
static FLOAT GUT99V50LT = 0.0f;
static FLOAT GUT99V50RT = 0.0f;
static FLOAT GUT99V50HatX = 0.0f;
static FLOAT GUT99V50HatY = 0.0f;
static UBOOL GUT99V50OldA = 0;
static UBOOL GUT99V50OldB = 0;
static UBOOL GUT99V50OldX = 0;
static UBOOL GUT99V50OldY = 0;
static UBOOL GUT99V50OldL1 = 0;
static UBOOL GUT99V50OldR1 = 0;
static UBOOL GUT99V50OldL2 = 0;
static UBOOL GUT99V50OldR2 = 0;
static UBOOL GUT99V50OldL3 = 0;
static UBOOL GUT99V50OldR3 = 0;
static UBOOL GUT99V50OldStart = 0;
static UBOOL GUT99V50OldSelect = 0;
static UBOOL GUT99V50OldRSLeft = 0;
static UBOOL GUT99V50OldRSRight = 0;
static UBOOL GUT99V50OldRSUp = 0;
static UBOOL GUT99V50OldRSDown = 0;
static UBOOL GUT99V50OldLSLeft = 0;
static UBOOL GUT99V50OldLSRight = 0;
static UBOOL GUT99V50OldLSUp = 0;
static UBOOL GUT99V50OldLSDown = 0;

static UBOOL UT99V50AbsOver( FLOAT V, FLOAT Dead ) { return (V < -Dead || V > Dead); }

static void UT99V50Pulse( UNSDLViewport* Viewport, INT Key, FLOAT AxisDelta=0.0f )
{
    if( !Viewport || Key <= 0 )
        return;

    if( Key == IK_MouseX || Key == IK_MouseY )
    {
        Viewport->CauseInputEvent( (EInputKey)Key, IST_Axis, AxisDelta );
    }
    else
    {
        Viewport->CauseInputEvent( (EInputKey)Key, IST_Press );
        Viewport->CauseInputEvent( (EInputKey)Key, IST_Release );
    }
}

static void UT99V50EdgePulse( UNSDLViewport* Viewport, UBOOL Now, UBOOL& Old, INT Key, const char* Name, FLOAT AxisDelta=0.0f )
{
    if( Now == Old )
        return;
    Old = Now;

    if( Now )
    {
        UT99V50Pulse( Viewport, Key, AxisDelta );
        UT99_ANDROID_SDL_LOGI( "v50 menu/capture %s key=%d axis=%.1f", Name, Key, AxisDelta );
    }
}

static void UT99V50MenuControlsTick( UNSDLViewport* Viewport, UBOOL bMenu )
{
#if defined(__ANDROID__)
    static UBOOL V61OldB = 0;
    static UBOOL V61OldStart = 0;
    static UBOOL V61OldSelect = 0;
    static UBOOL V88OldR1 = 0;

    if( !Viewport )
        return;

    const UBOOL EdgeB      = (GUT99V50BtnB      && !V61OldB);
    const UBOOL EdgeStart  = (GUT99V50BtnStart  && !V61OldStart);
    const UBOOL EdgeSelect = (GUT99V50BtnSelect && !V61OldSelect);
    const UBOOL EdgeR1     = (GUT99V50BtnR1     && !V88OldR1);

    // START is handled only by the v72 single-toggle queue.
    // SELECT is intentionally ignored here; older v61/v80 paths produced
    // duplicate Escape pulses on Retroid and made the menu bar blink twice.
    if( EdgeStart || EdgeSelect )
    {
        UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V72_START swallowed legacy menu edge start=%d select=%d", EdgeStart ? 1 : 0, EdgeSelect ? 1 : 0 );
    }
    // UT99_ANDROID_CONTROLLER_CAPTURE_FIX_V121:
    // B must be freely bindable in Preferences > Controls. The old menu-back
    // shortcut fired Escape immediately after the capture pulse, so B could not
    // be assigned reliably. Keep B as Joy2 only; use START/MENU for Back/Menu.
    else if( bMenu && EdgeB )
    {
        __android_log_print( ANDROID_LOG_INFO, "UT99SDL", "UT99_ANDROID_CONTROLLER_CAPTURE_FIX_V121 B bindable no Escape" );
    }
    else if( bMenu && EdgeR1 )
    {
        // UT99_ANDROID_V88_MENU_CURSOR_CLICK:
        // In UWindow menus the user can steer the visible mouse cursor with
        // touch/left stick and confirm with the right shoulder button.
        Viewport->CauseInputEvent( IK_LeftMouse, IST_Press, 0.0f );
        Viewport->CauseInputEvent( IK_LeftMouse, IST_Release, 0.0f );
        __android_log_print( ANDROID_LOG_INFO, "UT99SDL", "v88 menu R1/right shoulder -> LeftMouse click" );
    }

    V61OldB      = GUT99V50BtnB;
    V61OldStart  = GUT99V50BtnStart;
    V61OldSelect = GUT99V50BtnSelect;
    V88OldR1     = GUT99V50BtnR1;
#else
    return;
#endif
}
#endif
#endif

#ifdef PLATFORM_ANDROID
#ifndef UT99_ANDROID_CONTROLS_CAPTURE_V49
#define UT99_ANDROID_CONTROLS_CAPTURE_V49 1
#define UT99_ANDROID_CONTROLS_CAPTURE_V49D_POWERSHELL_REGEX_FIX 1
#define UT99_ANDROID_CONTROLS_CAPTURE_V49D_STATE 1
#ifndef UT99_ANDROID_CONTROLS_CAPTURE_V49B_STATE
#define UT99_ANDROID_CONTROLS_CAPTURE_V49B_STATE 1
static UBOOL GUT99V49BtnA = 0;
static UBOOL GUT99V49BtnB = 0;
static UBOOL GUT99V49BtnX = 0;
static UBOOL GUT99V49BtnY = 0;
static UBOOL GUT99V49BtnL1 = 0;
static UBOOL GUT99V49BtnR1 = 0;
static UBOOL GUT99V49BtnStart = 0;
static UBOOL GUT99V49BtnSelect = 0;
#endif
static UBOOL GUT99V49OldA = 0;
static UBOOL GUT99V49OldB = 0;
static UBOOL GUT99V49OldX = 0;
static UBOOL GUT99V49OldY = 0;
static UBOOL GUT99V49OldL1 = 0;
static UBOOL GUT99V49OldR1 = 0;
static UBOOL GUT99V49OldStart = 0;
static UBOOL GUT99V49OldSelect = 0;
static UBOOL GUT99V49BtnL2 = 0;
static UBOOL GUT99V49BtnR2 = 0;
static UBOOL GUT99V49BtnThumbL = 0;
static UBOOL GUT99V49BtnThumbR = 0;
static UBOOL GUT99V49OldL2 = 0;
static UBOOL GUT99V49OldR2 = 0;
static UBOOL GUT99V49OldThumbL = 0;
static UBOOL GUT99V49OldThumbR = 0;
static UBOOL GUT99V49OldDPadUp = 0;
static UBOOL GUT99V49OldDPadDown = 0;
static UBOOL GUT99V49OldDPadLeft = 0;
static UBOOL GUT99V49OldDPadRight = 0;
static UBOOL GUT99V49OldRJoyLeft = 0;
static UBOOL GUT99V49OldRJoyRight = 0;
static UBOOL GUT99V49OldRJoyUp = 0;
static UBOOL GUT99V49OldRJoyDown = 0;
static UBOOL GUT99V120OldLJoyLeft = 0; // UT99_ANDROID_CONTROLLER_FULL_REMAP_V120
static UBOOL GUT99V120OldLJoyRight = 0; // UT99_ANDROID_CONTROLLER_FULL_REMAP_V120
static UBOOL GUT99V120OldLJoyUp = 0; // UT99_ANDROID_CONTROLLER_FULL_REMAP_V120
static UBOOL GUT99V120OldLJoyDown = 0; // UT99_ANDROID_CONTROLLER_FULL_REMAP_V120

static void UT99V49PulseKey( UNSDLViewport* Viewport, INT Key )
{
    if( !Viewport || Key <= 0 )
        return;
    Viewport->CauseInputEvent( (EInputKey)Key, IST_Press );
    Viewport->CauseInputEvent( (EInputKey)Key, IST_Release );
}

static void UT99V49MenuEdgeKey( UNSDLViewport* Viewport, UBOOL Now, UBOOL& Old, INT CaptureKey, INT UiKey, const char* Name )
{
    if( Now == Old )
        return;
    Old = Now;

    if( Now )
    {
        // First send a bindable key so the Controls capture page can see it.
        // Then send the UI key for normal menus.
        if( CaptureKey > 0 )
            UT99V49PulseKey( Viewport, CaptureKey );
        if( UiKey > 0 )
            UT99V49PulseKey( Viewport, UiKey );
        UT99_ANDROID_SDL_LOGI( "v49 menu key bridge %s capture=%d ui=%d", Name, CaptureKey, UiKey );
    }
}

static void UT99V49MenuCaptureTick( UNSDLViewport* Viewport, UBOOL bMenu )
{
    if( !Viewport || !bMenu )
        return;

    // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117:
    // Capture the physical controller keys, not PC fallback keys. This makes
    // OPTIONS > PREFERENCES > CONTROLS remapping behave like the Unreal port:
    // Controller A becomes Joy1, triggers become Joy12/Joy13, shoulders become
    // Joy10/Joy11. A still sends Enter as UI helper in normal menus.
    // UT99_ANDROID_CONTROLLER_KEY_NAMES_V118:
    // Use friendly physical names in logs/capture to match Preferences > Controls.
    // UT99_ANDROID_CONTROLLER_KEY_NAMES_BUILD_FIX_V119:
    // This block is compiled before the native v47 state variables are declared,
    // so use the already-declared v50 mirror state. The v47 input path mirrors
    // every Android button/axis into these v50 values before the menu tick runs.
    UT99V49MenuEdgeKey( Viewport, GUT99V50BtnA,      GUT99V49OldA,      IK_Joy1,  13, "A" );
    UT99V49MenuEdgeKey( Viewport, GUT99V50BtnB,      GUT99V49OldB,      IK_Joy2,   0, "B" );
    UT99V49MenuEdgeKey( Viewport, GUT99V50BtnX,      GUT99V49OldX,      IK_Joy3,   0, "X" );
    UT99V49MenuEdgeKey( Viewport, GUT99V50BtnY,      GUT99V49OldY,      IK_Joy4,   0, "Y" );
    UT99V49MenuEdgeKey( Viewport, GUT99V50BtnL1,     GUT99V49OldL1,     IK_Joy10,  0, "ShoulderL" );
    UT99V49MenuEdgeKey( Viewport, GUT99V50BtnR1,     GUT99V49OldR1,     IK_Joy11,  0, "ShoulderR" );
    UT99V49MenuEdgeKey( Viewport, GUT99V50BtnL2 || GUT99V50LT > 0.60f, GUT99V49OldL2, IK_Joy12, 0, "TriggerL" );
    UT99V49MenuEdgeKey( Viewport, GUT99V50BtnR2 || GUT99V50RT > 0.60f, GUT99V49OldR2, IK_Joy13, 0, "TriggerR" );
    UT99V49MenuEdgeKey( Viewport, GUT99V50BtnThumbL, GUT99V49OldThumbL, IK_Joy8,   0, "LJoyPress" );
    UT99V49MenuEdgeKey( Viewport, GUT99V50BtnThumbR, GUT99V49OldThumbR, IK_Joy9,   0, "RJoyPress" );
    UT99V49MenuEdgeKey( Viewport, GUT99V50BtnDPadUp || GUT99V50HatY < -0.50f,    GUT99V49OldDPadUp,    IK_JoyPovUp,    38, "DPadUp" );
    UT99V49MenuEdgeKey( Viewport, GUT99V50BtnDPadDown || GUT99V50HatY > 0.50f,  GUT99V49OldDPadDown,  IK_JoyPovDown,  40, "DPadDown" );
    UT99V49MenuEdgeKey( Viewport, GUT99V50BtnDPadLeft || GUT99V50HatX < -0.50f, GUT99V49OldDPadLeft,  IK_JoyPovLeft,  37, "DPadLeft" );
    UT99V49MenuEdgeKey( Viewport, GUT99V50BtnDPadRight || GUT99V50HatX > 0.50f, GUT99V49OldDPadRight, IK_JoyPovRight, 39, "DPadRight" );

    // UT99_ANDROID_CONTROLLER_FULL_REMAP_V120:
    // Left stick is no longer only a menu-cursor helper while Controls capture
    // is open.  Emit the physical LJoy directions as bindable keys too.
    const FLOAT LJoyCaptureDead = 0.60f;
    UT99V49MenuEdgeKey( Viewport, GUT99V50LX < -LJoyCaptureDead, GUT99V120OldLJoyLeft,  IK_UnknownD8, 0, "LJoyLeft" );
    UT99V49MenuEdgeKey( Viewport, GUT99V50LX >  LJoyCaptureDead, GUT99V120OldLJoyRight, IK_UnknownD9, 0, "LJoyRight" );
    UT99V49MenuEdgeKey( Viewport, GUT99V50LY < -LJoyCaptureDead, GUT99V120OldLJoyUp,    IK_UnknownDA, 0, "LJoyUp" );
    UT99V49MenuEdgeKey( Viewport, GUT99V50LY >  LJoyCaptureDead, GUT99V120OldLJoyDown,  IK_UnknownDF, 0, "LJoyDown" );

    const FLOAT RJoyCaptureDead = 0.60f;
    UT99V49MenuEdgeKey( Viewport, GUT99V50RX < -RJoyCaptureDead, GUT99V49OldRJoyLeft,  IK_Joy14,      0, "RJoyLeft" );
    UT99V49MenuEdgeKey( Viewport, GUT99V50RX >  RJoyCaptureDead, GUT99V49OldRJoyRight, IK_Joy15,      0, "RJoyRight" );
    UT99V49MenuEdgeKey( Viewport, GUT99V50RY < -RJoyCaptureDead, GUT99V49OldRJoyUp,    IK_Joy16,      0, "RJoyUp" );
    UT99V49MenuEdgeKey( Viewport, GUT99V50RY >  RJoyCaptureDead, GUT99V49OldRJoyDown,  IK_UnknownEA,  0, "RJoyDown" );

    // START only opens menu from gameplay. In menu it should not immediately
    // close again. SELECT stays ignored.
    GUT99V49OldStart = GUT99V49BtnStart;
    GUT99V49OldSelect = GUT99V49BtnSelect;
}

static void UT99V49SetAndroidButtonExtra( INT KeyCode, UBOOL Down )
{
    switch( KeyCode )
    {
        case 96:  GUT99V50BtnA = Down; break;       // KEYCODE_BUTTON_A
        case 97:  GUT99V50BtnB = Down; break;       // KEYCODE_BUTTON_B
        case 99:  GUT99V50BtnX = Down; break;       // KEYCODE_BUTTON_X
        case 100: GUT99V50BtnY = Down; break;       // KEYCODE_BUTTON_Y
        case 102: GUT99V50BtnL1 = Down; break;      // KEYCODE_BUTTON_L1
        case 103: GUT99V50BtnR1 = Down; break;      // KEYCODE_BUTTON_R1
        case 104: GUT99V50BtnL2 = Down; break;      // KEYCODE_BUTTON_L2
        case 105: GUT99V50BtnR2 = Down; break;      // KEYCODE_BUTTON_R2
        case 106: GUT99V50BtnThumbL = Down; break;  // KEYCODE_BUTTON_THUMBL
        case 107: GUT99V50BtnThumbR = Down; break;  // KEYCODE_BUTTON_THUMBR
        case 19:  GUT99V50BtnDPadUp = Down; break;    // DPAD_UP - UT99_ANDROID_CONTROLLER_KEY_NAMES_BUILD_FIX_V119
        case 20:  GUT99V50BtnDPadDown = Down; break;  // DPAD_DOWN - UT99_ANDROID_CONTROLLER_KEY_NAMES_BUILD_FIX_V119
        case 21:  GUT99V50BtnDPadLeft = Down; break;  // DPAD_LEFT - UT99_ANDROID_CONTROLLER_KEY_NAMES_BUILD_FIX_V119
        case 22:  GUT99V50BtnDPadRight = Down; break; // DPAD_RIGHT - UT99_ANDROID_CONTROLLER_KEY_NAMES_BUILD_FIX_V119
        case 82:  GUT99V50BtnStart = Down; break;   // KEYCODE_MENU / OUYA center button
        case 108: GUT99V50BtnStart = Down; break;   // KEYCODE_BUTTON_START
        case 110: GUT99V50BtnStart = Down; break;   // KEYCODE_BUTTON_MODE
        case 109: GUT99V50BtnSelect = Down; break;  // KEYCODE_BUTTON_SELECT
    }
}

static UBOOL UT99AndroidShouldShowKeyboardV49( INT X, INT Y )
{
    // UT99_ANDROID_V76_KEYBOARD_EDITFIELD_HEURISTIC:
    // Only raise the Android IME for likely UWindow edit fields.  v74 was too
    // broad on some devices; it could wake the keyboard during normal menu/game
    // startup.  UT's text boxes (player name, server name/password, join fields)
    // are usually horizontal strips near the upper/left or right side of dialogs.
    if( Y < 40 || Y > 500 || X < 80 || X > 930 )
        return 0;

    // Player-name/preferences fields: often top-left/top-center.
    if( X >= 120 && X <= 520 && Y >= 45 && Y <= 175 )
        return 1;

    // Server/password and option edit boxes: wider right-side strips.
    if( X >= 520 && X <= 910 && Y >= 80 && Y <= 440 )
        return 1;

    // General dialog edit strips, but do not treat the centered main-menu button
    // stack as editable text.
    if( X >= 180 && X <= 890 && Y >= 180 && Y <= 470 )
    {
        if( X >= 350 && X <= 650 && Y >= 210 && Y <= 500 )
            return 0;
        return 1;
    }

    return 0;
}
#endif
#endif


#ifdef PLATFORM_ANDROID
#ifndef UT99_ANDROID_NATIVE_INPUT_V47
#define UT99_ANDROID_NATIVE_INPUT_V47 1
static FLOAT GUT99V47LX = 0.0f;
static FLOAT GUT99V47LY = 0.0f;
static FLOAT GUT99V47RX = 0.0f;
static FLOAT GUT99V47RY = 0.0f;
static FLOAT GUT99V101TouchLookX = 0.0f; // UT99_ANDROID_V103_TOUCH_OVERLAY accumulated relative touch look
static FLOAT GUT99V101TouchLookY = 0.0f; // UT99_ANDROID_V103_TOUCH_OVERLAY accumulated relative touch look
static FLOAT GUT99V47LT = 0.0f;
static FLOAT GUT99V47RT = 0.0f;
static FLOAT GUT99V47HatX = 0.0f;
static FLOAT GUT99V47HatY = 0.0f;
static UBOOL GUT99V47BtnA = 0;
static UBOOL GUT99V47BtnB = 0;
static UBOOL GUT99V47BtnX = 0;
static UBOOL GUT99V47BtnY = 0;
static UBOOL GUT99V47BtnL1 = 0;
static UBOOL GUT99V47BtnR1 = 0;
static UBOOL GUT99V47BtnThumbL = 0; // UT99_ANDROID_CONTROLLER_FULL_REMAP_V120
static UBOOL GUT99V47BtnThumbR = 0; // UT99_ANDROID_CONTROLLER_FULL_REMAP_V120
static UBOOL GUT99V47BtnStart = 0;
static UBOOL GUT99V47BtnSelect = 0;
static UBOOL GUT99V47BtnDup = 0;
static UBOOL GUT99V47BtnDdown = 0;
static UBOOL GUT99V47BtnDleft = 0;
static UBOOL GUT99V47BtnDright = 0;
static UBOOL GUT99V47MoveF = 0;
static UBOOL GUT99V47MoveB = 0;
static UBOOL GUT99V47MoveL = 0;
static UBOOL GUT99V47MoveR = 0;
static UBOOL GUT99V118RJoyLeft = 0;
static UBOOL GUT99V118RJoyRight = 0;
static UBOOL GUT99V118RJoyUp = 0;
static UBOOL GUT99V118RJoyDown = 0;
static UBOOL GUT99V47Fire = 0;
static UBOOL GUT99V47AltFire = 0;
static UBOOL GUT99V47Jump = 0;
static UBOOL GUT99V47Duck = 0;
static UBOOL GUT99V47Use = 0;
static UBOOL GUT99V47Prev = 0;
static UBOOL GUT99V47Next = 0;
static UBOOL GUT99V47Walk = 0;
static UBOOL GUT99V47Wave = 0;
static UBOOL GUT99V47MenuPrev = 0;
static INT GUT99V117MoveFKey = IK_None;
static INT GUT99V117MoveBKey = IK_None;
static INT GUT99V117MoveLKey = IK_None;
static INT GUT99V117MoveRKey = IK_None;
static INT GUT99V118RJoyLeftKey = IK_None;
static INT GUT99V118RJoyRightKey = IK_None;
static INT GUT99V118RJoyUpKey = IK_None;
static INT GUT99V118RJoyDownKey = IK_None;
static INT GUT99V117JumpKey = IK_None;
static INT GUT99V117DuckKey = IK_None;
static INT GUT99V117PrevKey = IK_None;
static INT GUT99V117NextKey = IK_None;
static INT GUT99V117WalkKey = IK_None;
static INT GUT99V117WaveKey = IK_None;
static INT GUT99V120LJoyPressKey = IK_None; // UT99_ANDROID_CONTROLLER_FULL_REMAP_V120
static INT GUT99V120RJoyPressKey = IK_None; // UT99_ANDROID_CONTROLLER_FULL_REMAP_V120
static UBOOL GUT99V120LJoyPress = 0; // UT99_ANDROID_CONTROLLER_FULL_REMAP_V120
static UBOOL GUT99V120RJoyPress = 0; // UT99_ANDROID_CONTROLLER_FULL_REMAP_V120
static INT GUT99V117FireKey = IK_None;
static INT GUT99V117AltFireKey = IK_None;
static UBOOL GUT99V117LoggedActive = 0;
static INT GUT99V47QueuedStartPress = 0;
static INT GUT99V47QueuedStartRelease = 0;
static INT GUT99V80QueuedBackspace = 0;
static char GUT99V82QueuedText[1024];
static INT GUT99V82QueuedTextLen = 0;
static DOUBLE GUT99V80StartFallbackTime = 0.0;
static DOUBLE GUT99V80StartFallbackExpire = 0.0;
static DOUBLE GUT99V47MenuRepeat[8] = {0};
static DOUBLE GUT99V47LastAxisLog = 0.0;
static DOUBLE GUT99V81LastCursorModeLog = 0.0;
static INT GUT99V72StartToggleRequests = 0;
static DOUBLE GUT99V72LastStartToggleQueueTime = 0.0;
static DOUBLE GUT99V72LastStartToggleSendTime = 0.0;

static FLOAT UT99V47AbsF( FLOAT V ) { return V < 0.0f ? -V : V; }
static FLOAT UT99V47ClampF( FLOAT V, FLOAT Lo, FLOAT Hi ) { return V < Lo ? Lo : (V > Hi ? Hi : V); }
static void UT99V47Event( UNSDLViewport* Viewport, INT Key, EInputAction Action, FLOAT Delta=0.0f )
{
    if( Viewport && Key > 0 )
        Viewport->CauseInputEvent( (EInputKey)Key, Action, Delta );
}

static void UT99V80SendKeyType( UNSDLViewport* Viewport, EInputKey Key )
{
    if( !Viewport || Key <= 0 )
        return;

    // UT99_ANDROID_V81_BUILD_FIX:
    // This helper lives outside UNSDLViewport, so it must not touch the private
    // Client member directly.  It remains a safe fallback for non-text keys
    // such as Backspace.  Real printable text is committed from TickInput()
    // through Client->Engine->Key(), where private Client access is legal.
    Viewport->CauseInputEvent( Key, IST_Press );
    Viewport->CauseInputEvent( Key, IST_Release );
}

static void UT99V80PulseEscape( UNSDLViewport* Viewport, const char* Reason )
{
    if( !Viewport )
        return;

    Viewport->CauseInputEvent( IK_Escape, IST_Press );
    Viewport->CauseInputEvent( IK_Escape, IST_Release );
    UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V72_ESCAPE pulse %s", Reason ? Reason : "" );
}

static void UT99V72QueueStartToggle( const char* Reason )
{
    DOUBLE Now = appSeconds();
    if( GUT99V72LastStartToggleQueueTime > 0.0 && (Now - GUT99V72LastStartToggleQueueTime) < 0.24 )
    {
        UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V72_START ignored duplicate source=%s", Reason ? Reason : "" );
        return;
    }

    GUT99V72LastStartToggleQueueTime = Now;
    if( GUT99V72StartToggleRequests < 4 )
        ++GUT99V72StartToggleRequests;
    UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V72_START queued source=%s pending=%d", Reason ? Reason : "", GUT99V72StartToggleRequests );
}

static void UT99V72PumpStartToggle( UNSDLViewport* Viewport, UBOOL bMenu )
{
    if( !Viewport || GUT99V72StartToggleRequests <= 0 )
        return;

    GUT99V72StartToggleRequests = 0;

    DOUBLE Now = appSeconds();
    if( GUT99V72LastStartToggleSendTime > 0.0 && (Now - GUT99V72LastStartToggleSendTime) < 0.18 )
    {
        UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V72_START dropped duplicate pump menu=%d", bMenu ? 1 : 0 );
        return;
    }

    GUT99V72LastStartToggleSendTime = Now;
    Viewport->CauseInputEvent( IK_Escape, IST_Press );
    Viewport->CauseInputEvent( IK_Escape, IST_Release );
    UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V72_START single Escape toggle menu=%d", bMenu ? 1 : 0 );
}
static void UT99V47PressState( UNSDLViewport* Viewport, INT Key, UBOOL& OldState, UBOOL NewState )
{
    if( OldState != NewState )
    {
        OldState = NewState;
        UT99V47Event( Viewport, Key, NewState ? IST_Press : IST_Release );
    }
}

static void UT99V117PressMapped( UNSDLViewport* Viewport, INT Key, UBOOL& OldState, INT& OldKey, UBOOL NewState, const char* Name )
{
    // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117
    // Runtime direct input must emit physical controller keys, not fixed PC
    // fallback keys. The default AndroidUser/DefUser binds these Joy/Unknown
    // keys to the same actions as before, but the user can now freely remap
    // them under Preferences > Controls. The separate OldKey avoids stuck keys
    // if a binding is changed while a button is held.
    if( !Viewport || Key <= IK_None || Key >= IK_MAX )
        return;

    if( OldState && ( !NewState || OldKey != Key ) )
    {
        UT99V47Event( Viewport, OldKey > IK_None ? OldKey : Key, IST_Release );
        OldState = 0;
        OldKey = IK_None;
    }
    if( NewState && !OldState )
    {
        UT99V47Event( Viewport, Key, IST_Press );
        OldState = 1;
        OldKey = Key;
        if( !GUT99V117LoggedActive )
        {
            GUT99V117LoggedActive = 1;
            UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_CONTROLLER_BINDING_FIX_V117 active physical-bind mode" );
        }
        if( Name )
            UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_CONTROLLER_BINDING_FIX_V117 %s key=%d down", Name, Key );
    }
}

static UBOOL UT99V47MenuPulse( UNSDLViewport* Viewport, INT Key, INT Slot, FLOAT AnalogValue = 1.0f )
{
    DOUBLE Now = appSeconds();
    if( GUT99V47MenuRepeat[Slot] > 0.0 && (Now - GUT99V47MenuRepeat[Slot]) < 0.20 )
        return 0;
    GUT99V47MenuRepeat[Slot] = Now;
    UT99V47Event( Viewport, Key, IST_Press );
    UT99V47Event( Viewport, Key, IST_Release );
    return 1;
}

static void UT99V88MenuMouseCursorTick( UNSDLViewport* Viewport )
{
    if( !Viewport )
        return;

    // UT99_ANDROID_V88_MENU_CURSOR_STICK:
    // In UWindow menus the left stick should move the actual PC/UWindow mouse
    // cursor instead of only stepping through menu entries.  D-Pad/HAT remains
    // available for classic menu navigation.
    const FLOAT Dead = 0.18f;
    FLOAT LX = GUT99V47LX;
    FLOAT LY = GUT99V47LY;
    if( UT99V47AbsF( LX ) <= Dead )
        LX = 0.0f;
    if( UT99V47AbsF( LY ) <= Dead )
        LY = 0.0f;
    if( LX == 0.0f && LY == 0.0f )
        return;

    const INT W = Max( 1, Viewport->SizeX );
    const INT H = Max( 1, Viewport->SizeY );
    if( Viewport->WindowsMouseX < 0.0f || Viewport->WindowsMouseX >= (FLOAT)W )
        Viewport->WindowsMouseX = (FLOAT)W * 0.5f;
    if( Viewport->WindowsMouseY < 0.0f || Viewport->WindowsMouseY >= (FLOAT)H )
        Viewport->WindowsMouseY = (FLOAT)H * 0.5f;

    // UT99_ANDROID_V107_OUYA_MENU_CURSOR_SPEED:
    // OUYA's SDL analog values/tick cadence make the menu cursor feel roughly
    // half as fast as Retroid.  Raise only the OUYA profile; all other devices
    // keep the proven v91 speed.
    const FLOAT Speed = GUT99V79OuyaLikeDevice ? 18.0f : 10.5f;
    const FLOAT DX = LX * Speed;
    const FLOAT DY = LY * Speed;

    Viewport->WindowsMouseX = Clamp( Viewport->WindowsMouseX + DX, 0.0f, (FLOAT)Max( 1, W - 1 ) );
    Viewport->WindowsMouseY = Clamp( Viewport->WindowsMouseY + DY, 0.0f, (FLOAT)Max( 1, H - 1 ) );
    Viewport->bWindowsMouseAvailable = 1;

    if( DX != 0.0f )
        UT99V47Event( Viewport, IK_MouseX, IST_Axis, DX );
    if( DY != 0.0f )
        UT99V47Event( Viewport, IK_MouseY, IST_Axis, -DY );

    DOUBLE Now = appSeconds();
    if( Now - GUT99V47LastAxisLog > 0.80 )
    {
        GUT99V47LastAxisLog = Now;
        UT99_ANDROID_SDL_LOGI( "v107 menu left stick cursor x=%.1f y=%.1f dx=%.1f dy=%.1f", Viewport->WindowsMouseX, Viewport->WindowsMouseY, DX, DY );
    }
}
#define UT99_ANDROID_START_MENU_HOLD_FIX_V48_BUILT 1
static void UT99V47ReleaseGameplayStatesV79( UNSDLViewport* Viewport )
{
    // When the Android IME has been open and gameplay resumes, stale button
    // states can leave Fire/AltFire pressed forever. Release all synthetic
    // gameplay keys once, then clear raw fire/button sources.
    UT99V117PressMapped( Viewport, IK_UnknownDA, GUT99V47MoveF, GUT99V117MoveFKey, 0, "MoveForward" );
    UT99V117PressMapped( Viewport, IK_UnknownDF, GUT99V47MoveB, GUT99V117MoveBKey, 0, "MoveBackward" );
    UT99V117PressMapped( Viewport, IK_UnknownD8, GUT99V47MoveL, GUT99V117MoveLKey, 0, "StrafeLeft" );
    UT99V117PressMapped( Viewport, IK_UnknownD9, GUT99V47MoveR, GUT99V117MoveRKey, 0, "StrafeRight" );
    UT99V117PressMapped( Viewport, IK_Joy14,     GUT99V118RJoyLeft,  GUT99V118RJoyLeftKey,  0, "RJoyLeft" );
    UT99V117PressMapped( Viewport, IK_Joy15,     GUT99V118RJoyRight, GUT99V118RJoyRightKey, 0, "RJoyRight" );
    UT99V117PressMapped( Viewport, IK_Joy16,     GUT99V118RJoyUp,    GUT99V118RJoyUpKey,    0, "RJoyUp" );
    UT99V117PressMapped( Viewport, IK_UnknownEA, GUT99V118RJoyDown,  GUT99V118RJoyDownKey,  0, "RJoyDown" );
    UT99V117PressMapped( Viewport, IK_Joy1,      GUT99V47Jump,  GUT99V117JumpKey,  0, "A" );
    UT99V117PressMapped( Viewport, IK_Joy2,      GUT99V47Duck,  GUT99V117DuckKey,  0, "Joy2" );
    UT99V117PressMapped( Viewport, IK_Joy10,     GUT99V47Prev,  GUT99V117PrevKey,  0, "Joy10" );
    UT99V117PressMapped( Viewport, IK_Joy11,     GUT99V47Next,  GUT99V117NextKey,  0, "Joy11" );
    UT99V117PressMapped( Viewport, IK_Joy4,      GUT99V47Walk,  GUT99V117WalkKey,  0, "Joy4" );
    UT99V117PressMapped( Viewport, IK_Joy3,      GUT99V47Wave,  GUT99V117WaveKey,  0, "Joy3" );
    UT99V117PressMapped( Viewport, IK_Joy8,      GUT99V120LJoyPress, GUT99V120LJoyPressKey, 0, "LJoyPress" ); // UT99_ANDROID_CONTROLLER_FULL_REMAP_V120
    UT99V117PressMapped( Viewport, IK_Joy9,      GUT99V120RJoyPress, GUT99V120RJoyPressKey, 0, "RJoyPress" ); // UT99_ANDROID_CONTROLLER_FULL_REMAP_V120
    UT99V117PressMapped( Viewport, IK_Joy13,     GUT99V47Fire,  GUT99V117FireKey,  0, "Joy13" );
    UT99V117PressMapped( Viewport, IK_Joy12,     GUT99V47AltFire, GUT99V117AltFireKey, 0, "Joy12" );
    GUT99V47BtnA = GUT99V47BtnB = GUT99V47BtnX = GUT99V47BtnY = 0;
    GUT99V47BtnL1 = GUT99V47BtnR1 = GUT99V47BtnThumbL = GUT99V47BtnThumbR = 0;
    GUT99V47LT = GUT99V47RT = 0.0f;
    GUT99V50BtnA = GUT99V50BtnB = GUT99V50BtnX = GUT99V50BtnY = 0;
    GUT99V50BtnL1 = GUT99V50BtnR1 = GUT99V50BtnL2 = GUT99V50BtnR2 = 0;
    GUT99V50BtnDPadUp = GUT99V50BtnDPadDown = GUT99V50BtnDPadLeft = GUT99V50BtnDPadRight = 0; // UT99_ANDROID_CONTROLLER_KEY_NAMES_BUILD_FIX_V119
    UT99_ANDROID_SDL_LOGI( "v79 gameplay resumed: IME hidden and synthetic fire/buttons released" );
}

static void UT99V82QueueTextBytes( const char* Text )
{
    if( !Text )
        return;

    while( *Text && GUT99V82QueuedTextLen < (INT)sizeof(GUT99V82QueuedText) - 1 )
    {
        unsigned char C = (unsigned char)*Text++;

        // UT99/UWindow text fields are byte based here.  Keep the bridge
        // conservative: commit printable ASCII and Return.  Backspace/Delete
        // stay on the dedicated key path that already works.
        if( (C >= 32 && C < 127) || C == '\r' || C == '\n' )
            GUT99V82QueuedText[GUT99V82QueuedTextLen++] = (char)((C == '\n') ? '\r' : C);
    }
    GUT99V82QueuedText[GUT99V82QueuedTextLen] = 0;
}

static UBOOL GUT99AndroidMenuVisibleV91 = 0; // UT99_ANDROID_V91_TOUCH_OVERLAY
static volatile INT GUT99RetroTouchInputResetSerial = 0; // RETROTOUCH_BETA4_INPUT_RESET_SYNC
static volatile INT GUT99RetroTouchUiState = 0; // RETROTOUCH_BETA4_STATE_MACHINE_V2: 0 pass-through, 1 navigation, 2 gameplay

static INT UT99RetroTouchDetermineUiState( UNSDLViewport* Viewport, UBOOL bMenu )
{
    if( bMenu )
        return 1;

    if( !Viewport || !Viewport->Actor || !Viewport->Actor->GetLevel() )
        return 0;

    // CityIntro is an interactive flyby: a normal screen tap must keep reaching
    // SDL/UT so the player can skip it. Once UWindow opens, bMenu above switches
    // the overlay to NAVIGATION. Every ordinary map is GAMEPLAY.
    const TCHAR* MapName = *Viewport->Actor->GetLevel()->URL.Map;
    if( MapName )
    {
        if( appStricmp(MapName,TEXT("CityIntro"))==0 || appStricmp(MapName,TEXT("CityIntro.unr"))==0 )
            return 0;
        if( appStricmp(MapName,TEXT("Entry"))==0 || appStricmp(MapName,TEXT("Entry.unr"))==0 )
            return 0;
    }

    return 2;
}

static void UT99V47TickInput( UNSDLViewport* Viewport, UBOOL bMenu )
{
    GUT99AndroidMenuVisibleV91 = bMenu ? 1 : 0;
    GUT99RetroTouchUiState = UT99RetroTouchDetermineUiState( Viewport, bMenu );
    static UBOOL V72LoggedActive = 0;
    if( !Viewport )
        return;
    if( !V72LoggedActive )
    {
        V72LoggedActive = 1;
        UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V81_ACTIVE native pointer hides UT99 software cursor; left stick restores it" );
    }

#if defined(__ANDROID__)
    if( GUT99V80QueuedBackspace > 0 )
    {
        INT Count = GUT99V80QueuedBackspace;
        GUT99V80QueuedBackspace = 0;
        while( Count-- > 0 )
            UT99V80SendKeyType( Viewport, IK_Backspace );
        UT99_ANDROID_SDL_LOGI( "v80 committed queued Backspace to focused edit box" );
    }

    if( !bMenu && GUT99AndroidImeOpenV79 )
    {
        UT99AndroidHideSoftKeyboardV72();
        UT99V47ReleaseGameplayStatesV79( Viewport );
    }

    GUT99V80StartFallbackTime = 0.0;
    GUT99V80StartFallbackExpire = 0.0;
    UT99V72PumpStartToggle( Viewport, bMenu );
#endif

    if( bMenu )
    {
#if defined(__ANDROID__)
        if( GUT99AndroidImeOpenV79 && (GUT99V47BtnB || GUT99V47BtnStart) )
        {
            // UT99_ANDROID_V108_OUYA_TEXTFIELD_REPAIR:
            // Do not close the Android/OUYA IME immediately after A/Enter selected
            // a UWindow edit box.  B/START still leave text entry cleanly.
            UT99AndroidHideSoftKeyboardV72();
            UT99_ANDROID_SDL_LOGI( "v108 menu hardware B/START hid Android IME" );
        }
#endif
#if defined(__ANDROID__)
        if( GUT99V112PendingKeyboardUntil > 0.0 )
        {
            const DOUBLE NowKeyboard = appSeconds();
            if( NowKeyboard <= GUT99V112PendingKeyboardUntil )
            {
                const UBOOL bFocusedPendingEditV170B = GUT99V170BPendingKeyboardFromEditTarget
                    ? UT99AndroidFocusedEditBoxWantsKeyboardV79()
                    : UT99AndroidFocusedEditBoxWantsKeyboardAtV84( GUT99V112PendingKeyboardX, GUT99V112PendingKeyboardY );
                if( bFocusedPendingEditV170B )
                {
                    UT99AndroidShowSoftKeyboardV44( NULL, GUT99V112PendingKeyboardX, GUT99V112PendingKeyboardY );
                    GUT99V112PendingKeyboardUntil = 0.0;
                    GUT99V170BPendingKeyboardFromEditTarget = 0;
                    UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V170B_EXACT_EDIT_TARGET_IME delayed keyboard x=%d y=%d", GUT99V112PendingKeyboardX, GUT99V112PendingKeyboardY );
                }
            }
            else
            {
                GUT99V112PendingKeyboardUntil = 0.0;
                GUT99V170BPendingKeyboardFromEditTarget = 0;
            }
        }
#endif
        // v72: START/MENU is processed once at the top of this tick.
        // Do not replay old queued-start-menu pulses.
        GUT99V47QueuedStartPress = 0;
        GUT99V47QueuedStartRelease = 0;
        // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117:
        // Feed physical bind keys before legacy menu helpers. This allows B,
        // triggers and stick directions to be captured before B-as-Escape or
        // R1-as-click menu convenience paths can consume the edge.
        UT99V49MenuCaptureTick( Viewport, bMenu );
        UT99V50MenuControlsTick( Viewport, bMenu );
        // UT99_ANDROID_V89_MENU_CURSOR_MEMBER_DRIVE:
        // Do not move the UWindow mouse from this static helper anymore.
        // The visible cursor must be driven from UNSDLViewport::TickInput(),
        // where the private Client->Engine pointer is available and
        // MousePosition() can be called.  Without that call the fields were
        // updated, but the displayed menu cursor did not move on Retroid/OUYA.
        // UT99V88MenuMouseCursorTick( Viewport );
        // UT99_ANDROID_V108_OUYA_DPAD_SINGLE_STEP:
        // D-Pad/HAT menu navigation is already handled by UT99V49MenuCaptureTick()
        // above, where one physical edge emits one UI key.  The legacy repeat
        // pulses below caused OUYA to skip every second menu item because OUYA
        // reports the same D-Pad edge through multiple paths.
        if( GUT99V47BtnA && !GUT99AndroidImeOpenV79 ) UT99V47MenuPulse( Viewport, 13, 4 );
        /* UT99_ANDROID_V52_B_NO_MENU_BACK: B is bindable, not menu back */
        // UT99_ANDROID_START_MENU_HOLD_FIX_V48:
// START opens UWindow from gameplay. Once the menu is already open,
// the still-held START button must not send another Escape pulse,
// otherwise the menu closes immediately again.
if( !GUT99V47BtnStart )
    GUT99V47MenuPrev = 0;
        return;
    }

    const FLOAT MoveDead = 0.35f;
    UT99V117PressMapped( Viewport, IK_UnknownDA, GUT99V47MoveF, GUT99V117MoveFKey, GUT99V47LY < -MoveDead, "LJoyUp" );
    UT99V117PressMapped( Viewport, IK_UnknownDF, GUT99V47MoveB, GUT99V117MoveBKey, GUT99V47LY >  MoveDead, "LJoyDown" );
    UT99V117PressMapped( Viewport, IK_UnknownD8, GUT99V47MoveL, GUT99V117MoveLKey, GUT99V47LX < -MoveDead, "LJoyLeft" );
    UT99V117PressMapped( Viewport, IK_UnknownD9, GUT99V47MoveR, GUT99V117MoveRKey, GUT99V47LX >  MoveDead, "LJoyRight" );

    // UT99_ANDROID_CONTROLLER_KEY_NAMES_V118:
    // Right stick remains analog look by default, but its four directions are
    // also exposed as bindable physical keys for Preferences > Controls.
    const FLOAT RJoyBindDead = 0.60f;
    UT99V117PressMapped( Viewport, IK_Joy14,     GUT99V118RJoyLeft,  GUT99V118RJoyLeftKey,  GUT99V47RX < -RJoyBindDead, "RJoyLeft" );
    UT99V117PressMapped( Viewport, IK_Joy15,     GUT99V118RJoyRight, GUT99V118RJoyRightKey, GUT99V47RX >  RJoyBindDead, "RJoyRight" );
    UT99V117PressMapped( Viewport, IK_Joy16,     GUT99V118RJoyUp,    GUT99V118RJoyUpKey,    GUT99V47RY < -RJoyBindDead, "RJoyUp" );
    UT99V117PressMapped( Viewport, IK_UnknownEA, GUT99V118RJoyDown,  GUT99V118RJoyDownKey,  GUT99V47RY >  RJoyBindDead, "RJoyDown" );

    // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117:
    // Defaults are kept through AndroidUser.ini/DefUser.ini, but the runtime
    // emits physical controller keys so every function can be remapped.
    UT99V117PressMapped( Viewport, IK_Joy1,  GUT99V47Jump, GUT99V117JumpKey, GUT99V47BtnA,  "A" );
    UT99V117PressMapped( Viewport, IK_Joy2,  GUT99V47Duck, GUT99V117DuckKey, GUT99V47BtnB,  "B" );
    UT99V117PressMapped( Viewport, IK_Joy10, GUT99V47Walk, GUT99V117WalkKey, GUT99V47BtnL1, "ShoulderL" ); // UT99_ANDROID_CONTROLLER_FULL_REMAP_V120
    UT99V117PressMapped( Viewport, IK_Joy11, GUT99V47Next, GUT99V117NextKey, GUT99V47BtnR1, "ShoulderR" );
    UT99V117PressMapped( Viewport, IK_Joy4,  GUT99V47Prev, GUT99V117PrevKey, GUT99V47BtnY,  "Y" ); // unbound by default, still freely assignable
    UT99V117PressMapped( Viewport, IK_Joy3,  GUT99V47Wave, GUT99V117WaveKey, GUT99V47BtnX,  "X" );
    UT99V117PressMapped( Viewport, IK_Joy8,  GUT99V120LJoyPress, GUT99V120LJoyPressKey, GUT99V47BtnThumbL, "LJoyPress" ); // UT99_ANDROID_CONTROLLER_FULL_REMAP_V120
    UT99V117PressMapped( Viewport, IK_Joy9,  GUT99V120RJoyPress, GUT99V120RJoyPressKey, GUT99V47BtnThumbR, "RJoyPress" ); // UT99_ANDROID_CONTROLLER_FULL_REMAP_V120

    UBOOL WantFire = GUT99V47RT > 0.35f;
    UBOOL WantAltFire = GUT99V47LT > 0.35f;
    UT99V117PressMapped( Viewport, IK_Joy13, GUT99V47Fire, GUT99V117FireKey, WantFire, "TriggerR" );
    UT99V117PressMapped( Viewport, IK_Joy12, GUT99V47AltFire, GUT99V117AltFireKey, WantAltFire, "TriggerL" );

    // v72: START/MENU is a single queued toggle handled at the top of TickInput.
    // The old queued-start-gameplay, held-start-gameplay and Retroid fallback paths
    // produced two or three Escape pulses for one physical Retroid START press.
    GUT99V47QueuedStartPress = 0;
    GUT99V47QueuedStartRelease = 0;
    GUT99V47MenuPrev = GUT99V47BtnStart;

    // UT99_ANDROID_V103_TOUCH_OVERLAY:
    // Right touch look is relative swipe aiming.  Consume accumulated Java deltas
    // once per tick, then reset them, so there is no virtual-stick deadzone and no
    // continued rotation after the thumb stops moving.
    {
        FLOAT TX = GUT99V101TouchLookX;
        FLOAT TY = GUT99V101TouchLookY;
        GUT99V101TouchLookX = 0.0f;
        GUT99V101TouchLookY = 0.0f;
        if( TX != 0.0f || TY != 0.0f )
        {
            const FLOAT Dx = TX * 42.0f;
            const FLOAT Dy = -TY * 30.0f;
            if( Dx != 0.0f ) UT99V47Event( Viewport, IK_MouseX, IST_Axis, Dx );
            if( Dy != 0.0f ) UT99V47Event( Viewport, IK_MouseY, IST_Axis, Dy );
        }
    }

    // UT99_ANDROID_CONTROLLER_FULL_REMAP_V120:
    // Do not additionally inject MouseX/MouseY from the physical right stick here.
    // Physical controllers keep using the v41 SDL continuous-look path.
}

static void UT99V47SetAndroidButton( INT KeyCode, UBOOL Down )
{
    if( Down && (KeyCode == 67 || KeyCode == 112) ) // KEYCODE_DEL / KEYCODE_FORWARD_DEL
    {
        ++GUT99V80QueuedBackspace;
        return;
    }
    if( Down && GUT99AndroidImeOpenV79 && (KeyCode == 97 || KeyCode == 99) ) // B or X deletes while editing
    {
        ++GUT99V80QueuedBackspace;
        return;
    }
    if( GUT99AndroidImeOpenV79 && KeyCode == 96 ) // OUYA/Android A while editing
    {
        GUT99V47BtnA = 0;
        GUT99V50BtnA = 0;
        return;
    }

    if( KeyCode == 108 || KeyCode == 82 || KeyCode == 110 ) // START / OUYA MENU / BUTTON_MODE
    {
        if( Down && !GUT99V47BtnStart )
            UT99V72QueueStartToggle( "android-key" );
        GUT99V47QueuedStartPress = 0;
        GUT99V47QueuedStartRelease = 0;
    }

    UT99V49SetAndroidButtonExtra( KeyCode, Down );
    switch( KeyCode )
    {
        case 96: GUT99V47BtnA = Down; break;     // KEYCODE_BUTTON_A
        case 97: GUT99V47BtnB = Down; break;     // KEYCODE_BUTTON_B
        case 99: GUT99V47BtnX = Down; break;     // KEYCODE_BUTTON_X
        case 100: GUT99V47BtnY = Down; break;    // KEYCODE_BUTTON_Y
        case 102: GUT99V47BtnL1 = Down; break;   // KEYCODE_BUTTON_L1
        case 103: GUT99V47BtnR1 = Down; break;   // KEYCODE_BUTTON_R1
        case 106: GUT99V47BtnThumbL = Down; break; // KEYCODE_BUTTON_THUMBL - UT99_ANDROID_CONTROLLER_FULL_REMAP_V120
        case 107: GUT99V47BtnThumbR = Down; break; // KEYCODE_BUTTON_THUMBR - UT99_ANDROID_CONTROLLER_FULL_REMAP_V120
        case 104: GUT99V47LT = Down ? 1.0f : 0.0f; break; // KEYCODE_BUTTON_L2 / left trigger as button
        case 105: GUT99V47RT = Down ? 1.0f : 0.0f; break; // KEYCODE_BUTTON_R2 / right trigger as button
        case 82: GUT99V47BtnStart = Down; break; // KEYCODE_MENU / OUYA center button
        case 108: GUT99V47BtnStart = Down; break;// KEYCODE_BUTTON_START
        case 110: GUT99V47BtnStart = Down; break;// KEYCODE_BUTTON_MODE
        case 109: GUT99V47BtnSelect = Down; break;// KEYCODE_BUTTON_SELECT
        case 19: GUT99V47BtnDup = Down; break;   // DPAD_UP
        case 20: GUT99V47BtnDdown = Down; break; // DPAD_DOWN
        case 21: GUT99V47BtnDleft = Down; break; // DPAD_LEFT
        case 22: GUT99V47BtnDright = Down; break;// DPAD_RIGHT
    }
}
static void UT99V50SetAndroidAxisExtra( INT Axis, FLOAT Value )
{
    switch( Axis )
    {
        case 0:  GUT99V50LX = Value; break;   // AXIS_X
        case 1:  GUT99V50LY = Value; break;   // AXIS_Y
        case 11: GUT99V50RX = Value; break;   // AXIS_Z
        case 12: GUT99V50RX = Value; break;   // AXIS_RX / OUYA right stick fallback
        case 14: GUT99V50RY = Value; break;   // AXIS_RZ
        case 13: GUT99V50RY = Value; break;   // AXIS_RY / OUYA right stick fallback
        case 17: GUT99V50LT = Value; break;   // AXIS_LTRIGGER
        case 18: GUT99V50RT = Value; break;   // AXIS_RTRIGGER
        case 15: GUT99V50HatX = Value; break; // AXIS_HAT_X
        case 16: GUT99V50HatY = Value; break; // AXIS_HAT_Y
    }
}
static void UT99V47SetAndroidAxis( INT Axis, FLOAT Value )
{
    UT99V50SetAndroidAxisExtra( Axis, Value );
    switch( Axis )
    {
        case 0: GUT99V47LX = Value; break;   // AXIS_X
        case 1: GUT99V47LY = Value; break;   // AXIS_Y
        case 11: GUT99V47RX = Value; break;  // AXIS_Z
        case 12: GUT99V47RX = Value; break;  // AXIS_RX / OUYA right stick fallback
        case 14: GUT99V47RY = Value; break;  // AXIS_RZ
        case 13: GUT99V47RY = Value; break;  // AXIS_RY / OUYA right stick fallback
        case 17: GUT99V47LT = Value; break;  // AXIS_LTRIGGER
        case 18: GUT99V47RT = Value; break;  // AXIS_RTRIGGER
        case 15: GUT99V47HatX = Value; break;// AXIS_HAT_X
        case 16: GUT99V47HatY = Value; break;// AXIS_HAT_Y
    }
}
extern "C" JNIEXPORT void JNICALL Java_com_ast_ut99_GameActivity_nativeAndroidButtonV47(JNIEnv*, jclass, jint keyCode, jboolean down) { UT99V47SetAndroidButton((INT)keyCode, down ? 1 : 0); }
extern "C" JNIEXPORT jboolean JNICALL Java_com_ast_ut99_GameActivity_nativeAndroidIsMenuV90(JNIEnv*, jclass) { return GUT99AndroidMenuVisibleV91 ? JNI_TRUE : JNI_FALSE; } // UT99_ANDROID_V92_TOUCH_OVERLAY compat
extern "C" JNIEXPORT jboolean JNICALL Java_com_ast_ut99_GameActivity_nativeAndroidIsMenuV91(JNIEnv*, jclass) { return GUT99AndroidMenuVisibleV91 ? JNI_TRUE : JNI_FALSE; } // UT99_ANDROID_V92_TOUCH_OVERLAY compat
extern "C" JNIEXPORT jboolean JNICALL Java_com_ast_ut99_GameActivity_nativeAndroidIsMenuV92(JNIEnv*, jclass) { return GUT99AndroidMenuVisibleV91 ? JNI_TRUE : JNI_FALSE; } // UT99_ANDROID_V92_TOUCH_OVERLAY
extern "C" JNIEXPORT jint JNICALL Java_com_ast_ut99_GameActivity_nativeAndroidTouchUiStateRT(JNIEnv*, jclass) { return (jint)GUT99RetroTouchUiState; } // RETROTOUCH_BETA4_STATE_MACHINE_V2
extern "C" JNIEXPORT jint JNICALL Java_com_ast_ut99_GameActivity_nativeAndroidInputResetSerialRT(JNIEnv*, jclass) { return (jint)GUT99RetroTouchInputResetSerial; } // RETROTOUCH_BETA4_INPUT_RESET_SYNC
extern "C" JNIEXPORT void JNICALL Java_com_ast_ut99_GameActivity_nativeAndroidAxisV47(JNIEnv*, jclass, jint axis, jfloat value) { UT99V47SetAndroidAxis((INT)axis, (FLOAT)value); }
extern "C" JNIEXPORT void JNICALL Java_com_ast_ut99_GameActivity_nativeAndroidTouchLookV101(JNIEnv*, jclass, jfloat x, jfloat y)
{
    GUT99V101TouchLookX = Clamp(GUT99V101TouchLookX + (FLOAT)x, -2.0f, 2.0f);
    GUT99V101TouchLookY = Clamp(GUT99V101TouchLookY + (FLOAT)y, -2.0f, 2.0f);
}
extern "C" JNIEXPORT void JNICALL Java_com_ast_ut99_GameActivity_nativeAndroidTextV82(JNIEnv* Env, jclass, jstring Text)
{
    if( !Env || !Text )
        return;
    const char* Utf = Env->GetStringUTFChars( Text, NULL );
    if( Utf )
    {
        UT99V82QueueTextBytes( Utf );
        UT99_ANDROID_SDL_LOGI( "v82 queued Android IME text" );
        Env->ReleaseStringUTFChars( Text, Utf );
    }
}
extern "C" JNIEXPORT void JNICALL Java_com_ast_ut99dc_GameActivity_nativeAndroidButtonV47(JNIEnv*, jclass, jint keyCode, jboolean down) { UT99V47SetAndroidButton((INT)keyCode, down ? 1 : 0); }
extern "C" JNIEXPORT void JNICALL Java_com_ast_ut99dc_GameActivity_nativeAndroidAxisV47(JNIEnv*, jclass, jint axis, jfloat value) { UT99V47SetAndroidAxis((INT)axis, (FLOAT)value); }
extern "C" JNIEXPORT void JNICALL Java_com_ast_ut99dc_GameActivity_nativeAndroidTouchLookV101(JNIEnv*, jclass, jfloat x, jfloat y)
{
    GUT99V101TouchLookX = Clamp(GUT99V101TouchLookX + (FLOAT)x, -2.0f, 2.0f);
    GUT99V101TouchLookY = Clamp(GUT99V101TouchLookY + (FLOAT)y, -2.0f, 2.0f);
}
extern "C" JNIEXPORT void JNICALL Java_com_ast_unreal_GameActivity_nativeAndroidButtonV47(JNIEnv*, jclass, jint keyCode, jboolean down) { UT99V47SetAndroidButton((INT)keyCode, down ? 1 : 0); }
extern "C" JNIEXPORT void JNICALL Java_com_ast_unreal_GameActivity_nativeAndroidAxisV47(JNIEnv*, jclass, jint axis, jfloat value) { UT99V47SetAndroidAxis((INT)axis, (FLOAT)value); }
extern "C" JNIEXPORT void JNICALL Java_com_ast_unreal_GameActivity_nativeAndroidTouchLookV101(JNIEnv*, jclass, jfloat x, jfloat y)
{
    GUT99V101TouchLookX = Clamp(GUT99V101TouchLookX + (FLOAT)x, -2.0f, 2.0f);
    GUT99V101TouchLookY = Clamp(GUT99V101TouchLookY + (FLOAT)y, -2.0f, 2.0f);
}
#endif
#endif /* UT99_ANDROID_NATIVE_INPUT_V47_END_V47B */

#ifdef PLATFORM_ANDROID
#ifndef UT99_ANDROID_INPUT_ROUTER_V46
#define UT99_ANDROID_INPUT_ROUTER_V46 1
static INT GUT99V46LX = 0;
static INT GUT99V46LY = 0;
static INT GUT99V46RX = 0;
static INT GUT99V46RY = 0;
static UBOOL GUT99V46Forward = 0;
static UBOOL GUT99V46Back = 0;
static UBOOL GUT99V46Left = 0;
static UBOOL GUT99V46Right = 0;
static UBOOL GUT99V46Fire = 0;
static UBOOL GUT99V46AltFire = 0;
static DOUBLE GUT99V46MenuLast[32] = {0};
static DOUBLE GUT99V46LastLookLog = 0.0;

static INT UT99V46AbsI( INT V ) { return V < 0 ? -V : V; }
static FLOAT UT99V46ClampF( FLOAT V, FLOAT Lo, FLOAT Hi ) { return V < Lo ? Lo : (V > Hi ? Hi : V); }
static EInputKey UT99V46Key( INT K ) { return (EInputKey)K; }

static void UT99V46Event( UNSDLViewport* Viewport, INT Key, EInputAction Action, FLOAT Delta=0.0f )
{
    if( Viewport && Key > 0 )
        Viewport->CauseInputEvent( UT99V46Key(Key), Action, Delta );
}
static void UT99V46PressState( UNSDLViewport* Viewport, INT Key, UBOOL& OldState, UBOOL NewState )
{
    if( OldState != NewState )
    {
        OldState = NewState;
        UT99V46Event( Viewport, Key, NewState ? IST_Press : IST_Release );
    }
}

static UBOOL UT99V46MenuButton( UNSDLViewport* Viewport, INT Button, UBOOL Down )
{
    if( !Down )
        return 1;

    INT Key = 0;
    switch( Button )
    {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:    Key = 38; break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  Key = 40; break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  Key = 37; break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: Key = 39; break;
        case SDL_CONTROLLER_BUTTON_A: Key = 13; break;
        case SDL_CONTROLLER_BUTTON_B: return 1; // UT99_ANDROID_CONTROLLER_FULL_REMAP_V120: B is bindable, not menu back.
        case SDL_CONTROLLER_BUTTON_START: Key = 27; break; // START opens/closes menu.
        case SDL_CONTROLLER_BUTTON_BACK: return 1; // SELECT must not open/close menu.
        default: return 0;
    }

    if( Key == 37 || Key == 38 || Key == 39 || Key == 40 )
    {
        INT Index = Button;
        if( Index < 0 || Index >= 32 ) Index = 31;
        DOUBLE Now = appSeconds();
        if( GUT99V46MenuLast[Index] > 0.0 && (Now - GUT99V46MenuLast[Index]) < 0.24 )
            return 1;
        GUT99V46MenuLast[Index] = Now;
    }

    UT99V46Event( Viewport, Key, IST_Press );
    UT99V46Event( Viewport, Key, IST_Release );
    UT99_ANDROID_SDL_LOGI( "v46 menu button=%d -> key=%d", Button, Key );
    return 1;
}

static UBOOL UT99V46GameButton( UNSDLViewport* Viewport, INT Button, UBOOL Down )
{
    switch( Button )
    {
        case SDL_CONTROLLER_BUTTON_A:
            UT99V46Event( Viewport, IK_Joy1, Down ? IST_Press : IST_Release ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117 Joy1/A
            return 1;
        case SDL_CONTROLLER_BUTTON_B:
            UT99V46Event( Viewport, IK_Joy2, Down ? IST_Press : IST_Release ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117 Joy2/B
            return 1;
        case SDL_CONTROLLER_BUTTON_X:
            UT99V46Event( Viewport, IK_Joy3, Down ? IST_Press : IST_Release ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117 Joy3/X
            return 1;
        case SDL_CONTROLLER_BUTTON_Y:
            UT99V46Event( Viewport, IK_Joy4, Down ? IST_Press : IST_Release ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117 Joy4/Y
            return 1;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
            UT99V46Event( Viewport, IK_Joy11, Down ? IST_Press : IST_Release ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117 RB/Joy11
            return 1;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
            UT99V46Event( Viewport, IK_Joy10, Down ? IST_Press : IST_Release ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117 LB/Joy10
            return 1;
        case SDL_CONTROLLER_BUTTON_LEFTSTICK:
            UT99V46Event( Viewport, IK_Joy8, Down ? IST_Press : IST_Release ); // UT99_ANDROID_CONTROLLER_FULL_REMAP_V120 LJoyPress
            return 1;
        case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
            UT99V46Event( Viewport, IK_Joy9, Down ? IST_Press : IST_Release ); // UT99_ANDROID_CONTROLLER_FULL_REMAP_V120 RJoyPress
            return 1;
        case SDL_CONTROLLER_BUTTON_START:
            UT99V46Event( Viewport, 27, Down ? IST_Press : IST_Release ); // Escape/Menu
            return 1;
        case SDL_CONTROLLER_BUTTON_BACK:
            return 1; // SELECT ignored; it must not be the menu key.
    }
    return 1; // Swallow unknown controller buttons in gameplay to avoid old joy mapper noise.
}

static UBOOL UT99V46Axis( UNSDLViewport* Viewport, INT Axis, INT Value, UBOOL bMenu )
{
    if( bMenu )
        return 1; // No analog axis in menu; DPad/buttons only.

    const INT Dead = 8500;
    INT V = (UT99V46AbsI(Value) > Dead) ? Value : 0;
    switch( Axis )
    {
        case SDL_CONTROLLER_AXIS_LEFTX:
            GUT99V46LX = V;
            return 1;
        case SDL_CONTROLLER_AXIS_LEFTY:
            GUT99V46LY = V;
            return 1;
        case SDL_CONTROLLER_AXIS_RIGHTX:
            GUT99V46RX = V;
            return 1;
        case SDL_CONTROLLER_AXIS_RIGHTY:
            GUT99V46RY = V;
            return 1;
        case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
        {
            UBOOL Down = Value > 14500;
            UT99V46PressState( Viewport, IK_Joy12, GUT99V46AltFire, Down ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117
            return 1;
        }
        case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
        {
            UBOOL Down = Value > 14500;
            UT99V46PressState( Viewport, IK_Joy13, GUT99V46Fire, Down ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117
            return 1;
        }
    }
    return 1;
}

static void UT99V46TickInput( UNSDLViewport* Viewport, UBOOL bMenu )
{
    if( !Viewport || bMenu )
        return;

    const INT MoveDead = 11000;
    UT99V46PressState( Viewport, IK_UnknownDA, GUT99V46Forward, GUT99V46LY < -MoveDead ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117
    UT99V46PressState( Viewport, IK_UnknownDF, GUT99V46Back,    GUT99V46LY >  MoveDead ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117
    UT99V46PressState( Viewport, IK_UnknownD8, GUT99V46Left,    GUT99V46LX < -MoveDead ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117
    UT99V46PressState( Viewport, IK_UnknownD9, GUT99V46Right,   GUT99V46LX >  MoveDead ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117

    const FLOAT LookScaleX = 0.0038f;
    const FLOAT LookScaleY = 0.0028f;
    const FLOAT MaxStepX = 128.0f;
    const FLOAT MaxStepY = 96.0f;
    if( GUT99V46RX )
    {
        FLOAT DX = UT99V46ClampF( ((FLOAT)GUT99V46RX) * LookScaleX, -MaxStepX, MaxStepX );
        UT99V46Event( Viewport, IK_MouseX, IST_Axis, DX );
        DOUBLE Now = appSeconds();
        if( Now - GUT99V46LastLookLog > 0.75 )
        {
            GUT99V46LastLookLog = Now;
            UT99_ANDROID_SDL_LOGI( "v46 look rx=%d dx=%.1f", GUT99V46RX, DX );
        }
    }
    if( GUT99V46RY )
    {
        FLOAT DY = UT99V46ClampF( ((FLOAT)-GUT99V46RY) * LookScaleY, -MaxStepY, MaxStepY );
        UT99V46Event( Viewport, IK_MouseY, IST_Axis, DY );
    }
}
#endif
#endif

#ifdef PLATFORM_ANDROID
#ifndef UT99_ANDROID_MENU_INPUT_V43
#define UT99_ANDROID_MENU_INPUT_V43 1
#include <android/log.h>
#ifndef UT99_ANDROID_SDL_LOGI
#define UT99_ANDROID_SDL_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "UT99SDL", __VA_ARGS__)
#endif
static DOUBLE GUT99AndroidMenuLastButtonV43[64] = {0};

static INT UT99AndroidMenuButtonToKeyV43( INT Button )
{
    switch( Button )
    {
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  return 37; // IK_Left
        case SDL_CONTROLLER_BUTTON_DPAD_UP:    return 38; // IK_Up
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return 39; // IK_Right
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  return 40; // IK_Down
        case SDL_CONTROLLER_BUTTON_A:          return 13; // IK_Enter
        case SDL_CONTROLLER_BUTTON_B:          return 0;  // UT99_ANDROID_CONTROLLER_FULL_REMAP_V120: B is bindable, not menu back
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return 1; // IK_LeftMouse / v88 menu click
        case SDL_CONTROLLER_BUTTON_BACK:       return 0;  // v72: SELECT ignored
        case SDL_CONTROLLER_BUTTON_START:      return 0;  // v72: START handled by single-toggle queue
    }

    // Some Android/Retroid paths arrive as raw button numbers instead of SDL enum names.
    switch( Button )
    {
        case 13: return 37; // left
        case 11: return 38; // up
        case 14: return 39; // right
        case 12: return 40; // down
        case 0:  return 13; // A / Enter
        case 1:  return 0;  // UT99_ANDROID_CONTROLLER_FULL_REMAP_V120: B is bindable, not menu back
        case 4:  return 0;  // v72: Back-like/Select ignored
        case 6:  return 0;  // v72: raw Start handled by single-toggle queue
    }
    return 0;
}

static UBOOL UT99AndroidMenuButtonDebounceV43( INT Button )
{
    INT Index = Button;
    if( Index < 0 || Index >= 64 )
        Index = 63;

    DOUBLE Now = appSeconds();
    DOUBLE Delay = 0.20; // seconds; enough to stop racing without feeling dead.
    if( GUT99AndroidMenuLastButtonV43[Index] > 0.0 && (Now - GUT99AndroidMenuLastButtonV43[Index]) < Delay )
        return 0;
    GUT99AndroidMenuLastButtonV43[Index] = Now;
    return 1;
}

static UBOOL UT99AndroidMenuControllerButtonV43( UNSDLViewport* Viewport, INT Button, UBOOL Down )
{
    if( !Viewport )
        return 1;

    // Use DOWN as a single menu action.  UP is swallowed, because we generate
    // a compact press+release below. This avoids stuck key states and DPad auto-repeat.
    if( !Down )
        return 1;

    if( Button == SDL_CONTROLLER_BUTTON_START || Button == 6 )
    {
        UT99V72QueueStartToggle( "sdl-menu-start" );
        return 1;
    }
    if( Button == SDL_CONTROLLER_BUTTON_BACK || Button == 4 )
        return 1;

    INT Key = UT99AndroidMenuButtonToKeyV43( Button );
    if( Key == 0 )
    {
        UT99_ANDROID_SDL_LOGI( "v43 swallowed unhandled menu controller button=%d", Button );
        return 1;
    }

    if( !UT99AndroidMenuButtonDebounceV43( Button ) )
    {
        UT99_ANDROID_SDL_LOGI( "v43 debounced menu button=%d key=%d", Button, Key );
        return 1;
    }

#if defined(__ANDROID__)
    if( GUT99AndroidImeOpenV79 && Key == 13 )
    {
        // UT99_ANDROID_V109_OUYA_TEXTFIELD_IME_REPAIR:
        // OUYA A/Enter must not instantly close or re-submit an active UWindow
        // edit field. Keep the IME open and let typed characters/backspace flow.
        UT99_ANDROID_SDL_LOGI( "v109 swallowed A/Enter while Android IME is open button=%d key=%d", Button, Key );
        return 1;
    }
    if( GUT99AndroidImeOpenV79 && Key == 27 )
    {
        UT99AndroidHideSoftKeyboardV72();
        UT99_ANDROID_SDL_LOGI( "v109 menu controller Escape hid Android IME button=%d key=%d", Button, Key );
        return 1;
    }
#endif

    Viewport->CauseInputEvent( (EInputKey)Key, IST_Press );
    Viewport->CauseInputEvent( (EInputKey)Key, IST_Release );
    UT99_ANDROID_SDL_LOGI( "v43 menu controller button=%d -> key=%d", Button, Key );
    return 1;
}
#endif
#endif

#ifdef PLATFORM_ANDROID
#ifndef UT99_ANDROID_GAMEPAD_GAMEPLAY_V39
#define UT99_ANDROID_GAMEPAD_GAMEPLAY_V39 1
#ifndef UT99_ANDROID_SDL_LOGI
#include <android/log.h>
#define UT99_ANDROID_SDL_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "UT99SDL", __VA_ARGS__)
#endif
static UBOOL GUT99AndroidGameplayKeyDownV39[256] = {0};
static UBOOL GUT99AndroidGameplayLastMouseShowV39 = 1;
static INT UT99AndroidAbsV39( INT V ) { return V < 0 ? -V : V; }
static void UT99AndroidGameplaySetKeyV39( UNSDLViewport* Viewport, EInputKey Key, UBOOL Down )
{
    if( !Viewport || Key <= IK_None || Key >= IK_MAX )
        return;
    if( GUT99AndroidGameplayKeyDownV39[(INT)Key] == Down )
        return;
    GUT99AndroidGameplayKeyDownV39[(INT)Key] = Down;
    Viewport->CauseInputEvent( Key, Down ? IST_Press : IST_Release );
    UT99_ANDROID_SDL_LOGI( "v39 gameplay key %d %s", (INT)Key, Down ? "down" : "up" );
}
static void UT99AndroidGameplayReleaseMoveKeysV39( UNSDLViewport* Viewport )
{
    UT99AndroidGameplaySetKeyV39( Viewport, IK_UnknownDA, 0 ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117 LJoyUp
    UT99AndroidGameplaySetKeyV39( Viewport, IK_UnknownDF, 0 ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117 LJoyDown
    UT99AndroidGameplaySetKeyV39( Viewport, IK_UnknownD8, 0 ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117 LJoyLeft
    UT99AndroidGameplaySetKeyV39( Viewport, IK_UnknownD9, 0 ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117 LJoyRight
}

/* UT99_ANDROID_INPUT_ROUTER_V41B_HELPERS_BEGIN
   Compilefix for v41: the event hooks were inserted, but the helper/state block
   was missing on some source layouts. Keep this block before
   UT99AndroidGameplayControllerAxisV39 because that function stores right-stick state. */
#ifndef UT99_ANDROID_INPUT_ROUTER_V41B_HELPERS
#define UT99_ANDROID_INPUT_ROUTER_V41B_HELPERS 1

static INT GUT99AndroidRightStickXv41 = 0;
static INT GUT99AndroidRightStickYv41 = 0;
static UBOOL GUT99AndroidTriggerFireDownV41 = 0;
static UBOOL GUT99AndroidTriggerAltFireDownV41 = 0;
static DOUBLE GUT99AndroidMenuLastButtonTimeV41[32] = {0};
static DOUBLE GUT99AndroidLastLookLogTimeV41 = 0.0;

static inline INT UT99AndroidClampIntV41( INT Value, INT MinValue, INT MaxValue )
{
    return Value < MinValue ? MinValue : ( Value > MaxValue ? MaxValue : Value );
}

static inline UBOOL UT99AndroidMenuSuppressJoyDuplicateV41( INT Button )
{
    /* SDL controller button numeric fallback:
       A=0, B=1, Back=4, Start=6, DPad Up/Down/Left/Right=11/12/13/14.
       In menu mode these are already routed as UI/controller events, so the
       parallel joystick event must not run again. */
    return Button == 0 || Button == 1 || Button == 4 || Button == 6 ||
           Button == 11 || Button == 12 || Button == 13 || Button == 14;
}

static UBOOL UT99AndroidGameplayButtonV41( UNSDLViewport* Viewport, INT Button, UBOOL Down )
{
    if( !Viewport )
        return 0;

    switch( Button )
    {
        case 0:  /* A / right face on Nintendo-style handhelds */
            UT99AndroidGameplaySetKeyV39( Viewport, IK_Joy1, Down ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117
            return 1;
        case 1:  /* B / lower face / OUYA O */
            UT99AndroidGameplaySetKeyV39( Viewport, IK_Joy2, Down ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117
            return 1;
        case 2:  /* X / upper face / OUYA Y */
            UT99AndroidGameplaySetKeyV39( Viewport, IK_Joy3, Down ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117
            return 1;
        case 3:  /* Y / left face / OUYA U */
            UT99AndroidGameplaySetKeyV39( Viewport, IK_Joy4, Down ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117
            return 1;
        case 6:  /* Start */
            if( Down )
                UT99V72QueueStartToggle( "sdl-game-start" );
            return 1;
        case 9:  /* Left shoulder */
            UT99AndroidGameplaySetKeyV39( Viewport, IK_Joy10, Down ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117
            return 1;
        case 10: /* Right shoulder */
            UT99AndroidGameplaySetKeyV39( Viewport, IK_Joy11, Down ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117
            return 1;
        case 7:  /* Left stick press */
            UT99AndroidGameplaySetKeyV39( Viewport, IK_Joy8, Down ); // UT99_ANDROID_CONTROLLER_FULL_REMAP_V120
            return 1;
        case 8:  /* Right stick press */
            UT99AndroidGameplaySetKeyV39( Viewport, IK_Joy9, Down ); // UT99_ANDROID_CONTROLLER_FULL_REMAP_V120
            return 1;
        default:
            break;
    }

    return 0;
}

static void UT99AndroidGameplayTriggerAxisV41( UNSDLViewport* Viewport, INT Axis, INT Value )
{
    if( !Viewport )
        return;

    const INT TriggerDead = 12000;
    if( Axis == 4 )
    {
        UBOOL Down = Value > TriggerDead;
        if( Down != GUT99AndroidTriggerAltFireDownV41 )
        {
            GUT99AndroidTriggerAltFireDownV41 = Down;
            UT99AndroidGameplaySetKeyV39( Viewport, IK_Joy12, Down ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117
        }
    }
    else if( Axis == 5 )
    {
        UBOOL Down = Value > TriggerDead;
        if( Down != GUT99AndroidTriggerFireDownV41 )
        {
            GUT99AndroidTriggerFireDownV41 = Down;
            UT99AndroidGameplaySetKeyV39( Viewport, IK_Joy13, Down ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117
        }
    }
}

static void UT99AndroidGameplayTickLookV41( UNSDLViewport* Viewport )
{
    if( !Viewport )
        return;

    if( GUT99AndroidRightStickXv41 == 0 && GUT99AndroidRightStickYv41 == 0 )
        return;

    // UT99_ANDROID_V83_RIGHT_STICK_SPEED:
    // v82 still felt slightly too slow on both OUYA and Retroid.  Increase the
    // real continuous-look delta, not just the diagnostic log value.  This is
    // intentionally moderate to avoid returning to the old over-fast spin bug.
    const FLOAT LookScaleX = 0.00122f;
    const FLOAT LookScaleY = 0.00088f;
    INT Dx = UT99AndroidClampIntV41( (INT)( (FLOAT)GUT99AndroidRightStickXv41 * LookScaleX ), -96, 96 );
    INT Dy = UT99AndroidClampIntV41( (INT)( (FLOAT)-GUT99AndroidRightStickYv41 * LookScaleY ), -38, 38 );

    if( Dx != 0 )
        Viewport->CauseInputEvent( IK_MouseX, IST_Axis, (FLOAT)Dx );
    if( Dy != 0 )
        Viewport->CauseInputEvent( IK_MouseY, IST_Axis, (FLOAT)Dy );

#if PLATFORM_ANDROID
    DOUBLE Now = appSeconds();
    if( Now - GUT99AndroidLastLookLogTimeV41 > 0.50 )
    {
        GUT99AndroidLastLookLogTimeV41 = Now;

UT99_ANDROID_SDL_LOGI( "v83 gameplay continuous look rx=%d ry=%d dx=%d dy=%d", GUT99AndroidRightStickXv41, GUT99AndroidRightStickYv41, Dx, Dy );
    }
#endif
}

#endif /* UT99_ANDROID_INPUT_ROUTER_V41B_HELPERS */
/* UT99_ANDROID_INPUT_ROUTER_V41B_HELPERS_END */
static UBOOL UT99AndroidGameplayControllerAxisV39( UNSDLViewport* Viewport, INT Axis, INT Value )
{
    if( !Viewport )
        return 0;

    const INT Dead = 10000; /* UT99_ANDROID_INPUT_ROUTER_V41 */
    const FLOAT LookScale = 0.00265f; /* UT99_ANDROID_LOOK_SPEED_V45 */ /* unused by v41 continuous look */

    // SDL controller axis order: 0=LX, 1=LY, 2=RX, 3=RY, 4/5=triggers.
    if( Axis == 0 )
    {
        UT99AndroidGameplaySetKeyV39( Viewport, IK_UnknownD8, Value < -Dead ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117
        UT99AndroidGameplaySetKeyV39( Viewport, IK_UnknownD9, Value >  Dead ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117
        if( UT99AndroidAbsV39(Value) <= Dead )
        {
            UT99AndroidGameplaySetKeyV39( Viewport, IK_UnknownD8, 0 ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117
            UT99AndroidGameplaySetKeyV39( Viewport, IK_UnknownD9, 0 ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117
        }
        return 1;
    }
    if( Axis == 1 )
    {
        UT99AndroidGameplaySetKeyV39( Viewport, IK_UnknownDA, Value < -Dead ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117
        UT99AndroidGameplaySetKeyV39( Viewport, IK_UnknownDF, Value >  Dead ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117
        if( UT99AndroidAbsV39(Value) <= Dead )
        {
            UT99AndroidGameplaySetKeyV39( Viewport, IK_UnknownDA, 0 ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117
            UT99AndroidGameplaySetKeyV39( Viewport, IK_UnknownDF, 0 ); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117
        }
        return 1;
    }
    if( Axis == 2 )
    {
        GUT99AndroidRightStickXv41 = (UT99AndroidAbsV39(Value) > Dead) ? Value : 0;
        return 1;
    }
    if( Axis == 3 )
    {
        GUT99AndroidRightStickYv41 = (UT99AndroidAbsV39(Value) > Dead) ? Value : 0;
        return 1;
    }
        if( Axis == 4 || Axis == 5 || Axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || Axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT )
    {
        UT99AndroidGameplayTriggerAxisV41( Viewport, Axis, Value );
        return 1;
    }
return 0;
}
#endif
#endif

#ifdef PLATFORM_ANDROID
#define UT99_ANDROID_NATIVE_SURFACE_V60 1
#ifndef UT99_ANDROID_SDL_LOGI
#include <android/log.h>
#define UT99_ANDROID_SDL_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "UT99SDL", __VA_ARGS__)
#endif
static void UT99AndroidKeepNativeViewportV60( INT& X, INT& Y, const char* Where )
{
    // UT99_ANDROID_V60_NATIVE_SURFACE_AUTOSCALE:
    // Keep Android in native drawable coordinates.  Android's SurfaceHolder now
    // follows the native layout size,
    // and UT99AndroidAdoptDrawableSizeV29/SDL_GL_GetDrawableSize supply the real
    // drawable resolution, e.g. 1612x720 on wide handhelds.
    UT99_ANDROID_SDL_LOGI("v60 native viewport keeps %s at %dx%d", Where ? Where : "?", X, Y);
}
#endif

#ifdef PLATFORM_ANDROID
#ifndef UT99_ANDROID_TOUCH_MOUSE_V32
#define UT99_ANDROID_TOUCH_MOUSE_V32 1
#include <android/log.h>
#ifndef UT99_ANDROID_SDL_LOGI
#define UT99_ANDROID_SDL_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "UT99SDL", __VA_ARGS__)
#endif
static UBOOL GUT99AndroidTouchMouseDownV32 = 0;
static UBOOL GUT99AndroidTouchMouseHadLastV32 = 0;
static UBOOL GUT99AndroidSuppressNextTouchClickV37 = 0;
static UBOOL GUT99AndroidMenuTopTapSyntheticOnlyV164 = 0; // UT99_ANDROID_V164_MENU_TOPBAR_SINGLE_SYNTH_CLICK
static INT   GUT99AndroidMenuTopTapXV164 = 0; // UT99_ANDROID_V164_MENU_TOPBAR_SINGLE_SYNTH_CLICK
static INT   GUT99AndroidMenuTopTapYV164 = 0; // UT99_ANDROID_V164_MENU_TOPBAR_SINGLE_SYNTH_CLICK
static INT   GUT99AndroidTouchMouseLastXV32 = 0;
static INT   GUT99AndroidTouchMouseLastYV32 = 0;
static UBOOL GUT99AndroidMenuTouchActiveV169B = 0; // UT99_ANDROID_V169B_MENU_INPUT_ROUTER
static INT   GUT99AndroidMenuTouchDownXV169B = 0; // UT99_ANDROID_V169B_MENU_INPUT_ROUTER
static INT   GUT99AndroidMenuTouchDownYV169B = 0; // UT99_ANDROID_V169B_MENU_INPUT_ROUTER
static UBOOL GUT99AndroidMenuTouchMovedV169B = 0; // UT99_ANDROID_V169B_MENU_INPUT_ROUTER
static UBOOL GUT99AndroidMenuTouchButtonHeldV169B = 0; // UT99_ANDROID_V169B_MENU_INPUT_ROUTER
static DOUBLE GUT99AndroidRecentFingerMouseUntilV169C = 0.0; // UT99_ANDROID_V169C_TOUCH_MENU_RESTORE
static INT    GUT99AndroidRecentFingerMouseXV169C = -100000; // UT99_ANDROID_V169C_TOUCH_MENU_RESTORE
static INT    GUT99AndroidRecentFingerMouseYV169C = -100000; // UT99_ANDROID_V169C_TOUCH_MENU_RESTORE
static DOUBLE GUT99AndroidRecentFingerMouseLastLogV169C = 0.0; // UT99_ANDROID_V169C_TOUCH_MENU_RESTORE

static void UT99AndroidRememberRecentFingerMouseV169C( INT MouseX, INT MouseY )
{
    // UT99_ANDROID_V169C_TOUCH_MENU_RESTORE:
    // Some Android/SDL stacks report touch-derived mouse button/motion events with
    // a regular-looking mouse id.  Keep a short coordinate-based guard so those
    // synthetic follow-up events cannot steal the working HID/BT mouse path.
    GUT99AndroidRecentFingerMouseXV169C = MouseX;
    GUT99AndroidRecentFingerMouseYV169C = MouseY;
    GUT99AndroidRecentFingerMouseUntilV169C = appSeconds() + 0.35;
}

static UBOOL UT99AndroidLooksLikeRecentFingerMouseV169C( INT MouseX, INT MouseY )
{
    if( appSeconds() > GUT99AndroidRecentFingerMouseUntilV169C )
        return 0;
    return Abs( MouseX - GUT99AndroidRecentFingerMouseXV169C ) <= 20
        && Abs( MouseY - GUT99AndroidRecentFingerMouseYV169C ) <= 20;
}

static void UT99AndroidLogRecentFingerMouseSuppressV169C( const char* Phase, INT MouseX, INT MouseY )
{
    DOUBLE Now = appSeconds();
    if( Now - GUT99AndroidRecentFingerMouseLastLogV169C > 0.30 )
    {
        GUT99AndroidRecentFingerMouseLastLogV169C = Now;
        UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V169C_TOUCH_MENU_RESTORE suppress synthetic %s x=%d y=%d", Phase, MouseX, MouseY );
    }
}

static void UT99AndroidMenuTouchRetargetV169B( UNSDLViewport* Viewport, INT MouseX, INT MouseY )
{
    // UT99_ANDROID_V169E_DIRECT_UWINDOW_TOUCH:
    // Keep the viewport cursor coordinates in sync for the next render pass, but
    // do not drive UWindow through MouseX/MouseY axis events. Touch clicks are
    // dispatched directly to UWindowRootWindow below.
    if( !Viewport )
        return;
    Viewport->WindowsMouseX = MouseX;
    Viewport->WindowsMouseY = MouseY;
    Viewport->SelectedCursor = 0;
}

static UObject* UT99AndroidFindObjectPropertyV169E( UObject* Obj, const TCHAR* Name )
{
    if( !Obj || !Obj->GetClass() || !Name )
        return NULL;
    for( TFieldIterator<UProperty> It( Obj->GetClass() ); It; ++It )
    {
        UProperty* Prop = *It;
        if( Prop && appStricmp( Prop->GetName(), Name ) == 0 && Prop->IsA( UObjectProperty::StaticClass() ) )
            return *(UObject**)((BYTE*)Obj + Prop->Offset);
    }
    return NULL;
}

static UObject* UT99AndroidFindWindowConsoleV169E()
{
    for( TObjectIterator<UObject> It; It; ++It )
    {
        UObject* Obj = *It;
        if( !Obj || !Obj->GetClass() )
            continue;
        const TCHAR* ClassName = Obj->GetClass()->GetName();
        if( ClassName && appStrstr( ClassName, TEXT("WindowConsole") ) )
            return Obj;
    }
    return NULL;
}

static UObject* UT99AndroidFindUWindowRootV169E()
{
    UObject* Console = UT99AndroidFindWindowConsoleV169E();
    if( !Console )
        return NULL;
    return UT99AndroidFindObjectPropertyV169E( Console, TEXT("Root") );
}

static FLOAT UT99AndroidGetRootScaleV169E( UObject* Root )
{
    if( !Root )
        return 1.0f;
    UFloatProperty* ScaleProp = UT99AndroidFindFloatPropertyV91( Root, TEXT("GUIScale") );
    if( !ScaleProp )
        return 1.0f;
    FLOAT Scale = *(FLOAT*)((BYTE*)Root + ScaleProp->Offset);
    if( Scale < 0.25f )
        Scale = 1.0f;
    return Scale;
}

static void UT99AndroidSetWindowConsoleMouseUIV169E( FLOAT X, FLOAT Y )
{
    UObject* Console = UT99AndroidFindWindowConsoleV169E();
    if( Console )
    {
        UT99AndroidSetFloatPropertyV91( Console, TEXT("MouseX"), X );
        UT99AndroidSetFloatPropertyV91( Console, TEXT("MouseY"), Y );
    }
}

static UBOOL UT99AndroidUWindowMoveMouseV169E( UNSDLViewport* Viewport, INT MouseX, INT MouseY, FLOAT* OutUIX, FLOAT* OutUIY )
{
    // UT99_ANDROID_V169E_DIRECT_UWINDOW_TOUCH:
    // WindowConsole only forwards MouseX/MouseY to Root during RenderUWindow().
    // A finger tap can arrive before that render update, so a normal LeftMouse
    // KeyEvent still hits Root.MouseWindow from the previous cursor position.
    // Call Root.MoveMouse() immediately and then dispatch WindowEvent directly.
    UObject* Root = UT99AndroidFindUWindowRootV169E();
    if( !Viewport || !Root )
        return 0;

    const FLOAT Scale = UT99AndroidGetRootScaleV169E( Root );
    const FLOAT UIX = ((FLOAT)MouseX) / Scale;
    const FLOAT UIY = ((FLOAT)MouseY) / Scale;
    if( OutUIX ) *OutUIX = UIX;
    if( OutUIY ) *OutUIY = UIY;

    UT99AndroidMenuTouchRetargetV169B( Viewport, MouseX, MouseY );
    UT99AndroidSetWindowConsoleMouseUIV169E( UIX, UIY );

    UFunction* MoveMouse = Root->FindFunction( FName(TEXT("MoveMouse"), FNAME_Find) );
    if( MoveMouse )
    {
        struct FMoveMouseParms
        {
            FLOAT X;
            FLOAT Y;
        } Parms;
        Parms.X = UIX;
        Parms.Y = UIY;
        Root->ProcessEvent( MoveMouse, &Parms );
        return 1;
    }
    return 0;
}

static void UT99AndroidHideOrParkMenuPointerV169K( UNSDLViewport* Viewport, UBOOL bForcePark, const char* Source )
{
    // UT99_ANDROID_V169K_TOUCH_IDLE_CURSOR_HIDE:
    // Park the UWindow mouse outside the top menu when touch is the active input
    // mode.  This prevents UMenuMenuBar from keeping the previous hover item as
    // the first click target.  HID/BT mouse and left-stick virtual mouse re-enable
    // a visible cursor through UT99AndroidMenuPointerActivityV169K().
    if( !Viewport )
        return;
    if( !bForcePark && UT99AndroidMenuPointerVisibleV169K() )
        return;

    // UT99_ANDROID_V169M_CENTER_IDLE_VIRTUAL_MOUSE:
    // Keep touch/idle mode away from the top menu hover area, but do not leave
    // the virtual left-stick mouse parked in the lower-right corner.  Centering
    // the hidden UWindow pointer makes the next left-stick activation start from
    // a predictable, comfortable position on both Retroid and OUYA.
    const INT ParkX = Clamp<INT>( Max( 1, Viewport->SizeX ) / 2, 0, Max( 1, Viewport->SizeX ) - 1 );
    const INT ParkY = Clamp<INT>( Max( 1, Viewport->SizeY ) / 2, 0, Max( 1, Viewport->SizeY ) - 1 );
    Viewport->WindowsMouseX = ParkX;
    Viewport->WindowsMouseY = ParkY;
    Viewport->SelectedCursor = 0;
    Viewport->bWindowsMouseAvailable = 1;
    UT99AndroidSetWindowConsoleMouseV91( ParkX, ParkY );
    FLOAT UIX = 0.0f, UIY = 0.0f;
    UT99AndroidUWindowMoveMouseV169E( Viewport, ParkX, ParkY, &UIX, &UIY );
    SDL_ShowCursor( SDL_DISABLE );

    DOUBLE Now = appSeconds();
    if( Now - GUT99V169KMenuPointerLastLog > 0.80 )
    {
        GUT99V169KMenuPointerLastLog = Now;
        UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V169K_TOUCH_IDLE_CURSOR_HIDE pointer parked source=%s x=%d y=%d", Source ? Source : "?", ParkX, ParkY );
    }
}

static UBOOL UT99AndroidClassHasNameV169H( UObject* Obj, const TCHAR* Needle )
{
    if( !Obj || !Obj->GetClass() || !Needle )
        return 0;

    for( UClass* Cls = Obj->GetClass(); Cls; Cls = Cls->GetSuperClass() )
    {
        const TCHAR* ClassName = Cls->GetName();
        if( ClassName && appStrstr( ClassName, Needle ) )
            return 1;
    }
    return 0;
}

static UObject* UT99AndroidFindWindowUnderV169H( UObject* Root, FLOAT UIX, FLOAT UIY )
{
    if( !Root )
        return NULL;

    UFunction* FindWindowUnder = Root->FindFunction( FName(TEXT("FindWindowUnder"), FNAME_Find) );
    if( !FindWindowUnder )
        return NULL;

    struct FFindWindowUnderParms
    {
        FLOAT X;
        FLOAT Y;
        UObject* ReturnValue;
    } Parms;
    appMemzero( &Parms, sizeof(Parms) );
    Parms.X = UIX;
    Parms.Y = UIY;
    Root->ProcessEvent( FindWindowUnder, &Parms );
    return Parms.ReturnValue;
}

static UBOOL UT99AndroidCallGetMouseXYV169H( UObject* Window, FLOAT* OutX, FLOAT* OutY )
{
    if( !Window )
        return 0;

    UFunction* GetMouseXY = Window->FindFunction( FName(TEXT("GetMouseXY"), FNAME_Find) );
    if( !GetMouseXY )
        return 0;

    struct FGetMouseXYParms
    {
        FLOAT X;
        FLOAT Y;
    } Parms;
    appMemzero( &Parms, sizeof(Parms) );
    Window->ProcessEvent( GetMouseXY, &Parms );
    if( OutX ) *OutX = Parms.X;
    if( OutY ) *OutY = Parms.Y;
    return 1;
}

static UBOOL UT99AndroidCallMouseFuncV169H( UObject* Window, const TCHAR* FunctionName, FLOAT X, FLOAT Y )
{
    if( !Window || !FunctionName )
        return 0;

    UFunction* Func = Window->FindFunction( FName(FunctionName, FNAME_Find) );
    if( !Func )
        return 0;

    struct FMouseFuncParms
    {
        FLOAT X;
        FLOAT Y;
    } Parms;
    Parms.X = X;
    Parms.Y = Y;
    Window->ProcessEvent( Func, &Parms );
    return 1;
}

static UObject* GUT99AndroidTouchDownWindowV169I = NULL;
static FLOAT GUT99AndroidTouchDownLocalXV169I = 0.0f;
static FLOAT GUT99AndroidTouchDownLocalYV169I = 0.0f;
static UBOOL GUT99AndroidTouchDownWasDirectV169I = 0;

static UBOOL UT99AndroidSetBoolPropertyV169I( UObject* Obj, const TCHAR* Name, UBOOL Value )
{
    UBoolProperty* BoolProp = UT99AndroidFindBoolPropertyV79( Obj, Name );
    if( !BoolProp )
        return 0;
    DWORD* Ptr = (DWORD*)((BYTE*)Obj + BoolProp->Offset);
    if( Value )
        *Ptr |= BoolProp->BitMask;
    else
        *Ptr &= ~BoolProp->BitMask;
    return 1;
}

static UBOOL UT99AndroidCallNoArgFuncV169I( UObject* Window, const TCHAR* FunctionName )
{
    if( !Window || !FunctionName )
        return 0;
    UFunction* Func = Window->FindFunction( FName(FunctionName, FNAME_Find) );
    if( !Func )
        return 0;
    Window->ProcessEvent( Func, NULL );
    return 1;
}

static void UT99AndroidTouchMouseMoveTargetV169I( UObject* Window, FLOAT LocalX, FLOAT LocalY );

static UBOOL UT99AndroidSetObjectPropertyV169J( UObject* Obj, const TCHAR* Name, UObject* Value )
{
    if( !Obj || !Obj->GetClass() || !Name )
        return 0;
    for( TFieldIterator<UProperty> It( Obj->GetClass() ); It; ++It )
    {
        UProperty* Prop = *It;
        if( Prop && appStricmp( Prop->GetName(), Name ) == 0 && Prop->IsA( UObjectProperty::StaticClass() ) )
        {
            *(UObject**)((BYTE*)Obj + Prop->Offset) = Value;
            return 1;
        }
    }
    return 0;
}

static UBOOL UT99AndroidGetFloatPropertyV169J( UObject* Obj, const TCHAR* Name, FLOAT* OutValue )
{
    UFloatProperty* FloatProp = UT99AndroidFindFloatPropertyV91( Obj, Name );
    if( !FloatProp )
        return 0;
    if( OutValue )
        *OutValue = *(FLOAT*)((BYTE*)Obj + FloatProp->Offset);
    return 1;
}

static UBOOL UT99AndroidCallObjectFuncV169J( UObject* Window, const TCHAR* FunctionName, UObject* Arg )
{
    if( !Window || !FunctionName )
        return 0;
    UFunction* Func = Window->FindFunction( FName(FunctionName, FNAME_Find) );
    if( !Func )
        return 0;

    struct FObjectFuncParms
    {
        UObject* I;
    } Parms;
    appMemzero( &Parms, sizeof(Parms) );
    Parms.I = Arg;
    Window->ProcessEvent( Func, &Parms );
    return 1;
}

static UObject* UT99AndroidFindMenuBarItemAtV169J( UObject* MenuBar, FLOAT LocalX )
{
    UObject* Items = UT99AndroidFindObjectPropertyV169E( MenuBar, TEXT("Items") );
    if( !Items )
        return NULL;

    UObject* Item = UT99AndroidFindObjectPropertyV169E( Items, TEXT("Next") );
    INT Guard = 0;
    while( Item && Item != Items && Guard++ < 96 )
    {
        FLOAT ItemLeft = 0.0f;
        FLOAT ItemWidth = 0.0f;
        UT99AndroidGetFloatPropertyV169J( Item, TEXT("ItemLeft"), &ItemLeft );
        UT99AndroidGetFloatPropertyV169J( Item, TEXT("ItemWidth"), &ItemWidth );

        if( ItemWidth > 0.0f && LocalX >= ItemLeft && LocalX <= ItemLeft + ItemWidth )
            return Item;

        Item = UT99AndroidFindObjectPropertyV169E( Item, TEXT("Next") );
    }
    return NULL;
}

static UBOOL UT99AndroidDirectMenuBarSelectV169J( UObject* MenuBar, FLOAT LocalX, FLOAT LocalY, INT Message )
{
    // UT99_ANDROID_V169J_TOP_MENU_DIRECT_SELECT:
    // v169i proved the top menu bar is hit-tested correctly, but plain
    // LMouseDown/LMouseUp still depends on the old UWindow hover/selected
    // state.  Select the UWindowMenuBarItem directly from ItemLeft/ItemWidth
    // and run the same Select() / item.Select() actions UWindow uses.
    if( !MenuBar || !UT99AndroidClassHasNameV169H( MenuBar, TEXT("MenuBar") ) )
        return 0;

    UObject* Item = UT99AndroidFindMenuBarItemAtV169J( MenuBar, LocalX );
    if( !Item )
    {
        // Fall back to the old behavior for edge cases such as the fullscreen
        // corner.  This keeps PC/HID-style behavior intact without guessing.
        UT99AndroidTouchMouseMoveTargetV169I( MenuBar, LocalX, LocalY );
        return UT99AndroidCallMouseFuncV169H( MenuBar, Message == 0 ? TEXT("LMouseDown") : TEXT("LMouseUp"), LocalX, LocalY );
    }

    if( Message == 0 )
    {
        UObject* OldSelected = UT99AndroidFindObjectPropertyV169E( MenuBar, TEXT("Selected") );
        if( OldSelected && OldSelected != Item )
            UT99AndroidCallNoArgFuncV169I( OldSelected, TEXT("DeSelect") );

        UT99AndroidSetObjectPropertyV169J( MenuBar, TEXT("Selected"), Item );
        UT99AndroidSetObjectPropertyV169J( MenuBar, TEXT("Over"), Item );
        UT99AndroidCallNoArgFuncV169I( Item, TEXT("Select") );
        UT99AndroidCallObjectFuncV169J( MenuBar, TEXT("Select"), Item );

        UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V169J_TOP_MENU_DIRECT_SELECT item=%s x=%.1f y=%.1f", Item->GetClass() ? Item->GetClass()->GetName() : "?", LocalX, LocalY );
    }

    // Selection/opening is done on down.  Release is intentionally swallowed so
    // the same tap cannot immediately close or re-route to the previous hover.
    return 1;
}

static void UT99AndroidTouchMouseMoveTargetV169I( UObject* Window, FLOAT LocalX, FLOAT LocalY )
{
    // UT99_ANDROID_V169J_TOP_MENU_DIRECT_SELECT:
    // Some UWindow classes (PulldownMenu/MenuBar) select their logical item in
    // MouseMove(), not in LMouseDown().  Always refresh the local target before
    // sending a click/action so the finger position, not the previous mouse
    // position, decides the target.
    UT99AndroidCallMouseFuncV169H( Window, TEXT("MouseMove"), LocalX, LocalY );
}

static UBOOL UT99AndroidIsComboStepButtonV169I( UObject* Window )
{
    if( !Window )
        return 0;
    if( UT99AndroidClassHasNameV169H( Window, TEXT("ComboLeftButton") ) )
        return 1;
    if( UT99AndroidClassHasNameV169H( Window, TEXT("ComboRightButton") ) )
        return 1;
    return 0;
}

static UBOOL UT99AndroidIsCheckboxTargetV169L( UObject* Window )
{
    // UT99_ANDROID_V169L_CHECKBOX_TOUCH_ROOT_PATH:
    // UWindowCheckbox must stay on the root UWindow event path.  Some builds
    // expose it through a Button-like class chain, but forcing the v169i direct
    // button Click() path prevents the checked state from toggling.
    return UT99AndroidClassHasNameV169H( Window, TEXT("Checkbox") );
}

static UBOOL UT99AndroidIsButtonActionV169I( UObject* Window )
{
    if( !Window )
        return 0;
    if( UT99AndroidIsCheckboxTargetV169L( Window ) )
        return 0;
    if( UT99AndroidIsComboStepButtonV169I( Window ) )
        return 0;
    if( UT99AndroidClassHasNameV169H( Window, TEXT("Button") ) )
        return 1;
    return 0;
}

static UBOOL UT99AndroidIsMenuTargetV169I( UObject* Window )
{
    if( !Window )
        return 0;
    if( UT99AndroidClassHasNameV169H( Window, TEXT("MenuBar") ) )
        return 1;
    if( UT99AndroidClassHasNameV169H( Window, TEXT("PulldownMenu") ) )
        return 1;
    if( UT99AndroidClassHasNameV169H( Window, TEXT("GameMenu") ) )
        return 1;
    if( UT99AndroidClassHasNameV169H( Window, TEXT("OptionsMenu") ) )
        return 1;
    if( UT99AndroidClassHasNameV169H( Window, TEXT("MultiplayerMenu") ) )
        return 1;
    if( UT99AndroidClassHasNameV169H( Window, TEXT("ToolsMenu") ) )
        return 1;
    if( UT99AndroidClassHasNameV169H( Window, TEXT("ModMenu") ) )
        return 1;
    return 0;
}

static UBOOL UT99AndroidIsEditTargetV169I( UObject* Window )
{
    return UT99AndroidClassHasNameV169H( Window, TEXT("EditBox") );
}

static UBOOL UT99AndroidNeedsDirectWindowClickV169H( UObject* Window )
{
    // UT99_ANDROID_V169J_TOP_MENU_DIRECT_SELECT:
    // Keep v169h/root dispatch for the classes that already worked cleanly
    // (tabs, sliders, checkboxes, combo controls/lists).  Only classes that
    // failed with plain WindowEvent/LMouseUp are handled directly here.
    if( !Window )
        return 0;
    if( UT99AndroidIsCheckboxTargetV169L( Window ) )
        return 0;
    if( UT99AndroidIsComboStepButtonV169I( Window ) )
        return 1;
    if( UT99AndroidIsButtonActionV169I( Window ) )
        return 1;
    if( UT99AndroidIsMenuTargetV169I( Window ) )
        return 1;
    if( UT99AndroidIsEditTargetV169I( Window ) )
        return 1;
    return 0;
}

static UBOOL UT99AndroidDirectWindowClickV169H( UObject* Window, INT Message )
{
    if( !Window || !UT99AndroidNeedsDirectWindowClickV169H( Window ) )
        return 0;

    FLOAT LocalX = 0.0f;
    FLOAT LocalY = 0.0f;
    UT99AndroidCallGetMouseXYV169H( Window, &LocalX, &LocalY );
    UT99AndroidTouchMouseMoveTargetV169I( Window, LocalX, LocalY );

    if( Message == 0 )
    {
        GUT99AndroidTouchDownWindowV169I = Window;
        GUT99AndroidTouchDownLocalXV169I = LocalX;
        GUT99AndroidTouchDownLocalYV169I = LocalY;
        GUT99AndroidTouchDownWasDirectV169I = 1;

        if( UT99AndroidIsEditTargetV169I( Window ) )
        {
            UT99AndroidCallMouseFuncV169H( Window, TEXT("LMouseDown"), LocalX, LocalY );
            UT99AndroidCallNoArgFuncV169I( Window, TEXT("FocusWindow") );
            return 1;
        }

        // Combo arrow buttons intentionally do their one-step action on
        // LMouseDown().  Do not synthesize Click() for them on release, or the
        // map selector starts skipping entries again.
        if( UT99AndroidIsComboStepButtonV169I( Window ) )
            return UT99AndroidCallMouseFuncV169H( Window, TEXT("LMouseDown"), LocalX, LocalY );

        // v169j: top menu bar needs direct item selection; pulldown menus keep
        // their normal MouseMove + LMouseDown/LMouseUp selection path.
        if( UT99AndroidClassHasNameV169H( Window, TEXT("MenuBar") ) )
            return UT99AndroidDirectMenuBarSelectV169J( Window, LocalX, LocalY, Message );
        if( UT99AndroidIsMenuTargetV169I( Window ) )
            return UT99AndroidCallMouseFuncV169H( Window, TEXT("LMouseDown"), LocalX, LocalY );

        if( UT99AndroidIsButtonActionV169I( Window ) )
            return UT99AndroidCallMouseFuncV169H( Window, TEXT("LMouseDown"), LocalX, LocalY );

        return 0;
    }

    if( Message == 1 )
    {
        UObject* UpTarget = GUT99AndroidTouchDownWasDirectV169I && GUT99AndroidTouchDownWindowV169I
            ? GUT99AndroidTouchDownWindowV169I
            : Window;
        FLOAT UpLocalX = LocalX;
        FLOAT UpLocalY = LocalY;
        if( UpTarget != Window )
            UT99AndroidCallGetMouseXYV169H( UpTarget, &UpLocalX, &UpLocalY );
        UT99AndroidTouchMouseMoveTargetV169I( UpTarget, UpLocalX, UpLocalY );

        UBOOL Handled = 0;
        if( UT99AndroidIsComboStepButtonV169I( UpTarget ) )
        {
            // Release only resets the visual bMouseDown state; the step already
            // happened on down.
            UT99AndroidCallMouseFuncV169H( UpTarget, TEXT("LMouseUp"), UpLocalX, UpLocalY );
            Handled = 1;
        }
        else if( UT99AndroidIsButtonActionV169I( UpTarget ) )
        {
            // v169h proved LMouseDown/LMouseUp reaches UWindowSmallButton, but
            // several button classes still did not fire their action.  Call the
            // concrete Click() directly on the same object pressed by the finger.
            Handled = UT99AndroidCallMouseFuncV169H( UpTarget, TEXT("Click"), UpLocalX, UpLocalY );
            UT99AndroidSetBoolPropertyV169I( UpTarget, TEXT("bMouseDown"), 0 );
        }
        else if( UT99AndroidIsEditTargetV169I( UpTarget ) )
        {
            UT99AndroidCallMouseFuncV169H( UpTarget, TEXT("LMouseUp"), UpLocalX, UpLocalY );
            UT99AndroidCallNoArgFuncV169I( UpTarget, TEXT("FocusWindow") );
            Handled = 1;
        }
        else if( UT99AndroidClassHasNameV169H( UpTarget, TEXT("MenuBar") ) )
        {
            Handled = UT99AndroidDirectMenuBarSelectV169J( UpTarget, UpLocalX, UpLocalY, Message );
            UT99AndroidSetBoolPropertyV169I( UpTarget, TEXT("bMouseDown"), 0 );
        }
        else if( UT99AndroidIsMenuTargetV169I( UpTarget ) )
        {
            Handled = UT99AndroidCallMouseFuncV169H( UpTarget, TEXT("LMouseUp"), UpLocalX, UpLocalY );
            UT99AndroidSetBoolPropertyV169I( UpTarget, TEXT("bMouseDown"), 0 );
        }

        GUT99AndroidTouchDownWindowV169I = NULL;
        GUT99AndroidTouchDownWasDirectV169I = 0;
        return Handled;
    }

    return 0;
}

static UBOOL UT99AndroidWriteScriptParamV170A( UFunction* Function, BYTE* Buffer, INT BufferSize, const TCHAR* ParamName, const void* Value, INT ValueSize )
{
    // UT99_ANDROID_V170A_ARM64_UWINDOW_PARAM_LAYOUT:
    // Never describe a script-call parameter block with a local C++ struct.
    // On LP64 a BYTE followed by UObject* gets compiler padding to offset 8,
    // while UnrealScript's pack(4) layout puts the pointer at offset 4.  Use
    // the linked UProperty offsets from the loaded UFunction instead, so this
    // remains correct on both armeabi-v7a and arm64-v8a.
    if( !Function || !Buffer || BufferSize <= 0 || !ParamName || !Value || ValueSize <= 0 )
        return 0;

    for( UField* Field=Function->Children; Field; Field=Field->Next )
    {
        UProperty* Property = Cast<UProperty>( Field );
        if( !Property || !(Property->PropertyFlags & CPF_Parm) )
            continue;
        if( appStricmp( Property->GetName(), ParamName ) != 0 )
            continue;
        if( Property->Offset < 0 || Property->Offset + ValueSize > BufferSize || ValueSize > Property->ElementSize )
            return 0;

        // appMemcpy avoids undefined/unaligned native pointer stores at the
        // valid pack(4) offset used by the script parameter frame on ARM64.
        appMemcpy( Buffer + Property->Offset, Value, ValueSize );
        return 1;
    }
    return 0;
}

static UBOOL UT99AndroidUWindowWindowEventV169E( UNSDLViewport* Viewport, INT Message, INT MouseX, INT MouseY, INT Key )
{
    UObject* Root = UT99AndroidFindUWindowRootV169E();
    FLOAT UIX = 0.0f;
    FLOAT UIY = 0.0f;
    if( !Viewport || !Root || !UT99AndroidUWindowMoveMouseV169E( Viewport, MouseX, MouseY, &UIX, &UIY ) )
        return 0;

    UObject* Target = UT99AndroidFindWindowUnderV169H( Root, UIX, UIY );
    if( Message == 0 )
    {
        // Save the exact finger-down target for the post-click IME decision.
        // A later non-edit touch clears this immediately, preventing stale focus
        // from reopening the keyboard on checkboxes or other menu controls.
        GUT99V170BLastTouchWasEditTarget = Target && UT99AndroidIsEditTargetV169I( Target );
    }
    if( Target && UT99AndroidDirectWindowClickV169H( Target, Message ) )
    {
        UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V169J_TOP_MENU_DIRECT_SELECT direct target=%s msg=%d x=%d y=%d", Target->GetClass() ? Target->GetClass()->GetName() : "?", Message, MouseX, MouseY );
        return 1;
    }

    UFunction* WindowEvent = Root->FindFunction( FName(TEXT("WindowEvent"), FNAME_Find) );
    if( WindowEvent && WindowEvent->ParmsSize > 0 )
    {
        TArray<BYTE> Parms;
        Parms.AddZeroed( WindowEvent->ParmsSize );
        BYTE* ParmsData = &Parms(0);
        const BYTE Msg = (BYTE)Message;
        UObject* C = NULL;

        const UBOOL bLayoutOK
            = UT99AndroidWriteScriptParamV170A( WindowEvent, ParmsData, Parms.Num(), TEXT("Msg"), &Msg, sizeof(Msg) )
           && UT99AndroidWriteScriptParamV170A( WindowEvent, ParmsData, Parms.Num(), TEXT("C"),   &C,   sizeof(C) )
           && UT99AndroidWriteScriptParamV170A( WindowEvent, ParmsData, Parms.Num(), TEXT("X"),   &UIX, sizeof(UIX) )
           && UT99AndroidWriteScriptParamV170A( WindowEvent, ParmsData, Parms.Num(), TEXT("Y"),   &UIY, sizeof(UIY) )
           && UT99AndroidWriteScriptParamV170A( WindowEvent, ParmsData, Parms.Num(), TEXT("Key"), &Key, sizeof(Key) );

        if( !bLayoutOK )
        {
            UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V170A_ARM64_UWINDOW_PARAM_LAYOUT failed ParmsSize=%d", (INT)WindowEvent->ParmsSize );
            return 0;
        }

        Root->ProcessEvent( WindowEvent, ParmsData );
        UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V169J_TOP_MENU_DIRECT_SELECT root msg=%d target=%s x=%d y=%d", Message, Target && Target->GetClass() ? Target->GetClass()->GetName() : "?", MouseX, MouseY );
        return 1;
    }
    return 0;
}

static void UT99AndroidMenuDirectTouchButtonV169D( UNSDLViewport* Viewport, UEngine* Engine, INT MouseX, INT MouseY, UBOOL bDown )
{
    // UT99_ANDROID_V169E_DIRECT_UWINDOW_TOUCH:
    // Compatibility wrapper name kept to minimize churn.  It no longer sends a
    // LeftMouse KeyEvent through WindowConsole; it calls UWindowRootWindow
    // directly so a finger tap is resolved from the finger position, not from the
    // last mouse/hover position.
    if( UT99AndroidUWindowWindowEventV169E( Viewport, bDown ? 0 : 1, MouseX, MouseY, IK_LeftMouse ) )
        return;

    if( !Viewport )
        return;
    UT99AndroidMenuTouchRetargetV169B( Viewport, MouseX, MouseY );
    if( Engine )
        Engine->MousePosition( Viewport, bDown ? MOUSE_Left : 0, MouseX, MouseY );
    Viewport->CauseInputEvent( IK_LeftMouse, bDown ? IST_Press : IST_Release );
}

static void UT99AndroidMenuDirectTouchMotionV169D( UNSDLViewport* Viewport, UEngine* Engine, INT MouseX, INT MouseY, INT DX, INT DY )
{
    // Drag is also delivered to UWindow directly.  Root.MoveMouse() triggers the
    // captured control's MouseMove(), which is what sliders need.
    FLOAT UIX = 0.0f;
    FLOAT UIY = 0.0f;
    if( UT99AndroidUWindowMoveMouseV169E( Viewport, MouseX, MouseY, &UIX, &UIY ) )
        return;

    if( !Viewport )
        return;
    UT99AndroidMenuTouchRetargetV169B( Viewport, MouseX, MouseY );
    if( Engine )
        Engine->MousePosition( Viewport, MOUSE_Left, MouseX, MouseY );
}

static UBOOL UT99AndroidIsTopMenuBarYV164( INT MouseY )
{
    // UWindowMenuBar paints at y=0..16.  Give touch a small safety margin
    // for Android scaling/rounding, but keep it away from pulldown items.
    return MouseY >= 0 && MouseY <= 24;
}
static INT UT99AndroidClampIntV32( INT Value, INT MinValue, INT MaxValue )
{
    if( Value < MinValue ) return MinValue;
    if( Value > MaxValue ) return MaxValue;
    return Value;
}
static void UT99AndroidTouchToViewportCoordsV32( UNSDLViewport* Viewport, const SDL_TouchFingerEvent& Finger, INT& OutX, INT& OutY )
{
    INT SizeX = Viewport && Viewport->SizeX > 0 ? Viewport->SizeX : 1;
    INT SizeY = Viewport && Viewport->SizeY > 0 ? Viewport->SizeY : 1;
    FLOAT FX = Finger.x;
    FLOAT FY = Finger.y;
    if( FX < 0.0f ) FX = 0.0f;
    if( FX > 1.0f ) FX = 1.0f;
    if( FY < 0.0f ) FY = 0.0f;
    if( FY > 1.0f ) FY = 1.0f;
    OutX = UT99AndroidClampIntV32( (INT)( FX * (FLOAT)SizeX + 0.5f ), 0, SizeX - 1 );
    OutY = UT99AndroidClampIntV32( (INT)( FY * (FLOAT)SizeY + 0.5f ), 0, SizeY - 1 );

#ifdef PLATFORM_ANDROID
    // UT99_ANDROID_V74_TOUCH_NATIVE_SCALE_FIX:
    // SDLSurface.java now normalizes touch against the fullscreen View/display
    // size instead of the smaller 75%/50% SurfaceHolder buffer.  Therefore the
    // normalized finger coordinate already represents the visible fullscreen
    // position.  Do not apply the v73 extra percentage scaling here, otherwise
    // touches drift toward the top-left and taps on the right/bottom side can
    // collapse to the center at 50%.
#endif
}
#endif
#endif

#ifdef PLATFORM_ANDROID
#define UT99_ANDROID_VIEWPORT_SIZE_V29 1
#ifndef UT99_ANDROID_SDL_LOGI
#include <android/log.h>
#define UT99_ANDROID_SDL_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "UT99SDL", __VA_ARGS__)
#endif
static void UT99AndroidAdoptDrawableSizeV29( SDL_Window* Window, INT& X, INT& Y, const char* Where )
{
    if( !Window )
        return;
    int DW = 0;
    int DH = 0;
    SDL_GL_GetDrawableSize( Window, &DW, &DH );
    if( DW <= 0 || DH <= 0 )
        SDL_GetWindowSize( Window, &DW, &DH );
    if( DW <= 0 || DH <= 0 )
        return;
    const UBOOL LooksLikeLegacyTinyMode = ( X <= 640 && Y <= 480 );
    const UBOOL LooksInvalid = ( X <= 0 || Y <= 0 || X == INDEX_NONE || Y == INDEX_NONE );
    if( LooksLikeLegacyTinyMode || LooksInvalid || X != DW || Y != DH )
    {
        UT99_ANDROID_SDL_LOGI("v29 adopt drawable in %s: requested=%dx%d drawable=%dx%d", Where ? Where : "?", X, Y, DW, DH);
        X = DW;
        Y = DH;
    }
}
#endif

#ifdef PLATFORM_ANDROID
#define UT99_ANDROID_INPUT_V28_HELPERS 1
#ifndef UT99_ANDROID_SDL_LOGI
#include <android/log.h>
#define UT99_ANDROID_SDL_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "UT99SDL", __VA_ARGS__)
#endif
static INT UT99AndroidControllerButtonToUiKeyV28( Uint8 Button )
{
    switch( Button )
    {
        case SDL_CONTROLLER_BUTTON_A: return IK_Enter;
        case SDL_CONTROLLER_BUTTON_B: return IK_None; // UT99_ANDROID_CONTROLLER_FULL_REMAP_V120: B is bindable, not menu back
        case SDL_CONTROLLER_BUTTON_BACK: return IK_None;
        case SDL_CONTROLLER_BUTTON_START: return IK_None;
        case SDL_CONTROLLER_BUTTON_DPAD_UP: return IK_Up;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return IK_Down;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return IK_Left;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return IK_Right;
        default: return IK_None;
    }
}
#endif

#ifdef PLATFORM_ANDROID
#define UT99_ANDROID_VIEWPORT_HELPERS_V27 1
#include <android/log.h>

#ifdef PLATFORM_ANDROID
#ifndef UT99_ANDROID_V47B_ANDROID_HEADERS
#define UT99_ANDROID_V47B_ANDROID_HEADERS 1
#include <jni.h>
#include <android/log.h>
#ifndef UT99_ANDROID_SDL_LOGI
#define UT99_ANDROID_SDL_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "UT99SDL", __VA_ARGS__)
#endif
#endif
#endif

#define UT99_ANDROID_SDL_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "UT99SDL", __VA_ARGS__)
static SDL_Joystick* GUT99AndroidJoysticks[8] = { NULL };
static void UT99AndroidOpenJoystickFallbacks()
{
    SDL_GameControllerEventState( SDL_ENABLE );
    SDL_JoystickEventState( SDL_ENABLE );
    int Slot = 0;
    const int Count = SDL_NumJoysticks();
    for( int i = 0; i < Count && Slot < 8; ++i )
    {
        // Game controllers are owned by GUT99AndroidControllers below.  The old
        // code opened them here as well and leaked an extra SDL_GameController
        // reference, which makes runtime remove/re-add handling unreliable.
        if( SDL_IsGameController( i ) )
            continue;

        SDL_Joystick* J = SDL_JoystickOpen( i );
        if( J )
        {
            GUT99AndroidJoysticks[Slot++] = J;
            UT99_ANDROID_SDL_LOGI("opened joystick %d: %s", i, SDL_JoystickName(J));
        }
    }
}
static void UT99AndroidAdoptWindowSize( SDL_Window* Window, INT& X, INT& Y )
{
    if( !Window ) return;
    int W = 0;
    int H = 0;
    SDL_GL_GetDrawableSize( Window, &W, &H );
    if( W <= 0 || H <= 0 ) SDL_GetWindowSize( Window, &W, &H );
    if( W > 0 && H > 0 )
    {
        X = W;
        Y = H;
    }
}
#endif

#ifdef PLATFORM_ANDROID
#define UT99_ANDROID_CONTROLLER_BRIDGE_V26B 1
static SDL_GameController* GUT99AndroidControllers[8] = { NULL };

// UT99_ANDROID_RETROTOUCH_NATIVE_CONTROLLER_HOTPLUG_V2:
// Android handhelds can disable and recreate their integrated gamepad while the
// Activity and SDL window remain alive.  Java/SDL then emits a new controller
// device, potentially with a different Android device id / SDL instance id.
// UT99 used to open controllers only once from OpenWindow(), so the new device
// never acquired a live SDL_GameController handle.
static SDL_JoystickID UT99AndroidControllerInstanceId( SDL_GameController* Controller )
{
    if( !Controller )
        return (SDL_JoystickID)-1;
    SDL_Joystick* Joystick = SDL_GameControllerGetJoystick( Controller );
    return Joystick ? SDL_JoystickInstanceID( Joystick ) : (SDL_JoystickID)-1;
}

static INT UT99AndroidFindControllerSlotByInstance( SDL_JoystickID InstanceId )
{
    for( INT i = 0; i < 8; ++i )
    {
        if( GUT99AndroidControllers[i] && UT99AndroidControllerInstanceId( GUT99AndroidControllers[i] ) == InstanceId )
            return i;
    }
    return INDEX_NONE;
}

static INT UT99AndroidFindFreeControllerSlot()
{
    for( INT i = 0; i < 8; ++i )
    {
        if( !GUT99AndroidControllers[i] )
            return i;
        if( SDL_GameControllerGetAttached( GUT99AndroidControllers[i] ) == SDL_FALSE )
        {
            SDL_GameControllerClose( GUT99AndroidControllers[i] );
            GUT99AndroidControllers[i] = NULL;
            return i;
        }
    }
    return INDEX_NONE;
}

static void UT99AndroidOpenControllerIndex( INT DeviceIndex, const char* Reason )
{
    if( DeviceIndex < 0 || DeviceIndex >= SDL_NumJoysticks() || !SDL_IsGameController( DeviceIndex ) )
    {
        UT99_ANDROID_SDL_LOGI( "controller hotplug open ignored index=%d reason=%s count=%d isgc=%d",
            DeviceIndex, Reason ? Reason : "?", SDL_NumJoysticks(),
            (DeviceIndex >= 0 && DeviceIndex < SDL_NumJoysticks()) ? (INT)SDL_IsGameController( DeviceIndex ) : 0 );
        return;
    }

    const SDL_JoystickID InstanceId = SDL_JoystickGetDeviceInstanceID( DeviceIndex );
    const INT ExistingSlot = UT99AndroidFindControllerSlotByInstance( InstanceId );
    if( ExistingSlot != INDEX_NONE )
    {
        // SDL may deliver duplicate add notifications while Java settles device
        // sources.  Never increase the controller refcount for the same instance.
        if( SDL_GameControllerGetAttached( GUT99AndroidControllers[ExistingSlot] ) == SDL_TRUE )
            return;
        SDL_GameControllerClose( GUT99AndroidControllers[ExistingSlot] );
        GUT99AndroidControllers[ExistingSlot] = NULL;
    }

    INT Slot = (ExistingSlot != INDEX_NONE) ? ExistingSlot : UT99AndroidFindFreeControllerSlot();
    if( Slot == INDEX_NONE )
    {
        UT99_ANDROID_SDL_LOGI( "controller hotplug open failed: no free slot index=%d instance=%d reason=%s",
            DeviceIndex, (INT)InstanceId, Reason ? Reason : "?" );
        return;
    }

    SDL_GameController* Controller = SDL_GameControllerOpen( DeviceIndex );
    if( !Controller )
    {
        UT99_ANDROID_SDL_LOGI( "controller hotplug SDL_GameControllerOpen failed index=%d instance=%d reason=%s err=%s",
            DeviceIndex, (INT)InstanceId, Reason ? Reason : "?", SDL_GetError() );
        return;
    }

    GUT99AndroidControllers[Slot] = Controller;
    const char* ControllerName = SDL_GameControllerName( Controller );
    if( ControllerName && strstr( ControllerName, "OUYA" ) )
    {
        GUT99V79OuyaLikeDevice = 1;
        UT99_ANDROID_SDL_LOGI( "v79 OUYA controller profile active: %s", ControllerName );
    }
    UT99_ANDROID_SDL_LOGI( "opened gamecontroller index=%d slot=%d instance=%d reason=%s name=%s",
        DeviceIndex, Slot, (INT)UT99AndroidControllerInstanceId( Controller ),
        Reason ? Reason : "?", ControllerName ? ControllerName : "<unknown>" );
}

static void UT99AndroidCloseControllerInstance( SDL_JoystickID InstanceId, const char* Reason )
{
    const INT Slot = UT99AndroidFindControllerSlotByInstance( InstanceId );
    if( Slot == INDEX_NONE )
    {
        UT99_ANDROID_SDL_LOGI( "controller hotplug remove instance=%d reason=%s handle=not-open",
            (INT)InstanceId, Reason ? Reason : "?" );
        return;
    }

    const char* Name = SDL_GameControllerName( GUT99AndroidControllers[Slot] );
    UT99_ANDROID_SDL_LOGI( "closed gamecontroller slot=%d instance=%d reason=%s name=%s",
        Slot, (INT)InstanceId, Reason ? Reason : "?", Name ? Name : "<unknown>" );
    SDL_GameControllerClose( GUT99AndroidControllers[Slot] );
    GUT99AndroidControllers[Slot] = NULL;
}

static void UT99AndroidOpenControllers( const char* Reason )
{
    SDL_GameControllerEventState( SDL_ENABLE );
    SDL_JoystickEventState( SDL_ENABLE );
    const int Count = SDL_NumJoysticks();
    for( int i = 0; i < Count; ++i )
    {
        if( SDL_IsGameController( i ) )
            UT99AndroidOpenControllerIndex( i, Reason );
    }
}

static void UT99AndroidStopTextInput()
{
#if defined(__ANDROID__)
    GUT99AndroidImeOpenV79 = 0;
    UT99AndroidSetJavaImeWantedV76( 0 );
#endif
    SDL_SetHint( SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1" );
    SDL_StopTextInput();
}
#endif

/*-----------------------------------------------------------------------------
	UNSDLViewport implementation.
-----------------------------------------------------------------------------*/

//
// SDL_BUTTON_ -> EInputKey translation map.
//
const BYTE UNSDLViewport::MouseButtonMap[6] =
{
	/* invalid           */ IK_None,
	/* SDL_BUTTON_LEFT   */ IK_LeftMouse,
	/* SDL_BUTTON_MIDDLE */ IK_MiddleMouse,
	/* SDL_BUTTON_RIGHT  */ IK_RightMouse,
	/* SDL_BUTTON_X1     */ IK_None,
	/* SDL_BUTTON_X2     */ IK_None
};

//
// SDL_CONTROLLER_BUTTON_ -> EInputKey translation map.
//
const BYTE UNSDLViewport::JoyButtonMap[SDL_CONTROLLER_BUTTON_MAX] =
{
	/* BUTTON_A             */ IK_Joy1,
	/* BUTTON_B             */ IK_Joy2,
	/* BUTTON_X             */ IK_Joy3,
	/* BUTTON_Y             */ IK_Joy4,
	/* BUTTON_BACK          */ IK_Joy5,
	/* BUTTON_GUIDE         */ IK_Joy6,
	/* BUTTON_START         */ IK_Joy7,
	/* BUTTON_LEFTSTICK     */ IK_Joy8,
	/* BUTTON_RIGHTSTICK    */ IK_Joy9,
	/* BUTTON_LEFTSHOULDER  */ IK_Joy10,
	/* BUTTON_RIGHTSHOULDER */ IK_Joy11,
	/* BUTTON_DPAD_UP       */ IK_JoyPovUp,
	/* BUTTON_DPAD_DOWN     */ IK_JoyPovDown,
	/* BUTTON_DPAD_LEFT     */ IK_JoyPovLeft,
	/* BUTTON_DPAD_RIGHT    */ IK_JoyPovRight,
};

//
// SDL_CONTROLLER_BUTTON_ -> EInputKey translation map for UI controls.
//
const BYTE UNSDLViewport::JoyButtonMapUI[SDL_CONTROLLER_BUTTON_MAX] =
{
	/* BUTTON_A             */ IK_Enter,
	/* BUTTON_B             */ IK_Joy2, /* UT99_ANDROID_CONTROLLER_FULL_REMAP_V120 */
	/* BUTTON_X             */ IK_N,
	/* BUTTON_Y             */ IK_Y,
	/* BUTTON_BACK          */ IK_Escape,
	/* BUTTON_GUIDE         */ IK_Escape,
	/* BUTTON_START         */ IK_Escape,
	/* BUTTON_LEFTSTICK     */ IK_Joy8,
	/* BUTTON_RIGHTSTICK    */ IK_Joy9,
	/* BUTTON_LEFTSHOULDER  */ IK_Joy10,
	/* BUTTON_RIGHTSHOULDER */ IK_Joy11,
	/* BUTTON_DPAD_UP       */ IK_Up,
	/* BUTTON_DPAD_DOWN     */ IK_Down,
	/* BUTTON_DPAD_LEFT     */ IK_Left,
	/* BUTTON_DPAD_RIGHT    */ IK_Right,
};

//
// SDL_CONTROLLER_BUTTON_ -> EInputKey translation map.
//
const BYTE UNSDLViewport::JoyAxisMap[SDL_CONTROLLER_AXIS_MAX] =
{
	/* AXIS_LEFT_X          */ IK_JoyX,
	/* AXIS_LEFT_Y          */ IK_JoyY,
	/* AXIS_RIGHT_X         */ IK_JoyU,
	/* AXIS_RIGHT_Y         */ IK_JoyV,
	/* AXIS_LTRIGGER        */ IK_Joy12,
	/* AXIS_RTRIGGER        */ IK_Joy13,
};

//
// Additional scale to apply per SDL axis.
//
const FLOAT UNSDLViewport::JoyAxisDefaultScale[SDL_CONTROLLER_AXIS_MAX] =
{
	/* AXIS_LEFT_X          */ +60.f,
	/* AXIS_LEFT_Y          */ -60.f,
	/* AXIS_RIGHT_X         */ +60.f,
	/* AXIS_RIGHT_Y         */ +60.f,
	/* AXIS_LTRIGGER        */ +60.f,
	/* AXIS_RTRIGGER        */ +60.f,
};

//
// SDL_Scancode -> EInputKey translation map.
//
BYTE UNSDLViewport::KeyMap[512];
void UNSDLViewport::InitKeyMap()
{
	#define INIT_KEY_RANGE( AStart, AEnd, BStart, BEnd ) \
		for( DWORD Key = AStart; Key <= AEnd; ++Key ) KeyMap[Key] = BStart + ( Key - AStart )

	appMemset( KeyMap, 0, sizeof( KeyMap ) );

	// TODO: IK_LControl, IK_LShift, etc exist, what are they for?
	KeyMap[SDL_SCANCODE_LSHIFT] = IK_Shift;
	KeyMap[SDL_SCANCODE_RSHIFT] = IK_Shift;
	KeyMap[SDL_SCANCODE_LCTRL] = IK_Ctrl;
	KeyMap[SDL_SCANCODE_RCTRL] = IK_Ctrl;
	KeyMap[SDL_SCANCODE_LALT] = IK_Alt;
	KeyMap[SDL_SCANCODE_RALT] = IK_Alt;
	KeyMap[SDL_SCANCODE_GRAVE] = IK_Tilde;
	KeyMap[SDL_SCANCODE_ESCAPE] = IK_Escape;
	KeyMap[SDL_SCANCODE_SPACE] = IK_Space;
	KeyMap[SDL_SCANCODE_RETURN] = IK_Enter;
	KeyMap[SDL_SCANCODE_BACKSPACE] = IK_Backspace;
	KeyMap[SDL_SCANCODE_CAPSLOCK] = IK_CapsLock;
	KeyMap[SDL_SCANCODE_TAB] = IK_Tab;
	KeyMap[SDL_SCANCODE_DELETE] = IK_Delete;
	KeyMap[SDL_SCANCODE_INSERT] = IK_Insert;
	KeyMap[SDL_SCANCODE_HOME] = IK_Home;
	KeyMap[SDL_SCANCODE_END] = IK_End;
	KeyMap[SDL_SCANCODE_PAGEUP] = IK_PageUp;
	KeyMap[SDL_SCANCODE_PAGEDOWN] = IK_PageDown;
	KeyMap[SDL_SCANCODE_PRINTSCREEN] = IK_PrintScrn;
	KeyMap[SDL_SCANCODE_PAUSE] = IK_Pause;
	KeyMap[SDL_SCANCODE_NUMLOCKCLEAR] = IK_NumLock;
	KeyMap[SDL_SCANCODE_SCROLLLOCK] = IK_ScrollLock;
	KeyMap[SDL_SCANCODE_EQUALS] = IK_Equals;
	KeyMap[SDL_SCANCODE_MINUS] = IK_Minus;
	KeyMap[SDL_SCANCODE_SEMICOLON] = IK_Semicolon;
	KeyMap[SDL_SCANCODE_APOSTROPHE] = IK_SingleQuote;
	KeyMap[SDL_SCANCODE_BACKSLASH] = IK_Backslash;
	KeyMap[SDL_SCANCODE_SLASH] = IK_Slash;
	KeyMap[SDL_SCANCODE_LEFTBRACKET] = IK_LeftBracket;
	KeyMap[SDL_SCANCODE_RIGHTBRACKET] = IK_RightBracket;
	KeyMap[SDL_SCANCODE_COMMA] = IK_Comma;
	KeyMap[SDL_SCANCODE_PERIOD] = IK_Period;
	KeyMap[SDL_SCANCODE_LEFT] = IK_Left;
	KeyMap[SDL_SCANCODE_UP] = IK_Up;
	KeyMap[SDL_SCANCODE_RIGHT] = IK_Right;
	KeyMap[SDL_SCANCODE_DOWN] = IK_Down;
	KeyMap[SDL_SCANCODE_0] = IK_0;
	KeyMap[SDL_SCANCODE_KP_0] = IK_NumPad0;
	KeyMap[SDL_SCANCODE_KP_PERIOD] = IK_NumPadPeriod;
	KeyMap[SDL_SCANCODE_KP_ENTER] = IK_Enter;
	KeyMap[SDL_SCANCODE_KP_MULTIPLY] = IK_GreyStar;
	KeyMap[SDL_SCANCODE_KP_PLUS] = IK_GreyPlus;
	KeyMap[SDL_SCANCODE_KP_COMMA] = IK_Separator;
	KeyMap[SDL_SCANCODE_KP_MINUS] = IK_GreyMinus;
	KeyMap[SDL_SCANCODE_KP_DIVIDE] = IK_GreySlash;
	KeyMap[SDL_SCANCODE_KP_EQUALS] = IK_Equals;

	INIT_KEY_RANGE( SDL_SCANCODE_1,    SDL_SCANCODE_9,    IK_1,       IK_9 );
	INIT_KEY_RANGE( SDL_SCANCODE_A,    SDL_SCANCODE_Z,    IK_A,       IK_Z );
	INIT_KEY_RANGE( SDL_SCANCODE_KP_1, SDL_SCANCODE_KP_9, IK_NumPad1, IK_NumPad9 );
	INIT_KEY_RANGE( SDL_SCANCODE_F1,   SDL_SCANCODE_F12,  IK_F1,      IK_F12 );
	INIT_KEY_RANGE( SDL_SCANCODE_F13,  SDL_SCANCODE_F24,  IK_F13,     IK_F24 );

	#undef INIT_KEY_RANGE
}

//
// Static init.
//
void UNSDLViewport::InternalClassInitializer( UClass* Class )
{
	guard(UNSDLViewport::InternalClassInitializer);
	// Fill in keymap.
	InitKeyMap();
	unguard;
}

//
// Constructor.
//
UNSDLViewport::UNSDLViewport( ULevel* InLevel, UNSDLClient* InClient )
:	UViewport()
,	Client( InClient )
{
	guard(UNSDLViewport::UNSDLViewport);

	// Populate the SDL scancode translation table. The newer
	// InternalClassInitializer hook is not invoked by this engine generation.
	InitKeyMap();

	// Set color bytes based on screen resolution.
	SDL_DisplayMode Mode;
	SDL_GetDesktopDisplayMode( InClient->DefaultDisplay, &Mode );
	ColorBytes = SDL_BYTESPERPIXEL( Mode.format );
	Caps = 0;
	if( ColorBytes == 2 && SDL_PIXELLAYOUT( Mode.format ) == SDL_PACKEDLAYOUT_565 )
	{
		Caps |= CC_RGB565;
	}

	// Inherit default display until we have a window.
	DisplayIndex = InClient->DefaultDisplay;
	DisplaySize.w = InClient->GetDefaultDisplayMode().w;
	DisplaySize.h = InClient->GetDefaultDisplayMode().h;

	// Init input.
	if( GIsEditor )
		Input->Init( this );

	Destroyed = false;
	QuitRequested = false;
	HoldCount = 0;

	unguard;
}

// UObject interface.
void UNSDLViewport::Destroy()
{
	guard(UNSDLViewport::Destroy);
	// Note: FullscreenViewport tracking removed - Unreal 1 specific
	UViewport::Destroy();
	unguard;
}

//
// Set the mouse cursor according to Unreal or UnrealEd's mode, or to
// an hourglass if a slow task is active. Not implemented.
//
void UNSDLViewport::SetModeCursor()
{
	guard(UNSDLViewport::SetModeCursor);
	unguard;
}

//
// Update user viewport interface.
//
void UNSDLViewport::UpdateWindowFrame()
{
	guard(UNSDLViewport::UpdateWindowFrame);

	// If not a window, exit.
	if( hWnd==NULL || HoldCount > 0 )
		return;

	// Set viewport window's name to show resolution.
	char WindowName[80];
	if( !GIsEditor || (Actor->ShowFlags&SHOW_PlayerCtrl) )
	{
		appSprintf( WindowName, LocalizeGeneral("Product","Core") );
	}
	else switch( Actor->RendMap )
	{
		case REN_Wire:		strcpy(WindowName,LocalizeGeneral("ViewPersp")); break;
		case REN_OrthXY:	strcpy(WindowName,LocalizeGeneral("ViewXY")); break;
		case REN_OrthXZ:	strcpy(WindowName,LocalizeGeneral("ViewXZ")); break;
		case REN_OrthYZ:	strcpy(WindowName,LocalizeGeneral("ViewYZ")); break;
		default:			strcpy(WindowName,LocalizeGeneral("ViewOther")); break;
	}

	// Set window title.
	if( SizeX && SizeY )
	{
		appSprintf(WindowName+strlen(WindowName)," (%i x %i)",SizeX,SizeY);
		if( this == Client->CurrentViewport() )
			strcat( WindowName, " *" );
	}
	SDL_SetWindowTitle( hWnd, WindowName );

	unguard;
}

//
// Open a viewport window.
//
void UNSDLViewport::OpenWindow( DWORD InParentWindow, UBOOL Temporary, INT NewX, INT NewY, INT OpenX, INT OpenY )
{
	guard(UNSDLViewport::OpenWindow);
	check(Actor);
	check(HoldCount == 0);
	UBOOL DoRepaint=0, DoSetActive=0;
	UBOOL DoOpenGL=0;
	UBOOL NoHard=ParseParam( appCmdLine(), "nohard" );
#if PLATFORM_ANDROID
	SDL_GLprofile GLProfile = SDL_GL_CONTEXT_PROFILE_ES;
#else
	SDL_GLprofile GLProfile = SDL_GL_CONTEXT_PROFILE_ES;
#endif

	// Clamp invalid sizes - SDL textures cannot be 0x0.
	// If no size specified, try to read from config
	if( NewX <= 0 || NewY <= 0 )
	{
		if( !Temporary && !GIsEditor )
		{
			// Try to get windowed viewport size from config
			INT ConfigX = 0, ConfigY = 0;
			if( GConfig->GetInt( TEXT("NSDLDrv.NSDLClient"), TEXT("WindowedViewportX"), ConfigX ) && ConfigX > 0 )
				NewX = ConfigX;
			if( GConfig->GetInt( TEXT("NSDLDrv.NSDLClient"), TEXT("WindowedViewportY"), ConfigY ) && ConfigY > 0 )
				NewY = ConfigY;
		}
		// Final fallback
		if( NewX <= 0 ) NewX = 320;
		if( NewY <= 0 ) NewY = 240;
	}
#if PLATFORM_ANDROID
	if( !Temporary && !GIsEditor )
	{
		SDL_DisplayMode AndroidMode;
		appMemzero( &AndroidMode, sizeof(AndroidMode) );
		if( SDL_GetCurrentDisplayMode( Client->DefaultDisplay, &AndroidMode ) != 0 || AndroidMode.w <= 0 || AndroidMode.h <= 0 )
		{
			appMemzero( &AndroidMode, sizeof(AndroidMode) );
			SDL_GetDesktopDisplayMode( Client->DefaultDisplay, &AndroidMode );
		}
		if( AndroidMode.w > 0 && AndroidMode.h > 0 )
		{
			NewX = AndroidMode.w;
			NewY = AndroidMode.h;
			OpenX = SDL_WINDOWPOS_UNDEFINED;
			OpenY = SDL_WINDOWPOS_UNDEFINED;
			debugf( NAME_Log, TEXT("Android native viewport request: %dx%d"), NewX, NewY );
		}
	}
#endif
	NewX = Align(NewX,4);
	debugf( NAME_Log, TEXT("OpenWindow: NewX=%d, NewY=%d"), NewX, NewY );

	if( !Temporary && !GIsEditor && !NoHard )
	{
		// HACK: Just check if we're about to load OpenGLDrv. Not sure how else you would know to add the GL flag.
		FString Temp;
		Parse( appCmdLine(), TEXT("GAMERENDERDEVICE="), Temp );
		if( Temp.Len() == 0 )
			GConfig->GetString( TEXT("Engine.Engine"), TEXT("GameRenderDevice"), Temp );
		appStrupr( (TCHAR*)*Temp );
		if( Temp.InStr(TEXT("OPENGL")) == INDEX_NONE )
		{
			Parse( appCmdLine(), TEXT("WINDOWEDRENDERDEVICE="), Temp );
			if( Temp.Len() == 0 )
				GConfig->GetString( TEXT("Engine.Engine"), TEXT("WindowedRenderDevice"), Temp );
			appStrupr( (TCHAR*)*Temp );
			if( Temp.InStr(TEXT("OPENGL")) != INDEX_NONE )
				DoOpenGL = 1;
		}
		else
		{
			DoOpenGL = 1;
		}
		if( DoOpenGL && Temp.InStr(TEXT("GLES")) != INDEX_NONE )
			GLProfile = SDL_GL_CONTEXT_PROFILE_ES;
#if PLATFORM_ANDROID
		if( DoOpenGL )
			GLProfile = SDL_GL_CONTEXT_PROFILE_ES;
#endif
	}

	// User window of launcher if no parent window was specified.
	if( !InParentWindow )
	{
		QWORD ParentPtr;
		Parse( appCmdLine(), TEXT("HWND="), ParentPtr );
		InParentWindow = (DWORD)ParentPtr;
	}

	if( Temporary )
	{
		// Create in-memory data.
		ColorBytes = 2;
		ScreenPointer = (BYTE*)appMalloc( 2 * NewX * NewY, "TemporaryViewportData" );	
		hWnd = NULL;
		debugf( NAME_Log, "Opened temporary viewport" );
	}
	else
	{
		// Get flags.
		DWORD Flags = 0;
		if( InParentWindow && (Actor->ShowFlags & SHOW_ChildWindow) )
		{
			Flags = SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS;
		}
		else
		{
			Flags = SDL_WINDOW_HIDDEN;
		}
		if( DoOpenGL )
		{
			Flags |= SDL_WINDOW_OPENGL;
		}
#if PLATFORM_ANDROID
		if( !Temporary && !GIsEditor )
		{
			Flags &= ~SDL_WINDOW_HIDDEN;
			Flags |= SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN;
		}
#endif

		// Set OpenGL attributes if needed.
		if( DoOpenGL )
		{
#if PLATFORM_ANDROID
			// Android provides OpenGL ES through EGL. Request GLES2 explicitly;
			// a desktop compatibility profile makes SDL fail while creating the window.
			GLProfile = SDL_GL_CONTEXT_PROFILE_ES;
			SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES );
			SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 2 );
			SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 0 );
			SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
			SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 16 );
#else
			if( GLProfile == SDL_GL_CONTEXT_PROFILE_ES )
			{
				// Request GLES2.
				SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 2 );
				SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 0 );
			}
			SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, GLProfile );
#endif
		}

		// Set position and size.
		if( OpenX==-1 )
			OpenX = SDL_WINDOWPOS_UNDEFINED;
		if( OpenY==-1 )
			OpenY = SDL_WINDOWPOS_UNDEFINED;

		// If switching renderers, destroy the old window.
		if( hWnd && ( DoOpenGL != !!( SDL_GetWindowFlags( hWnd ) & SDL_WINDOW_OPENGL ) ) )
		{
			CloseWindow();
		}

		// Create or update the window.
		if( !hWnd )
		{
			// Creating new viewport.
			hWnd = SDL_CreateWindow( "", OpenX, OpenY, NewX, NewY, Flags );
			if( !hWnd && DoOpenGL )
			{
				// Try without GL.
				debugf( NAME_Warning, "Could not create OpenGL window: %s. Trying without OpenGL.", SDL_GetError() );
				Flags &= ~SDL_WINDOW_OPENGL;
				DoOpenGL = 0;
				hWnd = SDL_CreateWindow( "", OpenX, OpenY, NewX, NewY, Flags );
			}
			if( !hWnd )
			{
				appErrorf( "Could not create SDL window: %s", SDL_GetError() );
			}

			// Set parent window.
			if( InParentWindow && (Actor->ShowFlags & SHOW_ChildWindow) )
			{
				SDL_SetWindowModalFor( hWnd, (SDL_Window*)InParentWindow );
			}

			debugf( NAME_Log, "Opened viewport" );
			DoSetActive = DoRepaint = 1;
		}
		else
		{
			// Resizing existing viewport.
			SetClientSize( NewX, NewY, false );
		}

		// Create GL context or SDL renderer if needed.
		if( DoOpenGL )
		{
			if( !GLCtx )
			{
				GLCtx = SDL_GL_CreateContext( hWnd );
				if( !GLCtx )
				{
					appErrorf( "Could not create GL context: %s", SDL_GetError() );
				}
			}
			SDL_GL_MakeCurrent( hWnd, GLCtx );
		}
		else
		{
			SDL_SetHint( SDL_HINT_RENDER_SCALE_QUALITY, "nearest" );
			SDLRen = SDL_CreateRenderer( hWnd, -1, 0 );
			if( !SDLRen )
			{
				// Fallback to software.
				debugf( NAME_Warning, "Could not create SDL renderer: %s. Trying software.", SDL_GetError() );
				SDLRen = SDL_CreateRenderer( hWnd, -1, SDL_RENDERER_SOFTWARE );
				if( !SDLRen )
				{
					appErrorf( "Could not create SDL renderer: %s", SDL_GetError() );
				}
			}
			// Create framebuffer texture.
			SDLTexFormat = SDL_PIXELFORMAT_ARGB8888;
			ColorBytes = SDL_BYTESPERPIXEL( SDLTexFormat );
			Caps = ( SDL_PIXELLAYOUT( SDLTexFormat ) == SDL_PACKEDLAYOUT_565 ) ? CC_RGB565 : 0;
			SDLTex = SDL_CreateTexture( SDLRen, SDLTexFormat, SDL_TEXTUREACCESS_STREAMING, NewX, NewY );
			if( !SDLTex )
			{
				appErrorf( "Could not create framebuffer texture: %s", SDL_GetError() );
			}
		}

		SDL_ShowWindow( hWnd );
#ifdef PLATFORM_ANDROID
        // v32/v76 stop Android text input after show: UWindow is mouse/touch driven.
        UT99AndroidSetJavaImeWantedV76( 0 );
        SDL_EventState( SDL_TEXTINPUT, SDL_IGNORE );
        SDL_StopTextInput();
#endif
#ifdef PLATFORM_ANDROID
        UT99AndroidSetJavaImeWantedV76( 0 );
        SDL_EventState( SDL_TEXTINPUT, SDL_IGNORE );
        SDL_StopTextInput();
        UT99AndroidOpenJoystickFallbacks();
        UT99AndroidAdoptWindowSize( hWnd, NewX, NewY );
        UT99_ANDROID_SDL_LOGI("OpenWindow adopted drawable/window size %dx%d", NewX, NewY);
#endif
#ifdef PLATFORM_ANDROID
        UT99AndroidOpenControllers( "startup" );
        UT99AndroidStopTextInput();
#endif
#ifdef PLATFORM_ANDROID
        UT99AndroidSetJavaImeWantedV76( 0 );
		SDL_StopTextInput();
#endif

		// Get this window's display parameters.
		SDL_DisplayMode DisplayMode;
		DisplayIndex = SDL_GetWindowDisplayIndex( hWnd );
		if( SDL_GetWindowDisplayMode( hWnd, &DisplayMode ) == 0 )
		{
			DisplaySize.w = DisplayMode.w;
			DisplaySize.h = DisplayMode.h;
		}
	}

#if PLATFORM_ANDROID
	if( hWnd )
	{
		INT AndroidWindowX = 0;
		INT AndroidWindowY = 0;
		SDL_GetWindowSize( hWnd, &AndroidWindowX, &AndroidWindowY );
		if( GLCtx )
		{
			INT AndroidDrawableX = 0;
			INT AndroidDrawableY = 0;
			SDL_GL_GetDrawableSize( hWnd, &AndroidDrawableX, &AndroidDrawableY );
			if( AndroidDrawableX > 0 && AndroidDrawableY > 0 )
			{
				AndroidWindowX = AndroidDrawableX;
				AndroidWindowY = AndroidDrawableY;
			}
		}
		if( AndroidWindowX > 0 && AndroidWindowY > 0 )
		{
			NewX = Align( AndroidWindowX, 4 );
			NewY = AndroidWindowY;
			debugf( NAME_Log, TEXT("Android native drawable viewport: %dx%d"), NewX, NewY );
		}
	}
#endif
	SizeX = NewX;
	SizeY = NewY;

	if( !RenDev && Temporary )
		Client->TryRenderDevice( this, "SoftDrv.SoftwareRenderDevice", 0 );
	if( !RenDev && !GIsEditor && !NoHard )
		#if PLATFORM_ANDROID
		Client->TryRenderDevice( this, "ini:Engine.Engine.GameRenderDevice", 1 );
#else
		#if PLATFORM_ANDROID
		Client->TryRenderDevice( this, "ini:Engine.Engine.GameRenderDevice", 1 );
#else
		Client->TryRenderDevice( this, "ini:Engine.Engine.GameRenderDevice", Client->StartupFullscreen );
#endif
#endif
	if( !RenDev )
		Client->TryRenderDevice( this, "ini:Engine.Engine.WindowedRenderDevice", 0 );
	check(RenDev);

	if( !Temporary )
		UpdateWindowFrame();
	if( DoRepaint )
		Repaint( 1 );

	unguard;
}

//
// Close a viewport window.  Assumes that the viewport has been opened with
// OpenViewportWindow.  Does not affect the viewport's object, only the
// platform-specific information associated with it.
//
void UNSDLViewport::CloseWindow()
{
	guard(UNSDLViewport::CloseWindow);

	if( hWnd )
	{
		if( SDLTex )
		{
			SDL_DestroyTexture( SDLTex );
			SDLTex = NULL;
		}
		if( SDLRen )
		{
			SDL_DestroyRenderer( SDLRen );
			SDLRen = NULL;
		}
		if( GLCtx )
		{
			SDL_GL_DeleteContext( GLCtx );
			GLCtx = NULL;
		}
		SDL_DestroyWindow( hWnd );
		hWnd = NULL;
	}

	unguard;
}

//
// Lock the viewport window and set the approprite Screen and RealScreen fields
// of Viewport.  Returns 1 if locked successfully, 0 if failed.  Note that a
// lock failing is not a critical error; it's a sign that a DirectDraw mode
// has ended or the user has closed a viewport window.
//
UBOOL UNSDLViewport::Lock( FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* HitData, INT* HitSize )
{
	guard(UNSDLViewport::LockWindow);
	clock(Client->DrawCycles);

	// Make sure window is lockable.
	if( !hWnd )
	{
		return 0;
	}

	if( HoldCount > 0 || !SizeX || !SizeY )
	{
		appErrorf( "Failed locking viewport" );
		return 0;
	}

	if( SDLRen && SDLTex )
	{
		// Obtain pointer to screen.
		Stride = SizeX;
		ScreenPointer = NULL;
		SDL_LockTexture( SDLTex, NULL, (void **)&ScreenPointer, &Stride );
		Stride /= ColorBytes;
		check(ScreenPointer);
	}

	// Success.
	unclock(Client->DrawCycles);

	return UViewport::Lock( FlashScale, FlashFog, ScreenClear, RenderLockFlags, HitData, HitSize );

	unguard;
}

//
// Return whether fullscreen.
//
UBOOL UNSDLViewport::IsFullscreen()
{
	guard(UNSDLViewport::IsFullscreen);
	if( !hWnd )
		return 0;
	Uint32 Flags = SDL_GetWindowFlags( hWnd );
	return (Flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) != 0;
	unguard;
}

//
// Resize the viewport.
//
UBOOL UNSDLViewport::ResizeViewport( DWORD NewBlitFlags, INT InNewX, INT InNewY, INT InNewColorBytes )
{
	guard(UNSDLViewport::ResizeViewport);

	// Remember viewport for audio (mimic X/Win behaviour).
	UViewport* SavedViewport = NULL;
	if( Client->Engine->Audio && !GIsEditor && !(GetFlags() & RF_Destroyed) )
		SavedViewport = Client->Engine->Audio->GetViewport();

	// Accept default parameters.
	INT NewX          = InNewX         == INDEX_NONE ? SizeX      : InNewX;
	INT NewY          = InNewY         == INDEX_NONE ? SizeY      : InNewY;
	INT NewColorBytes = InNewColorBytes== INDEX_NONE ? ColorBytes : InNewColorBytes;

	// Default resolution handling: use client defaults when not explicitly specified.
	if( InNewX == INDEX_NONE || InNewY == INDEX_NONE )
	{
		const SDL_DisplayMode& DefaultMode = Client->GetDefaultDisplayMode();
		#ifdef PLATFORM_ANDROID
    UT99AndroidAdoptDrawableSizeV29( hWnd, NewX, NewY, "ResizeViewport" );
#ifdef PLATFORM_ANDROID
    UT99AndroidKeepNativeViewportV60( NewX, NewY, "ResizeViewport" );
#endif
#endif
if( NewBlitFlags & BLIT_Fullscreen )
		{
			// In fullscreen, prefer desktop resolution if nothing is specified.
			NewX = InNewX != INDEX_NONE ? InNewX : DefaultMode.w;
			NewY = InNewY != INDEX_NONE ? InNewY : DefaultMode.h;
		}
	}

	// Apply new size / mode.
	#ifdef PLATFORM_ANDROID
    UT99AndroidAdoptDrawableSizeV29( hWnd, NewX, NewY, "ResizeViewport" );
#ifdef PLATFORM_ANDROID
    UT99AndroidKeepNativeViewportV60( NewX, NewY, "ResizeViewport" );
#endif
#endif
if( NewBlitFlags & BLIT_Fullscreen )
	{
		// Switching to fullscreen.
		MakeFullscreen( NewX, NewY, 1 );
	}
	else
	{
		// Windowed resize.
		SetClientSize( NewX, NewY, 1 );
		EndFullscreen();
	}

	// Update color depth if requested (render devices read this).
	ColorBytes = NewColorBytes;

	// Update audio.
	if( SavedViewport && SavedViewport!=Client->Engine->Audio->GetViewport() )
		Client->Engine->Audio->SetViewport( SavedViewport );

	// Update window frame.
	UpdateWindowFrame();

	return 1;

	unguard;
}

//
// Unlock the viewport window.  If Blit=1, blits the viewport's frame buffer.
//
void UNSDLViewport::Unlock( UBOOL Blit )
{
	guard(UNSDLViewport::Unlock);

	Client->DrawCycles=0;
	clock(Client->DrawCycles);

	// Unlock base.
	UViewport::Unlock( Blit );

	// Blit, if desired.
	if( Blit && hWnd && HoldCount == 0 )
	{
		if( GLCtx )
		{

			SDL_GL_SwapWindow( hWnd );
		}
		else if( SDLRen && SDLTex )
		{
			// Blitting with SDLRenderer.
			SDL_UnlockTexture( SDLTex );
			SDL_RenderCopy( SDLRen, SDLTex, NULL, NULL );
			SDL_RenderPresent( SDLRen );
		}
	}

	unclock(Client->DrawCycles);

	unguard;
}

//
// Make this viewport the current one.
// If Viewport=0, makes no viewport the current one.
//
void UNSDLViewport::MakeCurrent()
{
	guard(UNSDLViewport::MakeCurrent);
	Current = 1;
	for( INT i=0; i<Client->Viewports.Num(); i++ )
	{
		UViewport* OldViewport = Client->Viewports(i);
		if( OldViewport->Current && OldViewport != this )
		{
			OldViewport->Current = 0;
			OldViewport->UpdateWindowFrame();
		}
	}
	if( GLCtx )
	{
		SDL_GL_MakeCurrent( hWnd, GLCtx );
	}
	UpdateWindowFrame();
	unguard;
}

//
// Repaint the viewport.
//
void UNSDLViewport::Repaint( UBOOL Blit )
{
	guard(UNSDLViewport::Repaint);
	if( HoldCount == 0 && RenDev && SizeX && SizeY )
		Client->Engine->Draw( this, Blit );
	unguard;
}

//
// Set the client size (viewport view size) of a viewport.
//
void UNSDLViewport::SetClientSize( INT NewX, INT NewY, UBOOL UpdateProfile )
{
	guard(UNSDLViewport::SetClientSize);
#ifdef PLATFORM_ANDROID
    UT99AndroidAdoptDrawableSizeV29( hWnd, NewX, NewY, "SetClientSize" );
#ifdef PLATFORM_ANDROID
    UT99AndroidKeepNativeViewportV60( NewX, NewY, "SetClientSize" );
#endif
#endif

	// Guard against zero/negative sizes.
	if( NewX <= 0 ) NewX = 320;
	if( NewY <= 0 ) NewY = 240;

	if( hWnd )
	{
		SDL_SetWindowSize( hWnd, NewX, NewY );
		// Resize output texture if required.
		if( SDLRen && SDLTex )
		{
			SDL_DestroyTexture( SDLTex );
			SDLTex = SDL_CreateTexture( SDLRen, SDLTexFormat, SDL_TEXTUREACCESS_STREAMING, NewX, NewY );
			if( !SDLTex )
			{
				appErrorf( "Could not create framebuffer texture: %s", SDL_GetError() );
			}
		}
	}

#if PLATFORM_ANDROID
	if( hWnd )
	{
		INT AndroidWindowX = 0;
		INT AndroidWindowY = 0;
		SDL_GetWindowSize( hWnd, &AndroidWindowX, &AndroidWindowY );
		if( GLCtx )
		{
			INT AndroidDrawableX = 0;
			INT AndroidDrawableY = 0;
			SDL_GL_GetDrawableSize( hWnd, &AndroidDrawableX, &AndroidDrawableY );
			if( AndroidDrawableX > 0 && AndroidDrawableY > 0 )
			{
				AndroidWindowX = AndroidDrawableX;
				AndroidWindowY = AndroidDrawableY;
			}
		}
		if( AndroidWindowX > 0 && AndroidWindowY > 0 )
		{
			NewX = Align( AndroidWindowX, 4 );
			NewY = AndroidWindowY;
			debugf( NAME_Log, TEXT("Android native drawable viewport: %dx%d"), NewX, NewY );
		}
	}
#endif
	SizeX = NewX;
	SizeY = NewY;

	// Optionally save this size in the profile.
	if( UpdateProfile )
	{
		Client->FullscreenViewportX = NewX;
		Client->FullscreenViewportY = NewY;
		Client->SaveConfig();
	}

	unguard;
}

//
// Return the viewport's window.
//
void* UNSDLViewport::GetWindow()
{
	return (void*)hWnd;
}

//
// Try to make this viewport fullscreen, matching the fullscreen
// mode of the nearest x-size to the current window. If already in
// fullscreen, returns to non-fullscreen.
//
void UNSDLViewport::MakeFullscreen( INT NewX, INT NewY, UBOOL UpdateProfile )
{
	guard(UNSDLViewport::MakeFullscreen);
#ifdef PLATFORM_ANDROID
    UT99AndroidAdoptDrawableSizeV29( hWnd, NewX, NewY, "MakeFullscreen" );
#ifdef PLATFORM_ANDROID
    UT99AndroidKeepNativeViewportV60( NewX, NewY, "MakeFullscreen" );
#endif
#endif

	// If someone else is fullscreen, stop them.
	// Note: FullscreenViewport tracking removed - Unreal 1 specific
	Client->EndFullscreen();

	// Save this window.
	SavedX = SizeX;
	SavedY = SizeY;

	// Fullscreen rendering. For now no borderless.
	// Note: FullscreenViewport tracking removed - Unreal 1 specific
	SetClientSize( NewX, NewY, false );
	SDL_SetWindowFullscreen( hWnd, SDL_WINDOW_FULLSCREEN );

	if( UpdateProfile )
	{
		Client->FullscreenViewportX = NewX;
		Client->FullscreenViewportY = NewY;
		Client->SaveConfig();
	}

	unguard;
}

//
//
//
void UNSDLViewport::EndFullscreen()
{
	guard(UNSDLViewport::EndFullscreen);

	SDL_SetWindowFullscreen( hWnd, 0 );
	SetClientSize( SavedX, SavedY, false );

	unguard;
}

//
// Update input for viewport.
//
void UNSDLViewport::UpdateInput( UBOOL Reset )
{
	guard(UNSDLViewport::UpdateInput);

	if( Reset )
	{
		appMemset( (void*)JoyAxis, 0, sizeof(JoyAxis) );
#ifdef PLATFORM_ANDROID
        // RETROTOUCH_BETA4_INPUT_RESET_SYNC:
        // UInput::ResetInput() reaches this method on respawn, map transitions,
        // network player replacement and other engine-side input resets. Expose a
        // monotonically increasing serial to Java so RetroTouch drops its pointer
        // ownership/latches at the exact same boundary.
        ++GUT99RetroTouchInputResetSerial;
#endif
	}

	unguard;
}

//
// If the cursor is currently being captured, stop capturing, clipping, and 
// hiding it, and move its position back to where it was when it was initially
// captured.
//
void UNSDLViewport::SetMouseCapture( UBOOL Capture, UBOOL Clip, UBOOL OnlyFocus )
{
	guard(UNSDLViewport::SetMouseCapture);

	// If only focus, reject.
	if( OnlyFocus )
		if( hWnd != SDL_GetMouseFocus() )
			return;

	// If capturing, windows requires clipping in order to keep focus.
	Clip |= Capture;

	// Handle capturing.
	SDL_SetRelativeMouseMode( (SDL_bool)Capture );

	unguard;
}


void UNSDLViewport::AndroidNativeMouseDeltaV112( DWORD MouseFlags, INT DX, INT DY )
{
	guard(UNSDLViewport::AndroidNativeMouseDeltaV112);
	// UT99_ANDROID_NATIVE_MOUSE_GAMEPLAY_BUILD_FIX_V112
	// Static Android/OUYA mouse helpers cannot access private Client directly.
	// Keep the v111 gameplay-look path, but route it through a viewport member.
	if( Client && Client->Engine )
		Client->Engine->MouseDelta( this, MouseFlags, DX, DY );
	if( DX ) CauseInputEvent( IK_MouseX, IST_Axis, +DX );
	if( DY ) CauseInputEvent( IK_MouseY, IST_Axis, -DY );
	unguard;
}

void UNSDLViewport::AndroidUpdateNativeMouseCaptureV113( UBOOL bMenuMode )
{
	guard(UNSDLViewport::AndroidUpdateNativeMouseCaptureV113);
	// UT99_ANDROID_NATIVE_MOUSE_RELATIVE_CAPTURE_V113
	// v112 still used absolute Android mouse coordinates.  That made gameplay look
	// stop at the screen edge because the visible platform cursor physically hit the
	// window border.  Once a real mouse was seen, capture it only during gameplay:
	// SDL relative mode hides/locks the pointer and gives us infinite xrel/yrel.
	const UBOOL WantCapture = ( !bMenuMode && GUT99V113NativeMouseSeen && hWnd ) ? 1 : 0;
	if( WantCapture != GUT99V113NativeMouseCaptureActive )
	{
		if( WantCapture )
		{
			SDL_ShowCursor( SDL_DISABLE );
			SDL_SetWindowGrab( hWnd, SDL_TRUE );
			const INT RelResult = SDL_SetRelativeMouseMode( SDL_TRUE );
			GUT99V111NativeMouseHadLast = 0;
			for( INT i=0; i<8; ++i ) GUT99V115MenuButtonDownValid[i] = 0; // UT99_ANDROID_NATIVE_MOUSE_MENU_CLICK_STABLE_V115
			UT99AndroidMarkNativeMouseActivityV111(); // UT99_ANDROID_NATIVE_MOUSE_MENU_CLICK_STABLE_V115
			// UT99_ANDROID_CHROMEOS_CAPTURE_RESULT_V214:
			// Do not claim capture when Android/ChromeOS rejected it. Leaving the
			// flag clear keeps the existing absolute-delta gameplay fallback alive
			// and lets later ticks retry after the Surface gains focus.
			GUT99V113NativeMouseCaptureActive =
				( RelResult == 0 && SDL_GetRelativeMouseMode() ) ? 1 : 0;
			if( !GUT99V113NativeMouseCaptureActive )
			{
				SDL_SetWindowGrab( hWnd, SDL_FALSE );
				SDL_ShowCursor( SDL_ENABLE );
			}
			UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_CHROMEOS_CAPTURE_RESULT_V214 enabled result=%d active=%d viewport=%dx%d", RelResult, GUT99V113NativeMouseCaptureActive ? 1 : 0, SizeX, SizeY );
		}
		else
		{
			SDL_SetRelativeMouseMode( SDL_FALSE );
			if( hWnd )
				SDL_SetWindowGrab( hWnd, SDL_FALSE );
			SDL_ShowCursor( SDL_ENABLE );
			GUT99V111NativeMouseHadLast = 0;
			for( INT i=0; i<8; ++i ) GUT99V115MenuButtonDownValid[i] = 0; // UT99_ANDROID_NATIVE_MOUSE_MENU_CLICK_STABLE_V115
			GUT99V113NativeMouseCaptureActive = 0;
			UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_NATIVE_MOUSE_MENU_CLICK_STABLE_V115 disabled menu=%d viewport=%dx%d", bMenuMode ? 1 : 0, SizeX, SizeY );
		}
	}
	unguard;
}

UBOOL UNSDLViewport::CauseInputEvent( INT iKey, EInputAction Action, FLOAT Delta )
{
	guard(UWindowsViewport::CauseInputEvent);

	// Route to engine if a valid key
	if( iKey > 0 )
		return Client->Engine->InputEvent( this, (EInputKey)iKey, Action, Delta );
	else
		return 0;

	unguard;
}

UBOOL UNSDLViewport::TickInput()
{
	guard(UNSDLViewport::TickInput);

	SDL_Event Ev;
	INT Tmp;
	const FLOAT CurTime = appSeconds();
	const FLOAT DeltaTime = CurTime - InputUpdateTime;

#ifdef PLATFORM_ANDROID
    if( GUT99AndroidPendingResolutionXV219 > 0
     && GUT99AndroidPendingResolutionYV219 > 0
     && hWnd )
    {
        INT DrawableX = 0;
        INT DrawableY = 0;
        SDL_GL_GetDrawableSize( hWnd, &DrawableX, &DrawableY );
        if( DrawableX <= 0 || DrawableY <= 0 )
            SDL_GetWindowSize( hWnd, &DrawableX, &DrawableY );

        const INT TargetX = GUT99AndroidPendingResolutionXV219;
        const INT TargetY = GUT99AndroidPendingResolutionYV219;
        if( Abs(DrawableX - TargetX) <= 2 && Abs(DrawableY - TargetY) <= 2 )
        {
            const INT OldSizeX = Max( 1, SizeX );
            const INT OldSizeY = Max( 1, SizeY );
            const FLOAT OldMouseX = WindowsMouseX;
            const FLOAT OldMouseY = WindowsMouseY;

            // Clear before SetClientSize() so any SDL size event produced by
            // the commit cannot run this block recursively on the next tick.
            GUT99AndroidPendingResolutionXV219 = 0;
            GUT99AndroidPendingResolutionYV219 = 0;
            GUT99AndroidPendingResolutionSinceV219 = 0.0;

            // Android already owns the fullscreen Surface. Only synchronize
            // Unreal's viewport/profile with the now-valid render buffer; do
            // not toggle fullscreen a second time.
            SetClientSize( TargetX, TargetY, false );
            Client->FullscreenViewportX = SizeX;
            Client->FullscreenViewportY = SizeY;
            Client->SaveConfig();

            WindowsMouseX = Clamp( OldMouseX * (FLOAT)SizeX / (FLOAT)OldSizeX,
                0.0f, (FLOAT)Max( 1, SizeX - 1 ) );
            WindowsMouseY = Clamp( OldMouseY * (FLOAT)SizeY / (FLOAT)OldSizeY,
                0.0f, (FLOAT)Max( 1, SizeY - 1 ) );
            UT99AndroidSetWindowConsoleMouseV91( WindowsMouseX, WindowsMouseY );

            // Drop coordinates and press ownership from the former resolution.
            GUT99V111NativeMouseHadLast = 0;
            GUT99AndroidTouchMouseHadLastV32 = 0;
            GUT99AndroidTouchMouseDownV32 = 0;
            GUT99AndroidMenuTouchActiveV169B = 0;
            GUT99AndroidMenuTouchButtonHeldV169B = 0;
            GUT99AndroidRecentFingerMouseUntilV169C = 0.0;

            UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_ASYNC_RESOLUTION_COMMIT_V219 committed drawable=%dx%d viewport=%dx%d mouse=%.1f,%.1f",
                DrawableX, DrawableY, SizeX, SizeY, WindowsMouseX, WindowsMouseY );
        }
        else
        {
            const DOUBLE NowV219 = appSeconds();
            if( NowV219 - GUT99AndroidPendingResolutionLastLogV219 > 0.75 )
            {
                GUT99AndroidPendingResolutionLastLogV219 = NowV219;
                UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_ASYNC_RESOLUTION_COMMIT_V219 waiting target=%dx%d drawable=%dx%d age=%.2f",
                    TargetX, TargetY, DrawableX, DrawableY,
                    NowV219 - GUT99AndroidPendingResolutionSinceV219 );
            }
        }
    }
#endif

#ifdef PLATFORM_ANDROID
	// Aggregate physical relative motion once per input tick. SDL's ordinary
	// integer stream remains the fallback for non-ChromeOS Android mice.
	FLOAT AndroidPhysicalMouseHiResX = 0.0f;
	FLOAT AndroidPhysicalMouseHiResY = 0.0f;
	INT AndroidPhysicalMouseFallbackX = 0;
	INT AndroidPhysicalMouseFallbackY = 0;
	DWORD AndroidPhysicalMouseButtonFlags = 0;
	UBOOL bAndroidPhysicalMouseHiResSeen = 0;
#endif

#ifdef PLATFORM_ANDROID
        if( GUT99V82QueuedTextLen > 0 )
        {
            char LocalText[1024];
            INT LocalLen = GUT99V82QueuedTextLen;
            if( LocalLen >= (INT)sizeof(LocalText) )
                LocalLen = (INT)sizeof(LocalText) - 1;
            appMemcpy( LocalText, GUT99V82QueuedText, LocalLen );
            LocalText[LocalLen] = 0;
            GUT99V82QueuedTextLen = 0;
            GUT99V82QueuedText[0] = 0;

            if( Client && Client->Engine )
            {
                for( INT i = 0; i < LocalLen; ++i )
                {
                    BYTE C = (BYTE)LocalText[i];
                    if( C >= 32 || C == '\r' )
                    {
                        // UT99_ANDROID_V83_IME_KEYTYPE_FIX:
                        // UWindowEditBox::KeyType only inserts text while its
                        // bKeyDown flag is true. v82 called Engine->Key() only,
                        // so the log said text was committed but the edit box
                        // ignored it. Pulse a matching key down, call KeyType,
                        // then release. Lowercase letters are preserved because
                        // KeyType receives the original byte value.
                        if( !UT99AndroidInsertFocusedEditBoxCharV83( C ) )
                        {
                            CauseInputEvent( (EInputKey)C, IST_Press );
                            Client->Engine->Key( this, (EInputKey)C );
                            CauseInputEvent( (EInputKey)C, IST_Release );
                        }
                    }
                }
                UT99_ANDROID_SDL_LOGI( "v83 committed queued Android IME text/direct edit len=%d", LocalLen );
            }
        }
#endif

	#ifdef PLATFORM_ANDROID
        if( !bShowWindowsMouse )
        {
            UT99AndroidGameplayTickLookV41( this );
        }
#endif
#ifdef PLATFORM_ANDROID
        UT99V46TickInput( this, bShowWindowsMouse );
#endif
#ifdef PLATFORM_ANDROID
        UT99V47TickInput( this, bShowWindowsMouse );
        AndroidUpdateNativeMouseCaptureV113( bShowWindowsMouse ); // UT99_ANDROID_NATIVE_MOUSE_RELATIVE_CAPTURE_V113
        if( bShowWindowsMouse )
        {
            // UT99_ANDROID_V81_CURSOR_MODE_SWITCH:
            // There are two real menu pointer modes on Android/OUYA:
            //  1) Left stick drives the UT99/UWindow software cursor.
            //  2) Native Android/OUYA mouse/touchpad uses the platform cursor.
            // Trying to draw both cursors at once exposes scaling differences at
            // 75%/50% native Res.  Therefore native mouse activity hides the
            // UT99 software cursor until the left stick is moved again.
            const FLOAT Dead = 0.16f;
            FLOAT LX = GUT99V47LX;
            FLOAT LY = GUT99V47LY;
            if( UT99V47AbsF( LX ) <= Dead ) LX = 0.0f;
            if( UT99V47AbsF( LY ) <= Dead ) LY = 0.0f;
            const UBOOL bStickCursorActive = (LX != 0.0f || LY != 0.0f);

            if( !UT99AndroidMenuPointerVisibleV169K() && !bStickCursorActive && !GUT99AndroidMenuTouchActiveV169B )
            {
                // Touch-only/idle menu mode: no active cursor hover.  Parking the
                // mouse away from the menu bar makes direct touch selection stable.
                GUT99V81NativePointerActive = 0;
                UT99AndroidHideOrParkMenuPointerV169K( this, 0, "idle-menu" );
            }
            else if( GUT99V81NativePointerActive && !bStickCursorActive )
            {
                // Hide UT99's drawn software cursor; the Android/OUYA native
                // cursor remains visible and clicks are still delivered at the
                // scaled viewport coordinates by the mouse event path.
                bWindowsMouseAvailable = 1;
                SelectedCursor = 0;

                DOUBLE Now = appSeconds();
                if( Now - GUT99V81LastCursorModeLog > 1.20 )
                {
                    GUT99V81LastCursorModeLog = Now;
                    UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V81_CURSOR_MODE native pointer active: hiding UT99 software cursor viewport=%dx%d", SizeX, SizeY );
                }
            }
            else
            {
                if( GUT99V81NativePointerActive && bStickCursorActive )
                {
                    GUT99V81NativePointerActive = 0;
                    UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V81_CURSOR_MODE left stick restored UT99 software cursor" );
                }

                // Keep the software cursor visible while the left stick is actively
                // used as a virtual mouse.  It will hide again after the v169k idle
                // timeout.
                if( bStickCursorActive )
                    UT99AndroidMenuPointerActivityV169K( 0, "left-stick" );
                bWindowsMouseAvailable = 0;
                SelectedCursor = 0;

                if( bStickCursorActive )
                {
                    const INT W = Max( 1, SizeX );
                    const INT H = Max( 1, SizeY );
                    if( WindowsMouseX < 0.0f || WindowsMouseX >= (FLOAT)W ) WindowsMouseX = (FLOAT)W * 0.5f;
                    if( WindowsMouseY < 0.0f || WindowsMouseY >= (FLOAT)H ) WindowsMouseY = (FLOAT)H * 0.5f;

                    // Retroid already feels right at v91 speed.  OUYA reports lower
                    // effective movement, so compensate only on the OUYA profile.
                    const FLOAT Speed = GUT99V79OuyaLikeDevice ? 14.0f : 7.5f;
                    const FLOAT DX = LX * Speed;
                    const FLOAT DY = LY * Speed;

                    WindowsMouseX = Clamp( WindowsMouseX + DX, 0.0f, (FLOAT)Max( 1, W - 1 ) );
                    WindowsMouseY = Clamp( WindowsMouseY + DY, 0.0f, (FLOAT)Max( 1, H - 1 ) );
                    UT99AndroidSetWindowConsoleMouseV91( WindowsMouseX, WindowsMouseY );
                    if( DX != 0.0f ) CauseInputEvent( IK_MouseX, IST_Axis, DX );
                    if( DY != 0.0f ) CauseInputEvent( IK_MouseY, IST_Axis, -DY );

                    DOUBLE Now = appSeconds();
                    if( Now - GUT99V47LastAxisLog > 0.80 )
                    {
                        GUT99V47LastAxisLog = Now;
                        UT99_ANDROID_SDL_LOGI( "v107 software menu cursor x=%.1f y=%.1f dx=%.1f dy=%.1f", WindowsMouseX, WindowsMouseY, DX, DY );
                    }
                }
            }
        }
#endif
while( SDL_PollEvent( &Ev ) )
	{
#ifdef PLATFORM_ANDROID
                if( bShowWindowsMouse && !GUT99AndroidGameplayLastMouseShowV39 )
                {
                    UT99AndroidGameplayReleaseMoveKeysV39( this );
                }
                GUT99AndroidGameplayLastMouseShowV39 = bShowWindowsMouse ? 1 : 0;
#endif
		switch( Ev.type )
		{
#ifdef PLATFORM_ANDROID
            case SDL_CONTROLLERDEVICEADDED:
            {
                // For ADDED, cdevice.which is the current SDL joystick device
                // index (not an instance id). Open it now so subsequent axis and
                // button events are promoted to SDL_CONTROLLER* events for UT99.
                UT99_ANDROID_SDL_LOGI( "controller hotplug SDL_CONTROLLERDEVICEADDED which=%d; rescanning all current controllers", (INT)Ev.cdevice.which );
                UT99AndroidOpenControllers( "SDL_CONTROLLERDEVICEADDED" );
                break;
            }
            case SDL_CONTROLLERDEVICEREMOVED:
            {
                // For REMOVED, cdevice.which is the SDL joystick instance id.
                UT99AndroidCloseControllerInstance( (SDL_JoystickID)Ev.cdevice.which, "SDL_CONTROLLERDEVICEREMOVED" );
                // A Retroid mode switch can replace one Android device with
                // another almost immediately. Scan again after closing so a
                // replacement that is already present is opened in this tick.
                UT99AndroidOpenControllers( "post-remove-rescan" );
                break;
            }
#endif
			case SDL_QUIT:
				// signal to client and remember set a flag just in case
				QuitRequested = true;
				return true;
			case SDL_TEXTINPUT:
#if defined(__ANDROID__)
                if( GUT99V79OuyaLikeDevice && GUT99AndroidImeOpenV79 )
                {
                    UT99_ANDROID_SDL_LOGI( "v112 OUYA ignored SDL_TEXTINPUT because Java IME bridge is active" );
                    break;
                }
#endif
				for( const char *p = Ev.text.text; *p && p < Ev.text.text + sizeof( Ev.text.text ); ++p )
				{
					if( *p < 0 )
						break;
					if( isprint( *p ) || *p == '\r' )
					{
						// UT99_ANDROID_V83_TEXTINPUT_KEYTYPE:
						// Keep UWindowEditBox::bKeyDown true while KeyType runs.
						if( Client && Client->Engine )
						{
							BYTE C = (BYTE)*p;
							if( !UT99AndroidInsertFocusedEditBoxCharV83( C ) )
							{
								CauseInputEvent( (EInputKey)C, IST_Press );
								Client->Engine->Key( this, (EInputKey)C );
								CauseInputEvent( (EInputKey)C, IST_Release );
							}
						}
					}
				}
#ifdef PLATFORM_ANDROID
				UT99_ANDROID_SDL_LOGI( "v83 SDL_TEXTINPUT committed through direct edit/keytype path" );
#endif
				break;
			case SDL_KEYDOWN:
			case SDL_KEYUP:
#if defined(__ANDROID__)
				if( Ev.type == SDL_KEYDOWN && GUT99AndroidImeOpenV79 )
				{
					if( Ev.key.keysym.sym == SDLK_BACKSPACE || Ev.key.keysym.sym == SDLK_DELETE || KeyMap[Ev.key.keysym.scancode] == IK_Backspace )
					{
						UT99V80SendKeyType( this, IK_Backspace );
						break;
					}
					if( Ev.key.keysym.sym >= 32 && Ev.key.keysym.sym < 127 )
					{
                        if( GUT99V79OuyaLikeDevice )
                        {
                            UT99_ANDROID_SDL_LOGI( "v112 OUYA ignored printable SDL_KEYDOWN because Java IME bridge is active sym=%d", (INT)Ev.key.keysym.sym );
                            break;
                        }
						if( Client && Client->Engine )
						{
							BYTE C = (BYTE)Ev.key.keysym.sym;
							if( !UT99AndroidInsertFocusedEditBoxCharV83( C ) )
							{
								CauseInputEvent( (EInputKey)C, IST_Press );
								Client->Engine->Key( this, (EInputKey)C );
								CauseInputEvent( (EInputKey)C, IST_Release );
							}
						}
						UT99_ANDROID_SDL_LOGI( "v83 SDL_KEYDOWN printable committed sym=%d", (INT)Ev.key.keysym.sym );
						break;
					}
				}
#endif
				CauseInputEvent( KeyMap[Ev.key.keysym.scancode], ( Ev.type == SDL_KEYDOWN ) ? IST_Press : IST_Release );
				break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
#ifdef PLATFORM_ANDROID
                if( !UT99AndroidIsNativeMouseEventV110( Ev.button.which ) )
                {
                    // UT99_ANDROID_V169B_MENU_INPUT_ROUTER:
                    // SDL emits synthetic mouse-button events for finger touches.
                    // Touch is handled by SDL_FINGER* below, so suppress these
                    // events before they can click the old UWindow target.
                    if( bShowWindowsMouse )
                        UT99AndroidLogSuppressSyntheticTouchV115( (Ev.type == SDL_MOUSEBUTTONDOWN) ? "MOUSEBUTTONDOWN_TOUCHID" : "MOUSEBUTTONUP_TOUCHID", Ev.button.x, Ev.button.y, bShowWindowsMouse );
                    break;
                }
                if( UT99AndroidIsNativeMouseEventV110( Ev.button.which ) )
                {
                    INT MouseX = Ev.button.x;
                    INT MouseY = Ev.button.y;
                    UT99AndroidScaleNativeMouseMotionV114( this, MouseX, MouseY );
                    if( bShowWindowsMouse && UT99AndroidLooksLikeRecentFingerMouseV169C( MouseX, MouseY ) )
                    {
                        UT99AndroidLogRecentFingerMouseSuppressV169C( (Ev.type == SDL_MOUSEBUTTONDOWN) ? "mousebuttondown-after-finger" : "mousebuttonup-after-finger", MouseX, MouseY );
                        break;
                    }
                    const DWORD ButtonFlag = UT99AndroidMouseButtonFlagsV110( Ev.button.button );
                    UT99AndroidMarkNativeMouseActivityV111();
                    AndroidUpdateNativeMouseCaptureV113( bShowWindowsMouse );
                    if( bShowWindowsMouse )
                    {
                        // UT99_ANDROID_V169B_NATIVE_MOUSE_MENU_CLEAN:
                        // Real HID/BT mouse gets exactly one normal UWindow
                        // button event. No Engine->Click and no confirm pulse,
                        // otherwise sliders/selectors step once on press and again
                        // on release.
                        UT99AndroidApplyNativeMouseToUWindowV110( this, MouseX, MouseY );
                        const UBOOL SavedWindowsMouseAvailable = bWindowsMouseAvailable;
                        WindowsMouseX = MouseX;
                        WindowsMouseY = MouseY;
                        bWindowsMouseAvailable = 0;
                        UT99AndroidSetWindowConsoleMouseV91( MouseX, MouseY );
                        if( Client && Client->Engine )
                            Client->Engine->MousePosition( this, 0, MouseX, MouseY );
                        CauseInputEvent( MouseButtonMap[Ev.button.button], ( Ev.type == SDL_MOUSEBUTTONDOWN ) ? IST_Press : IST_Release );
                        if( Ev.type == SDL_MOUSEBUTTONUP && ButtonFlag == MOUSE_Left )
                            UT99AndroidShowKeyboardForClickedEditV110( hWnd, MouseX, MouseY, "native-mouse-v169b" );
                        bWindowsMouseAvailable = SavedWindowsMouseAvailable;
                        UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V169B_NATIVE_MOUSE_MENU_CLEAN button=%d type=%d pos=%d,%d viewport=%dx%d", (INT)Ev.button.button, (INT)Ev.type, MouseX, MouseY, SizeX, SizeY );
                    }
                    else
                    {
                        CauseInputEvent( MouseButtonMap[Ev.button.button], ( Ev.type == SDL_MOUSEBUTTONDOWN ) ? IST_Press : IST_Release );
                        UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_NATIVE_MOUSE_MENU_CLICK_STABLE_V115_GAME_CLICK button=%d type=%d viewport=%dx%d", (INT)Ev.button.button, (INT)Ev.type, SizeX, SizeY );
                    }
                    break;
                }
#endif
				CauseInputEvent( MouseButtonMap[Ev.button.button], ( Ev.type == SDL_MOUSEBUTTONDOWN ) ? IST_Press : IST_Release );
				break;
			case SDL_MOUSEWHEEL:
				if( Ev.wheel.y )
				{
					CauseInputEvent( IK_MouseW, IST_Axis, Ev.wheel.y );
					if( Ev.wheel.y < 0 )
					{
						CauseInputEvent( IK_MouseWheelDown, IST_Press );
						CauseInputEvent( IK_MouseWheelDown, IST_Release );
					}
					else if( Ev.wheel.y > 0 )
					{
						CauseInputEvent( IK_MouseWheelUp, IST_Press );
						CauseInputEvent( IK_MouseWheelUp, IST_Release );
					}
				}
				break;
			#ifdef PLATFORM_ANDROID
            case SDL_JOYBUTTONDOWN:
#if __ANDROID__
        /* UT99_ANDROID_MENU_INPUT_V43_SDL_JOYBUTTONDOWN */
        if( bShowWindowsMouse )
        {
            // Controller events already handled the action. Swallow the duplicate JOY copy.
            break;
        }
#endif
            case SDL_JOYBUTTONUP:
#if __ANDROID__
        /* UT99_ANDROID_MENU_INPUT_V43_SDL_JOYBUTTONUP */
        if( bShowWindowsMouse )
        {
            break;
        }
#endif
            {
#ifdef PLATFORM_ANDROID
            if( bShowWindowsMouse && UT99AndroidMenuSuppressJoyDuplicateV41( Ev.jbutton.button ) )
                break;
#endif
                #define UT99_ANDROID_JOY_FALLBACK_V27 1
                const UBOOL bDown = (Ev.type == SDL_JOYBUTTONDOWN);
                EInputKey Key = IK_None;
                switch( Ev.jbutton.button )
                {
                    case 0: Key = IK_LeftMouse; break; /* v33 menu click */
                    case 1: Key = IK_None; break; /* v32 */
                    case 2: Key = IK_N; break;
                    case 3: Key = IK_Y; break;
                    case 4: Key = IK_Joy10; break;
                    case 5: Key = IK_Joy11; break;
                    case 6: Key = IK_None; break; /* v32 */
                    case 7: Key = IK_None; break; /* v32 */
                    default: Key = (EInputKey)(IK_Joy1 + Clamp<INT>(Ev.jbutton.button,0,15)); break;
                }
                if( Key != IK_None )
                    CauseInputEvent( Key, bDown ? IST_Press : IST_Release );
                if( Ev.jbutton.button == 11 ) { CauseInputEvent( IK_Up, bDown ? IST_Press : IST_Release ); }
                if( Ev.jbutton.button == 12 ) { CauseInputEvent( IK_Down, bDown ? IST_Press : IST_Release ); }
                if( Ev.jbutton.button == 13 ) { CauseInputEvent( IK_Left, bDown ? IST_Press : IST_Release ); }
                if( Ev.jbutton.button == 14 ) { CauseInputEvent( IK_Right, bDown ? IST_Press : IST_Release ); }
                if( Ev.jbutton.button == 0 ) { CauseInputEvent( IK_Enter, bDown ? IST_Press : IST_Release ); CauseInputEvent( IK_LeftMouse, bDown ? IST_Press : IST_Release ); }
                #define UT99_ANDROID_JOY_BUTTON_UI_MIRROR_V28 1
                UT99_ANDROID_SDL_LOGI("joy button %d %s key=%d", Ev.jbutton.button, bDown ? "down" : "up", (int)Key);
                break;
            }
            case SDL_JOYHATMOTION:
            {
                if( Ev.jhat.value & SDL_HAT_UP )    { CauseInputEvent( IK_Up, IST_Press ); CauseInputEvent( IK_Up, IST_Release ); }
                if( Ev.jhat.value & SDL_HAT_DOWN )  { CauseInputEvent( IK_Down, IST_Press ); CauseInputEvent( IK_Down, IST_Release ); }
                if( Ev.jhat.value & SDL_HAT_LEFT )  { CauseInputEvent( IK_Left, IST_Press ); CauseInputEvent( IK_Left, IST_Release ); }
                if( Ev.jhat.value & SDL_HAT_RIGHT ) { CauseInputEvent( IK_Right, IST_Press ); CauseInputEvent( IK_Right, IST_Release ); }
                UT99_ANDROID_SDL_LOGI("joy hat value=%d", Ev.jhat.value);
                break;
            }
            case SDL_JOYAXISMOTION:
#if __ANDROID__
        /* UT99_ANDROID_MENU_INPUT_V43_SDL_JOYAXISMOTION */
        if( bShowWindowsMouse )
        {
            // Retroid/Android accelerometer/joy axes are very noisy here. Swallow silently.
            break;
        }
#endif
            {
                const int Dead = 12000;
                if( Ev.jaxis.axis == 0 )
                {
                    if( Ev.jaxis.value < -Dead ) { CauseInputEvent( IK_Left, IST_Press ); CauseInputEvent( IK_Left, IST_Release ); }
                    else if( Ev.jaxis.value > Dead ) { CauseInputEvent( IK_Right, IST_Press ); CauseInputEvent( IK_Right, IST_Release ); }
                }
                else if( Ev.jaxis.axis == 1 )
                {
                    if( Ev.jaxis.value < -Dead ) { CauseInputEvent( IK_Up, IST_Press ); CauseInputEvent( IK_Up, IST_Release ); }
                    else if( Ev.jaxis.value > Dead ) { CauseInputEvent( IK_Down, IST_Press ); CauseInputEvent( IK_Down, IST_Release ); }
                }
                break;
            }
#endif
			case SDL_CONTROLLERBUTTONDOWN:
#if __ANDROID__
        /* UT99_ANDROID_MENU_INPUT_V43_SDL_CONTROLLERBUTTONDOWN */
        if( bShowWindowsMouse )
        {
            UT99AndroidMenuControllerButtonV43( this, (int)Ev.cbutton.button, 1 );
            break;
        }
#endif
            case SDL_CONTROLLERBUTTONUP:
#if __ANDROID__
        /* UT99_ANDROID_MENU_INPUT_V43_SDL_CONTROLLERBUTTONUP */
        if( bShowWindowsMouse )
        {
            UT99AndroidMenuControllerButtonV43( this, (int)Ev.cbutton.button, 0 );
            break;
        }
#endif
            {
#ifdef PLATFORM_ANDROID
                const UBOOL bAndroidDown = (Ev.type == SDL_CONTROLLERBUTTONDOWN);
            if( !bShowWindowsMouse && UT99AndroidGameplayButtonV41( this, Ev.cbutton.button, bAndroidDown ) )
                break;
#endif
#ifdef PLATFORM_ANDROID
                #define UT99_ANDROID_CONTROLLER_BUTTON_MIRROR_V28 1
                {
                    const INT AndroidUiKey = UT99AndroidControllerButtonToUiKeyV28( Ev.cbutton.button );
                    if( AndroidUiKey != IK_None )
                    {
                        CauseInputEvent( AndroidUiKey, bAndroidDown ? IST_Press : IST_Release );
                        if( AndroidUiKey == IK_Enter )
                            CauseInputEvent( IK_LeftMouse, bAndroidDown ? IST_Press : IST_Release );
                        UT99_ANDROID_SDL_LOGI("controller button %d %s uiKey=%d", Ev.cbutton.button, bAndroidDown ? "down" : "up", AndroidUiKey);
                    }
                }
#endif
					// HACK: Swap to alternate bindings when in menus, but not when waiting for keypress in the keybind menu.
					// Note: GetMainFrame() is Unreal 1 specific, disabled for UT99
					const UBOOL bIsInUI = 0; // Console && ((UObject*)Console)->GetMainFrame() && ...
					const BYTE* JoyMap = bIsInUI ? JoyButtonMapUI : JoyButtonMap;
					CauseInputEvent( JoyMap[Ev.cbutton.button], ( Ev.type == SDL_CONTROLLERBUTTONDOWN ) ? IST_Press : IST_Release );
				}
				break;
			case SDL_CONTROLLERAXISMOTION:
#if __ANDROID__
        /* UT99_ANDROID_MENU_INPUT_V43_SDL_CONTROLLERAXISMOTION */
        if( bShowWindowsMouse )
        {
            // UT99_ANDROID_CONTROLLER_CAPTURE_FIX_V121:
            // Menu mode previously updated only the left-stick cursor variables
            // and then broke out. The Controls capture path reads the v50 mirror
            // state, so LJoy/RJoy directions never reached Preferences > Controls.
            // Mirror every SDL controller axis into the Android/v50/v47 state here.
            // Left stick still drives the visible menu cursor via GUT99V47LX/LY.
            const FLOAT Norm = UT99V47ClampF( (FLOAT)Ev.caxis.value / 32767.0f, -1.0f, 1.0f );
            switch( Ev.caxis.axis )
            {
                case SDL_CONTROLLER_AXIS_LEFTX:
                    GUT99V50LX = Norm;
                    GUT99V47LX = Norm;
                    break;
                case SDL_CONTROLLER_AXIS_LEFTY:
                    GUT99V50LY = Norm;
                    GUT99V47LY = Norm;
                    break;
                case SDL_CONTROLLER_AXIS_RIGHTX:
                    GUT99V50RX = Norm;
                    GUT99V47RX = Norm;
                    break;
                case SDL_CONTROLLER_AXIS_RIGHTY:
                    GUT99V50RY = Norm;
                    GUT99V47RY = Norm;
                    break;
                case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
                    GUT99V50LT = Norm;
                    GUT99V47LT = Norm;
                    break;
                case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
                    GUT99V50RT = Norm;
                    GUT99V47RT = Norm;
                    break;
            }
            break;
        }
#endif
            {
#ifdef PLATFORM_ANDROID
            {
                const INT Axis = Ev.caxis.axis;
                const INT Value = Ev.caxis.value;
                if( !bShowWindowsMouse && UT99AndroidGameplayControllerAxisV39( this, Axis, Value ) )
                    break;
            }
#endif
#ifdef PLATFORM_ANDROID
                #define UT99_ANDROID_CONTROLLER_MOUSE_AXIS_V28 1
                {
                    const int Dead = 7000;
                    const FLOAT Norm = (FLOAT)Ev.caxis.value / 32767.0f;
                    FLOAT DX = 0.0f;
                    FLOAT DY = 0.0f;
                    if( Ev.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX || Ev.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTX )
                    {
                        if( Ev.caxis.value < -Dead || Ev.caxis.value > Dead ) DX = Norm * 42.0f;
                    }
                    else if( Ev.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY || Ev.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTY )
                    {
                        if( Ev.caxis.value < -Dead || Ev.caxis.value > Dead ) DY = Norm * 42.0f;
                    }
                    if( DX != 0.0f || DY != 0.0f )
                    {
                        if( WindowsMouseX < 0 || WindowsMouseX >= SizeX ) WindowsMouseX = SizeX / 2;
                        if( WindowsMouseY < 0 || WindowsMouseY >= SizeY ) WindowsMouseY = SizeY / 2;
                        WindowsMouseX = Clamp( WindowsMouseX + DX, 0.0f, (FLOAT)Max(1, SizeX - 1) );
                        WindowsMouseY = Clamp( WindowsMouseY + DY, 0.0f, (FLOAT)Max(1, SizeY - 1) );
                        if( Client && Client->Engine )
                            Client->Engine->MousePosition( this, 0, WindowsMouseX, WindowsMouseY );
                        CauseInputEvent( IK_MouseX, IST_Axis, DX );
                        CauseInputEvent( IK_MouseY, IST_Axis, -DY );
                        static INT AxisLogCount = 0;
                        if( AxisLogCount < 40 )
                        {
                            UT99_ANDROID_SDL_LOGI("controller axis %d value=%d mouse=%.1f,%.1f d=%.1f,%.1f size=%dx%d", Ev.caxis.axis, Ev.caxis.value, WindowsMouseX, WindowsMouseY, DX, DY, SizeX, SizeY);
                            AxisLogCount++;
                        }
                    }
                }
#endif
					const BYTE Key = JoyAxisMap[Ev.caxis.axis];
					const INT PrevValue = JoyAxis[Ev.caxis.axis];
					INT NewValue = Ev.caxis.value;
					INT DeadZone = 0;
					if ( Key < IK_JoyX )
					{
						// Treat the axis like a trigger.
						if ( PrevValue < JoyAxisPressThreshold && NewValue >= JoyAxisPressThreshold )
							CauseInputEvent( Key, IST_Press );
						else if ( PrevValue >= JoyAxisPressThreshold && NewValue < JoyAxisPressThreshold )
							CauseInputEvent( Key, IST_Release );
					}
					else
					{
						// Apply deadzone.
						if ( Key >= IK_JoyX && Key <= IK_JoyZ )
							DeadZone = Client->DeadZoneXYZ * 32767.f;
						else if ( Key == IK_JoyR || Key == IK_JoyU || Key == IK_JoyV )
							DeadZone = Client->DeadZoneRUV * 32767.f;
						if ( Abs(NewValue) < DeadZone )
							NewValue = 0;
					}
					JoyAxis[Ev.caxis.axis] = NewValue;
				}
				break;
			#ifdef PLATFORM_ANDROID
            case SDL_FINGERDOWN:
            {
                INT MouseX = 0;
                INT MouseY = 0;
                UT99AndroidTouchToViewportCoordsV32( this, Ev.tfinger, MouseX, MouseY );
                if( UT99AndroidShouldSuppressSyntheticTouchV111() )
                {
                    GUT99AndroidTouchMouseDownV32 = 0;
                    GUT99AndroidTouchMouseHadLastV32 = 0;
                    GUT99AndroidSuppressNextTouchClickV37 = 0;
                    GUT99AndroidMenuTopTapSyntheticOnlyV164 = 0;
                    GUT99AndroidMenuTouchActiveV169B = 0;
                    GUT99AndroidMenuTouchButtonHeldV169B = 0;
                    UT99AndroidLogSuppressSyntheticTouchV115( "FINGERDOWN", MouseX, MouseY, bShowWindowsMouse );
                    break;
                }

                if( bShowWindowsMouse )
                {
                    UT99AndroidMenuPointerTouchModeV169K();
                    UT99AndroidHideOrParkMenuPointerV169K( this, 1, "finger-down" );
                    UT99AndroidRememberRecentFingerMouseV169C( MouseX, MouseY );
                    // UT99_ANDROID_V169E_DIRECT_UWINDOW_TOUCH:
                    // Direct touch = press exactly where the finger goes down.
                    // No MouseX/Y axis pulse and no touchpad-style cursor travel.
                    GUT99AndroidMenuTouchActiveV169B = 1;
                    GUT99AndroidMenuTouchDownXV169B = MouseX;
                    GUT99AndroidMenuTouchDownYV169B = MouseY;
                    GUT99AndroidMenuTouchMovedV169B = 0;
                    GUT99AndroidMenuTouchButtonHeldV169B = 1;
                    GUT99AndroidTouchMouseDownV32 = 0;
                    GUT99AndroidTouchMouseHadLastV32 = 1;
                    GUT99AndroidTouchMouseLastXV32 = MouseX;
                    GUT99AndroidTouchMouseLastYV32 = MouseY;
                    GUT99AndroidMenuTopTapSyntheticOnlyV164 = 0;
                    UT99AndroidMenuDirectTouchButtonV169D( this, Client ? Client->Engine : NULL, MouseX, MouseY, 1 );
                    UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V169J_TOP_MENU_DIRECT_SELECT finger down x=%d y=%d viewport=%dx%d", MouseX, MouseY, SizeX, SizeY );
                    UT99_ANDROID_SDL_LOGI( "v33 uwindow mouse fields x=%.1f y=%.1f available=%d show=%d", WindowsMouseX, WindowsMouseY, bWindowsMouseAvailable ? 1 : 0, bShowWindowsMouse ? 1 : 0 );
                    break;
                }

                // Gameplay/non-menu touch keeps the existing Android touch bridge.
                WindowsMouseX = MouseX;
                WindowsMouseY = MouseY;
                bWindowsMouseAvailable = 1;
                GUT99AndroidTouchMouseDownV32 = 1;
                GUT99AndroidTouchMouseHadLastV32 = 1;
                GUT99AndroidTouchMouseLastXV32 = MouseX;
                GUT99AndroidTouchMouseLastYV32 = MouseY;
                if( Client && Client->Engine )
                    Client->Engine->MousePosition( this, MOUSE_Left, MouseX, MouseY );
                GUT99AndroidSuppressNextTouchClickV37 = 1;
                UT99_ANDROID_SDL_LOGI( "v32 touch down -> mouse x=%d y=%d viewport=%dx%d", MouseX, MouseY, SizeX, SizeY );
                UT99_ANDROID_SDL_LOGI( "v33 uwindow mouse fields x=%.1f y=%.1f available=%d show=%d", WindowsMouseX, WindowsMouseY, bWindowsMouseAvailable ? 1 : 0, bShowWindowsMouse ? 1 : 0 );
                if( !bShowWindowsMouse )
                {
                    UT99_ANDROID_SDL_LOGI( "v36 UWindow config bringup: touch requests UWindow launch show=%d available=%d /* UT99_ANDROID_UWINDOW_CONFIG_V36_LOG */", bShowWindowsMouse ? 1 : 0, bWindowsMouseAvailable ? 1 : 0 );
                    GUT99AndroidSuppressNextTouchClickV37 = 1; /* UT99_ANDROID_ENTRY_BRINGUP_V35B */
                    UT99AndroidMenuPointerTouchModeV169K();
                    CauseInputEvent( IK_Escape, IST_Press );
                    CauseInputEvent( IK_Escape, IST_Release );
                }
                break;
            }
            case SDL_FINGERMOTION:
            {
                INT MouseX = 0;
                INT MouseY = 0;
                UT99AndroidTouchToViewportCoordsV32( this, Ev.tfinger, MouseX, MouseY );
                if( UT99AndroidShouldSuppressSyntheticTouchV111() )
                {
                    GUT99AndroidTouchMouseDownV32 = 0;
                    GUT99AndroidTouchMouseHadLastV32 = 0;
                    GUT99AndroidSuppressNextTouchClickV37 = 0;
                    GUT99AndroidMenuTopTapSyntheticOnlyV164 = 0;
                    GUT99AndroidMenuTouchActiveV169B = 0;
                    GUT99AndroidMenuTouchButtonHeldV169B = 0;
                    UT99AndroidLogSuppressSyntheticTouchV115( "FINGERMOTION", MouseX, MouseY, bShowWindowsMouse );
                    break;
                }

                if( bShowWindowsMouse && GUT99AndroidMenuTouchActiveV169B )
                {
                    UT99AndroidRememberRecentFingerMouseV169C( MouseX, MouseY );
                    INT DX = MouseX - GUT99AndroidTouchMouseLastXV32;
                    INT DY = MouseY - GUT99AndroidTouchMouseLastYV32;
                    if( Abs( MouseX - GUT99AndroidMenuTouchDownXV169B ) > 5 || Abs( MouseY - GUT99AndroidMenuTouchDownYV169B ) > 5 )
                    {
                        GUT99AndroidMenuTouchMovedV169B = 1;
                        UT99AndroidMenuDirectTouchMotionV169D( this, Client ? Client->Engine : NULL, MouseX, MouseY, DX, DY );
                    }
                    GUT99AndroidTouchMouseHadLastV32 = 1;
                    GUT99AndroidTouchMouseLastXV32 = MouseX;
                    GUT99AndroidTouchMouseLastYV32 = MouseY;
                    break;
                }

                // Gameplay/non-menu touch keeps the existing drag/look behavior.
                WindowsMouseX = MouseX;
                WindowsMouseY = MouseY;
                bWindowsMouseAvailable = 1;
                DWORD MouseFlags = GUT99AndroidTouchMouseDownV32 ? MOUSE_Left : 0;
                if( Client && Client->Engine )
                    Client->Engine->MousePosition( this, MouseFlags, MouseX, MouseY );
                if( GUT99AndroidTouchMouseHadLastV32 )
                {
                    INT DX = MouseX - GUT99AndroidTouchMouseLastXV32;
                    INT DY = MouseY - GUT99AndroidTouchMouseLastYV32;
                    if( DX || DY )
                    {
                        if( Client && Client->Engine )
                            Client->Engine->MouseDelta( this, MouseFlags, DX, DY );
                        if( DX ) CauseInputEvent( IK_MouseX, IST_Axis, +DX );
                        if( DY ) CauseInputEvent( IK_MouseY, IST_Axis, -DY );
                    }
                }
                GUT99AndroidTouchMouseHadLastV32 = 1;
                GUT99AndroidTouchMouseLastXV32 = MouseX;
                GUT99AndroidTouchMouseLastYV32 = MouseY;
                break;
            }
            case SDL_FINGERUP:
            {
                INT MouseX = 0;
                INT MouseY = 0;
                UT99AndroidTouchToViewportCoordsV32( this, Ev.tfinger, MouseX, MouseY );
                if( UT99AndroidShouldSuppressSyntheticTouchV111() )
                {
                    GUT99AndroidTouchMouseDownV32 = 0;
                    GUT99AndroidTouchMouseHadLastV32 = 0;
                    GUT99AndroidSuppressNextTouchClickV37 = 0;
                    GUT99AndroidMenuTopTapSyntheticOnlyV164 = 0;
                    GUT99AndroidMenuTouchActiveV169B = 0;
                    if( GUT99AndroidMenuTouchButtonHeldV169B )
                        CauseInputEvent( IK_LeftMouse, IST_Release );
                    GUT99AndroidMenuTouchButtonHeldV169B = 0;
                    UT99AndroidLogSuppressSyntheticTouchV115( "FINGERUP", MouseX, MouseY, bShowWindowsMouse );
                    break;
                }

                if( bShowWindowsMouse && GUT99AndroidMenuTouchActiveV169B )
                {
                    UT99AndroidRememberRecentFingerMouseV169C( MouseX, MouseY );
                    // Use down coordinate for taps; use release coordinate only after
                    // a real drag.  This keeps selector taps single-step while still
                    // allowing direct slider dragging.
                    INT ClickX = GUT99AndroidMenuTouchMovedV169B ? MouseX : GUT99AndroidMenuTouchDownXV169B;
                    INT ClickY = GUT99AndroidMenuTouchMovedV169B ? MouseY : GUT99AndroidMenuTouchDownYV169B;
                    const UBOOL bWasHeldV169B = GUT99AndroidMenuTouchButtonHeldV169B;
                    if( bWasHeldV169B )
                        UT99AndroidMenuDirectTouchButtonV169D( this, Client ? Client->Engine : NULL, ClickX, ClickY, 0 );
                    else
                    {
                        UT99AndroidMenuDirectTouchButtonV169D( this, Client ? Client->Engine : NULL, ClickX, ClickY, 1 );
                        UT99AndroidMenuDirectTouchButtonV169D( this, Client ? Client->Engine : NULL, ClickX, ClickY, 0 );
                    }
                    if( Client && Client->Engine )
                        Client->Engine->MousePosition( this, 0, ClickX, ClickY );
                    GUT99AndroidTouchMouseDownV32 = 0;
                    GUT99AndroidTouchMouseHadLastV32 = 0;
                    GUT99AndroidMenuTopTapSyntheticOnlyV164 = 0;
                    GUT99AndroidMenuTouchActiveV169B = 0;
                    GUT99AndroidMenuTouchButtonHeldV169B = 0;
                    UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V169J_TOP_MENU_DIRECT_SELECT finger up x=%d y=%d release=%d,%d moved=%d held=%d show=%d", ClickX, ClickY, MouseX, MouseY, GUT99AndroidMenuTouchMovedV169B ? 1 : 0, bWasHeldV169B ? 1 : 0, bShowWindowsMouse ? 1 : 0 );
#if defined(__ANDROID__)
                    UT99AndroidShowKeyboardForClickedEditV110( hWnd, ClickX, ClickY, "finger-v169k-touch-direct" );
#endif
                    UT99AndroidMenuPointerTouchModeV169K();
                    UT99_ANDROID_SDL_LOGI( "v33 uwindow mouse fields after click x=%.1f y=%.1f available=%d show=%d", WindowsMouseX, WindowsMouseY, bWindowsMouseAvailable ? 1 : 0, bShowWindowsMouse ? 1 : 0 );
                    break;
                }

                if( GUT99AndroidSuppressNextTouchClickV37 )
                {
                    GUT99AndroidSuppressNextTouchClickV37 = 0;
                    if( Client && Client->Engine )
                        Client->Engine->MousePosition( this, 0, MouseX, MouseY );
                    CauseInputEvent( IK_LeftMouse, IST_Release );
                    GUT99AndroidTouchMouseDownV32 = 0;
                    GUT99AndroidTouchMouseHadLastV32 = 0;
                    GUT99AndroidMenuTopTapSyntheticOnlyV164 = 0;
                    GUT99AndroidMenuTouchActiveV169B = 0;
                    GUT99AndroidMenuTouchButtonHeldV169B = 0;
                    UT99_ANDROID_SDL_LOGI( "v37 swallowed first UWindow-launch touch click x=%d y=%d", MouseX, MouseY );
                    break;
                }

                WindowsMouseX = MouseX;
                WindowsMouseY = MouseY;
                bWindowsMouseAvailable = 1;
                UT99AndroidSetWindowConsoleMouseV91( MouseX, MouseY );
                if( Client && Client->Engine )
                {
                    Client->Engine->MousePosition( this, MOUSE_Left, MouseX, MouseY );
                    Client->Engine->Click( this, MOUSE_Left, MouseX, MouseY );
                    Client->Engine->MousePosition( this, 0, MouseX, MouseY );
                }
                CauseInputEvent( IK_LeftMouse, IST_Release );
                GUT99AndroidTouchMouseDownV32 = 0;
                GUT99AndroidTouchMouseHadLastV32 = 0;
                GUT99AndroidMenuTopTapSyntheticOnlyV164 = 0;
                GUT99AndroidMenuTouchActiveV169B = 0;
                GUT99AndroidMenuTouchButtonHeldV169B = 0;
                UT99_ANDROID_SDL_LOGI( "v32 touch up/click -> mouse x=%d y=%d viewport=%dx%d", MouseX, MouseY, SizeX, SizeY );
                UT99_ANDROID_SDL_LOGI( "v33 uwindow mouse fields after click x=%.1f y=%.1f available=%d show=%d", WindowsMouseX, WindowsMouseY, bWindowsMouseAvailable ? 1 : 0, bShowWindowsMouse ? 1 : 0 );
                break;
            }
#endif
#ifdef PLATFORM_ANDROID
            case SDL_USEREVENT + 0x210:
                // UT99_ANDROID_CHROMEOS_MOUSE_FRAMEPACED_FLOAT_V210
                if( Ev.user.code == GUT99AndroidHighResMouseMagicV210 )
                {
                    AndroidPhysicalMouseHiResX += (FLOAT)(PTRINT)Ev.user.data1 * GUT99AndroidHighResMouseInvScaleV210;
                    AndroidPhysicalMouseHiResY += (FLOAT)(PTRINT)Ev.user.data2 * GUT99AndroidHighResMouseInvScaleV210;
                    bAndroidPhysicalMouseHiResSeen = 1;
                }
                break;
#endif
case SDL_MOUSEMOTION:
				if( !SDL_GetRelativeMouseMode() )
				{
					// If cursor isn't captured, just do MousePosition.
#ifdef PLATFORM_ANDROID
                    INT MouseX = Ev.motion.x;
                    INT MouseY = Ev.motion.y;
                    const UBOOL bNativeMouseMotionV114 = UT99AndroidIsNativeMouseEventV110( Ev.motion.which );
                    if( bNativeMouseMotionV114 )
                    {
                        // UT99_ANDROID_NATIVE_MOUSE_STABLE_V114:
                        // Motion is the one path that can still arrive in fullscreen
                        // View coordinates at 75%/50% render scale.  Scale it before
                        // UWindow so menu hover and clicks share the same coordinate
                        // system.  Do not let the initial 0,0 startup motion enable
                        // relative capture; it creates a huge first xrel/yrel warp.
                        const UBOOL bStartupZeroMotion = ( Ev.motion.x == 0 && Ev.motion.y == 0 && Ev.motion.xrel == 0 && Ev.motion.yrel == 0 );
                        UT99AndroidScaleNativeMouseMotionV114( this, MouseX, MouseY );
                        if( bShowWindowsMouse && UT99AndroidLooksLikeRecentFingerMouseV169C( MouseX, MouseY ) )
                        {
                            UT99AndroidLogRecentFingerMouseSuppressV169C( "mousemotion-after-finger", MouseX, MouseY );
                            break;
                        }
                        if( !bStartupZeroMotion )
                            UT99AndroidMarkNativeMouseActivityV111(); // UT99_ANDROID_NATIVE_MOUSE_GAMEPLAY_V111
                        if( bShowWindowsMouse )
                            UT99AndroidApplyNativeMouseToUWindowV110( this, MouseX, MouseY );
                        AndroidUpdateNativeMouseCaptureV113( bShowWindowsMouse ); // UT99_ANDROID_NATIVE_MOUSE_RELATIVE_CAPTURE_V113
                        if( !bShowWindowsMouse && !SDL_GetRelativeMouseMode() )
                            UT99AndroidNativeMouseGameplayMotionV111( this, MouseX, MouseY, UT99AndroidMouseStateFlagsV110( Ev.motion.state ) );
                        static DOUBLE GUT99V110LastMouseMotionLog = 0.0;
                        DOUBLE NowV110Mouse = appSeconds();
                        if( NowV110Mouse - GUT99V110LastMouseMotionLog > 0.80 )
                        {
                            GUT99V110LastMouseMotionLog = NowV110Mouse;
                            UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_NATIVE_MOUSE_MENU_CLICK_STABLE_V115_MOTION x=%d y=%d raw=%d,%d viewport=%dx%d show=%d", MouseX, MouseY, (INT)Ev.motion.x, (INT)Ev.motion.y, SizeX, SizeY, bShowWindowsMouse ? 1 : 0 );
                        }
                    }
                    else if( bShowWindowsMouse )
                    {
                        // UT99_ANDROID_V169B_MENU_INPUT_ROUTER:
                        // SDL_TOUCH_MOUSEID synthetic motion belongs to the finger
                        // router.  Letting it run here re-activates native pointer
                        // mode and makes the next tap hit the previous UWindow target.
                        static DOUBLE GUT99V169BLastSyntheticMotionLog = 0.0;
                        DOUBLE NowV169BMouse = appSeconds();
                        if( NowV169BMouse - GUT99V169BLastSyntheticMotionLog > 0.80 )
                        {
                            GUT99V169BLastSyntheticMotionLog = NowV169BMouse;
                            UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V169B_SUPPRESS_TOUCH_MOUSE_MOTION x=%d y=%d viewport=%dx%d", MouseX, MouseY, SizeX, SizeY );
                        }
                        break;
                    }
                    if( Client && Client->Engine && ( bNativeMouseMotionV114 || !bShowWindowsMouse ) )
                        Client->Engine->MousePosition( this, UT99AndroidMouseStateFlagsV110( Ev.motion.state ), MouseX, MouseY );
#else
					Client->Engine->MousePosition( this, 0, Ev.motion.x, Ev.motion.y );
#endif
				}
				else
				{
					DWORD ViewportButtonFlags = 0;
					if( Ev.motion.state & SDL_BUTTON_LMASK ) ViewportButtonFlags |= MOUSE_Left;
					if( Ev.motion.state & SDL_BUTTON_RMASK ) ViewportButtonFlags |= MOUSE_Right;
					if( Ev.motion.state & SDL_BUTTON_MMASK ) ViewportButtonFlags |= MOUSE_Middle;
					if( Ev.motion.xrel || Ev.motion.yrel )
					{
#ifdef PLATFORM_ANDROID
						// UT99_ANDROID_CHROMEOS_MOUSE_FRAMEPACED_FLOAT_V210
						// Preserve every SDL fallback delta and dispatch once after the
						// queue is drained. The parallel fixed-point event wins when present.
						UT99AndroidMarkNativeMouseActivityV111();
						AndroidPhysicalMouseFallbackX += Ev.motion.xrel;
						AndroidPhysicalMouseFallbackY += Ev.motion.yrel;
						AndroidPhysicalMouseButtonFlags = ViewportButtonFlags;
#else
						Client->Engine->MouseDelta( this, ViewportButtonFlags, Ev.motion.xrel, -Ev.motion.yrel );
						if( Ev.motion.xrel ) CauseInputEvent( IK_MouseX, IST_Axis, Ev.motion.xrel );
						if( Ev.motion.yrel ) CauseInputEvent( IK_MouseY, IST_Axis, -Ev.motion.yrel );
#endif
					}
				}
				break;
			default:
				break;
		}
	}

#ifdef PLATFORM_ANDROID
	// UT99_ANDROID_CHROMEOS_MOUSE_FRAMEPACED_FLOAT_V210
	// This is frame aggregation, not smoothing: no previous-frame state is kept.
	if( !bShowWindowsMouse && SDL_GetRelativeMouseMode() && Client && Client->Engine )
	{
		const FLOAT RelScale = 0.34f;
		const FLOAT RawMouseX = bAndroidPhysicalMouseHiResSeen
			? AndroidPhysicalMouseHiResX : (FLOAT)AndroidPhysicalMouseFallbackX;
		const FLOAT RawMouseY = bAndroidPhysicalMouseHiResSeen
			? AndroidPhysicalMouseHiResY : (FLOAT)AndroidPhysicalMouseFallbackY;
		const FLOAT MouseDX = RawMouseX * RelScale;
		const FLOAT MouseDY = RawMouseY * RelScale;
		const Uint32 CurrentMouseState = SDL_GetMouseState( NULL, NULL );
		if( CurrentMouseState & SDL_BUTTON_LMASK ) AndroidPhysicalMouseButtonFlags |= MOUSE_Left;
		if( CurrentMouseState & SDL_BUTTON_RMASK ) AndroidPhysicalMouseButtonFlags |= MOUSE_Right;
		if( CurrentMouseState & SDL_BUTTON_MMASK ) AndroidPhysicalMouseButtonFlags |= MOUSE_Middle;
		if( Abs(MouseDX) > 0.00001f || Abs(MouseDY) > 0.00001f )
		{
			Client->Engine->MouseDelta( this, AndroidPhysicalMouseButtonFlags, MouseDX, -MouseDY );
			if( Abs(MouseDX) > 0.00001f ) CauseInputEvent( IK_MouseX, IST_Axis, MouseDX );
			if( Abs(MouseDY) > 0.00001f ) CauseInputEvent( IK_MouseY, IST_Axis, -MouseDY );
		}
		DOUBLE NowV210Rel = appSeconds();
		if( (RawMouseX != 0.0f || RawMouseY != 0.0f)
		 && NowV210Rel - GUT99V113LastRelativeMotionLog > 0.80 )
		{
			GUT99V113LastRelativeMotionLog = NowV210Rel;
			UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_CHROMEOS_MOUSE_V210 raw=%.3f,%.3f scaled=%.3f,%.3f hires=%d flags=%lu", RawMouseX, RawMouseY, MouseDX, MouseDY, bAndroidPhysicalMouseHiResSeen ? 1 : 0, (unsigned long)AndroidPhysicalMouseButtonFlags );
		}
	}
#endif

	// Constantly hammer the input system with axis events for axes that are not zero.
	for ( INT i = 0; i < SDL_CONTROLLER_AXIS_MAX; ++i )
	{
		const BYTE Key = JoyAxisMap[i];
		const SWORD Value = JoyAxis[i];
		if ( Value && Key && Key >= IK_JoyX )
		{
			const FLOAT FltValue = Clamp( Value / 32767.f, -1.f, 1.f );
			FLOAT Scale = ( Key >= IK_JoyX && Key <= IK_JoyZ ) ? Client->ScaleXYZ : Client->ScaleRUV;
			Scale *= JoyAxisDefaultScale[i] * DeltaTime;
			if ( ( Client->InvertV && Key == IK_JoyV ) || ( Client->InvertY && Key == IK_JoyY ) )
				Scale = -Scale;
			CauseInputEvent( Key, IST_Axis, FltValue * Scale );
		}
	}

#ifdef PLATFORM_ANDROID
	UT99AndroidPersistAutoAimV78( this );
#endif

	InputUpdateTime = CurTime;

	return QuitRequested;

	unguard;
}

/*-----------------------------------------------------------------------------
	Command line.
-----------------------------------------------------------------------------*/

UBOOL UNSDLViewport::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
    // UT99_ANDROID_BLOCK_WEB_COMMANDS_V51:
    // Avoid unsupported PC web/browser commands on Android.
    if( ParseCommand(&Cmd,TEXT("STARTWEB"))
     || ParseCommand(&Cmd,TEXT("OPENURL"))
     || ParseCommand(&Cmd,TEXT("BROWSE"))
     || ParseCommand(&Cmd,TEXT("WEB"))
     || ParseCommand(&Cmd,TEXT("UWEB")) )
    {
        return 1;
    }
#endif /* UT99_ANDROID_BLOCK_WEB_COMMANDS_V51 */
#ifdef PLATFORM_ANDROID
    if( ParseCommand(&Cmd,TEXT("ANDROID_AUDIO_PING")) )
    {
        UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V50_SOUND_ATTEMPT audio ping command reached" );
        return 1;
    }
#endif
#ifdef PLATFORM_ANDROID
    if( ParseCommand(&Cmd,TEXT("ANDROID_SHOW_KEYBOARD")) )
    {
        UT99AndroidShowSoftKeyboardV44( hWnd, SizeX / 2, SizeY / 2 );
        UT99_ANDROID_SDL_LOGI( "v79 ANDROID_SHOW_KEYBOARD" );
        return 1;
    }
    if( ParseCommand(&Cmd,TEXT("ANDROID_HIDE_KEYBOARD")) )
    {
        UT99AndroidHideSoftKeyboardV72();
        UT99_ANDROID_SDL_LOGI( "v79 ANDROID_HIDE_KEYBOARD" );
        return 1;
    }
#endif
	guard(UNSDLViewport::Exec);
	if( UViewport::Exec( Cmd, Ar ) )
	{
		return 1;
	}
	else if( ParseCommand(&Cmd, TEXT("ToggleFullscreen")) )
	{
		// Toggle fullscreen.
		// Note: FullscreenViewport tracking removed - Unreal 1 specific
		Client->EndFullscreen();
		if( !(Actor->ShowFlags & SHOW_ChildWindow) )
			Client->TryRenderDevice( this, "ini:Engine.Engine.GameRenderDevice", 1 );
		return 1;
	}
	else if( ParseCommand(&Cmd, TEXT("GetCurrentRes")) )
	{
#ifdef PLATFORM_ANDROID
        UT99AndroidResolutionModeLabelV166( UT99AndroidGetConfiguredResolutionModeV166(), Ar );
#else
		Ar.Logf( TEXT("%ix%i"), SizeX, SizeY );
#endif
		return 1;
	}
#ifdef PLATFORM_ANDROID
    else if( ParseCommand(&Cmd, TEXT("GetColorDepths")) )
    {
        // GLES runs true colour on Android; keep the Preferences label compact.
        Ar.Log( TEXT("32") );
        return 1;
    }
    else if( ParseCommand(&Cmd, TEXT("GetCurrentColorDepth")) )
    {
        Ar.Log( TEXT("32") );
        return 1;
    }
#endif
	else if( ParseCommand(&Cmd, TEXT("SetRes")) )
	{
#ifdef PLATFORM_ANDROID
        FString AndroidMode;
        INT AndroidModeX = 0;
        INT AndroidModeY = 0;
        if( UT99AndroidParseResolutionModeTokenV166( Cmd, AndroidMode, AndroidModeX, AndroidModeY, hWnd ) )
        {
            UT99AndroidSaveResolutionModeV166( AndroidMode );
            const UBOOL bAsyncCommitV219 = UT99AndroidUseAsyncResolutionCommitV219();
            UT99AndroidCallJavaResolutionModeV166( AndroidMode );
            if( AndroidModeX > 0 && AndroidModeY > 0 )
            {
                if( bAsyncCommitV219 )
                {
                    UT99AndroidQueueResolutionCommitV219( AndroidModeX, AndroidModeY );
                    UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_ASYNC_RESOLUTION_COMMIT_V219 SetRes mode=%s target=%dx%d awaiting SurfaceHolder", TCHAR_TO_ANSI(*AndroidMode), AndroidModeX, AndroidModeY );
                }
                else
                {
                    // ChromeOS and Automotive retain their established live
                    // SetRes path; v219 is deliberately handheld-scoped.
                    MakeFullscreen( AndroidModeX, AndroidModeY, 1 );
                    UT99_ANDROID_SDL_LOGI( "UT99_ANDROID_V219_PRESERVED_PLATFORM_SETRES mode=%s size=%dx%d", TCHAR_TO_ANSI(*AndroidMode), AndroidModeX, AndroidModeY );
                }
            }
            return 1;
        }
#endif
		INT X=appAtoi(Cmd), Y=appAtoi(appStrchr(Cmd,TEXT('x')) ? appStrchr(Cmd,TEXT('x'))+1 : appStrchr(Cmd,TEXT('X')) ? appStrchr(Cmd,TEXT('X'))+1 : TEXT(""));
		if( X && Y )
		{
			// Note: FullscreenViewport tracking removed - Unreal 1 specific
			MakeFullscreen( X, Y, 1 );
		}
		return 1;
	}
	else if( ParseCommand(&Cmd, TEXT("Preferences")) )
	{
		// Note: FullscreenViewport tracking removed - Unreal 1 specific
		Client->EndFullscreen();
		return 1;
	}
	else return 0;
	unguard;
}
