/* UT99_ANDROID_V74_TOUCH_NATIVE_SCALE_FIX */
package com.ast.ut99;

import android.content.pm.ActivityInfo;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Toast;

import org.libsdl.app.SDLActivity;

import java.io.File;

/**
 * SDL entry point for the UT99 Dreamcast-code Android port.
 *
 * The APK contains both armeabi-v7a and arm64-v8a engine builds. Android selects
 * the matching ABI automatically; OUYA continues to use the 32-bit ARMv7 build.
 */
public class GameActivity extends SDLActivity {
    private static final String TAG = "UT99Android";

    // UT99_ANDROID_V166_REAL_RENDER_RESOLUTIONS:
    // Preferences > Video > Resolution now uses real SurfaceHolder backbuffer sizes:
    // Native, computed 16:9 1080p/720p, and stretched 4:3 1080p/768p modes.
    // The Android view stays fullscreen; only the render buffer changes.
    private static java.lang.ref.WeakReference<GameActivity> sUt99V64ActivityRef;
    private static final String UT99_V166_PREFS = "ut99_v166_render_resolution";
    private static final String UT99_V166_PREF_KEY_MODE = "mode";
    private String ut99V166ResolutionMode = "Native";

    // UT99_ANDROID_V76_KEYBOARD_START_SAFE:
    // The SDL dummy text view must not summon the IME during engine start.
    // Native UWindow code toggles this flag only when an actual edit-field candidate is tapped.
    private static volatile boolean sUt99ImeWanted;
    private static boolean sUt99V72LoggedActive = false;
    private int ut99V212LastDesktopInputReason = Integer.MIN_VALUE;

    public static void ut99SetImeWanted(boolean wanted) {
        sUt99ImeWanted = wanted;
    }

    public static boolean ut99IsImeWanted() {
        return sUt99ImeWanted;
    }

    public static boolean ut99CommitImeText(String text) {
        // UT99_ANDROID_V82_IME_COMMIT_BRIDGE:
        // Some Android/SDL combinations show the DummyEdit keyboard correctly but
        // never deliver SDL_TEXTINPUT to the game.  SDLActivity forwards committed
        // IME text here; native code queues it and commits it to UWindow KeyType
        // on the SDL/game thread.
        if (!sUt99ImeWanted || text == null || text.length() == 0) {
            return false;
        }
        try {
            nativeAndroidTextV82(text);
            Log.i(TAG, "v82 committed IME text through GameActivity bridge len=" + text.length());
            return true;
        } catch (Throwable t) {
            Log.w(TAG, "v82 IME bridge unavailable, falling back to SDL text path", t);
            return false;
        }
    }

    public static void ut99ApplyResolutionScaleV64(final int percent) {
        // Compatibility bridge for older native code paths. The old 75%/50% modes
        // are intentionally mapped to Native because v166 removes percentage surface scaling.
        ut99ApplyResolutionModeV166("Native");
    }

    public static void ut99ApplyResolutionModeV166(final String mode) {
        final GameActivity activity = sUt99V64ActivityRef != null ? sUt99V64ActivityRef.get() : null;
        if (activity == null) {
            Log.w(TAG, "v166 resolution mode request ignored because GameActivity is not active mode=" + mode);
            return;
        }

        activity.runOnUiThread(new Runnable() {
            @Override public void run() {
                activity.ut99V166ApplyResolutionMode(mode, true);
            }
        });
    }

    public static boolean ut99UseAsyncResolutionCommitV219() {
        // Retroid/regular Android SurfaceHolder changes are asynchronous and
        // need the native v219 commit handshake. Preserve the already verified
        // ChromeOS path and the dedicated Automotive behavior byte-for-byte.
        final GameActivity activity = sUt99V64ActivityRef != null
                ? sUt99V64ActivityRef.get() : null;
        return activity != null
                && !activity.ut99IsChromeOSV211()
                && !activity.isAutomotiveDevice();
    }

    private static boolean bridgeLoaded;
    private static Throwable bridgeLoadError;

    static {
        try {
            System.loadLibrary("ut99dc_android_bridge");
            bridgeLoaded = true;
        } catch (Throwable t) {
            bridgeLoaded = false;
            bridgeLoadError = t;
        }
    }

    private static native boolean nativePrepareProcess(String dataRoot, String homeDir, String versionName);
    private static native void nativeAndroidTextV82(String text);
    private static native boolean nativeAndroidIsMenuV92(); // UT99_ANDROID_V92_TOUCH_OVERLAY
    private static native int nativeAndroidTouchUiStateRT(); // RETROTOUCH_BETA4_STATE_MACHINE_V2

    private File dataRoot;
    private File homeDir;
    private boolean legacySafeMode;
    private Ut99RetroTouchBridge ut99RetroTouchBridge;

    private boolean isAutomotiveDevice() {
        try {
            return getPackageManager().hasSystemFeature("android.hardware.type.automotive");
        } catch (Throwable ignored) {
            return false;
        }
    }

    private File resolveDataRootForGame() {
        String fromIntent = getIntent() != null ? getIntent().getStringExtra(UT99Paths.EXTRA_DATA_ROOT) : null;
        if (fromIntent != null && fromIntent.length() > 0) {
            File candidate = new File(fromIntent);
            if (UT99Paths.hasUsableGameData(candidate)) {
                Log.i(TAG, "Using UT99 data root from installer intent: " + candidate.getAbsolutePath());
                return candidate;
            }
            Log.w(TAG, "Installer intent data root is not usable, rescanning: " + candidate.getAbsolutePath());
        }
        return UT99Paths.resolveDataRoot(this);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        sUt99V64ActivityRef = new java.lang.ref.WeakReference<GameActivity>(this);
        dataRoot = resolveDataRootForGame();
        homeDir = UT99Paths.homeDir(this);
        legacySafeMode = resolveLegacySafeMode();
        ut99V166ResolutionMode = ut99V166ReadResolutionMode();

        applyUt99ImmersiveMode();
        sUt99ImeWanted = false;
        ut99V76HideImeUnlessRequested();

        if (!bridgeLoaded) {
            // UT99_ANDROID_V78_OUYA_STATIC_STL_FIX:
            // On Android 4.1.2 the bridge can fail before SDL starts if a
            // transitive native dependency is missing.  Always call through to
            // Activity.onCreate via SDLActivity before finishing, otherwise old
            // Android reports a misleading SuperNotCalledException and hides the
            // real native-load error.
            Log.e(TAG, "Android bridge library failed to load", bridgeLoadError);
            Toast.makeText(this, "UT99 bridge load failed: " +
                    (bridgeLoadError != null ? bridgeLoadError.getMessage() : "unknown"), Toast.LENGTH_LONG).show();
            try {
                super.onCreate(savedInstanceState);
            } catch (Throwable superError) {
                Log.e(TAG, "SDLActivity fallback onCreate after bridge failure also failed", superError);
            }
            finish();
            return;
        }

        boolean androidIniCreatedV86 = false;
        try {
            UT99Paths.normalizeInstalledDataRoot(dataRoot);
            UT99Paths.rememberDataRoot(this, dataRoot);
            UT99Paths.ensureBundledSystemPatches(this, dataRoot);
            androidIniCreatedV86 = UT99Paths.ensureAndroidIni(dataRoot);
            if (legacySafeMode && androidIniCreatedV86) {
                applyLegacyOuyaSafeIni(dataRoot);
            } else if (legacySafeMode) {
                Log.i(TAG, "UT99_ANDROID_V86_CONFIG_PRESERVE: keeping existing OUYA audio/settings config");
            }
            Log.i(TAG, "Android UT99 ini prepared below " + dataRoot.getAbsolutePath()
                    + " legacySafe=" + legacySafeMode
                    + " createdDefaults=" + androidIniCreatedV86);
        } catch (Exception e) {
            Log.e(TAG, "Failed to prepare Android UT99 ini", e);
            Toast.makeText(this, "UT99 ini setup failed: " + e.getMessage(), Toast.LENGTH_LONG).show();
        }

        boolean nativeOk = nativePrepareProcess(dataRoot.getAbsolutePath(), homeDir.getAbsolutePath(), BuildConfig.VERSION_NAME);
        if (!nativeOk) {
            Log.e(TAG, "nativePrepareProcess failed; engine may not find its data path");
            Toast.makeText(this, "UT99 native path setup failed", Toast.LENGTH_LONG).show();
        } else {
            Log.i(TAG, "Runtime build label exported to engine: " + BuildConfig.VERSION_NAME);
        }

        android.util.Log.i("UT99Android", "UT99_ANDROID_V63_CITYINTRO_AUDIO_SAFE_START direct CityIntro.unr startup");
        if (androidIniCreatedV86) {
            // UT99_ANDROID_V86_CONFIG_PRESERVE:
            // Apply generated Android defaults only when the Android INI files were
            // just created.  Older builds appended these defaults on every launch,
            // which made UI changes appear to vanish after restart.
            applyUt99V36UWindowConfig();
            applyUt99V40UiSafeInputConfig();
            applyUt99V45SafeAreaLookLogoConfig();
        } else {
            android.util.Log.i("UT99Android", "UT99_ANDROID_V86_CONFIG_PRESERVE keeping existing AndroidUT99.ini/AndroidUser.ini");
        }
        applyUt99V65InitialNativeFontScaleConfig(androidIniCreatedV86);
        ut99V166EnsureResolutionModeConfig();
        super.onCreate(savedInstanceState);
        ut99V55ScheduleFixedSurface(); // v55 onCreate
        ut99V52ScheduleImmersive(); // v52 onCreate
        // UT99_ANDROID_V166J_BRANDING_ASSET_CLEANUP:
        // Old experimental static start-menu/background images are no longer staged or displayed.
        android.util.Log.i("UT99Android", "UT99_ANDROID_V166_REAL_RENDER_RESOLUTIONS active mode=" + ut99V166ResolutionMode);
        ut99V50Immersive(); // v50 onCreate
        applyUt99ImmersiveMode();
        ut99InstallRetroTouchBeta4();
    }


    @Override
    protected void onNewIntent(android.content.Intent intent) {
        // UT99_ANDROID_V87_RELIABLE_RELAUNCH:
        // GameActivity should normally be launched as a fresh standard Activity.
        // This is a safety net for devices/old installs that still deliver a
        // new intent to an existing SDL Activity instance.  Close it instead of
        // trying to run SDL_main twice in the same Java/native state.
        super.onNewIntent(intent);
        setIntent(intent);
        Log.w(TAG, "v87 stale GameActivity received new launch intent; finishing for clean restart");
        try {
            finish();
        } catch (Throwable ignored) {
        }
    }

    @Override
    protected void onDestroy() {
        final boolean finishing = isFinishing();
        try {
            if (ut99RetroTouchBridge != null) {
                ut99RetroTouchBridge.destroy();
                ut99RetroTouchBridge = null;
            }
        } catch (Throwable t) {
            Log.w(TAG, "RetroTouch destroy cleanup failed", t);
        }
        super.onDestroy();

        try {
            if (sUt99V64ActivityRef != null && sUt99V64ActivityRef.get() == this) {
                sUt99V64ActivityRef.clear();
            }
        } catch (Throwable ignored) {
        }

        // UT99_ANDROID_V87_RELIABLE_RELAUNCH:
        // The engine runs in its own :game process.  Some devices keep that
        // process alive after SDLActivity/SDL_main exits, which leaves stale
        // native state behind.  The next launcher tap can then revive the old
        // process instead of starting the engine cleanly.  Kill only the :game
        // process, never the installer/main process.
        if (finishing || !isChangingConfigurations()) {
            try {
                Log.i(TAG, "v87 GameActivity destroyed; scheduling clean :game process exit finishing=" + finishing);
                new android.os.Handler(android.os.Looper.getMainLooper()).postDelayed(new Runnable() {
                    @Override public void run() {
                        try {
                            android.os.Process.killProcess(android.os.Process.myPid());
                        } catch (Throwable ignored) {
                        }
                    }
                }, 180L);
            } catch (Throwable ignored) {
            }
        }
    }

    private boolean isLegacyOuyaLikeDevice() {
        if (android.os.Build.VERSION.SDK_INT <= 17) return true;
        String model = String.valueOf(android.os.Build.MODEL).toLowerCase(java.util.Locale.US);
        String manufacturer = String.valueOf(android.os.Build.MANUFACTURER).toLowerCase(java.util.Locale.US);
        String product = String.valueOf(android.os.Build.PRODUCT).toLowerCase(java.util.Locale.US);
        return model.contains("ouya") || manufacturer.contains("ouya") || product.contains("ouya");
    }

    private boolean resolveLegacySafeMode() {
        boolean fromIntent = getIntent() != null &&
                getIntent().getBooleanExtra(UT99Paths.EXTRA_LEGACY_SAFE_MODE, false);
        if (fromIntent) return true;
        return isLegacyOuyaLikeDevice();
    }

    private void applyLegacyOuyaSafeIni(java.io.File root) throws java.io.IOException {
        // UT99_ANDROID_V79_OUYA_AUDIO_REENABLE:
        // v78 proved the static STL start path is stable on OUYA. Do not disable
        // audio anymore; keep only conservative GenericAudio settings suitable for
        // Android 4.1 / SDL AudioTrack.
        if (root == null) return;
        java.io.File systemDir = new java.io.File(root, "System");
        if (!systemDir.exists() && !systemDir.mkdirs()) {
            throw new java.io.IOException("Cannot create System folder: " + systemDir.getAbsolutePath());
        }
        java.io.File ini = new java.io.File(systemDir, "AndroidUT99.ini");
        java.io.FileWriter fw = new java.io.FileWriter(ini, true);
        try {
            fw.write("\n; UT99_ANDROID_V79_OUYA_AUDIO_REENABLE\n");
            fw.write("[Engine.Engine]\n");
            fw.write("AudioDevice=Audio.GenericAudioSubsystem\n");
            fw.write("[Engine.GameEngine]\n");
            fw.write("UseSound=True\n");
            fw.write("[Audio.GenericAudioSubsystem]\n");
            fw.write("UseDigitalMusic=True\n");
            fw.write("UseStereo=True\n");
            fw.write("Use3dHardware=False\n");
            fw.write("UseSpatial=False\n");
            fw.write("UseReverb=False\n");
            fw.write("Latency=20\n");
            fw.write("Channels=8\n");
            fw.write("OutputRate=22050Hz\n");
        } finally {
            fw.close();
        }
        Log.i(TAG, "OUYA/Android4 compatibility mode: audio enabled with conservative GenericAudio settings");
    }

    /**
     * Android fullscreen/immersive mode for the SDL surface.
     */
    private void applyUt99ImmersiveMode() {
        // AAOS owns persistent vehicle/system bars. Render inside the safe app area.
        if (isAutomotiveDevice()) return;

        // UT99_ANDROID_IMMERSIVE_V28
        try {
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        } catch (Throwable ignored) {
        }

        Window window = getWindow();
        if (window == null) {
            return;
        }

        window.setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);
        window.setFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON,
                WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        // UT99_ANDROID_V76_KEYBOARD_START_SAFE:
        // Keep the IME hidden during normal engine/game startup. Native UWindow
        // edit-field handling explicitly requests it when text input is wanted.
        window.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING | WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN);

        View decor = window.getDecorView();
        if (decor != null) {
            decor.setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        }
    }

    private void ut99V76HideImeUnlessRequested() {
        if (sUt99ImeWanted) {
            return;
        }
        try {
            android.view.View view = getWindow() != null ? getWindow().getDecorView() : null;
            android.view.inputmethod.InputMethodManager imm =
                    (android.view.inputmethod.InputMethodManager)getSystemService(android.content.Context.INPUT_METHOD_SERVICE);
            if (view != null && imm != null) {
                imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
            }
        } catch (Throwable ignored) {
        }
    }

    private java.io.File getUt99ConfigRootV63() {
        // UT99_ANDROID_V63_CONFIG_ROOT_FIX:
        // All generated Android INI overrides must be appended to the same data root
        // that nativePrepareProcess passes to the engine.  Older helper patches wrote
        // to externalFilesDir/System while the real data often lives below
        // externalFilesDir/UT99/System, leaving the active AndroidUT99.ini incomplete.
        if (dataRoot != null) {
            return dataRoot;
        }
        java.io.File fallback = getExternalFilesDir(null);
        if (fallback != null) {
            java.io.File preferred = new java.io.File(fallback, UT99Paths.DATA_DIR_NAME);
            if (preferred.isDirectory()) {
                return preferred;
            }
        }
        return fallback;
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        if (hasFocus && (!ut99V56SurfaceFixedOnce || !ut99V59FullscreenLayoutOnce)) ut99V55ScheduleFixedSurface(); // v59 focus-until-ready
        if (hasFocus) ut99V52ScheduleImmersive(); // v52 focus
        if (hasFocus) ut99V50Immersive(); // v50 focus
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            applyUt99ImmersiveMode();
            ut99V76HideImeUnlessRequested();
        }
    }

    /**
     * v19+ links Core/Engine/Render/IpDrv/Fire/NSDLDrv/NOpenGLESDrv statically into
     * libUnrealTournament.so. Loading the individual package libraries on Android
     * made Java happy, but Unreal's old package loader still failed with:
     * "Can't find file for package 'NSDLDrv'" during Engine->Init.
     */
    @Override
    protected String[] getLibraries() {
        final java.util.ArrayList<String> libraries = new java.util.ArrayList<String>();
        libraries.add("SDL2");

        // UT99_ANDROID_V175_ABI_SAFE_LIBXMP_PRELOAD:
        // libxmp must be loaded before UnrealTournament for both supported ABIs.
        // Check Android's selected native-library directory so the app loads
        // only the libxmp.so matching the ABI chosen by the package manager.
        // This keeps ARMv7/OUYA and ARM64 in one APK without cross-ABI loading.
        boolean hasSelectedAbiXmp = false;
        try {
            final android.content.pm.ApplicationInfo appInfo = getApplicationInfo();
            final String nativeDir = appInfo != null ? appInfo.nativeLibraryDir : null;
            hasSelectedAbiXmp = nativeDir != null
                    && new java.io.File(nativeDir, System.mapLibraryName("xmp")).isFile();
            Log.i(TAG, "Selected-ABI libxmp available=" + hasSelectedAbiXmp
                    + " nativeLibraryDir=" + nativeDir);
        } catch (Throwable t) {
            Log.w(TAG, "Could not inspect selected ABI native library directory", t);
        }
        if (hasSelectedAbiXmp) {
            libraries.add("xmp");
        }

        libraries.add("UnrealTournament");
        return libraries.toArray(new String[libraries.size()]);
    }

    /**
     * Android-specific ini names are generated before SDL starts.
     */
    @Override
    protected String[] getArguments() {
        return new String[] {
                "CityIntro.unr",
                "LOG=UT99Android.log",
                "INI=AndroidUT99.ini",
                "USERINI=AndroidUser.ini"
        };
    }

    private void applyUt99V36UWindowConfig() {
        // UT99_ANDROID_UWINDOW_CONFIG_V36
        java.io.File root = getUt99ConfigRootV63();
        if (root == null) {
            android.util.Log.e("UT99Android", "v36 could not get external files dir for UWindow config");
            return;
        }
        java.io.File systemDir = new java.io.File(root, "System");
        if (!systemDir.exists() && !systemDir.mkdirs()) {
            android.util.Log.e("UT99Android", "v36 could not create System dir: " + systemDir.getAbsolutePath());
            return;
        }
        java.io.File ini = new java.io.File(systemDir, "AndroidUT99.ini");
        String block =
                "\n" +
                "; UT99_ANDROID_UWINDOW_CONFIG_V36 - appended Android override based on PC v400 Default.ini\n" +
                "[Engine.Engine]\n" +
                "Console=UTMenu.UTConsole\n" +
                "Input=Engine.Input\n" +
                "Canvas=Engine.Canvas\n" +
                "GameEngine=Engine.GameEngine\n" +
                "ViewportManager=NSDLDrv.NSDLClient\n" +
                "GameRenderDevice=NOpenGLESDrv.NOpenGLESRenderDevice\n" +
                "Render=Render.Render\n" +
                "\n" +
                "[UMenu.UnrealConsole]\n" +
                "RootWindow=UMenu.UMenuRootWindow\n" +
                "UWindowKey=IK_Esc\n" +
                "ShowDesktop=True\n" +
                "bShowConsole=False\n" +
                "\n" +
                "[UWindow.WindowConsole]\n" +
                "RootWindow=UMenu.UMenuRootWindow\n" +
                "UWindowKey=IK_Esc\n" +
                "ShowDesktop=True\n" +
                "bShowConsole=False\n" +
                "\n" +
                "[UTMenu.UTConsole]\n" +
                "RootWindow=UMenu.UMenuRootWindow\n" +
                "UWindowKey=IK_Esc\n" +
                "ShowDesktop=True\n" +
                "bShowConsole=False\n";
        try {
            java.io.FileWriter fw = new java.io.FileWriter(ini, true);
            try {
                fw.write(block);
            } finally {
                fw.close();
            }
            android.util.Log.i("UT99Android", "UT99_ANDROID_UWINDOW_CONFIG_V36 appended to " + ini.getAbsolutePath());
        } catch (java.io.IOException ex) {
            android.util.Log.e("UT99Android", "v36 failed to append UWindow config", ex);
        }
    }

    private void applyUt99V40UiSafeInputConfig() {
        // UT99_ANDROID_UI_SAFE_INPUT_V40
        java.io.File root = getUt99ConfigRootV63();
        if (root == null) {
            android.util.Log.e("UT99Android", "v40 could not get external files dir");
            return;
        }
        java.io.File systemDir = new java.io.File(root, "System");
        if (!systemDir.exists() && !systemDir.mkdirs()) {
            android.util.Log.e("UT99Android", "v40 could not create System dir: " + systemDir.getAbsolutePath());
            return;
        }

        String inputBlock =
                "\n" +
                "; UT99_ANDROID_UI_SAFE_INPUT_V40 - safe UI scale + explicit gameplay binds\n" +
                "[UWindow.UWindowRootWindow]\n" +
                "GUIScale=1.000000\n" +
                "LookAndFeelClass=UMenu.UMenuBlueLookAndFeel\n" +
                "\n" +
                "[UMenu.UMenuRootWindow]\n" +
                "GUIScale=1.000000\n" +
                "LookAndFeelClass=UMenu.UMenuBlueLookAndFeel\n" +
                "\n" +
                "[Engine.Input]\n" +
                "W=MoveForward\n" +
                "S=MoveBackward\n" +
                "A=StrafeLeft\n" +
                "D=StrafeRight\n" +
                "Space=Jump\n" +
                "C=Duck\n" +
                "Shift=Walking\n" +
                "Q=PrevWeapon\n" +
                "N=NextWeapon\n" +
                "X=Taunt Wave\n" +
                "Joy1=Jump\n" +
                "Joy2=Duck\n" +
                "Joy3=Taunt Wave\n" +
                "Joy4=Walking\n" +
                "Joy10=PrevWeapon\n" +
                "Joy11=NextWeapon\n" +
                "LeftMouse=Fire\n" +
                "RightMouse=AltFire\n" +
                "MouseX=Axis aMouseX Speed=1.0\n" +
                "MouseY=Axis aMouseY Speed=1.0\n" +
                "\n" +
                "[UMenu.UnrealConsole]\n" +
                "RootWindow=UMenu.UMenuRootWindow\n" +
                "UWindowKey=IK_Esc\n" +
                "ShowDesktop=True\n" +
                "bShowConsole=False\n" +
                "\n" +
                "[UWindow.WindowConsole]\n" +
                "RootWindow=UMenu.UMenuRootWindow\n" +
                "UWindowKey=IK_Esc\n" +
                "ShowDesktop=True\n" +
                "bShowConsole=False\n" +
                "\n" +
                "[UTMenu.UTConsole]\n" +
                "RootWindow=UMenu.UMenuRootWindow\n" +
                "UWindowKey=IK_Esc\n" +
                "ShowDesktop=True\n" +
                "bShowConsole=False\n";

        appendTextToFileV40(new java.io.File(systemDir, "AndroidUT99.ini"), inputBlock);
        appendTextToFileV40(new java.io.File(systemDir, "AndroidUser.ini"), inputBlock);
        android.util.Log.i("UT99Android", "UT99_ANDROID_UI_SAFE_INPUT_V40 appended safe UI/input config");
    }

    private void appendTextToFileV40(java.io.File file, String text) {
        try {
            java.io.FileWriter fw = new java.io.FileWriter(file, true);
            try {
                fw.write(text);
            } finally {
                fw.close();
            }
        } catch (java.io.IOException ex) {
            android.util.Log.e("UT99Android", "v40 failed to append config to " + file.getAbsolutePath(), ex);
        }
    }

    // UT99_ANDROID_V60_NATIVE_SURFACE_AUTOSCALE:
    // Keep the engine/UI in native display coordinates.  On panels taller than
    // 540px we switch UWindow/UMenu to its built-in DOUBLE GUI scale (2.0),
    // which is the same setting exposed by the in-game Preferences GUI Scale combo.
    private static final int UT99_V60_DOUBLE_FONT_HEIGHT_THRESHOLD = 540;

    private int[] ut99V60ResolveNativeDisplaySize() {
        int bestW = 0;
        int bestH = 0;

        try {
            android.view.View decor = getWindow() != null ? getWindow().getDecorView() : null;
            if (decor != null && decor.getWidth() > 0 && decor.getHeight() > 0) {
                bestW = decor.getWidth();
                bestH = decor.getHeight();
            }
        } catch (Throwable ignored) {
        }

        try {
            android.util.DisplayMetrics dm = new android.util.DisplayMetrics();
            getWindowManager().getDefaultDisplay().getRealMetrics(dm);
            if (dm.widthPixels > 0 && dm.heightPixels > 0 && (dm.widthPixels * dm.heightPixels) > (bestW * bestH)) {
                bestW = dm.widthPixels;
                bestH = dm.heightPixels;
            }
        } catch (Throwable ignored) {
        }

        try {
            android.util.DisplayMetrics dm = getResources().getDisplayMetrics();
            if (dm != null && dm.widthPixels > 0 && dm.heightPixels > 0 && (dm.widthPixels * dm.heightPixels) > (bestW * bestH)) {
                bestW = dm.widthPixels;
                bestH = dm.heightPixels;
            }
        } catch (Throwable ignored) {
        }

        return new int[] { bestW, bestH };
    }

    private void applyUt99V65InitialNativeFontScaleConfig(boolean androidIniCreated) {
        // UT99_ANDROID_V65_VIDEO_PREF_LABELS_FONT_PERSIST:
        // Auto-select DOUBLE font only on the very first generated config.  After
        // that the in-game Preferences > Video > Font Size setting owns GUIScale
        // and must not be overwritten by Java on every launch.
        if (!androidIniCreated) {
            android.util.Log.i("UT99Android", "UT99_ANDROID_V65_FONT_PERSIST preserving existing GUIScale settings");
            return;
        }

        java.io.File root = getUt99ConfigRootV63();
        if (root == null) {
            android.util.Log.e("UT99Android", "v65 could not get config root for initial native font scale");
            return;
        }

        int[] displaySize = ut99V60ResolveNativeDisplaySize();
        int displayW = displaySize[0];
        int displayH = displaySize[1];
        int landscapeHeight = 0;
        if (displayW > 0 && displayH > 0) {
            landscapeHeight = Math.min(displayW, displayH);
        } else if (displayH > 0) {
            landscapeHeight = displayH;
        }

        String guiScale = landscapeHeight > UT99_V60_DOUBLE_FONT_HEIGHT_THRESHOLD ? "2.000000" : "1.000000";

        java.io.File systemDir = new java.io.File(root, "System");
        if (!systemDir.exists() && !systemDir.mkdirs()) {
            android.util.Log.e("UT99Android", "v65 could not create System dir: " + systemDir.getAbsolutePath());
            return;
        }

        java.io.File androidIni = new java.io.File(systemDir, "AndroidUT99.ini");
        java.io.File userIni = new java.io.File(systemDir, "AndroidUser.ini");
        ut99V60UpsertGuiScale(androidIni, guiScale);
        ut99V60UpsertGuiScale(userIni, guiScale);

        android.util.Log.i("UT99Android", "UT99_ANDROID_V65_INITIAL_FONT_SCALE display="
                + displayW + "x" + displayH + " landscapeHeight=" + landscapeHeight
                + " initial GUIScale=" + guiScale);
    }


    private void ut99V60UpsertGuiScale(java.io.File ini, String guiScale) {
        if (ini == null) return;

        try {
            String text = ini.exists() ? ut99V60ReadUtf8(ini) : "";
            text = ut99V60UpsertKey(text, "UWindow.UWindowRootWindow", "GUIScale", guiScale);
            text = ut99V60UpsertKey(text, "UMenu.UMenuRootWindow", "GUIScale", guiScale);
            ut99V60WriteUtf8(ini, text);
        } catch (Throwable t) {
            android.util.Log.e("UT99Android", "v60 failed to upsert GUI scale in " + ini.getAbsolutePath(), t);
        }
    }

    private String ut99V60UpsertKey(String text, String section, String key, String value) {
        if (text == null) text = "";
        String[] lines = text.split("\\r?\\n", -1);
        StringBuilder out = new StringBuilder(text.length() + 128);
        boolean inSection = false;
        boolean sectionFound = false;
        boolean keyWritten = false;
        String sectionHeader = "[" + section + "]";
        String keyPrefix = key + "=";

        for (int i = 0; i < lines.length; i++) {
            String line = lines[i];
            String trimmed = line.trim();
            boolean isHeader = trimmed.startsWith("[") && trimmed.endsWith("]");

            if (isHeader) {
                if (inSection && !keyWritten) {
                    out.append(keyPrefix).append(value).append('\n');
                    keyWritten = true;
                }
                inSection = trimmed.equalsIgnoreCase(sectionHeader);
                if (inSection) {
                    sectionFound = true;
                    keyWritten = false;
                }
            } else if (inSection && trimmed.toLowerCase(java.util.Locale.US).startsWith(keyPrefix.toLowerCase(java.util.Locale.US))) {
                if (!keyWritten) {
                    out.append(keyPrefix).append(value).append('\n');
                    keyWritten = true;
                }
                continue;
            }

            out.append(line);
            if (i < lines.length - 1) {
                out.append('\n');
            }
        }

        if (inSection && !keyWritten) {
            if (out.length() > 0 && out.charAt(out.length() - 1) != '\n') out.append('\n');
            out.append(keyPrefix).append(value).append('\n');
        }

        if (!sectionFound) {
            if (out.length() > 0 && out.charAt(out.length() - 1) != '\n') out.append('\n');
            out.append('\n').append(sectionHeader).append('\n').append(keyPrefix).append(value).append('\n');
        }

        return out.toString();
    }

    private String ut99V60ReadUtf8(java.io.File file) throws java.io.IOException {
        java.io.ByteArrayOutputStream out = new java.io.ByteArrayOutputStream();
        java.io.FileInputStream in = new java.io.FileInputStream(file);
        try {
            byte[] buf = new byte[8192];
            int n;
            while ((n = in.read(buf)) >= 0) {
                out.write(buf, 0, n);
            }
        } finally {
            in.close();
        }
        return out.toString("UTF-8");
    }

    private void ut99V60WriteUtf8(java.io.File file, String text) throws java.io.IOException {
        java.io.File parent = file.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            throw new java.io.IOException("Cannot create folder: " + parent.getAbsolutePath());
        }
        java.io.FileOutputStream out = new java.io.FileOutputStream(file, false);
        try {
            out.write(text.getBytes("UTF-8"));
        } finally {
            out.close();
        }
    }

    // UT99_ANDROID_V166_REAL_RENDER_RESOLUTIONS
    private static final String UT99_V166_RES_SECTION = "NSDLDrv.NSDLClient";
    private static final String UT99_V166_RES_KEY = "AndroidResolutionMode";

    private String ut99V166NormalizeResolutionMode(String raw) {
        if (raw == null) return "Native";
        String value = raw.trim();
        if (value.length() == 0) return "Native";
        String lower = value.toLowerCase(java.util.Locale.US).replace('\u00a0', ' ');
        if (lower.equals("native") || lower.equals("100") || lower.equals("100%")) return "Native";
        // Old percentage modes are deliberately retired in v166.
        if (lower.contains("75") || lower.contains("50")) return "Native";
        int xPos = Math.max(value.indexOf('x'), value.indexOf('X'));
        if (xPos > 0) {
            try {
                int w = Integer.parseInt(value.substring(0, xPos).trim());
                int h = Integer.parseInt(value.substring(xPos + 1).trim());
                if (w >= 320 && h >= 240 && w <= 4096 && h <= 2160) {
                    w = w & ~1;
                    h = h & ~1;
                    return w + "x" + h;
                }
            } catch (Throwable ignored) {
            }
        }
        return "Native";
    }

    private int[] ut99V166ParseResolutionMode(String mode) {
        String normalized = ut99V166NormalizeResolutionMode(mode);
        if ("Native".equals(normalized)) return null;
        int xPos = normalized.indexOf('x');
        if (xPos <= 0) return null;
        try {
            int w = Integer.parseInt(normalized.substring(0, xPos));
            int h = Integer.parseInt(normalized.substring(xPos + 1));
            if (w >= 320 && h >= 240) return new int[] { w, h };
        } catch (Throwable ignored) {
        }
        return null;
    }

    private String ut99V166ReadResolutionMode() {
        // UT99_ANDROID_V166C_RESOLUTION_PERSIST:
        // Keep the user's selected render resolution in Android SharedPreferences as
        // the authoritative fast path.  The INI entry is still mirrored for native
        // Preferences/GetCurrentRes, but UE1 may rewrite INI files during shutdown.
        try {
            android.content.SharedPreferences prefs = getSharedPreferences(UT99_V166_PREFS, MODE_PRIVATE);
            if (prefs != null && prefs.contains(UT99_V166_PREF_KEY_MODE)) {
                return ut99V166NormalizeResolutionMode(prefs.getString(UT99_V166_PREF_KEY_MODE, "Native"));
            }
        } catch (Throwable t) {
            android.util.Log.w("UT99Android", "v166c could not read resolution mode prefs", t);
        }

        java.io.File root = getUt99ConfigRootV63();
        if (root == null) return "Native";
        java.io.File systemDir = new java.io.File(root, "System");
        java.io.File[] candidates = new java.io.File[] {
                new java.io.File(systemDir, "AndroidUT99.ini"),
                new java.io.File(systemDir, "AndroidUser.ini")
        };

        for (java.io.File ini : candidates) {
            try {
                if (ini == null || !ini.exists()) continue;
                String text = ut99V60ReadUtf8(ini);
                String[] lines = text.split("\\r?\\n", -1);
                for (String line : lines) {
                    String trimmed = line != null ? line.trim() : "";
                    if (trimmed.toLowerCase(java.util.Locale.US).startsWith((UT99_V166_RES_KEY + "=").toLowerCase(java.util.Locale.US))) {
                        return ut99V166NormalizeResolutionMode(trimmed.substring(trimmed.indexOf('=') + 1));
                    }
                }
            } catch (Throwable t) {
                android.util.Log.w("UT99Android", "v166 could not read resolution mode from " + ini.getAbsolutePath(), t);
            }
        }
        return "Native";
    }

    private void ut99V166WriteResolutionModePrefs(String mode) {
        try {
            mode = ut99V166NormalizeResolutionMode(mode);
            android.content.SharedPreferences.Editor editor = getSharedPreferences(UT99_V166_PREFS, MODE_PRIVATE).edit();
            editor.putString(UT99_V166_PREF_KEY_MODE, mode);
            editor.apply();
            android.util.Log.i("UT99Android", "UT99_ANDROID_V166C_RESOLUTION_PERSIST prefs mode=" + mode);
        } catch (Throwable t) {
            android.util.Log.e("UT99Android", "v166c failed to persist resolution mode prefs", t);
        }
    }

    private void ut99V166EnsureResolutionModeConfig() {
        java.io.File root = getUt99ConfigRootV63();
        if (root == null) return;
        java.io.File systemDir = new java.io.File(root, "System");
        if (!systemDir.exists() && !systemDir.mkdirs()) {
            android.util.Log.e("UT99Android", "v166 could not create System dir: " + systemDir.getAbsolutePath());
            return;
        }
        java.io.File androidIni = new java.io.File(systemDir, "AndroidUT99.ini");
        ut99V166WriteResolutionModePrefs(ut99V166ResolutionMode);
        ut99V166WriteResolutionModeConfig(androidIni, ut99V166ResolutionMode, false);
    }

    private void ut99V166WriteResolutionModeConfig(java.io.File ini, String mode, boolean overwrite) {
        if (ini == null) return;
        mode = ut99V166NormalizeResolutionMode(mode);
        try {
            String text = ini.exists() ? ut99V60ReadUtf8(ini) : "";
            boolean hasKey = text.toLowerCase(java.util.Locale.US).contains((UT99_V166_RES_KEY + "=").toLowerCase(java.util.Locale.US));
            if (!hasKey || overwrite) {
                text = ut99V60UpsertKey(text, UT99_V166_RES_SECTION, UT99_V166_RES_KEY, mode);
                ut99V60WriteUtf8(ini, text);
            }
        } catch (Throwable t) {
            android.util.Log.e("UT99Android", "v166 failed to write resolution mode to " + ini.getAbsolutePath(), t);
        }
    }

    private void ut99V64ApplyResolutionScaleToSurface(android.view.SurfaceView sv) {
        // Method name kept because older v55/v56 scheduling calls it.
        if (sv == null) return;
        String mode = ut99V166NormalizeResolutionMode(ut99V166ResolutionMode);
        int[] fixed = ut99V166ParseResolutionMode(mode);

        android.view.SurfaceHolder holder = sv.getHolder();
        if (holder == null) return;

        if (fixed == null) {
            holder.setSizeFromLayout();
            android.util.Log.i("UT99Android", "UT99_ANDROID_V166_REAL_RENDER_RESOLUTIONS SurfaceHolder Native layout size on " + sv.getClass().getName());
            return;
        }

        holder.setFixedSize(fixed[0], fixed[1]);
        android.util.Log.i("UT99Android", "UT99_ANDROID_V166_REAL_RENDER_RESOLUTIONS SurfaceHolder fixed render="
                + fixed[0] + "x" + fixed[1] + " fullscreen view on " + sv.getClass().getName());
    }

    private void ut99V166ApplyResolutionMode(String mode, boolean persist) {
        ut99V166ResolutionMode = ut99V166NormalizeResolutionMode(mode);
        if (persist) {
            ut99V166WriteResolutionModePrefs(ut99V166ResolutionMode);
            java.io.File root = getUt99ConfigRootV63();
            if (root != null) {
                java.io.File systemDir = new java.io.File(root, "System");
                if (!systemDir.exists()) systemDir.mkdirs();
                ut99V166WriteResolutionModeConfig(new java.io.File(systemDir, "AndroidUT99.ini"), ut99V166ResolutionMode, true);
            }
        }

        ut99V56SurfaceFixedOnce = false;
        ut99V55ApplyFixedSurface();
        ut99V55ScheduleFixedSurface();
        android.util.Log.i("UT99Android", "UT99_ANDROID_V166_REAL_RENDER_RESOLUTIONS applied mode=" + ut99V166ResolutionMode);
    }

    // UT99_ANDROID_IMMERSIVE_V44: keep the visible surface stable on Android handhelds.
    private void ut99HideSystemUiV44() {
        try {
            getWindow().setFlags(android.view.WindowManager.LayoutParams.FLAG_FULLSCREEN,
                    android.view.WindowManager.LayoutParams.FLAG_FULLSCREEN);
            android.view.View decor = getWindow().getDecorView();
            int flags = android.view.View.SYSTEM_UI_FLAG_FULLSCREEN
                    | android.view.View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | android.view.View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | android.view.View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | android.view.View.SYSTEM_UI_FLAG_LAYOUT_STABLE;
            if (android.os.Build.VERSION.SDK_INT >= 19) {
                flags |= android.view.View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
            }
            decor.setSystemUiVisibility(flags);
        } catch (Throwable ignored) {
        }
    }


    private void applyUt99V45SafeAreaLookLogoConfig() {
        // UT99_ANDROID_SAFEAREA_LOOK_LOGO_V45
        java.io.File root = getUt99ConfigRootV63();
        if (root == null) {
            android.util.Log.e("UT99Android", "v45 could not get external files dir");
            return;
        }
        java.io.File systemDir = new java.io.File(root, "System");
        if (!systemDir.exists() && !systemDir.mkdirs()) {
            android.util.Log.e("UT99Android", "v45 could not create System dir: " + systemDir.getAbsolutePath());
            return;
        }
        String block =
                "\n" +
                "; UT99_ANDROID_SAFEAREA_LOOK_LOGO_V45\n" +
                "; Hide oversized tiled UMenu desktop logo on Android handhelds.\n" +
                "[UWindow.WindowConsole]\n" +
                "ShowDesktop=False\n" +
                "[UMenu.UnrealConsole]\n" +
                "ShowDesktop=False\n" +
                "[UTMenu.UTConsole]\n" +
                "ShowDesktop=False\n" +
                "[Engine.Input]\n" +
                "MouseX=Axis aMouseX Speed=2.8\n" +
                "MouseY=Axis aMouseY Speed=2.2\n";
        appendTextToFileV45(new java.io.File(systemDir, "AndroidUT99.ini"), block);
        appendTextToFileV45(new java.io.File(systemDir, "AndroidUser.ini"), block);
        android.util.Log.i("UT99Android", "UT99_ANDROID_SAFEAREA_LOOK_LOGO_V45 appended config / UT99_ANDROID_V50_SOUND_ATTEMPT");
    }

    private void appendTextToFileV45(java.io.File file, String text) {
        try {
            java.io.FileWriter fw = new java.io.FileWriter(file, true);
            try { fw.write(text); } finally { fw.close(); }
        } catch (java.io.IOException ex) {
            android.util.Log.e("UT99Android", "v45 failed to append config to " + file.getAbsolutePath(), ex);
        }
    }


    // UT99_ANDROID_NATIVE_INPUT_V47
    private static native void nativeAndroidButtonV47(int keyCode, boolean down);
    private static native void nativeAndroidAxisV47(int axis, float value);
    private static native void nativeAndroidTouchLookV101(float x, float y);
    private static native int nativeAndroidInputResetSerialRT();

    // RetroTouch Beta 4 host bridge. Keep all game-specific mappings in Java and
    // feed the same native controller/touch path already used by UT99.
    void ut99RetroTouchButton(int keyCode, boolean down) {
        nativeAndroidButtonV47(keyCode, down);
    }

    void ut99RetroTouchMove(float x, float y) {
        nativeAndroidAxisV47(android.view.MotionEvent.AXIS_X, x);
        nativeAndroidAxisV47(android.view.MotionEvent.AXIS_Y, y);
    }

    void ut99RetroTouchLook(float x, float y) {
        nativeAndroidTouchLookV101(x, y);
    }

    boolean ut99RetroTouchMenuVisible() {
        try {
            return nativeAndroidIsMenuV92();
        } catch (Throwable t) {
            return true;
        }
    }

    int ut99RetroTouchUiState() {
        // Native state is authoritative because Java cannot reliably distinguish
        // CityIntro/flyby from a live map merely from SDL Activity lifecycle state.
        // 0 = pass-through/intro/loading, 1 = UWindow menu, 2 = live gameplay.
        try {
            return nativeAndroidTouchUiStateRT();
        } catch (Throwable t) {
            return 0;
        }
    }

    boolean ut99RetroTouchHasTouchscreen() {
        try {
            if (isAutomotiveDevice()) return true;
            android.content.pm.PackageManager pm = getPackageManager();
            return pm.hasSystemFeature(android.content.pm.PackageManager.FEATURE_TOUCHSCREEN);
        } catch (Throwable t) {
            return true;
        }
    }

    boolean ut99RetroTouchHasDesktopInput() {
        // UT99_ANDROID_CHROMEOS_INPUT_MODE_V212:
        // RetroTouch is for a genuinely touch-only device. ChromeOS commonly
        // exposes a mouse as MOUSE|TOUCHSCREEN, so FEATURE_TOUCHSCREEN alone is
        // not sufficient. On non-ChromeOS Android, only externally attached
        // keyboard/mouse devices count. Internal gpio/uinput devices on Retroid
        // and similar handhelds often claim keyboard or mouse capabilities even
        // though they are neither usable desktop input nor an active controller.
        try {
            if (ut99IsChromeOSV211()) {
                return ut99DesktopInputResultV211(1, null);
            }

            int[] ids = android.view.InputDevice.getDeviceIds();
            for (int id : ids) {
                android.view.InputDevice device = android.view.InputDevice.getDevice(id);
                if (device == null || device.isVirtual()) continue;
                if (ut99IsSoftwareVirtualDesktopDeviceV212(device)) continue;
                if (!ut99IsExternalInputDeviceV211(device)) continue;

                final int sources = device.getSources();
                final boolean controllerClass =
                        (sources & android.view.InputDevice.SOURCE_GAMEPAD)
                                == android.view.InputDevice.SOURCE_GAMEPAD
                                || (sources & android.view.InputDevice.SOURCE_JOYSTICK)
                                == android.view.InputDevice.SOURCE_JOYSTICK;
                if (controllerClass) continue;

                final boolean alphabeticKeyboard =
                        device.getKeyboardType() == android.view.InputDevice.KEYBOARD_TYPE_ALPHABETIC
                                && (sources & android.view.InputDevice.SOURCE_KEYBOARD)
                                == android.view.InputDevice.SOURCE_KEYBOARD;
                final boolean mouse =
                        (sources & android.view.InputDevice.SOURCE_MOUSE)
                                == android.view.InputDevice.SOURCE_MOUSE
                                || (android.os.Build.VERSION.SDK_INT >= 26
                                && (sources & android.view.InputDevice.SOURCE_MOUSE_RELATIVE)
                                == android.view.InputDevice.SOURCE_MOUSE_RELATIVE);
                if (alphabeticKeyboard) return ut99DesktopInputResultV211(2, device);
                if (mouse) return ut99DesktopInputResultV211(3, device);
            }
            return ut99DesktopInputResultV211(0, null);
        } catch (Throwable t) {
            // A real keyboard/mouse event still suppresses RetroTouch immediately
            // through onHardwareDesktopInput(). Do not permanently hide touch
            // controls merely because one vendor InputDevice is malformed.
            Log.w(TAG, "desktop input inventory unavailable", t);
            return ut99DesktopInputResultV211(-1, null);
        }
    }

    private boolean ut99IsChromeOSV211() {
        try {
            return getPackageManager().hasSystemFeature("org.chromium.arc.device_management");
        } catch (Throwable ignored) {
            return false;
        }
    }

    private static boolean ut99IsExternalInputDeviceV211(android.view.InputDevice device) {
        if (device == null || android.os.Build.VERSION.SDK_INT < 29) return false;
        try {
            return Ut99Api29InputDevice.isExternal(device);
        } catch (Throwable ignored) {
            return false;
        }
    }

    private static boolean ut99IsSoftwareVirtualDesktopDeviceV212(android.view.InputDevice device) {
        if (device == null) return false;
        try {
            String name = device.getName();
            if (name == null) return false;
            String normalized = name.toLowerCase(java.util.Locale.US);
            // Some handheld firmware keeps software cursor devices registered
            // permanently and even reports them as external/non-virtual. They
            // are not evidence of a connected physical mouse or keyboard.
            return normalized.contains("virtual mouse")
                    || normalized.contains("virtual keyboard")
                    || normalized.contains("virtual pointer");
        } catch (Throwable ignored) {
            return false;
        }
    }

    private boolean ut99DesktopInputResultV211(int reason, android.view.InputDevice device) {
        if (reason != ut99V212LastDesktopInputReason) {
            ut99V212LastDesktopInputReason = reason;
            String detail;
            if (reason == 1) {
                detail = "chromeos-arc";
            } else if (reason == 2) {
                detail = "external-keyboard";
            } else if (reason == 3) {
                detail = "external-mouse";
            } else if (reason < 0) {
                detail = "inventory-error";
            } else {
                detail = "none";
            }
            if (device != null) {
                detail += " name=" + device.getName()
                        + " id=" + device.getId()
                        + " sources=0x" + Integer.toHexString(device.getSources());
            }
            Log.i(TAG, "UT99_ANDROID_DESKTOP_INPUT_V212 reason=" + detail
                    + " present=" + (reason > 0));
        }
        return reason > 0;
    }

    int ut99RetroTouchInputResetSerial() {
        try {
            return nativeAndroidInputResetSerialRT();
        } catch (Throwable t) {
            return 0;
        }
    }

    boolean ut99RetroTouchPreferenceEnabled() {
        return ut99V91ReadTouchOverlayEnabled();
    }

    private static boolean isAndroidGamepadSourceV47(android.view.InputEvent event) {
        int source = event.getSource();
        return ((source & android.view.InputDevice.SOURCE_GAMEPAD) == android.view.InputDevice.SOURCE_GAMEPAD)
                || ((source & android.view.InputDevice.SOURCE_JOYSTICK) == android.view.InputDevice.SOURCE_JOYSTICK)
                || ((source & android.view.InputDevice.SOURCE_DPAD) == android.view.InputDevice.SOURCE_DPAD);
    }

    private boolean isPhysicalKeyboardEventV210(android.view.KeyEvent event) {
        if (event == null) return false;
        try {
            android.view.InputDevice device = event.getDevice();
            if (device == null) return false;
            final boolean chromeOS = ut99IsChromeOSV211();
            if (!chromeOS && device.isVirtual()) return false;
            // UT99_ANDROID_CHROMEOS_VIRTUAL_HID_BYPASS_V213:
            // ARC may expose the real Chromebook keyboard through a device name
            // containing "Virtual Keyboard".  That name is only a false-positive
            // filter for handheld firmware outside ChromeOS.
            if (!chromeOS
                    && ut99IsSoftwareVirtualDesktopDeviceV212(device)) return false;
            final int sources = device.getSources();
            final boolean controllerClass =
                    (sources & android.view.InputDevice.SOURCE_GAMEPAD)
                            == android.view.InputDevice.SOURCE_GAMEPAD
                            || (sources & android.view.InputDevice.SOURCE_JOYSTICK)
                            == android.view.InputDevice.SOURCE_JOYSTICK;
            final boolean desktopKeyboardDevice = chromeOS
                    || android.os.Build.VERSION.SDK_INT < 29
                    || ut99IsExternalInputDeviceV211(device);
            return !controllerClass && desktopKeyboardDevice
                    && device.getKeyboardType() == android.view.InputDevice.KEYBOARD_TYPE_ALPHABETIC
                    && (event.getSource() & android.view.InputDevice.SOURCE_KEYBOARD)
                    == android.view.InputDevice.SOURCE_KEYBOARD;
        } catch (Throwable ignored) {
            return false;
        }
    }

    private boolean isPhysicalMouseEventV210(android.view.MotionEvent event) {
        if (event == null) return false;
        try {
            // UT99_ANDROID_CHROMEOS_VIRTUAL_HID_BYPASS_V213:
            // ChromeOS/ARC frequently presents a physical mouse or trackpad as a
            // "Virtual Mouse".  Filtering it here drops hover/relative motion but
            // still lets a separate click path through, producing an invisible
            // cursor that flashes only when clicked.  Keep the Retroid software
            // cursor safeguard on Android handhelds, never on confirmed ARC.
            if (!ut99IsChromeOSV211()
                    && ut99IsSoftwareVirtualDesktopDeviceV212(event.getDevice())) return false;
        } catch (Throwable ignored) {
        }
        final int source = event.getSource();
        if ((source & android.view.InputDevice.SOURCE_MOUSE)
                == android.view.InputDevice.SOURCE_MOUSE) return true;
        if (android.os.Build.VERSION.SDK_INT >= 26
                && (source & android.view.InputDevice.SOURCE_MOUSE_RELATIVE)
                == android.view.InputDevice.SOURCE_MOUSE_RELATIVE) return true;
        // UT99_ANDROID_CHROMEOS_TOUCHPAD_SOURCE_V214:
        // ARC can keep SOURCE_TOUCHPAD (or combine it with SOURCE_MOUSE) on
        // generic-motion events instead of presenting an exact SOURCE_MOUSE.
        if (ut99IsChromeOSV211()
                && (source & android.view.InputDevice.SOURCE_TOUCHPAD)
                == android.view.InputDevice.SOURCE_TOUCHPAD) return true;
        if (ut99IsChromeOSV211()
                && !isAndroidGamepadSourceV47(event)
                && (((source & android.view.InputDevice.SOURCE_CLASS_POINTER)
                == android.view.InputDevice.SOURCE_CLASS_POINTER)
                || ((source & android.view.InputDevice.SOURCE_CLASS_POSITION)
                == android.view.InputDevice.SOURCE_CLASS_POSITION))) return true;
        try {
            return event.getPointerCount() > 0
                    && event.getToolType(0) == android.view.MotionEvent.TOOL_TYPE_MOUSE;
        } catch (Throwable ignored) {
            return false;
        }
    }

    private static boolean ut99HasPointerCaptureV210() {
        if (android.os.Build.VERSION.SDK_INT < 26) return false;
        try {
            android.view.View content = SDLActivity.getContentView();
            return content != null && Ut99Api26View.hasPointerCapture(content);
        } catch (Throwable ignored) {
            return false;
        }
    }

    private float ut99MapChromeOSAbsoluteMouseXV218(float x) {
        if (!ut99IsChromeOSV211()) return x;
        android.view.View content = SDLActivity.getContentView();
        if (content instanceof org.libsdl.app.SDLSurface) {
            return ((org.libsdl.app.SDLSurface) content)
                    .ut99MapChromeOSAbsoluteMouseXV218(x);
        }
        return x;
    }

    private float ut99MapChromeOSAbsoluteMouseYV218(float y) {
        if (!ut99IsChromeOSV211()) return y;
        android.view.View content = SDLActivity.getContentView();
        if (content instanceof org.libsdl.app.SDLSurface) {
            return ((org.libsdl.app.SDLSurface) content)
                    .ut99MapChromeOSAbsoluteMouseYV218(y);
        }
        return y;
    }

    @android.annotation.TargetApi(26)
    private static final class Ut99Api26View {
        private Ut99Api26View() {}
        static boolean hasPointerCapture(android.view.View view) {
            return view.hasPointerCapture();
        }
    }

    @android.annotation.TargetApi(29)
    private static final class Ut99Api29InputDevice {
        private Ut99Api29InputDevice() {}
        static boolean isExternal(android.view.InputDevice device) {
            return device.isExternal();
        }
    }

    private static boolean isOuyaMenuKeyV79(int keyCode) {
        // OUYA's center/system button is reported as KEYCODE_MENU on Android 4.1.2.
        return keyCode == android.view.KeyEvent.KEYCODE_MENU
                || keyCode == android.view.KeyEvent.KEYCODE_BUTTON_MODE;
    }

    private static boolean isOuyaDeviceV108() {
        // UT99_ANDROID_V108_OUYA_INPUT_REPAIR:
        // Keep the fixes strictly scoped to OUYA/Android 4 console-class devices
        // so Retroid/modern Android controller and touch behaviour stays unchanged.
        final String model = android.os.Build.MODEL != null ? android.os.Build.MODEL.toLowerCase(java.util.Locale.US) : "";
        final String maker = android.os.Build.MANUFACTURER != null ? android.os.Build.MANUFACTURER.toLowerCase(java.util.Locale.US) : "";
        final String device = android.os.Build.DEVICE != null ? android.os.Build.DEVICE.toLowerCase(java.util.Locale.US) : "";
        return model.contains("ouya") || maker.contains("ouya") || device.contains("ouya");
    }

    private static float firstActiveAxisV108(android.view.MotionEvent event, int primary, int fallback) {
        float value = event.getAxisValue(primary);
        if (Math.abs(value) < 0.01f) {
            float fallbackValue = event.getAxisValue(fallback);
            if (Math.abs(fallbackValue) > Math.abs(value)) value = fallbackValue;
        }
        return value;
    }

    private float applyDeadzoneV47(float value, float deadzone) {
        return Math.abs(value) >= deadzone ? value : 0.0f;
    }

    @Override
    public boolean dispatchKeyEvent(android.view.KeyEvent event) {
        final int action = event.getAction();
        final int keyCode = event.getKeyCode();
        final boolean physicalKeyboard = isPhysicalKeyboardEventV210(event);
        boolean textDeleteKey = !physicalKeyboard
                && (keyCode == android.view.KeyEvent.KEYCODE_DEL
                || keyCode == android.view.KeyEvent.KEYCODE_FORWARD_DEL);
        if (ut99RetroTouchBridge != null && physicalKeyboard) {
            ut99RetroTouchBridge.onHardwareDesktopInput();
        }
        if (!sUt99V72LoggedActive) {
            sUt99V72LoggedActive = true;
            android.util.Log.i("UT99Android", "UT99_ANDROID_V72_ACTIVE GameActivity single START toggle path loaded");
        }
        if (isAndroidGamepadSourceV47(event) || isOuyaMenuKeyV79(keyCode) || textDeleteKey) {
            if (ut99RetroTouchBridge != null && isAndroidGamepadSourceV47(event)) {
                ut99RetroTouchBridge.onHardwareControllerInput();
            }
            if (action == android.view.KeyEvent.ACTION_DOWN || action == android.view.KeyEvent.ACTION_UP) {
                if (isOuyaDeviceV108()
                        && (keyCode == android.view.KeyEvent.KEYCODE_DPAD_UP
                        || keyCode == android.view.KeyEvent.KEYCODE_DPAD_DOWN
                        || keyCode == android.view.KeyEvent.KEYCODE_DPAD_LEFT
                        || keyCode == android.view.KeyEvent.KEYCODE_DPAD_RIGHT)) {
                    // OUYA sends D-Pad through both Android key events and SDL.
                    // Let SDL own it to avoid double menu steps.
                    android.util.Log.i("UT99Android", "UT99_ANDROID_V108_OUYA_DPAD pass-through key=" + keyCode);
                    return super.dispatchKeyEvent(event);
                }
                if (action == android.view.KeyEvent.ACTION_DOWN && event.getRepeatCount() > 0
                        && (keyCode == android.view.KeyEvent.KEYCODE_BUTTON_START
                        || keyCode == android.view.KeyEvent.KEYCODE_MENU
                        || keyCode == android.view.KeyEvent.KEYCODE_BUTTON_MODE)) {
                    android.util.Log.i("UT99Android", "UT99_ANDROID_V72_KEY ignored repeated START/MENU key=" + keyCode);
                    return true;
                }
                nativeAndroidButtonV47(keyCode, action == android.view.KeyEvent.ACTION_DOWN);
                android.util.Log.i("UT99Android", "UT99_ANDROID_V72_KEY key=" + keyCode + " down=" + (action == android.view.KeyEvent.ACTION_DOWN));
                return true;
            }
        }
        return super.dispatchKeyEvent(event);
    }

    @Override
    public boolean dispatchGenericMotionEvent(android.view.MotionEvent event) {
        // UT99_ANDROID_CHROMEOS_MOUSE_ACTIVITY_ROUTE_V210:
        // ChromeOS may deliver mouse hover, capture and button events to the
        // Activity/DecorView instead of the SDL Surface's generic-motion
        // listener. Route real mouse input into SDL exactly once and keep
        // MOUSE|TOUCHSCREEN devices out of RetroTouch's touch path.
        if (isPhysicalMouseEventV210(event)) {
            if (ut99RetroTouchBridge != null) {
                ut99RetroTouchBridge.onHardwareDesktopInput();
            }

            final int action = event.getActionMasked();
            final int source = event.getSource();
            final boolean relative = ut99HasPointerCaptureV210()
                    || (android.os.Build.VERSION.SDK_INT >= 26
                    && (source & android.view.InputDevice.SOURCE_MOUSE_RELATIVE)
                    == android.view.InputDevice.SOURCE_MOUSE_RELATIVE);

            switch (action) {
                case android.view.MotionEvent.ACTION_HOVER_MOVE:
                case android.view.MotionEvent.ACTION_MOVE: {
                    float x = event.getX(0);
                    float y = event.getY(0);
                    if (relative && android.os.Build.VERSION.SDK_INT >= 26) {
                        float relativeX = event.getAxisValue(android.view.MotionEvent.AXIS_RELATIVE_X, 0);
                        float relativeY = event.getAxisValue(android.view.MotionEvent.AXIS_RELATIVE_Y, 0);
                        if (relativeX != 0.0f || relativeY != 0.0f) {
                            x = relativeX;
                            y = relativeY;
                        }
                    } else {
                        x = ut99MapChromeOSAbsoluteMouseXV218(x);
                        y = ut99MapChromeOSAbsoluteMouseYV218(y);
                    }
                    SDLActivity.onNativeMouse(0, action, x, y, relative);
                    return true;
                }

                case android.view.MotionEvent.ACTION_BUTTON_PRESS:
                case android.view.MotionEvent.ACTION_BUTTON_RELEASE:
                case android.view.MotionEvent.ACTION_DOWN:
                case android.view.MotionEvent.ACTION_UP: {
                    final int nativeAction =
                            (action == android.view.MotionEvent.ACTION_BUTTON_PRESS
                                    || action == android.view.MotionEvent.ACTION_DOWN)
                                    ? android.view.MotionEvent.ACTION_DOWN
                                    : android.view.MotionEvent.ACTION_UP;
                    final float rawX = event.getX(0);
                    final float rawY = event.getY(0);
                    final float mappedX = relative
                            ? rawX : ut99MapChromeOSAbsoluteMouseXV218(rawX);
                    final float mappedY = relative
                            ? rawY : ut99MapChromeOSAbsoluteMouseYV218(rawY);
                    SDLActivity.onNativeMouse(event.getButtonState(), nativeAction,
                            mappedX, mappedY, relative);
                    return true;
                }

                case android.view.MotionEvent.ACTION_SCROLL:
                    SDLActivity.onNativeMouse(0, action,
                            event.getAxisValue(android.view.MotionEvent.AXIS_HSCROLL, 0),
                            event.getAxisValue(android.view.MotionEvent.AXIS_VSCROLL, 0), false);
                    return true;

                default:
                    break;
            }
        }
        return super.dispatchGenericMotionEvent(event);
    }

    @Override
    public boolean onGenericMotionEvent(android.view.MotionEvent event) {
        if (isAndroidGamepadSourceV47(event) && event.getAction() == android.view.MotionEvent.ACTION_MOVE) {
            if (ut99RetroTouchBridge != null) {
                ut99RetroTouchBridge.onHardwareControllerInput();
            }
            final boolean ouya = isOuyaDeviceV108();
            float lx = event.getAxisValue(android.view.MotionEvent.AXIS_X);
            float ly = event.getAxisValue(android.view.MotionEvent.AXIS_Y);
            float rx = ouya
                    ? firstActiveAxisV108(event, android.view.MotionEvent.AXIS_Z, android.view.MotionEvent.AXIS_RX)
                    : event.getAxisValue(android.view.MotionEvent.AXIS_Z);
            float ry = ouya
                    ? firstActiveAxisV108(event, android.view.MotionEvent.AXIS_RZ, android.view.MotionEvent.AXIS_RY)
                    : event.getAxisValue(android.view.MotionEvent.AXIS_RZ);

            // UT99_ANDROID_V109_RETROID_TOUCH_RESTORE:
            // Keep the non-OUYA Android axis path byte-for-byte compatible with
            // the known-good touch/controller behaviour. OUYA-only fallback axes
            // stay scoped to OUYA so Retroid touch-look is not starved.
            nativeAndroidAxisV47(android.view.MotionEvent.AXIS_X, applyDeadzoneV47(lx, 0.12f));
            nativeAndroidAxisV47(android.view.MotionEvent.AXIS_Y, applyDeadzoneV47(ly, 0.12f));
            nativeAndroidAxisV47(android.view.MotionEvent.AXIS_Z, applyDeadzoneV47(rx, ouya ? 0.08f : 0.10f));
            nativeAndroidAxisV47(android.view.MotionEvent.AXIS_RZ, applyDeadzoneV47(ry, ouya ? 0.08f : 0.10f));
            nativeAndroidAxisV47(android.view.MotionEvent.AXIS_LTRIGGER, event.getAxisValue(android.view.MotionEvent.AXIS_LTRIGGER));
            nativeAndroidAxisV47(android.view.MotionEvent.AXIS_RTRIGGER, event.getAxisValue(android.view.MotionEvent.AXIS_RTRIGGER));
            nativeAndroidAxisV47(android.view.MotionEvent.AXIS_HAT_X, event.getAxisValue(android.view.MotionEvent.AXIS_HAT_X));
            nativeAndroidAxisV47(android.view.MotionEvent.AXIS_HAT_Y, event.getAxisValue(android.view.MotionEvent.AXIS_HAT_Y));
            android.util.Log.i("UT99Android", "UT99_ANDROID_V109_AXIS lx=" + lx
                    + " ly=" + ly
                    + " rx=" + rx
                    + " ry=" + ry
                    + " ouya=" + ouya);
            return true;
        }
        return super.onGenericMotionEvent(event);
    }

    // UT99_ANDROID_V50_IMMERSIVE
    private void ut99V50Immersive() {
        if (isAutomotiveDevice()) return;
        try {
            getWindow().setFlags(android.view.WindowManager.LayoutParams.FLAG_FULLSCREEN,
                    android.view.WindowManager.LayoutParams.FLAG_FULLSCREEN);
            android.view.View decor = getWindow().getDecorView();
            int flags = android.view.View.SYSTEM_UI_FLAG_FULLSCREEN
                    | android.view.View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | android.view.View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | android.view.View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | android.view.View.SYSTEM_UI_FLAG_LAYOUT_STABLE;
            if (android.os.Build.VERSION.SDK_INT >= 19) {
                flags |= android.view.View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
            }
            decor.setSystemUiVisibility(flags);
            // UT99_ANDROID_V72_UI_EDIT_FOCUS_KEYBOARD:
            // Do not hide the IME from immersive re-apply. SDL_StopTextInput()
            // is now responsible for closing it when the user taps outside an
            // edit field.
            android.util.Log.i("UT99Android", "UT99_ANDROID_V50_IMMERSIVE");
        } catch (Throwable t) {
            android.util.Log.e("UT99Android", "v50 immersive failed", t);
        }
    }




    // UT99_ANDROID_V52_IMMERSIVE_HARD
    private android.os.Handler ut99V52Handler;

    private void ut99V52HardImmersive() {
        if (isAutomotiveDevice()) return;
        try {
            final android.view.Window w = getWindow();
            w.setFlags(android.view.WindowManager.LayoutParams.FLAG_FULLSCREEN,
                    android.view.WindowManager.LayoutParams.FLAG_FULLSCREEN);
            w.addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            if (android.os.Build.VERSION.SDK_INT >= 28) {
                android.view.WindowManager.LayoutParams lp = w.getAttributes();
                lp.layoutInDisplayCutoutMode = android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
                w.setAttributes(lp);
            }

            final android.view.View decor = w.getDecorView();
            int flags = android.view.View.SYSTEM_UI_FLAG_FULLSCREEN
                    | android.view.View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | android.view.View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                    | android.view.View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | android.view.View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | android.view.View.SYSTEM_UI_FLAG_LAYOUT_STABLE;
            decor.setSystemUiVisibility(flags);

            if (android.os.Build.VERSION.SDK_INT >= 30) {
                try {
                    w.setDecorFitsSystemWindows(false);
                    android.view.WindowInsetsController c = decor.getWindowInsetsController();
                    if (c != null) {
                        c.hide(android.view.WindowInsets.Type.statusBars() | android.view.WindowInsets.Type.navigationBars());
                        c.setSystemBarsBehavior(android.view.WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                    }
                } catch (Throwable ignored) {}
            }

            // UT99_ANDROID_V76_KEYBOARD_START_SAFE:
            // Keep the soft keyboard alive only while native UWindow edit handling requested it.
            ut99V76HideImeUnlessRequested();

            if (java.lang.System.currentTimeMillis() % 1000 < 40) if (java.lang.System.currentTimeMillis() % 1000 < 40) android.util.Log.i("UT99Android", "UT99_ANDROID_V52_IMMERSIVE_HARD"); /* UT99_ANDROID_V54_REDUCE_IMMERSIVE_LOG_SPAM */ /* UT99_ANDROID_V54_REDUCE_IMMERSIVE_LOG_SPAM */
        } catch (Throwable t) {
            android.util.Log.e("UT99Android", "v52 immersive failed", t);
        }
    }

    private void ut99V52ScheduleImmersive() {
        if (isAutomotiveDevice()) return;
        if (ut99V52Handler == null) {
            ut99V52Handler = new android.os.Handler(android.os.Looper.getMainLooper());
        }
        ut99V52HardImmersive();
        final int[] delays = new int[]{50, 150, 350, 750, 1500, 3000};
        for (int d : delays) {
            ut99V52Handler.postDelayed(new Runnable() {
                @Override public void run() { ut99V52HardImmersive(); }
            }, d);
        }
        try {
            getWindow().getDecorView().setOnSystemUiVisibilityChangeListener(
                new android.view.View.OnSystemUiVisibilityChangeListener() {
                    @Override public void onSystemUiVisibilityChange(int visibility) {
                        if (ut99V52Handler != null) {
                            ut99V52Handler.postDelayed(new Runnable() {
                                @Override public void run() { ut99V52HardImmersive(); }
                            }, 80);
                        }
                    }
                }
            );
        } catch (Throwable ignored) {}
    }



    // UT99_ANDROID_AAOS_LIFECYCLE_CLEANUP:
    // Stop Java-side redraw/surface/fullscreen callbacks before SDL enters the
    // background.  SDLActivity remains responsible for pausing the native game
    // thread and audio.  Releasing the overlay first also prevents virtual
    // buttons/axes from remaining latched when AAOS switches away from the game.
    @Override
    protected void onPause() {
        try {
            if (ut99RetroTouchBridge != null) {
                ut99RetroTouchBridge.pause();
            }
        } catch (Throwable t) {
            Log.w(TAG, "RetroTouch pause cleanup failed", t);
        }

        if (isAutomotiveDevice()) {
            try {
                if (ut99V52Handler != null) {
                    ut99V52Handler.removeCallbacksAndMessages(null);
                }
                if (ut99V55Handler != null) {
                    ut99V55Handler.removeCallbacksAndMessages(null);
                }
                Window w = getWindow();
                View decor = w != null ? w.getDecorView() : null;
                if (decor != null) {
                    decor.setOnSystemUiVisibilityChangeListener(null);
                }
            } catch (Throwable t) {
                Log.w(TAG, "AAOS lifecycle: pending UI callback cleanup failed", t);
            }
        }

        super.onPause();
    }

    @Override
    protected void onResume() {
        if (isAutomotiveDevice()) {
            // Let SDL restore the native thread/audio/GL state first on AAOS.
            super.onResume();

            try {
                if (ut99RetroTouchBridge != null) {
                    ut99RetroTouchBridge.start();
                }
            } catch (Throwable t) {
                Log.w(TAG, "RetroTouch resume failed", t);
            }

            ut99V55ScheduleFixedSurface(); // v55 onResume
        } else {
            // Preserve the established phone/handheld/OUYA ordering.
            ut99V55ScheduleFixedSurface(); // v55 onResume
            super.onResume();
            try {
                if (ut99RetroTouchBridge != null) {
                    ut99RetroTouchBridge.start();
                }
            } catch (Throwable t) {
                Log.w(TAG, "RetroTouch resume failed", t);
            }
        }

        ut99V52ScheduleImmersive(); // v52 onResume
        ut99V76HideImeUnlessRequested();
    }


    @Override
    public void onUserInteraction() {
        /* UT99_ANDROID_V56_INPUT_SAFE_FIXED_SURFACE: disabled v55 userInteraction surface reapply */
        super.onUserInteraction();
        ut99V52ScheduleImmersive(); // v52 userInteraction
    }


    // UT99_ANDROID_V60_NATIVE_SURFACE_AUTOSCALE
    private android.os.Handler ut99V55Handler;

    private void ut99V55ApplyFixedSurfaceToView(android.view.View view) {
        if (view == null) return;

        try {
            if (view instanceof android.view.SurfaceView) {
                android.view.SurfaceView sv = (android.view.SurfaceView)view;

                // v60: fullscreen native layout.  Do not request a fixed low-res buffer:
                // that forces Android to render a small backbuffer and stretch it to the panel.
                ut99V59ApplyFullscreenLayoutOnce(sv);

                if (!ut99V56SurfaceFixedOnce) {
                    ut99V64ApplyResolutionScaleToSurface(sv);
                    ut99V56SurfaceFixedOnce = true;
                }

                sv.setKeepScreenOn(true);
                sv.setFocusable(true);
                sv.setFocusableInTouchMode(true);
                try {
                    sv.requestFocus();
                } catch (Throwable ignored) {
                }
            }

            if (view instanceof android.view.ViewGroup) {
                android.view.ViewGroup vg = (android.view.ViewGroup)view;
                for (int i = 0; i < vg.getChildCount(); i++) {
                    ut99V55ApplyFixedSurfaceToView(vg.getChildAt(i));
                }
            }
        } catch (Throwable t) {
            android.util.Log.e("UT99Android", "v60 native-surface layout patch failed", t);
        }
    }





    private void ut99V55ApplyFixedSurface() {
        try {
            android.view.Window w = getWindow();
            if (w != null) {
                android.view.View decor = w.getDecorView();
                ut99V55ApplyFixedSurfaceToView(decor);
            }
        } catch (Throwable t) {
            android.util.Log.e("UT99Android", "v55 fixed-surface apply failed", t);
        }
    }

    private void ut99V55ScheduleFixedSurface() {
        long now = android.os.SystemClock.uptimeMillis();

        if (ut99V56SurfaceFixedOnce && ut99V59FullscreenLayoutOnce && (now - ut99V56LastSurfaceScheduleMs) < 5000L) {
            return;
        }
        ut99V56LastSurfaceScheduleMs = now;

        if (ut99V55Handler == null) {
            ut99V55Handler = new android.os.Handler(android.os.Looper.getMainLooper());
        }

        ut99V55ApplyFixedSurface();

        ut99V55Handler.postDelayed(new Runnable() {
            @Override public void run() {
                ut99V55ApplyFixedSurface();
            }
        }, 120);

        ut99V55Handler.postDelayed(new Runnable() {
            @Override public void run() {
                ut99V55ApplyFixedSurface();
            }
        }, 450);

        ut99V55Handler.postDelayed(new Runnable() {
            @Override public void run() {
                ut99V55ApplyFixedSurface();
            }
        }, 1200);
    }






    // UT99_ANDROID_V56_INPUT_SAFE_FIXED_SURFACE
    private boolean ut99V56SurfaceFixedOnce = false;
    private long ut99V56LastSurfaceScheduleMs = 0L;


    // UT99_ANDROID_V57_FIXED_SURFACE_FULLSCREEN_LAYOUT
    private boolean ut99V57SurfaceLayoutFullscreenOnce = false;

    private void ut99V57ApplyFullscreenLayoutOnce(android.view.SurfaceView sv) {
        if (sv == null || ut99V57SurfaceLayoutFullscreenOnce) return;

        try {
            android.view.ViewGroup.LayoutParams lp = sv.getLayoutParams();
            if (lp == null) {
                lp = new android.view.ViewGroup.LayoutParams(
                    android.view.ViewGroup.LayoutParams.MATCH_PARENT,
                    android.view.ViewGroup.LayoutParams.MATCH_PARENT
                );
            }

            lp.width = android.view.ViewGroup.LayoutParams.MATCH_PARENT;
            lp.height = android.view.ViewGroup.LayoutParams.MATCH_PARENT;

            if (lp instanceof android.view.ViewGroup.MarginLayoutParams) {
                android.view.ViewGroup.MarginLayoutParams mlp = (android.view.ViewGroup.MarginLayoutParams)lp;
                mlp.leftMargin = 0;
                mlp.topMargin = 0;
                mlp.rightMargin = 0;
                mlp.bottomMargin = 0;
            }

            if (lp instanceof android.widget.FrameLayout.LayoutParams) {
                android.widget.FrameLayout.LayoutParams flp = (android.widget.FrameLayout.LayoutParams)lp;
                flp.gravity = android.view.Gravity.FILL;
            }

            sv.setLayoutParams(lp);
            sv.setX(0.0f);
            sv.setY(0.0f);
            sv.setTranslationX(0.0f);
            sv.setTranslationY(0.0f);
            sv.setScaleX(1.0f);
            sv.setScaleY(1.0f);
            sv.requestLayout();
            sv.invalidate();

            android.view.ViewParent parent = sv.getParent();
            if (parent instanceof android.view.View) {
                android.view.View pv = (android.view.View)parent;
                pv.requestLayout();
                pv.invalidate();
            }

            ut99V57SurfaceLayoutFullscreenOnce = true;
            android.util.Log.i("UT99Android", "UT99_ANDROID_V57_FIXED_SURFACE_FULLSCREEN_LAYOUT applied");
        } catch (Throwable t) {
            android.util.Log.e("UT99Android", "v57 fullscreen SurfaceView layout failed", t);
        }
    }


    // UT99_ANDROID_V58_SCALED_SURFACE_INPUT_SAFE_LAYOUT
    private boolean ut99V58ScaledSurfaceLayoutOnce = false;

    private void ut99V58ApplyScaledSurfaceLayoutOnce(android.view.SurfaceView sv) {
        // v60: the old scaled low-res visual layout is intentionally disabled.
        // Native rendering uses the fullscreen SurfaceView/layout size directly.
        if (sv == null || ut99V58ScaledSurfaceLayoutOnce) return;
        ut99V59ApplyFullscreenLayoutOnce(sv);
        ut99V58ScaledSurfaceLayoutOnce = true;
        android.util.Log.i("UT99Android", "UT99_ANDROID_V60_NATIVE_SURFACE_AUTOSCALE disabled old scaled layout");
    }


    // UT99_ANDROID_V60_NATIVE_SURFACE_LAYOUT
    private boolean ut99V59FullscreenLayoutOnce = false;
    private boolean ut99V59TouchScaleEnabled = false;
    private long ut99V59LastTouchLogMs = 0L;

    private void ut99V59ApplyFullscreenLayoutOnce(android.view.SurfaceView sv) {
        if (sv == null || ut99V59FullscreenLayoutOnce) return;

        try {
            android.view.ViewParent parent = sv.getParent();
            if (parent instanceof android.view.ViewGroup) {
                android.view.ViewGroup vg = (android.view.ViewGroup)parent;
                vg.setClipChildren(false);
                vg.setClipToPadding(false);
            }

            android.view.ViewGroup.LayoutParams lp = sv.getLayoutParams();
            if (lp == null) {
                lp = new android.view.ViewGroup.LayoutParams(
                    android.view.ViewGroup.LayoutParams.MATCH_PARENT,
                    android.view.ViewGroup.LayoutParams.MATCH_PARENT
                );
            }

            // v73: SurfaceView occupies fullscreen; SurfaceHolder may use Native/75%/50% drawable size.
            // This lets SDL/EGL expose the real drawable size to Unreal.
            lp.width = android.view.ViewGroup.LayoutParams.MATCH_PARENT;
            lp.height = android.view.ViewGroup.LayoutParams.MATCH_PARENT;

            if (lp instanceof android.view.ViewGroup.MarginLayoutParams) {
                android.view.ViewGroup.MarginLayoutParams mlp = (android.view.ViewGroup.MarginLayoutParams)lp;
                mlp.leftMargin = 0;
                mlp.topMargin = 0;
                mlp.rightMargin = 0;
                mlp.bottomMargin = 0;
            }

            if (lp instanceof android.widget.FrameLayout.LayoutParams) {
                android.widget.FrameLayout.LayoutParams flp = (android.widget.FrameLayout.LayoutParams)lp;
                flp.gravity = android.view.Gravity.FILL;
            }

            sv.setLayoutParams(lp);
            sv.setPivotX(0.0f);
            sv.setPivotY(0.0f);
            sv.setX(0.0f);
            sv.setY(0.0f);
            sv.setTranslationX(0.0f);
            sv.setTranslationY(0.0f);
            sv.setScaleX(1.0f);
            sv.setScaleY(1.0f);
            sv.setKeepScreenOn(true);
            sv.setFocusable(true);
            sv.setFocusableInTouchMode(true);
            sv.requestLayout();
            sv.invalidate();

            try {
                sv.requestFocus();
            } catch (Throwable ignored) {
            }

            ut99V59FullscreenLayoutOnce = true;
            ut99V59TouchScaleEnabled = false;
            android.util.Log.i("UT99Android", "UT99_ANDROID_V60_NATIVE_SURFACE_AUTOSCALE fullscreen native layout applied");
        } catch (Throwable t) {
            android.util.Log.e("UT99Android", "v59 fullscreen touchscale layout failed", t);
        }
    }

    @Override
    public boolean dispatchTouchEvent(android.view.MotionEvent ev) {
        // RetroTouch must see the current native menu state before a fresh pointer
        // is dispatched, otherwise the first UWindow tap after opening a menu can
        // be consumed by a stale GAMEPLAY mode.
        if (ut99RetroTouchBridge != null && isPhysicalMouseEventV210(ev)) {
            ut99RetroTouchBridge.onHardwareDesktopInput();
        } else if (ut99RetroTouchBridge != null) {
            ut99RetroTouchBridge.beforeHostTouch(ev);
        }
        // v60: no low-res touch rescaling. Touch/mouse events stay in native
        // SurfaceView coordinates so SDL and Unreal see the same drawable size.
        return super.dispatchTouchEvent(ev);
    }


    // RETROTOUCH_BETA4_UT99_INTEGRATION
    private void ut99InstallRetroTouchBeta4() {
        try {
            if (ut99RetroTouchBridge != null) return;
            // UT99_ANDROID_CHROMEOS_NO_OVERLAY_VIEW_V215:
            // RetroTouchMode.OFF only stops drawing/touch handling; the AAR's
            // full-screen View remains focusable above SDL. ChromeOS can then
            // bind hover/pointer capture to that invisible View, so clicks reach
            // SDL intermittently while motion never does. A Chromebook is never
            // a RetroTouch target, therefore do not add the overlay View at all.
            if (ut99IsChromeOSV211()) {
                Log.i(TAG, "UT99_ANDROID_CHROMEOS_NO_OVERLAY_VIEW_V215 skipped RetroTouch attachment");
                return;
            }
            ut99V91EnsureTouchOverlayConfigDefault();
            ut99V91MigrateLegacyTouchOverlayForRetroTouch();
            ut99RetroTouchBridge = new Ut99RetroTouchBridge(this);
            ut99RetroTouchBridge.attach();
            Log.i(TAG, "RetroTouch 1.0.0-beta.4 installed over SDL gameplay view");
        } catch (Throwable t) {
            Log.e(TAG, "RetroTouch Beta 4 install failed", t);
        }
    }

    private java.io.File ut99V91UserIniFile() {
        java.io.File root = getUt99ConfigRootV63();
        if (root == null) return null;
        return new java.io.File(new java.io.File(root, "System"), "AndroidUser.ini");
    }

    private void ut99V91EnsureTouchOverlayConfigDefault() {
        java.io.File ini = ut99V91UserIniFile();
        if (ini == null) return;
        try {
            java.io.File parent = ini.getParentFile();
            if (parent != null && !parent.exists()) parent.mkdirs();
            String text = ini.exists() ? ut99V91ReadSmallTextFile(ini) : "";
            if (text.indexOf("[UMenu.UMenuGameOptionsClientWindow]") < 0 && text.indexOf("bTouchOverlay=") < 0) {
                java.io.FileWriter fw = new java.io.FileWriter(ini, true);
                try {
                    fw.write("\n; RETROTOUCH_BETA4 default enabled on touch-only installs\n");
                    fw.write("[UMenu.UMenuGameOptionsClientWindow]\n");
                    fw.write("bTouchOverlay=True\n");
                } finally { fw.close(); }
                Log.i(TAG, "RetroTouch default config appended to " + ini.getAbsolutePath());
            }
        } catch (Throwable t) { Log.w(TAG, "v91 could not ensure touch overlay config", t); }
    }

    private String ut99V91ReadSmallTextFile(java.io.File file) throws java.io.IOException {
        java.io.BufferedReader br = new java.io.BufferedReader(new java.io.FileReader(file));
        try {
            StringBuilder sb = new StringBuilder();
            String line;
            while ((line = br.readLine()) != null) sb.append(line).append('\n');
            return sb.toString();
        } finally { br.close(); }
    }

    private void ut99V91MigrateLegacyTouchOverlayForRetroTouch() {
        // RETROTOUCH_BETA4_STATE_MACHINE_V2:
        // Older UT99 Android installs can legitimately contain bTouchOverlay=False
        // because that flag belonged to the previous built-in overlay. Migrate it
        // to enabled exactly once so an existing install immediately gets RetroTouch.
        // After the marker is written, the Game Options checkbox is user-controlled
        // again and future False/True changes are respected by the bridge.
        try {
            android.content.SharedPreferences prefs = getSharedPreferences(
                    "ut99_retrotouch_migrations", MODE_PRIVATE);
            final String marker = "beta4_state_machine_v2_enabled";
            if (prefs.getBoolean(marker, false)) return;

            java.io.File userIni = ut99V91UserIniFile();
            java.io.File systemDir = userIni != null ? userIni.getParentFile() : null;
            java.io.File[] files = new java.io.File[] {
                    userIni,
                    systemDir != null ? new java.io.File(systemDir, "UMenu.ini") : null,
                    systemDir != null ? new java.io.File(systemDir, "AndroidUT99.ini") : null,
                    systemDir != null ? new java.io.File(systemDir, "User.ini") : null
            };

            boolean found = false;
            for (java.io.File file : files) {
                if (file != null && file.exists()) {
                    found |= ut99V91SetTouchOverlayFlagInFile(file, true);
                }
            }

            if (!found && userIni != null) {
                java.io.File parent = userIni.getParentFile();
                if (parent != null && !parent.exists()) parent.mkdirs();
                java.io.FileWriter fw = new java.io.FileWriter(userIni, true);
                try {
                    fw.write("\n; RETROTOUCH_BETA4_STATE_MACHINE_V2 legacy-overlay migration\n");
                    fw.write("[UMenu.UMenuGameOptionsClientWindow]\n");
                    fw.write("bTouchOverlay=True\n");
                } finally {
                    fw.close();
                }
            }

            prefs.edit().putBoolean(marker, true).apply();
            Log.i(TAG, "RetroTouch migrated legacy bTouchOverlay setting to enabled once");
        } catch (Throwable t) {
            Log.w(TAG, "RetroTouch legacy touch-overlay migration failed", t);
        }
    }

    private boolean ut99V91SetTouchOverlayFlagInFile(java.io.File file, boolean enabled) {
        try {
            String text = ut99V91ReadSmallTextFile(file);
            String[] lines = text.split("\n", -1);
            StringBuilder out = new StringBuilder(text.length() + 16);
            boolean found = false;
            boolean changed = false;
            String replacement = enabled ? "bTouchOverlay=True" : "bTouchOverlay=False";

            for (int i = 0; i < lines.length; i++) {
                String line = lines[i];
                String trimmed = line.trim();
                if (trimmed.regionMatches(true, 0, "bTouchOverlay", 0, "bTouchOverlay".length())) {
                    int eq = trimmed.indexOf('=');
                    if (eq > 0 && trimmed.substring(0, eq).trim().equalsIgnoreCase("bTouchOverlay")) {
                        found = true;
                        if (!trimmed.equalsIgnoreCase(replacement)) changed = true;
                        line = replacement;
                    }
                }
                out.append(line);
                if (i + 1 < lines.length) out.append('\n');
            }

            if (found && changed) {
                java.io.FileWriter fw = new java.io.FileWriter(file, false);
                try {
                    fw.write(out.toString());
                } finally {
                    fw.close();
                }
            }
            return found;
        } catch (Throwable t) {
            Log.w(TAG, "Could not migrate bTouchOverlay in " + file, t);
            return false;
        }
    }

    private boolean ut99V91ReadTouchOverlayEnabled() {
        java.io.File userIni = ut99V91UserIniFile();
        java.io.File systemDir = userIni != null ? userIni.getParentFile() : null;

        // RETROTOUCH_BETA4_STATE_MACHINE_V2:
        // UMenuGameOptionsClientWindow is a UMenu globalconfig class, so UMenu.ini
        // is authoritative whenever it contains bTouchOverlay. AndroidUser.ini is
        // the packaged/default fallback. This avoids a stale copy in another INI
        // overriding a checkbox change made in Preferences > Game.
        if (systemDir != null) {
            Boolean value = ut99V96ReadTouchOverlayFlag(new java.io.File(systemDir, "UMenu.ini"), null);
            if (value != null) return value.booleanValue();
        }
        if (userIni != null) {
            Boolean value = ut99V96ReadTouchOverlayFlag(userIni, null);
            if (value != null) return value.booleanValue();
        }
        if (systemDir != null) {
            Boolean value = ut99V96ReadTouchOverlayFlag(new java.io.File(systemDir, "AndroidUT99.ini"), null);
            if (value != null) return value.booleanValue();
            value = ut99V96ReadTouchOverlayFlag(new java.io.File(systemDir, "User.ini"), null);
            if (value != null) return value.booleanValue();
        }
        return true;
    }

    private Boolean ut99V96ReadTouchOverlayFlag(java.io.File ini, Boolean current) {
        if (ini == null || !ini.exists()) return current;
        try {
            java.io.BufferedReader br = new java.io.BufferedReader(new java.io.FileReader(ini));
            try {
                String line;
                boolean inSection = false;
                Boolean found = current;
                while ((line = br.readLine()) != null) {
                    String t = line.trim();
                    if (t.length() == 0 || t.startsWith(";") || t.startsWith("#")) continue;
                    if (t.startsWith("[") && t.endsWith("]")) {
                        inSection = t.equalsIgnoreCase("[UMenu.UMenuGameOptionsClientWindow]") || t.equalsIgnoreCase("[UMenuGameOptionsClientWindow]") || t.equalsIgnoreCase("[UMenu.UT99TouchOverlayConfig]") || t.equalsIgnoreCase("[UT99TouchOverlayConfig]");
                        continue;
                    }
                    int eq = t.indexOf('=');
                    if (eq > 0) {
                        String key = t.substring(0, eq).trim();
                        String value = t.substring(eq + 1).trim();
                        if ((inSection && key.equalsIgnoreCase("bTouchOverlay")) || key.equalsIgnoreCase("bTouchOverlay")) {
                            found = !(value.equalsIgnoreCase("false") || value.equals("0") || value.equalsIgnoreCase("no") || value.equalsIgnoreCase("off"));
                        }
                    }
                }
                return found;
            } finally { br.close(); }
        } catch (Throwable t) { return current; }
    }



}
