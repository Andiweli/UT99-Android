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
 * This intentionally stays 32-bit only. The Gradle/CMake configuration restricts
 * the native build to armeabi-v7a for Android 4.1.2 / OUYA-class devices.
 */
public class GameActivity extends SDLActivity {
    private static final String TAG = "UT99Android";

    // UT99_ANDROID_V73_RESOLUTION_SCALE_RESTORED:
    // Preferences > Video > Resolution can request Native, 75% native Res. or 50% native Res.
    // Java owns the Android SurfaceHolder buffer size so the EGL drawable really becomes smaller.
    private static java.lang.ref.WeakReference<GameActivity> sUt99V64ActivityRef;
    private int ut99V64ResolutionScalePercent = 100;

    // UT99_ANDROID_V76_KEYBOARD_START_SAFE:
    // The SDL dummy text view must not summon the IME during engine start.
    // Native UWindow code toggles this flag only when an actual edit-field candidate is tapped.
    private static volatile boolean sUt99ImeWanted;
    private static boolean sUt99V72LoggedActive = false;

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
        final GameActivity activity = sUt99V64ActivityRef != null ? sUt99V64ActivityRef.get() : null;
        if (activity == null) {
            Log.w(TAG, "v73 resolution scale request ignored because GameActivity is not active percent=" + percent);
            return;
        }

        activity.runOnUiThread(new Runnable() {
            @Override public void run() {
                activity.ut99V64ApplyResolutionScalePercent(percent, true);
            }
        });
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

    private static native boolean nativePrepareProcess(String dataRoot, String homeDir);
    private static native void nativeAndroidTextV82(String text);
    private static native boolean nativeAndroidIsMenuV92(); // UT99_ANDROID_V92_TOUCH_OVERLAY

    private File dataRoot;
    private File homeDir;
    private boolean legacySafeMode;
    private Ut99TouchOverlayViewV91 ut99TouchOverlayViewV91;

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
        ut99V64ResolutionScalePercent = ut99V64ReadResolutionScalePercent();

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

        boolean nativeOk = nativePrepareProcess(dataRoot.getAbsolutePath(), homeDir.getAbsolutePath());
        if (!nativeOk) {
            Log.e(TAG, "nativePrepareProcess failed; engine may not find its data path");
            Toast.makeText(this, "UT99 native path setup failed", Toast.LENGTH_LONG).show();
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
        ut99V64EnsureResolutionScaleConfig();
        super.onCreate(savedInstanceState);
        ut99V55ScheduleFixedSurface(); // v55 onCreate
        ut99V52ScheduleImmersive(); // v52 onCreate
        // UT99_ANDROID_V63_CITYINTRO_START: do not cover CityIntro with the old static title/menu overlay.
        // ut99V52ShowStartupOverlay();
        android.util.Log.i("UT99Android", "UT99_ANDROID_V73_RESOLUTION_SCALE_RESTORED active percent=" + ut99V64ResolutionScalePercent);
        ut99V50StageBranding();
        ut99V50Immersive(); // v50 onCreate
        stageBrandingAssetV47();
        applyUt99ImmersiveMode();
        ut99V91InstallTouchOverlay();
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
        return new String[] {
                "SDL2",
                // UT99_ANDROID_V137_OUYA_LIBXMP_PRELOAD:
                // Android 4.1.2 / OUYA does not reliably resolve native
                // DT_NEEDED dependencies from the app lib directory when
                // libUnrealTournament.so is loaded.  Preload libxmp explicitly
                // before UnrealTournament so the direct-linked UMX music backend
                // can start on OUYA as well as newer Android devices.
                "xmp",
                "UnrealTournament"
        };
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

    // UT99_ANDROID_V73_RESOLUTION_SCALE_RESTORED
    private static final String UT99_V64_SCALE_SECTION = "NSDLDrv.NSDLClient";
    private static final String UT99_V64_SCALE_KEY = "AndroidResolutionScale";

    private int ut99V64NormalizeScalePercent(int percent) {
        if (percent <= 55) return 50;
        if (percent <= 85) return 75;
        return 100;
    }

    private int ut99V64ParseScalePercent(String raw, int fallback) {
        if (raw == null) return fallback;
        String value = raw.trim().toLowerCase(java.util.Locale.US);
        if (value.length() == 0) return fallback;
        if (value.contains("50")) return 50;
        if (value.contains("75")) return 75;
        if (value.contains("native") || value.contains("100") || value.equals("1") || value.equals("1.0") || value.equals("1.000000")) return 100;
        try {
            float f = Float.parseFloat(value);
            if (f > 0.0f && f <= 1.0f) {
                return ut99V64NormalizeScalePercent(Math.round(f * 100.0f));
            }
            return ut99V64NormalizeScalePercent(Math.round(f));
        } catch (Throwable ignored) {
        }
        return fallback;
    }

    private int ut99V64ReadResolutionScalePercent() {
        java.io.File root = getUt99ConfigRootV63();
        if (root == null) return 100;
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
                    if (trimmed.toLowerCase(java.util.Locale.US).startsWith((UT99_V64_SCALE_KEY + "=").toLowerCase(java.util.Locale.US))) {
                        return ut99V64ParseScalePercent(trimmed.substring(trimmed.indexOf('=') + 1), 100);
                    }
                }
            } catch (Throwable t) {
                android.util.Log.w("UT99Android", "v64 could not read resolution scale from " + ini.getAbsolutePath(), t);
            }
        }
        return 100;
    }

    private void ut99V64EnsureResolutionScaleConfig() {
        java.io.File root = getUt99ConfigRootV63();
        if (root == null) return;
        java.io.File systemDir = new java.io.File(root, "System");
        if (!systemDir.exists() && !systemDir.mkdirs()) {
            android.util.Log.e("UT99Android", "v64 could not create System dir: " + systemDir.getAbsolutePath());
            return;
        }
        java.io.File androidIni = new java.io.File(systemDir, "AndroidUT99.ini");
        ut99V64WriteResolutionScaleConfig(androidIni, ut99V64ResolutionScalePercent, false);
    }

    private void ut99V64WriteResolutionScaleConfig(java.io.File ini, int percent, boolean overwrite) {
        if (ini == null) return;
        percent = ut99V64NormalizeScalePercent(percent);
        try {
            String text = ini.exists() ? ut99V60ReadUtf8(ini) : "";
            boolean hasKey = text.toLowerCase(java.util.Locale.US).contains((UT99_V64_SCALE_KEY + "=").toLowerCase(java.util.Locale.US));
            if (!hasKey || overwrite) {
                text = ut99V60UpsertKey(text, UT99_V64_SCALE_SECTION, UT99_V64_SCALE_KEY, String.valueOf(percent));
                ut99V60WriteUtf8(ini, text);
            }
        } catch (Throwable t) {
            android.util.Log.e("UT99Android", "v64 failed to write resolution scale to " + ini.getAbsolutePath(), t);
        }
    }

    private int[] ut99V64ResolveBaseSurfaceSize(android.view.SurfaceView sv) {
        int baseW = 0;
        int baseH = 0;
        try {
            if (sv != null && sv.getWidth() > 0 && sv.getHeight() > 0) {
                baseW = sv.getWidth();
                baseH = sv.getHeight();
            }
        } catch (Throwable ignored) {
        }

        if (baseW <= 0 || baseH <= 0) {
            int[] displaySize = ut99V60ResolveNativeDisplaySize();
            int displayW = displaySize[0];
            int displayH = displaySize[1];
            if (displayW > 0 && displayH > 0) {
                baseW = Math.max(displayW, displayH);
                baseH = Math.min(displayW, displayH);
            }
        }

        return new int[] { baseW, baseH };
    }

    private void ut99V64ApplyResolutionScaleToSurface(android.view.SurfaceView sv) {
        if (sv == null) return;
        int percent = ut99V64NormalizeScalePercent(ut99V64ResolutionScalePercent);

        android.view.SurfaceHolder holder = sv.getHolder();
        if (holder == null) return;

        if (percent >= 100) {
            holder.setSizeFromLayout();
            android.util.Log.i("UT99Android", "UT99_ANDROID_V73_RESOLUTION_SCALE_RESTORED SurfaceHolder Native layout size on " + sv.getClass().getName());
            return;
        }

        int[] base = ut99V64ResolveBaseSurfaceSize(sv);
        int baseW = base[0];
        int baseH = base[1];
        if (baseW <= 0 || baseH <= 0) {
            holder.setSizeFromLayout();
            android.util.Log.w("UT99Android", "v64 could not resolve base surface size, using native layout");
            return;
        }

        int scaledW = Math.max(320, Math.round((baseW * percent) / 100.0f));
        int scaledH = Math.max(240, Math.round((baseH * percent) / 100.0f));
        scaledW = Math.max(320, scaledW & ~1);
        scaledH = Math.max(240, scaledH & ~1);

        holder.setFixedSize(scaledW, scaledH);
        android.util.Log.i("UT99Android", "UT99_ANDROID_V73_RESOLUTION_SCALE_RESTORED SurfaceHolder "
                + percent + "% of native " + baseW + "x" + baseH + " -> " + scaledW + "x" + scaledH
                + " on " + sv.getClass().getName());
    }

    private void ut99V64ApplyResolutionScalePercent(int percent, boolean persist) {
        ut99V64ResolutionScalePercent = ut99V64NormalizeScalePercent(percent);
        if (persist) {
            java.io.File root = getUt99ConfigRootV63();
            if (root != null) {
                java.io.File systemDir = new java.io.File(root, "System");
                if (!systemDir.exists()) systemDir.mkdirs();
                ut99V64WriteResolutionScaleConfig(new java.io.File(systemDir, "AndroidUT99.ini"), ut99V64ResolutionScalePercent, true);
            }
        }

        ut99V56SurfaceFixedOnce = false;
        ut99V55ApplyFixedSurface();
        ut99V55ScheduleFixedSurface();
        android.util.Log.i("UT99Android", "UT99_ANDROID_V73_RESOLUTION_SCALE_RESTORED applied percent=" + ut99V64ResolutionScalePercent);
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

    private static boolean isAndroidGamepadSourceV47(android.view.InputEvent event) {
        int source = event.getSource();
        return ((source & android.view.InputDevice.SOURCE_GAMEPAD) == android.view.InputDevice.SOURCE_GAMEPAD)
                || ((source & android.view.InputDevice.SOURCE_JOYSTICK) == android.view.InputDevice.SOURCE_JOYSTICK)
                || ((source & android.view.InputDevice.SOURCE_DPAD) == android.view.InputDevice.SOURCE_DPAD);
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

    private void stageBrandingAssetV47() {
        try {
            java.io.File root = getExternalFilesDir(null);
            if (root == null) {
                return;
            }
            java.io.File uiDir = new java.io.File(root, "UI");
            if (!uiDir.exists()) {
                uiDir.mkdirs();
            }
            java.io.File out = new java.io.File(uiDir, "ut99_start_menu_v47.png");
            java.io.InputStream in = getAssets().open("ut99_start_menu_v47.png");
            try {
                java.io.FileOutputStream fos = new java.io.FileOutputStream(out, false);
                try {
                    byte[] buf = new byte[8192];
                    int r;
                    while ((r = in.read(buf)) > 0) {
                        fos.write(buf, 0, r);
                    }
                } finally {
                    fos.close();
                }
            } finally {
                in.close();
            }
            android.util.Log.i("UT99Android", "v47 staged branding image " + out.getAbsolutePath());
        } catch (Throwable t) {
            android.util.Log.e("UT99Android", "v47 could not stage branding asset", t);
        }
    }

    @Override
    public boolean dispatchKeyEvent(android.view.KeyEvent event) {
        final int action = event.getAction();
        final int keyCode = event.getKeyCode();
        boolean textDeleteKey = keyCode == android.view.KeyEvent.KEYCODE_DEL
                || keyCode == android.view.KeyEvent.KEYCODE_FORWARD_DEL;
        if (!sUt99V72LoggedActive) {
            sUt99V72LoggedActive = true;
            android.util.Log.i("UT99Android", "UT99_ANDROID_V72_ACTIVE GameActivity single START toggle path loaded");
        }
        if (isAndroidGamepadSourceV47(event) || isOuyaMenuKeyV79(keyCode) || textDeleteKey) {
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
    public boolean onGenericMotionEvent(android.view.MotionEvent event) {
        if (isAndroidGamepadSourceV47(event) && event.getAction() == android.view.MotionEvent.ACTION_MOVE) {
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

    private void ut99V50StageBranding() {
        try {
            java.io.File root = getExternalFilesDir(null);
            if (root == null) return;
            java.io.File uiDir = new java.io.File(root, "UI");
            if (!uiDir.exists()) uiDir.mkdirs();
            java.io.File out = new java.io.File(uiDir, "ut99_start_menu_v50.png");
            java.io.InputStream in = getAssets().open("ut99_start_menu_v50.png");
            try {
                java.io.FileOutputStream fos = new java.io.FileOutputStream(out, false);
                try {
                    byte[] buf = new byte[8192];
                    int r;
                    while ((r = in.read(buf)) > 0) fos.write(buf, 0, r);
                } finally { fos.close(); }
            } finally { in.close(); }
            android.util.Log.i("UT99Android", "v50 staged branding image " + out.getAbsolutePath());
        } catch (Throwable t) {
            android.util.Log.e("UT99Android", "v50 branding stage failed", t);
        }
    }



    // UT99_ANDROID_V52_IMMERSIVE_HARD
    private android.os.Handler ut99V52Handler;
    private android.widget.ImageView ut99V52Overlay;

    private void ut99V52HardImmersive() {
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

    private void ut99V52ShowStartupOverlay() {
        try {
            int resId = getResources().getIdentifier("ut99_start_menu_v52", "drawable", getPackageName());
            if (resId == 0) return;

            if (ut99V52Overlay != null) {
                return;
            }

            ut99V52Overlay = new android.widget.ImageView(this);
            ut99V52Overlay.setImageResource(resId);
            ut99V52Overlay.setScaleType(android.widget.ImageView.ScaleType.FIT_CENTER);
            ut99V52Overlay.setBackgroundColor(android.graphics.Color.BLACK);
            ut99V52Overlay.setClickable(false);
            ut99V52Overlay.setFocusable(false);
            android.widget.FrameLayout.LayoutParams lp = new android.widget.FrameLayout.LayoutParams(
                    android.widget.FrameLayout.LayoutParams.MATCH_PARENT,
                    android.widget.FrameLayout.LayoutParams.MATCH_PARENT);
            addContentView(ut99V52Overlay, lp);

            if (ut99V52Handler == null) {
                ut99V52Handler = new android.os.Handler(android.os.Looper.getMainLooper());
            }
            ut99V52Handler.postDelayed(new Runnable() {
                @Override public void run() {
                    try {
                        if (ut99V52Overlay != null) {
                            ut99V52Overlay.setVisibility(android.view.View.GONE);
                            android.util.Log.i("UT99Android", "UT99_ANDROID_V52_STARTUP_OVERLAY hidden");
                        }
                    } catch (Throwable ignored) {}
                }
            }, 900); // UT99_ANDROID_V55_OVERLAY_SHORT

            android.util.Log.i("UT99Android", "UT99_ANDROID_V52_STARTUP_OVERLAY shown");
        } catch (Throwable t) {
            android.util.Log.e("UT99Android", "v52 startup overlay failed", t);
        }
    }


    @Override
    protected void onResume() {
        ut99V55ScheduleFixedSurface(); // v55 onResume
        super.onResume();
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
        // v60: no low-res touch rescaling.  Touch/mouse events now stay in native
        // SurfaceView coordinates so SDL and Unreal see the same drawable size.
        return super.dispatchTouchEvent(ev);
    }


    // UT99_ANDROID_V91_TOUCH_OVERLAY
    private void ut99V91InstallTouchOverlay() {
        try {
            if (ut99TouchOverlayViewV91 != null) return;
            ut99V91EnsureTouchOverlayConfigDefault();
            ut99TouchOverlayViewV91 = new Ut99TouchOverlayViewV91(this);
            android.widget.FrameLayout.LayoutParams lp = new android.widget.FrameLayout.LayoutParams(
                    android.widget.FrameLayout.LayoutParams.MATCH_PARENT,
                    android.widget.FrameLayout.LayoutParams.MATCH_PARENT);
            addContentView(ut99TouchOverlayViewV91, lp);
            Log.i(TAG, "UT99_ANDROID_V109_OUYA_IME_RETROID_TOUCH_REPAIR");
        } catch (Throwable t) {
            Log.e(TAG, "v91 touch overlay install failed", t);
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
                    fw.write("\n; UT99_ANDROID_V96_TOUCH_OVERLAY default disabled on first install\n");
                    fw.write("[UMenu.UMenuGameOptionsClientWindow]\n");
                    fw.write("bTouchOverlay=False\n");
                } finally { fw.close(); }
                Log.i(TAG, "v91 touch overlay default config appended to " + ini.getAbsolutePath());
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

    private boolean ut99V91ReadTouchOverlayEnabled() {
        java.io.File userIni = ut99V91UserIniFile();
        java.io.File systemDir = userIni != null ? userIni.getParentFile() : null;
        Boolean found = null;
        if (userIni != null) found = ut99V96ReadTouchOverlayFlag(userIni, found);
        if (systemDir != null) {
            // v96: UMenu globalconfig may be saved outside AndroidUser.ini depending on the package build.
            // Read these after AndroidUser.ini so the most recent UMenu save can override older defaults.
            found = ut99V96ReadTouchOverlayFlag(new java.io.File(systemDir, "UMenu.ini"), found);
            found = ut99V96ReadTouchOverlayFlag(new java.io.File(systemDir, "AndroidUT99.ini"), found);
            found = ut99V96ReadTouchOverlayFlag(new java.io.File(systemDir, "User.ini"), found);
        }
        return found != null ? found.booleanValue() : false;
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

    private static float ut99V91Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

    private static final class Ut99TouchOverlayViewV91 extends View {
        private final GameActivity activity;
        private final android.graphics.Paint paint = new android.graphics.Paint(android.graphics.Paint.ANTI_ALIAS_FLAG);
        private final android.graphics.Paint labelPaint = new android.graphics.Paint(android.graphics.Paint.ANTI_ALIAS_FLAG);
        private final android.graphics.RectF rect = new android.graphics.RectF();
        private final android.util.SparseArray<TouchRole> roles = new android.util.SparseArray<TouchRole>();
        private final android.graphics.Bitmap iconFire;
        private final android.graphics.Bitmap iconAltFire;
        private final android.graphics.Bitmap iconJump;
        private final android.graphics.Bitmap iconCrouch;
        private final android.graphics.Bitmap iconNext;
        private final android.graphics.Bitmap iconMenu;
        private long lastConfigReadMs, lastMenuReadMs;
        private boolean enabled = false, menuVisible = false;
        private float leftBaseX, leftBaseY, rightBaseX, rightBaseY, rightLastX, rightLastY, lx, ly, rx, ry;
        private boolean fire, altFire, jump, crouch, next, menu;
        private enum TouchRole { LEFT_STICK, RIGHT_LOOK, FIRE, ALTFIRE, JUMP, CROUCH, NEXT, MENU }

        Ut99TouchOverlayViewV91(GameActivity activity) {
            super(activity);
            this.activity = activity;
            setWillNotDraw(false);
            setFocusable(false);
            labelPaint.setTextAlign(android.graphics.Paint.Align.CENTER);
            labelPaint.setFakeBoldText(true);
            iconFire = loadIcon("touch_overlay/fire.png");
            iconAltFire = loadIcon("touch_overlay/alternate-fire.png");
            iconJump = loadIcon("touch_overlay/jump.png");
            iconCrouch = loadIcon("touch_overlay/crouch.png");
            iconNext = loadIcon("touch_overlay/next-weapon.png");
            iconMenu = loadIcon("touch_overlay/menu.png");
            postDelayed(redrawRunnable, 66L);
        }

        private final Runnable redrawRunnable = new Runnable() {
            @Override public void run() {
                invalidate();
                postDelayed(this, 66L);
            }
        };

        private android.graphics.Bitmap loadIcon(String assetPath) {
            try {
                java.io.InputStream in = activity.getAssets().open(assetPath);
                try { return android.graphics.BitmapFactory.decodeStream(in); }
                finally { in.close(); }
            } catch (Throwable t) {
                Log.w(TAG, "v91 touch overlay missing icon " + assetPath, t);
                return null;
            }
        }

        private void refreshState() {
            long now = android.os.SystemClock.uptimeMillis();
            if (now - lastConfigReadMs > 900L) { lastConfigReadMs = now; enabled = activity.ut99V91ReadTouchOverlayEnabled(); }
            if (now - lastMenuReadMs > 120L) {
                lastMenuReadMs = now;
                try { menuVisible = nativeAndroidIsMenuV92(); } catch (Throwable ignored) { menuVisible = false; }
            }
        }

        private boolean active() { refreshState(); return enabled && !menuVisible; }

        @Override protected void onDraw(android.graphics.Canvas canvas) {
            super.onDraw(canvas);
            refreshState();
            if (!enabled || menuVisible || canvas == null) return;
            float w = getWidth(), h = getHeight();
            if (w <= 0 || h <= 0) return;
            float s = Math.min(w, h);
            float pad = Math.max(10f, s * 0.020f);
            float r = Math.max(50f, s * 0.0705f); // v96: restored larger v94-style buttons
            float gap = Math.max(12f, s * 0.018f);

            drawIconButton(canvas, pad + r * 0.72f, pad + r * 0.72f, r * 0.72f, iconMenu, null, true);
            float actionY1 = h * 0.42f + r * 2.0f; // v96: right-side Fire/AltFire lower
            float actionY2 = actionY1 + r * 2.0f + gap;
            float nextY = actionY1 - r * 2.0f - gap;
            drawIconButton(canvas, w - pad - r, nextY, r, iconNext, null, true);
            drawIconButton(canvas, w - pad - r, actionY1, r, iconFire, null, true);
            drawIconButton(canvas, w - pad - r, actionY2, r, iconAltFire, null, true);

            float bottomY = h - pad - r;
            float bottomShiftLeft = r; // v95: move Jump/Crouch half a button-width left
            drawIconButton(canvas, w - pad - r - bottomShiftLeft, bottomY, r, iconCrouch, null, true);
            drawIconButton(canvas, w - pad - r * 3.05f - gap - bottomShiftLeft, bottomY, r, iconJump, null, true);
        }

        private void drawIconButton(android.graphics.Canvas canvas, float cx, float cy, float r, android.graphics.Bitmap icon, String label, boolean round) {
            paint.setStyle(android.graphics.Paint.Style.FILL);
            paint.setColor(0x06202020); // v95: 50% more transparent than v94
            if (round) {
                canvas.drawCircle(cx, cy, r, paint);
            } else {
                canvas.drawCircle(cx, cy, r, paint);
            }
            paint.setStyle(android.graphics.Paint.Style.STROKE);
            paint.setStrokeWidth(Math.max(2f, r * 0.055f));
            paint.setColor(0x1AFFFFFF);
            canvas.drawCircle(cx, cy, r, paint);
            if (icon != null) {
                float iconR = r * 0.64f;
                rect.set(cx - iconR, cy - iconR, cx + iconR, cy + iconR);
                paint.setStyle(android.graphics.Paint.Style.FILL);
                paint.setAlpha(40);
                canvas.drawBitmap(icon, null, rect, paint);
                paint.setAlpha(255);
            } else if (label != null && label.equals("NEXT")) {
                labelPaint.setColor(0x33FFFFFF);
                labelPaint.setTextSize(Math.max(17f, r * 0.43f));
                android.graphics.Paint.FontMetrics fm = labelPaint.getFontMetrics();
                canvas.drawText("NEXT", cx, cy - (fm.ascent + fm.descent) * 0.5f, labelPaint);
            }
            if (label != null && !label.equals("NEXT")) {
                labelPaint.setColor(0x28FFFFFF);
                labelPaint.setTextSize(Math.max(12f, r * 0.24f));
                canvas.drawText(label, cx, cy + r + Math.max(12f, r * 0.32f), labelPaint);
            }
        }

        @Override public boolean onTouchEvent(android.view.MotionEvent event) {
            if (event == null) return false;
            if (!active()) { releaseAll(); return false; }
            int action = event.getActionMasked(), index = event.getActionIndex();
            if (action == android.view.MotionEvent.ACTION_DOWN || action == android.view.MotionEvent.ACTION_POINTER_DOWN) {
                int pointerId = event.getPointerId(index);
                TouchRole role = hitRole(event.getX(index), event.getY(index));
                roles.put(pointerId, role);
                if (role == TouchRole.LEFT_STICK) { leftBaseX = event.getX(index); leftBaseY = event.getY(index); }
                else if (role == TouchRole.RIGHT_LOOK) { rightBaseX = event.getX(index); rightBaseY = event.getY(index); rightLastX = rightBaseX; rightLastY = rightBaseY; }
                updateRole(role, event.getX(index), event.getY(index), true);
                return true;
            }
            if (action == android.view.MotionEvent.ACTION_MOVE) {
                for (int i = 0; i < event.getPointerCount(); ++i) {
                    TouchRole role = roles.get(event.getPointerId(i));
                    if (role != null) updateRole(role, event.getX(i), event.getY(i), true);
                }
                return true;
            }
            if (action == android.view.MotionEvent.ACTION_CANCEL) { releaseAll(); roles.clear(); return true; }
            if (action == android.view.MotionEvent.ACTION_UP || action == android.view.MotionEvent.ACTION_POINTER_UP) {
                int pointerId = event.getPointerId(index);
                TouchRole role = roles.get(pointerId);
                if (role != null) { updateRole(role, event.getX(index), event.getY(index), false); roles.remove(pointerId); return true; }
            }
            return true;
        }

        private TouchRole hitRole(float x, float y) {
            float w = getWidth(), h = getHeight(), s = Math.min(w, h), pad = Math.max(10f, s * 0.020f), r = Math.max(50f, s * 0.0705f), gap = Math.max(12f, s * 0.018f);
            if (insideCircle(x, y, pad + r * 0.72f, pad + r * 0.72f, r * 1.10f)) return TouchRole.MENU;
            float actionY1 = h * 0.42f + r * 2.0f, actionY2 = actionY1 + r * 2.0f + gap, nextY = actionY1 - r * 2.0f - gap;
            if (insideCircle(x, y, w - pad - r, nextY, r * 1.35f)) return TouchRole.NEXT;
            if (insideCircle(x, y, w - pad - r, actionY1, r * 1.35f)) return TouchRole.FIRE;
            if (insideCircle(x, y, w - pad - r, actionY2, r * 1.35f)) return TouchRole.ALTFIRE;
            float bottomY = h - pad - r;
            float bottomShiftLeft = r;
            if (insideCircle(x, y, w - pad - r - bottomShiftLeft, bottomY, r * 1.35f)) return TouchRole.CROUCH;
            if (insideCircle(x, y, w - pad - r * 3.05f - gap - bottomShiftLeft, bottomY, r * 1.35f)) return TouchRole.JUMP;
            return x < w * 0.5f ? TouchRole.LEFT_STICK : TouchRole.RIGHT_LOOK;
        }

        private boolean insideCircle(float x, float y, float cx, float cy, float r) {
            float dx = x - cx, dy = y - cy;
            return dx * dx + dy * dy <= r * r;
        }

        private float analogValue(float delta, float radius, float dead, float scale) {
            float v = ut99V91Clamp(delta / radius, -1f, 1f);
            if (Math.abs(v) < dead) return 0f;
            if (v > 0f) v = (v - dead) / (1f - dead);
            else v = (v + dead) / (1f - dead);
            return ut99V91Clamp(v * scale, -1f, 1f);
        }

        private float analogValueSoftLook(float delta, float radius, float dead, float scale) {
            float v = ut99V91Clamp(delta / radius, -1f, 1f);
            float sign = v < 0f ? -1f : 1f;
            float a = Math.abs(v);
            if (a < dead) return 0f;
            a = (a - dead) / (1f - dead);
            return ut99V91Clamp(sign * a * scale, -1f, 1f);
        }

        private float touchLookDeltaV106(float deltaPx, float gain) {
            // UT99_ANDROID_V107_TOUCH_OVERLAY:
            // Smartphone FPS aiming should be relative-drag based, like a mouse,
            // not a virtual stick with a large centre deadzone.  Keep only a tiny
            // jitter filter for touch noise and send proportional deltas to native.
            if (Math.abs(deltaPx) < 0.25f) return 0f;
            return ut99V91Clamp(deltaPx * gain, -1f, 1f);
        }

        private void updateRole(TouchRole role, float x, float y, boolean down) {
            float s = Math.min(getWidth(), getHeight());
            float moveRadius = Math.max(112f, s * 0.145f); // v96: stable left virtual stick
            float lookRadius = Math.max(118f, s * 0.145f); // v102: keep horizontal precision from v101
            switch (role) {
                case LEFT_STICK:
                    lx = down ? analogValue(x - leftBaseX, moveRadius, 0.075f, 0.74f) : 0f;
                    ly = down ? analogValue(y - leftBaseY, moveRadius, 0.075f, 0.74f) : 0f;
                    nativeAndroidAxisV47(android.view.MotionEvent.AXIS_X, lx);
                    nativeAndroidAxisV47(android.view.MotionEvent.AXIS_Y, ly);
                    break;
                case RIGHT_LOOK:
                    if (down) {
                        float dx = x - rightLastX;
                        float dy = y - rightLastY;
                        rightLastX = x;
                        rightLastY = y;
                        rx = touchLookDeltaV106(dx, 0.0210f);
                        ry = touchLookDeltaV106(dy, 0.0210f);
                    } else {
                        rx = ry = 0f;
                    }
                    nativeAndroidTouchLookV101(rx, ry);
                    break;
                case FIRE:
                    if (fire != down) { fire = down; nativeAndroidButtonV47(105, down); }
                    break;
                case ALTFIRE:
                    if (altFire != down) { altFire = down; nativeAndroidButtonV47(104, down); }
                    break;
                case JUMP:
                    if (jump != down) { jump = down; nativeAndroidButtonV47(96, down); }
                    break;
                case CROUCH:
                    if (crouch != down) { crouch = down; nativeAndroidButtonV47(97, down); }
                    break;
                case NEXT:
                    if (next != down) { next = down; nativeAndroidButtonV47(103, down); }
                    break;
                case MENU:
                    if (menu != down) { menu = down; nativeAndroidButtonV47(82, down); }
                    break;
            }
        }

        private void releaseAll() {
            if (lx != 0f || ly != 0f) { lx = ly = 0f; nativeAndroidAxisV47(android.view.MotionEvent.AXIS_X, 0f); nativeAndroidAxisV47(android.view.MotionEvent.AXIS_Y, 0f); }
            if (rx != 0f || ry != 0f) { rx = ry = 0f; nativeAndroidTouchLookV101(0f, 0f); }
            if (fire) { fire = false; nativeAndroidButtonV47(105, false); }
            if (altFire) { altFire = false; nativeAndroidButtonV47(104, false); }
            if (jump) { jump = false; nativeAndroidButtonV47(96, false); }
            if (crouch) { crouch = false; nativeAndroidButtonV47(97, false); }
            if (next) { next = false; nativeAndroidButtonV47(103, false); }
            if (menu) { menu = false; nativeAndroidButtonV47(82, false); }
        }
    }

}
