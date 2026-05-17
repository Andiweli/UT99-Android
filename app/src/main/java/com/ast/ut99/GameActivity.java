/* UT99_ANDROID_V55_FIXED_SURFACE_LOGICAL_960X540 */
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

    // UT99_ANDROID_V76_KEYBOARD_START_SAFE:
    // The SDL dummy text view must not summon the IME during engine start.
    // Native UWindow code toggles this flag only when an actual edit-field candidate is tapped.
    private static volatile boolean sUt99ImeWanted;

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

    private File dataRoot;
    private File homeDir;
    private boolean legacySafeMode;

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
        dataRoot = resolveDataRootForGame();
        homeDir = UT99Paths.homeDir(this);
        legacySafeMode = resolveLegacySafeMode();

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
        super.onCreate(savedInstanceState);
        ut99V55ScheduleFixedSurface(); // v55 onCreate
        ut99V52ScheduleImmersive(); // v52 onCreate
        // UT99_ANDROID_V63_CITYINTRO_START: do not cover CityIntro with the old static title/menu overlay.
        // ut99V52ShowStartupOverlay();
        ut99V50StageBranding();
        ut99V50Immersive(); // v50 onCreate
        stageBrandingAssetV47();
        applyUt99ImmersiveMode();
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
        if (isAndroidGamepadSourceV47(event) || isOuyaMenuKeyV79(keyCode) || textDeleteKey) {
            if (action == android.view.KeyEvent.ACTION_DOWN || action == android.view.KeyEvent.ACTION_UP) {
                nativeAndroidButtonV47(keyCode, action == android.view.KeyEvent.ACTION_DOWN);
                android.util.Log.i("UT99Android", "v80 android key code=" + keyCode + " down=" + (action == android.view.KeyEvent.ACTION_DOWN));
                return true;
            }
        }
        return super.dispatchKeyEvent(event);
    }

    @Override
    public boolean onGenericMotionEvent(android.view.MotionEvent event) {
        if (isAndroidGamepadSourceV47(event) && event.getAction() == android.view.MotionEvent.ACTION_MOVE) {
            nativeAndroidAxisV47(android.view.MotionEvent.AXIS_X, applyDeadzoneV47(event.getAxisValue(android.view.MotionEvent.AXIS_X), 0.12f));
            nativeAndroidAxisV47(android.view.MotionEvent.AXIS_Y, applyDeadzoneV47(event.getAxisValue(android.view.MotionEvent.AXIS_Y), 0.12f));
            nativeAndroidAxisV47(android.view.MotionEvent.AXIS_Z, applyDeadzoneV47(event.getAxisValue(android.view.MotionEvent.AXIS_Z), 0.10f));
            nativeAndroidAxisV47(android.view.MotionEvent.AXIS_RZ, applyDeadzoneV47(event.getAxisValue(android.view.MotionEvent.AXIS_RZ), 0.10f));
            nativeAndroidAxisV47(android.view.MotionEvent.AXIS_LTRIGGER, event.getAxisValue(android.view.MotionEvent.AXIS_LTRIGGER));
            nativeAndroidAxisV47(android.view.MotionEvent.AXIS_RTRIGGER, event.getAxisValue(android.view.MotionEvent.AXIS_RTRIGGER));
            nativeAndroidAxisV47(android.view.MotionEvent.AXIS_HAT_X, event.getAxisValue(android.view.MotionEvent.AXIS_HAT_X));
            nativeAndroidAxisV47(android.view.MotionEvent.AXIS_HAT_Y, event.getAxisValue(android.view.MotionEvent.AXIS_HAT_Y));
            android.util.Log.i("UT99Android", "v47 android axis lx=" + event.getAxisValue(android.view.MotionEvent.AXIS_X)
                    + " ly=" + event.getAxisValue(android.view.MotionEvent.AXIS_Y)
                    + " rx=" + event.getAxisValue(android.view.MotionEvent.AXIS_Z)
                    + " ry=" + event.getAxisValue(android.view.MotionEvent.AXIS_RZ));
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


    // UT99_ANDROID_V55_FIXED_SURFACE_960X540
    private android.os.Handler ut99V55Handler;
    private static final int UT99_V55_SURFACE_W = 960;
    private static final int UT99_V55_SURFACE_H = 540;

    private void ut99V55ApplyFixedSurfaceToView(android.view.View view) {
        if (view == null) return;

        try {
            if (view instanceof android.view.SurfaceView) {
                android.view.SurfaceView sv = (android.view.SurfaceView)view;

                // v59: fullscreen visual layout, explicit touch scaling in Activity.
                ut99V59ApplyFullscreenLayoutOnce(sv);

                if (!ut99V56SurfaceFixedOnce) {
                    android.view.SurfaceHolder holder = sv.getHolder();
                    if (holder != null) {
                        holder.setFixedSize(UT99_V55_SURFACE_W, UT99_V55_SURFACE_H);
                    }
                    ut99V56SurfaceFixedOnce = true;
                    android.util.Log.i("UT99Android", "UT99_ANDROID_V56_INPUT_SAFE_FIXED_SURFACE fixed once on " + sv.getClass().getName());
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
            android.util.Log.e("UT99Android", "v59 fixed-surface fullscreen-touchscale patch failed", t);
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
        if (sv == null || ut99V58ScaledSurfaceLayoutOnce) return;

        try {
            int screenW = 0;
            int screenH = 0;

            android.view.View decor = null;
            try {
                decor = getWindow().getDecorView();
            } catch (Throwable ignored) {
            }

            if (decor != null && decor.getWidth() > 0 && decor.getHeight() > 0) {
                screenW = decor.getWidth();
                screenH = decor.getHeight();
            }

            if (screenW <= 0 || screenH <= 0) {
                android.util.DisplayMetrics dm = new android.util.DisplayMetrics();
                try {
                    getWindowManager().getDefaultDisplay().getRealMetrics(dm);
                } catch (Throwable ignored) {
                    dm = getResources().getDisplayMetrics();
                }
                screenW = dm.widthPixels;
                screenH = dm.heightPixels;
            }

            if (screenW <= 0) screenW = UT99_V55_SURFACE_W;
            if (screenH <= 0) screenH = UT99_V55_SURFACE_H;

            float sx = ((float)screenW) / ((float)UT99_V55_SURFACE_W);
            float sy = ((float)screenH) / ((float)UT99_V55_SURFACE_H);

            android.view.ViewParent parent = sv.getParent();
            if (parent instanceof android.view.ViewGroup) {
                android.view.ViewGroup vg = (android.view.ViewGroup)parent;
                vg.setClipChildren(false);
                vg.setClipToPadding(false);
            }

            android.view.ViewGroup.LayoutParams lp = sv.getLayoutParams();
            if (lp == null) {
                lp = new android.view.ViewGroup.LayoutParams(UT99_V55_SURFACE_W, UT99_V55_SURFACE_H);
            }

            // v58 key change:
            // The View itself stays 960x540, so MotionEvent local coords remain 0..960/0..540.
            // Only the visual transform scales it to the physical screen.
            lp.width = UT99_V55_SURFACE_W;
            lp.height = UT99_V55_SURFACE_H;

            if (lp instanceof android.view.ViewGroup.MarginLayoutParams) {
                android.view.ViewGroup.MarginLayoutParams mlp = (android.view.ViewGroup.MarginLayoutParams)lp;
                mlp.leftMargin = 0;
                mlp.topMargin = 0;
                mlp.rightMargin = 0;
                mlp.bottomMargin = 0;
            }

            if (lp instanceof android.widget.FrameLayout.LayoutParams) {
                android.widget.FrameLayout.LayoutParams flp = (android.widget.FrameLayout.LayoutParams)lp;
                flp.gravity = android.view.Gravity.LEFT | android.view.Gravity.TOP;
            }

            sv.setLayoutParams(lp);
            sv.setPivotX(0.0f);
            sv.setPivotY(0.0f);
            sv.setX(0.0f);
            sv.setY(0.0f);
            sv.setTranslationX(0.0f);
            sv.setTranslationY(0.0f);
            sv.setScaleX(sx);
            sv.setScaleY(sy);
            sv.setKeepScreenOn(true);
            sv.setFocusable(true);
            sv.setFocusableInTouchMode(true);
            sv.requestLayout();
            sv.invalidate();

            try {
                sv.requestFocus();
            } catch (Throwable ignored) {
            }

            ut99V58ScaledSurfaceLayoutOnce = true;
            android.util.Log.i("UT99Android", "UT99_ANDROID_V58_SCALED_SURFACE_INPUT_SAFE_LAYOUT applied surface=960x540 screen=" + screenW + "x" + screenH + " scale=" + sx + "x" + sy);
        } catch (Throwable t) {
            android.util.Log.e("UT99Android", "v58 scaled SurfaceView layout failed", t);
        }
    }


    // UT99_ANDROID_V59_FULLSCREEN_TOUCHSCALE
    private boolean ut99V59FullscreenLayoutOnce = false;
    private boolean ut99V59TouchScaleEnabled = true;
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

            // v59: use the visually-working v57 layout again.
            // Surface buffer stays fixed at 960x540, but SurfaceView occupies fullscreen.
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
            ut99V59TouchScaleEnabled = true;
            android.util.Log.i("UT99Android", "UT99_ANDROID_V59_FULLSCREEN_TOUCHSCALE_LAYOUT applied");
        } catch (Throwable t) {
            android.util.Log.e("UT99Android", "v59 fullscreen touchscale layout failed", t);
        }
    }

    @Override
    public boolean dispatchTouchEvent(android.view.MotionEvent ev) {
        if (ut99V59TouchScaleEnabled && ev != null) {
            try {
                android.view.View decor = getWindow().getDecorView();
                int screenW = (decor != null && decor.getWidth() > 0) ? decor.getWidth() : getResources().getDisplayMetrics().widthPixels;
                int screenH = (decor != null && decor.getHeight() > 0) ? decor.getHeight() : getResources().getDisplayMetrics().heightPixels;

                if (screenW > UT99_V55_SURFACE_W || screenH > UT99_V55_SURFACE_H) {
                    float sx = ((float)UT99_V55_SURFACE_W) / ((float)Math.max(1, screenW));
                    float sy = ((float)UT99_V55_SURFACE_H) / ((float)Math.max(1, screenH));

                    float rawX = ev.getX();
                    float rawY = ev.getY();
                    float scaledX = Math.max(0.0f, Math.min((float)(UT99_V55_SURFACE_W - 1), rawX * sx));
                    float scaledY = Math.max(0.0f, Math.min((float)(UT99_V55_SURFACE_H - 1), rawY * sy));

                    android.view.MotionEvent copy = android.view.MotionEvent.obtain(ev);
                    copy.setLocation(scaledX, scaledY);

                    long now = android.os.SystemClock.uptimeMillis();
                    if (now - ut99V59LastTouchLogMs > 750L) {
                        ut99V59LastTouchLogMs = now;
                        android.util.Log.i("UT99Android", "UT99_ANDROID_V59_TOUCHSCALE dispatch raw=" + rawX + "," + rawY + " scaled=" + scaledX + "," + scaledY + " screen=" + screenW + "x" + screenH);
                    }

                    try {
                        return super.dispatchTouchEvent(copy);
                    } finally {
                        copy.recycle();
                    }
                }
            } catch (Throwable t) {
                android.util.Log.e("UT99Android", "v59 touchscale dispatch failed", t);
            }
        }

        return super.dispatchTouchEvent(ev);
    }
}
