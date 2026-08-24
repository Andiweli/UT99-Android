package com.ast.ut99;

import android.content.Context;
import android.util.Log;


import java.io.File;
import java.io.FileOutputStream;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.IOException;
import java.nio.charset.Charset;

final class UT99Paths {
    static final String DATA_DIR_NAME = "UT99";
    static final String EXTRA_DATA_ROOT = "com.ast.ut99.EXTRA_DATA_ROOT";
    static final String EXTRA_LEGACY_SAFE_MODE = "com.ast.ut99.EXTRA_LEGACY_SAFE_MODE";
    private static final String PREFS = "ut99_paths_v79";
    private static final String KEY_LAST_DATA_ROOT = "last_data_root";
    private static final String KEY_SYSTEM_PATCH_FAST_ROOT = "system_patch_fast_root_v164";
    private static final String KEY_SYSTEM_PATCH_FAST_VERSION = "system_patch_fast_version_v164";
    private static final String KEY_SYSTEM_PATCH_APK_LAST_UPDATE = "system_patch_apk_last_update_v166k";
    private static final String ANDROID_CONTROLLER_FAST_MARKER = ".ut99_android_controller_full_remap_v120_fast_v1l";

    // UT99_ANDROID_V136_BUNDLED_UMENU_PACKAGE:
    // Put the rebuilt UMenu.u into app/src/main/assets/ut99_patches/System/UMenu.u.
    // On launch it is copied into <files>/UT99/System/UMenu.u once per version,
    // so release APKs carry the lean Android menu without manual adb pushes.
    private static final String TAG = "UT99Paths";
    private static final int UT_PACKAGE_TAG = 0x9E2A83C1;
    private static final int UT_V400_PACKAGE_VERSION = 68;
    private static final String BUNDLED_SYSTEM_PATCH_DIR = "ut99_patches/System";
    private static final String BUNDLED_SYSTEM_PATCH_MARKER = ".ut99_android_bundled_system_patches_v166k_apk_install_refresh";
    private static final String BUNDLED_SYSTEM_PATCH_VERSION = "UT99_ANDROID_V166K_APK_INSTALL_UMENU_REFRESH_20260531";

    private UT99Paths() {
    }

    static File appFilesRoot(Context context) {
        File external = context.getExternalFilesDir(null);
        if (external != null) {
            return external;
        }
        // Extremely defensive fallback for devices where external app storage is temporarily unavailable.
        return new File(context.getFilesDir(), "external-files");
    }

    static File preferredRoot(Context context) {
        return new File(appFilesRoot(context), DATA_DIR_NAME);
    }

    static File legacyRoot(Context context) {
        return appFilesRoot(context);
    }

    static File homeDir(Context context) {
        return new File(context.getFilesDir(), "home");
    }

    static File installRoot(Context context) {
        return preferredRoot(context);
    }


    static void normalizeInstalledDataRoot(File root) {
        if (root == null || !root.exists() || !root.isDirectory()) {
            return;
        }

        String[] canonicalDirs = {"System", "Maps", "Textures", "Sounds", "Music", "Cache", "Save", "Logs"};
        for (String name : canonicalDirs) {
            try {
                mergeCaseVariantDirectory(root, name);
            } catch (IOException ignored) {
                // Best-effort only. The engine can still try the existing layout.
            }
        }

        File system = new File(root, "System");
        String[] systemFiles = {
                "Core.u", "Engine.u", "Botpack.u", "UTMenu.u", "UMenu.u", "UWindow.u",
                "Fire.u", "IpDrv.u", "Render.u", "UnrealShare.u", "UnrealI.u", "Editor.u",
                "UnrealTournament.ini", "Default.ini", "AndroidUT99.ini", "AndroidUser.ini", "NOpenGLESDrv.int", "UMenu.int"
        };
        for (String name : systemFiles) {
            renameCaseVariantFile(system, name);
        }

        File maps = new File(root, "Maps");
        String[] mapFiles = {"CityIntro.unr", "Entry.unr"};
        for (String name : mapFiles) {
            renameCaseVariantFile(maps, name);
        }
    }

    static boolean hasLaunchableGameData(File root) {
        if (!hasUsableGameData(root)) {
            return false;
        }
        normalizeInstalledDataRoot(root);
        File system = new File(root, "System");
        File maps = new File(root, "Maps");
        return isSupportedV400Package(new File(system, "Core.u"))
                && isSupportedV400Package(new File(system, "Engine.u"))
                && isSupportedV400Package(new File(system, "Botpack.u"))
                && isSupportedV400Package(new File(maps, "CityIntro.unr"));
    }

    private static void mergeCaseVariantDirectory(File root, String canonicalName) throws IOException {
        if (root == null || canonicalName == null) return;
        File exact = new File(root, canonicalName);
        if (exact.exists()) {
            File[] children = root.listFiles();
            if (children != null) {
                for (File child : children) {
                    if (child != null
                            && child.isDirectory()
                            && !child.getName().equals(canonicalName)
                            && child.getName().equalsIgnoreCase(canonicalName)) {
                        mergeDirectoryContents(child, exact);
                        deleteEmptyDirectory(child);
                    }
                }
            }
            return;
        }

        File variant = findCaseVariant(root, canonicalName);
        if (variant == null) return;
        if (!variant.renameTo(exact)) {
            if (variant.isDirectory()) {
                mergeDirectoryContents(variant, exact);
                deleteEmptyDirectory(variant);
            }
        }
    }

    private static File findCaseVariant(File dir, String canonicalName) {
        if (dir == null || canonicalName == null || !dir.isDirectory()) return null;
        File[] children = dir.listFiles();
        if (children == null) return null;
        for (File child : children) {
            if (child != null && child.getName().equals(canonicalName)) return child;
        }
        for (File child : children) {
            if (child != null && child.getName().equalsIgnoreCase(canonicalName)) return child;
        }
        return null;
    }

    private static void renameCaseVariantFile(File dir, String canonicalName) {
        if (dir == null || canonicalName == null || !dir.isDirectory()) return;
        File exact = new File(dir, canonicalName);
        if (exact.isFile()) return;
        File variant = findCaseVariant(dir, canonicalName);
        if (variant != null && variant.isFile()) {
            variant.renameTo(exact);
        }
    }

    private static void mergeDirectoryContents(File source, File target) throws IOException {
        if (source == null || target == null || !source.isDirectory()) return;
        if (!target.exists() && !target.mkdirs()) {
            throw new IOException("Cannot create " + target.getAbsolutePath());
        }
        File[] children = source.listFiles();
        if (children == null) return;
        for (File child : children) {
            if (child == null) continue;
            File out = new File(target, child.getName());
            File caseOut = findCaseVariant(target, child.getName());
            if (caseOut != null) out = caseOut;
            if (child.isDirectory()) {
                mergeDirectoryContents(child, out);
                deleteEmptyDirectory(child);
            } else if (child.isFile()) {
                if (!out.exists()) {
                    if (!child.renameTo(out)) {
                        copySmallFile(child, out);
                        child.delete();
                    }
                }
            }
        }
    }

    private static void copySmallFile(File source, File target) throws IOException {
        File parent = target.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            throw new IOException("Cannot create " + parent.getAbsolutePath());
        }
        java.io.FileInputStream in = new java.io.FileInputStream(source);
        try {
            java.io.FileOutputStream out = new java.io.FileOutputStream(target, false);
            try {
                byte[] buffer = new byte[64 * 1024];
                int read;
                while ((read = in.read(buffer)) != -1) {
                    out.write(buffer, 0, read);
                }
            } finally {
                out.close();
            }
        } finally {
            in.close();
        }
    }

    private static void deleteEmptyDirectory(File dir) {
        if (dir == null || !dir.isDirectory()) return;
        File[] children = dir.listFiles();
        if (children == null || children.length == 0) {
            dir.delete();
        }
    }

    private static boolean sameFilePath(File a, File b) {
        if (a == null || b == null) return false;
        try {
            return a.getCanonicalPath().equals(b.getCanonicalPath());
        } catch (IOException ignored) {
            return a.getAbsolutePath().equals(b.getAbsolutePath());
        }
    }

    static File resolveDataRoot(Context context) {
        File remembered = rememberedDataRoot(context);
        if (hasUsableGameData(remembered)) {
            return remembered;
        }

        File preferred = preferredRoot(context);
        if (hasUsableGameData(preferred)) {
            rememberDataRoot(context, preferred);
            return preferred;
        }

        // Older test builds and some manual copies placed System/Maps/... directly below files/.
        // Keep supporting that layout so a misplaced data copy does not hard-crash at startup.
        File legacy = legacyRoot(context);
        if (hasUsableGameData(legacy)) {
            rememberDataRoot(context, legacy);
            return legacy;
        }

        ensureSkeleton(preferred);
        return preferred;
    }

    static void rememberDataRoot(Context context, File root) {
        if (context == null || root == null || !hasUsableGameData(root)) {
            return;
        }
        try {
            context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
                    .edit()
                    .putString(KEY_LAST_DATA_ROOT, root.getAbsolutePath())
                    .commit();
        } catch (Throwable ignored) {
        }
    }

    private static File rememberedDataRoot(Context context) {
        if (context == null) return null;
        try {
            String path = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
                    .getString(KEY_LAST_DATA_ROOT, null);
            if (path != null && path.length() > 0) {
                return new File(path);
            }
        } catch (Throwable ignored) {
        }
        return null;
    }

    static boolean hasUsableGameData(File root) {
        if (root == null || !root.isDirectory()) {
            return false;
        }

        File system = new File(root, "System");
        if (!system.isDirectory()) {
            return false;
        }

        String[] requiredDirs = {"Maps", "Textures", "Sounds", "Music"};
        for (String name : requiredDirs) {
            if (!new File(root, name).isDirectory()) {
                return false;
            }
        }

        // Native field offsets and replication tables in this port target the
        // v400 package format. A directory or INI marker alone is insufficient:
        // later Core/Engine/Botpack packages can load far enough to corrupt native
        // state before failing. Validate the package tag and exact file version.
        return isSupportedV400Package(new File(system, "Core.u"))
                && isSupportedV400Package(new File(system, "Engine.u"))
                && isSupportedV400Package(new File(system, "Botpack.u"));
    }

    private static boolean isSupportedV400Package(File file) {
        if (file == null || !file.isFile() || file.length() < 8) {
            return false;
        }

        FileInputStream input = null;
        try {
            input = new FileInputStream(file);
            byte[] header = new byte[8];
            int offset = 0;
            while (offset < header.length) {
                int count = input.read(header, offset, header.length - offset);
                if (count < 0) {
                    return false;
                }
                offset += count;
            }

            int tag = (header[0] & 0xff)
                    | ((header[1] & 0xff) << 8)
                    | ((header[2] & 0xff) << 16)
                    | ((header[3] & 0xff) << 24);
            int version = (header[4] & 0xff)
                    | ((header[5] & 0xff) << 8)
                    | ((header[6] & 0xff) << 16)
                    | ((header[7] & 0xff) << 24);
            return tag == UT_PACKAGE_TAG && version == UT_V400_PACKAGE_VERSION;
        } catch (IOException ignored) {
            return false;
        } finally {
            if (input != null) {
                try {
                    input.close();
                } catch (IOException ignored) {
                }
            }
        }
    }

    static void ensureSkeleton(File root) {
        if (root == null) {
            return;
        }
        if (!root.exists()) {
            root.mkdirs();
        }
        String[] dirs = {"System", "Maps", "Textures", "Sounds", "Music", "Cache", "Save", "Logs"};
        for (String name : dirs) {
            File dir = new File(root, name);
            if (!dir.exists()) {
                dir.mkdirs();
            }
        }
    }

    static void ensureBundledSystemPatches(Context context, File root) throws IOException {
        if (context == null || root == null) {
            return;
        }
        long started = System.nanoTime();

        // UT99_ANDROID_V166K_APK_INSTALL_UMENU_REFRESH:
        // Keep the v163/v1j fast path for normal launches, but do not let it hide
        // a bundled UMenu.u update after the APK was freshly installed over an
        // existing data folder. Android keeps app data and SharedPreferences on
        // over-install, so use the APK lastUpdateTime as a cheap install/update
        // marker and force UMenu.u refresh once after every APK install.
        String rootPath = root.getAbsolutePath();
        long apkLastUpdateTime = currentApkLastUpdateTime(context);
        boolean apkInstallRefreshNeeded = isApkInstallRefreshNeededV166k(context, apkLastUpdateTime);
        try {
            android.content.SharedPreferences prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
            String fastRoot = prefs.getString(KEY_SYSTEM_PATCH_FAST_ROOT, null);
            String fastVersion = prefs.getString(KEY_SYSTEM_PATCH_FAST_VERSION, null);
            if (!apkInstallRefreshNeeded && rootPath.equals(fastRoot) && BUNDLED_SYSTEM_PATCH_VERSION.equals(fastVersion)) {
                long elapsedMs = (System.nanoTime() - started) / 1000000L;
                Log.i(TAG, "UT99_ANDROID_V166K_SYSTEM_PATCHES: shared-pref current, skipped filesystem and asset probes packages=2");
                Log.i("UT99_LOADPROF", "JAVA_SYSTEM_PATCHES copied=false skipped=2 markerSkip=true prefSkip=true apkRefresh=false ms=" + elapsedMs);
                return;
            }
        } catch (Throwable ignored) {
        }

        ensureSkeleton(root);
        File system = new File(root, "System");
        if (!system.isDirectory() && !system.mkdirs()) {
            throw new IOException("Cannot create System folder: " + system.getAbsolutePath());
        }

        // UT99_ANDROID_V161_LOADPROF_FAST_SYSTEM_PATCH_COPY:
        // v160 intentionally refreshed bundled UMenu.u/UTMenu.u on every launch,
        // which was safe for menu rollout but wastes I/O every start. v161 keeps
        // the same safety on first run/new marker, then skips copying while the
        // installed marker and files are already current.
        String[] patchPackages = {"UMenu.u", "UTMenu.u"};
        File marker = new File(system, BUNDLED_SYSTEM_PATCH_MARKER);
        boolean markerCurrent = false;
        if (marker.isFile()) {
            String markerText = readUtf8(marker);
            markerCurrent = markerText.indexOf(BUNDLED_SYSTEM_PATCH_VERSION) >= 0;
        }

        // UT99_ANDROID_V163_SYSTEM_PATCH_MARKER_FAST_SKIP:
        // OUYA showed almost 3 seconds spent here even when both packages were already current.
        // The expensive part is probing APK assets on Android 4.x.  Once the marker and target
        // files are present, skip the asset-open path completely.
        boolean allTargetsPresent = true;
        for (String packageName : patchPackages) {
            File target = new File(system, packageName);
            if (!target.isFile() || target.length() <= 0L) {
                allTargetsPresent = false;
                break;
            }
        }
        if (!apkInstallRefreshNeeded && markerCurrent && allTargetsPresent) {
            rememberFastSystemPatchSkip(context, rootPath);
            long elapsedMs = (System.nanoTime() - started) / 1000000L;
            Log.i(TAG, "UT99_ANDROID_V166K_SYSTEM_PATCHES: marker current, skipped asset probes packages=" + patchPackages.length);
            Log.i("UT99_LOADPROF", "JAVA_SYSTEM_PATCHES copied=false skipped=" + patchPackages.length + " markerSkip=true prefSkip=false apkRefresh=false ms=" + elapsedMs);
            return;
        }

        boolean copiedAny = false;
        int skippedCurrent = 0;
        for (String packageName : patchPackages) {
            String assetPath = BUNDLED_SYSTEM_PATCH_DIR + "/" + packageName;
            if (!assetExists(context, assetPath)) {
                Log.i(TAG, "UT99_ANDROID_V161_SYSTEM_PATCHES: asset missing, keeping existing " + packageName);
                continue;
            }

            File target = new File(system, packageName);
            boolean forceUMenuForApkInstall = apkInstallRefreshNeeded && "UMenu.u".equals(packageName);
            if (!forceUMenuForApkInstall && markerCurrent && target.isFile() && target.length() > 0L) {
                skippedCurrent++;
                Log.i(TAG, "UT99_ANDROID_V166K_SYSTEM_PATCHES: current, skipped System/" + packageName + " size=" + target.length());
                continue;
            }

            copyAssetToFile(context, assetPath, target);
            copiedAny = true;
            Log.i(TAG, "UT99_ANDROID_V166K_SYSTEM_PATCHES: refreshed bundled System/" + packageName
                    + " size=" + target.length()
                    + " forceApkInstall=" + forceUMenuForApkInstall);
        }

        if (copiedAny || !markerCurrent || !marker.isFile()) {
            writeUtf8(marker, BUNDLED_SYSTEM_PATCH_VERSION + "\n");
        }
        if (new File(system, "UMenu.u").isFile() && new File(system, "UTMenu.u").isFile()) {
            rememberFastSystemPatchSkip(context, rootPath);
            rememberApkInstallRefreshV166k(context, apkLastUpdateTime);
        }
        long elapsedMs = (System.nanoTime() - started) / 1000000L;
        Log.i("UT99_LOADPROF", "JAVA_SYSTEM_PATCHES copied=" + copiedAny
                + " skipped=" + skippedCurrent
                + " markerSkip=false prefSkip=false apkRefresh=" + apkInstallRefreshNeeded
                + " ms=" + elapsedMs);
    }

    private static long currentApkLastUpdateTime(Context context) {
        if (context == null) {
            return 0L;
        }
        try {
            android.content.pm.PackageInfo info = context.getPackageManager().getPackageInfo(context.getPackageName(), 0);
            return info != null ? info.lastUpdateTime : 0L;
        } catch (Throwable ignored) {
            return 0L;
        }
    }

    private static boolean isApkInstallRefreshNeededV166k(Context context, long apkLastUpdateTime) {
        if (context == null || apkLastUpdateTime <= 0L) {
            return false;
        }
        try {
            long remembered = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
                    .getLong(KEY_SYSTEM_PATCH_APK_LAST_UPDATE, 0L);
            return remembered != apkLastUpdateTime;
        } catch (Throwable ignored) {
            return false;
        }
    }

    private static void rememberApkInstallRefreshV166k(Context context, long apkLastUpdateTime) {
        if (context == null || apkLastUpdateTime <= 0L) {
            return;
        }
        try {
            context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
                    .edit()
                    .putLong(KEY_SYSTEM_PATCH_APK_LAST_UPDATE, apkLastUpdateTime)
                    .commit();
        } catch (Throwable ignored) {
        }
    }

    private static void rememberFastSystemPatchSkip(Context context, String rootPath) {
        if (context == null || rootPath == null || rootPath.length() == 0) {
            return;
        }
        try {
            context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
                    .edit()
                    .putString(KEY_SYSTEM_PATCH_FAST_ROOT, rootPath)
                    .putString(KEY_SYSTEM_PATCH_FAST_VERSION, BUNDLED_SYSTEM_PATCH_VERSION)
                    .commit();
        } catch (Throwable ignored) {
        }
    }

    private static boolean assetExists(Context context, String assetPath) {
        InputStream in = null;
        try {
            in = context.getAssets().open(assetPath);
            return true;
        } catch (IOException ignored) {
            return false;
        } finally {
            if (in != null) {
                try { in.close(); } catch (IOException ignored) {}
            }
        }
    }

    private static void copyAssetToFile(Context context, String assetPath, File target) throws IOException {
        File parent = target.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            throw new IOException("Cannot create folder: " + parent.getAbsolutePath());
        }

        File tmp = new File(target.getParentFile(), target.getName() + ".tmp");
        InputStream in = context.getAssets().open(assetPath);
        try {
            FileOutputStream out = new FileOutputStream(tmp, false);
            try {
                byte[] buffer = new byte[64 * 1024];
                int read;
                while ((read = in.read(buffer)) != -1) {
                    out.write(buffer, 0, read);
                }
                out.flush();
            } finally {
                out.close();
            }
        } finally {
            in.close();
        }

        if (target.exists() && !target.delete()) {
            throw new IOException("Cannot replace old file: " + target.getAbsolutePath());
        }
        if (!tmp.renameTo(target)) {
            copySmallFile(tmp, target);
            tmp.delete();
        }
    }

    static boolean ensureAndroidIni(File root) throws IOException {
        if (root == null) {
            throw new IOException("Data root is null");
        }
        ensureSkeleton(root);
        File system = new File(root, "System");
        if (!system.isDirectory() && !system.mkdirs()) {
            throw new IOException("Cannot create System folder: " + system.getAbsolutePath());
        }
        ensureOpenGLESRegistry(system);

        // UT99_ANDROID_V138_VISUAL_GAMEPLAY_DEFAULTS:
        // Keep gameplay-visible transient effects enabled on Android/OUYA.
        // Weapon.RenderOverlays() only draws first-person muzzle flash while
        // Level.bHighDetailMode is true, and that flag follows the active
        // renderer's HighDetailActors config.  Older generated AndroidUT99.ini
        // files did not contain that key, so preserve user settings but append
        // a final override once.
        ensureAndroidVisualGameplayConfig(system);
        ensureAndroidWidescreenFovConfig(system);

        // UT99_ANDROID_V86_CONFIG_PRESERVE:
        // Do not rewrite AndroidUT99.ini / AndroidUser.ini after they already
        // exist.  Unreal's menus save user changes into these files; older
        // Android bootstrap code recreated them on every launch and thereby
        // discarded all UI changes.
        boolean created = false;
        File mainIni = new File(system, "AndroidUT99.ini");
        File userIni = new File(system, "AndroidUser.ini");
        if (!mainIni.isFile() || mainIni.length() == 0L) {
            writeUtf8(mainIni, buildAndroidIni());
            created = true;
        }
        if (!userIni.isFile() || userIni.length() == 0L) {
            writeUtf8(userIni, buildAndroidUserIni());
            created = true;
        }
        ensureAndroidEnglishLanguageConfig(system);
        ensureAndroidLeanMenuConfig(system);
        ensureAndroidControllerBindingConfig(system); // UT99_ANDROID_CONTROLLER_BINDING_FIX_V117
        ensureAndroidControllerFriendlyKeyNames(system); // UT99_ANDROID_CONTROLLER_KEY_NAMES_V118
        return created;
    }



    private static String buildAndroidControllerFriendlyKeyNamesIntV118() {
        return "; UT99_ANDROID_CONTROLLER_KEY_NAMES_V118\n" +
                "; Friendly controller labels for Preferences > Controls.\n" +
                "; Technical binding names stay unchanged in AndroidUser.ini.\n" +
                "[UMenuCustomizeClientWindow]\n" +
                "LocalizedKeyName[200]=\"A\"\n" +
                "LocalizedKeyName[201]=\"B\"\n" +
                "LocalizedKeyName[202]=\"X\"\n" +
                "LocalizedKeyName[203]=\"Y\"\n" +
                "LocalizedKeyName[204]=\"Back\"\n" +
                "LocalizedKeyName[205]=\"Guide\"\n" +
                "LocalizedKeyName[206]=\"Start\"\n" +
                "LocalizedKeyName[207]=\"LJoyPress\"\n" +
                "LocalizedKeyName[208]=\"RJoyPress\"\n" +
                "LocalizedKeyName[209]=\"ShoulderL\"\n" +
                "LocalizedKeyName[210]=\"ShoulderR\"\n" +
                "LocalizedKeyName[211]=\"TriggerL\"\n" +
                "LocalizedKeyName[212]=\"TriggerR\"\n" +
                "LocalizedKeyName[213]=\"RJoyLeft\"\n" +
                "LocalizedKeyName[214]=\"RJoyRight\"\n" +
                "LocalizedKeyName[215]=\"RJoyUp\"\n" +
                "LocalizedKeyName[216]=\"LJoyLeft\"\n" +
                "LocalizedKeyName[217]=\"LJoyRight\"\n" +
                "LocalizedKeyName[218]=\"LJoyUp\"\n" +
                "LocalizedKeyName[223]=\"LJoyDown\"\n" +
                "LocalizedKeyName[224]=\"LJoyAxisX\"\n" +
                "LocalizedKeyName[225]=\"LJoyAxisY\"\n" +
                "LocalizedKeyName[226]=\"RJoyAxisX\"\n" +
                "LocalizedKeyName[227]=\"RJoyAxisY\"\n" +
                "LocalizedKeyName[232]=\"RJoyX\"\n" +
                "LocalizedKeyName[233]=\"RJoyY\"\n" +
                "LocalizedKeyName[234]=\"RJoyDown\"\n" +
                "LocalizedKeyName[240]=\"DPadUp\"\n" +
                "LocalizedKeyName[241]=\"DPadDown\"\n" +
                "LocalizedKeyName[242]=\"DPadLeft\"\n" +
                "LocalizedKeyName[243]=\"DPadRight\"\n" +
                "\n";
    }

    private static void ensureAndroidControllerFriendlyKeyNames(File systemDir) throws IOException {
        if (systemDir == null) return;
        File uMenuInt = new File(systemDir, "UMenu.int");
        String text = readUtf8(uMenuInt);
        if (text.indexOf("UT99_ANDROID_CONTROLLER_KEY_NAMES_V118") >= 0) {
            return;
        }
        if (text.length() > 0 && !text.endsWith("\n")) text += "\n";
        writeUtf8(uMenuInt, text + "\n" + buildAndroidControllerFriendlyKeyNamesIntV118());
        Log.i(TAG, "UT99_ANDROID_CONTROLLER_KEY_NAMES_V118 wrote UMenu.int controller labels");
    }

    private static String androidControllerBindingDefaultsV117() {
        return "; UT99_ANDROID_CONTROLLER_BINDING_FIX_V117\n" +
                "; UT99_ANDROID_CONTROLLER_FULL_REMAP_V120\n" +
                "; Physical Android controller keys are bound directly so Preferences > Controls can remap them.\n" +
                "UnknownDA=MoveForward\n" +
                "UnknownDF=MoveBackward\n" +
                "UnknownD8=StrafeLeft\n" +
                "UnknownD9=StrafeRight\n" +
                "Joy1=Jump\n" +
                "Joy2=Duck\n" +
                "Joy3=ThrowWeapon\n" +
                "Joy4=\n" +
                "Joy8=\n" +
                "Joy9=\n" +
                "Joy10=Walking\n" +
                "Joy11=NextWeapon\n" +
                "Joy12=AltFire\n" +
                "Joy13=Fire\n" +
                "Joy14=TurnLeft\n" +
                "Joy15=TurnRight\n" +
                "Joy16=LookUp\n" +
                "UnknownEA=LookDown\n" +
                "JoyX=Axis aStrafe Speed=1\n" +
                "JoyY=Axis aBaseY Speed=1\n" +
                "JoyU=Axis aTurn Speed=1\n" +
                "JoyV=Axis aLookUp Speed=-1\n" +
                "JoyPovRight=NextWeapon\n" +
                "JoyPovLeft=PrevWeapon\n" +
                "JoyPovUp=InventoryPrevious\n" +
                "JoyPovDown=InventoryNext\n";
    }

    private static String buildAndroidDefUserIni() {
        return "[DefaultPlayer]\n" +
                "Name=Player\n" +
                "Class=Botpack.TMale1\n" +
                "team=0\n" +
                "skin=CommandoSkins.cmdo\n" +
                "Face=CommandoSkins.Blake\n" +
                "\n" +
                "[Engine.Input]\n" +
                "Aliases[0]=(Command=\"Button bFire | Fire\",Alias=Fire)\n" +
                "Aliases[1]=(Command=\"Button bAltFire | AltFire\",Alias=AltFire)\n" +
                "Aliases[2]=(Command=\"Axis aBaseY Speed=+300.0\",Alias=MoveForward)\n" +
                "Aliases[3]=(Command=\"Axis aBaseY Speed=-300.0\",Alias=MoveBackward)\n" +
                "Aliases[4]=(Command=\"Axis aBaseX Speed=-150.0\",Alias=TurnLeft)\n" +
                "Aliases[5]=(Command=\"Axis aBaseX Speed=+150.0\",Alias=TurnRight)\n" +
                "Aliases[6]=(Command=\"Axis aStrafe Speed=-300.0\",Alias=StrafeLeft)\n" +
                "Aliases[7]=(Command=\"Axis aStrafe Speed=+300.0\",Alias=StrafeRight)\n" +
                "Aliases[8]=(Command=\"Jump | Axis aUp Speed=+300.0\",Alias=Jump)\n" +
                "Aliases[9]=(Command=\"Button bDuck | Axis aUp Speed=-300.0\",Alias=Duck)\n" +
                "Escape=ShowMenu\n" +
                "Space=Jump\n" +
                "Enter=Fire\n" +
                "LeftMouse=Fire\n" +
                "RightMouse=AltFire\n" +
                "MouseX=Axis aMouseX Speed=1.0\n" +
                "MouseY=Axis aMouseY Speed=1.0\n" +
                "W=MoveForward\n" +
                "S=MoveBackward\n" +
                "A=StrafeLeft\n" +
                "D=StrafeRight\n" +
                androidControllerBindingDefaultsV117();
    }

    private static void ensureAndroidControllerBindingConfig(File systemDir) throws IOException {
        if (systemDir == null) return;

        File marker = new File(systemDir, ANDROID_CONTROLLER_FAST_MARKER);
        if (marker.isFile()) {
            return;
        }

        File defUser = new File(systemDir, "DefUser.ini");
        String defText = readUtf8(defUser);
        if (defText.indexOf("UT99_ANDROID_CONTROLLER_FULL_REMAP_V120") < 0) {
            writeUtf8(defUser, buildAndroidDefUserIni());
            Log.i(TAG, "UT99_ANDROID_CONTROLLER_FULL_REMAP_V120 wrote DefUser.ini reset defaults");
        }

        File userIni = new File(systemDir, "AndroidUser.ini");
        String text = readUtf8(userIni);
        if (text.length() == 0) {
            writeUtf8(userIni, buildAndroidUserIni());
            writeUtf8(marker, "UT99_ANDROID_CONTROLLER_FULL_REMAP_V120_FAST_V1L\n");
            Log.i(TAG, "UT99_ANDROID_CONTROLLER_FULL_REMAP_V120 wrote AndroidUser.ini defaults and fast marker");
            return;
        }

        if (text.indexOf("UT99_ANDROID_CONTROLLER_FULL_REMAP_V120") >= 0) {
            writeUtf8(marker, "UT99_ANDROID_CONTROLLER_FULL_REMAP_V120_FAST_V1L\n");
            Log.i(TAG, "UT99_ANDROID_CONTROLLER_FULL_REMAP_V120 fast marker current, skipped AndroidUser.ini migration");
            return;
        }

        String patched = text;
        boolean firstV120Migration = true;
        patched = ensureEngineInputLine(patched, "UnknownDA", "MoveForward", firstV120Migration);
        patched = ensureEngineInputLine(patched, "UnknownDF", "MoveBackward", firstV120Migration);
        patched = ensureEngineInputLine(patched, "UnknownD8", "StrafeLeft", firstV120Migration);
        patched = ensureEngineInputLine(patched, "UnknownD9", "StrafeRight", firstV120Migration);
        patched = ensureEngineInputLine(patched, "Joy1", "Jump", firstV120Migration);
        patched = ensureEngineInputLine(patched, "Joy2", "Duck", firstV120Migration);
        patched = ensureEngineInputLine(patched, "Joy3", "ThrowWeapon", firstV120Migration);
        patched = ensureEngineInputLine(patched, "Joy4", "", firstV120Migration);
        patched = ensureEngineInputLine(patched, "Joy8", "", firstV120Migration);
        patched = ensureEngineInputLine(patched, "Joy9", "", firstV120Migration);
        patched = ensureEngineInputLine(patched, "Joy10", "Walking", firstV120Migration);
        patched = ensureEngineInputLine(patched, "Joy11", "NextWeapon", firstV120Migration);
        patched = ensureEngineInputLine(patched, "Joy12", "AltFire", firstV120Migration);
        patched = ensureEngineInputLine(patched, "Joy13", "Fire", firstV120Migration);
        patched = ensureEngineInputLine(patched, "Joy14", "TurnLeft", firstV120Migration);
        patched = ensureEngineInputLine(patched, "Joy15", "TurnRight", firstV120Migration);
        patched = ensureEngineInputLine(patched, "Joy16", "LookUp", firstV120Migration);
        patched = ensureEngineInputLine(patched, "UnknownEA", "LookDown", firstV120Migration);
        patched = ensureEngineInputLine(patched, "JoyX", "Axis aStrafe Speed=1", firstV120Migration);
        patched = ensureEngineInputLine(patched, "JoyY", "Axis aBaseY Speed=1", firstV120Migration);
        patched = ensureEngineInputLine(patched, "JoyU", "Axis aTurn Speed=1", firstV120Migration);
        patched = ensureEngineInputLine(patched, "JoyV", "Axis aLookUp Speed=-1", firstV120Migration);
        patched = ensureEngineInputLine(patched, "JoyPovRight", "NextWeapon", firstV120Migration);
        patched = ensureEngineInputLine(patched, "JoyPovLeft", "PrevWeapon", firstV120Migration);
        patched = ensureEngineInputLine(patched, "JoyPovUp", "InventoryPrevious", firstV120Migration);
        patched = ensureEngineInputLine(patched, "JoyPovDown", "InventoryNext", firstV120Migration);
        if (patched.indexOf("UT99_ANDROID_CONTROLLER_FULL_REMAP_V120") < 0) {
            if (!patched.endsWith("\n")) patched += "\n";
            patched += "; UT99_ANDROID_CONTROLLER_FULL_REMAP_V120 existing-user migration complete\n";
        }
        if (!patched.equals(text)) {
            writeUtf8(userIni, patched);
            Log.i(TAG, "UT99_ANDROID_CONTROLLER_FULL_REMAP_V120 patched AndroidUser.ini controller defaults");
        }
        writeUtf8(marker, "UT99_ANDROID_CONTROLLER_FULL_REMAP_V120_FAST_V1L\n");
    }

    private static String ensureEngineInputLine(String text, String key, String value, boolean replaceExisting) {
        if (text == null) text = "";
        String lower = text.toLowerCase(java.util.Locale.US);
        String keyLower = key.toLowerCase(java.util.Locale.US);
        java.util.regex.Pattern p = java.util.regex.Pattern.compile("(?im)^" + java.util.regex.Pattern.quote(keyLower) + "\\s*=.*$");
        java.util.regex.Matcher m = p.matcher(text);
        if (m.find()) {
            return replaceExisting ? m.replaceAll(java.util.regex.Matcher.quoteReplacement(key + "=" + value)) : text;
        }
        if (text.length() > 0 && !text.endsWith("\n")) text += "\n";
        if (lower.indexOf("[engine.input]") < 0) {
            text += "\n[Engine.Input]\n";
        }
        return text + key + "=" + value + "\n";
    }


    private static void ensureAndroidVisualGameplayConfig(File systemDir) throws IOException {
        if (systemDir == null) return;

        String visualBlock = "\n\n; UT99_ANDROID_V138_VISUAL_GAMEPLAY_DEFAULTS\n" +
                "[Engine.Engine.ViewportManager]\n" +
                "AndroidWidescreenFOV=False\n" +
                "\n" +
                "[NOpenGLESDrv.NOpenGLESRenderDevice]\n" +
                "HighDetailActors=True\n" +
                "Coronas=True\n" +
                "VolumetricLighting=True\n" +
                "ShinySurfaces=True\n" +
                "\n" +
                "[NSDLDrv.NSDLClient]\n" +
                "ScreenFlashes=True\n" +
                "NoDynamicLights=False\n";

        String[] names = {"AndroidUT99.ini", "Default.ini", "UnrealTournament.ini", "DCUtil.ini", "DefaultDCUtil.ini"};
        for (String name : names) {
            File ini = new File(systemDir, name);
            if (!ini.isFile() || ini.length() == 0L) continue;
            String text = readBytePreservingText(ini);
            if (text.indexOf("UT99_ANDROID_V138_VISUAL_GAMEPLAY_DEFAULTS") < 0) {
                writeBytePreservingText(ini, text + visualBlock);
            }
        }
    }


    private static void ensureAndroidWidescreenFovConfig(File systemDir) throws IOException {
        if (systemDir == null) return;

        String[] names = {"AndroidUT99.ini", "Default.ini", "UnrealTournament.ini", "DCUtil.ini", "DefaultDCUtil.ini"};
        String section = "[Engine.Engine.ViewportManager]";
        for (String name : names) {
            File ini = new File(systemDir, name);
            if (!ini.isFile() || ini.length() == 0L) continue;

            String text = readBytePreservingText(ini);
            boolean hasViewportKey = java.util.regex.Pattern
                    .compile("(?im)^\\s*AndroidWidescreenFOV\\s*=\\s*(True|False|1|0|On|Off|Yes|No)\\s*$")
                    .matcher(text).find();

            boolean legacyTrue = false;
            java.util.regex.Matcher legacyMatcher = java.util.regex.Pattern
                    .compile("(?im)^\\s*WidescreenFOV\\s*=\\s*(True|False|1|0|On|Off|Yes|No)\\s*$")
                    .matcher(text);
            while (legacyMatcher.find()) {
                String value = legacyMatcher.group(1);
                if ("1".equalsIgnoreCase(value) || "true".equalsIgnoreCase(value) ||
                        "on".equalsIgnoreCase(value) || "yes".equalsIgnoreCase(value)) {
                    legacyTrue = true;
                }
            }

            if (hasViewportKey) {
                continue;
            }

            String valueToAdd = legacyTrue ? "True" : "False";
            int sectionPos = text.indexOf(section);
            String updated;
            if (sectionPos >= 0) {
                int insertPos = sectionPos + section.length();
                updated = text.substring(0, insertPos) + "\nAndroidWidescreenFOV=" + valueToAdd + text.substring(insertPos);
            } else {
                updated = text;
                if (updated.length() > 0 && !updated.endsWith("\n")) updated += "\n";
                updated += "\n; UT99_ANDROID_V159_WIDESCREEN_FOV_DEFAULT\n" + section + "\nAndroidWidescreenFOV=" + valueToAdd + "\n";
            }

            if (!updated.equals(text)) {
                writeBytePreservingText(ini, updated);
                Log.i(TAG, "UT99_ANDROID_V159_WIDESCREEN_FOV_DEFAULT added " + name + "=" + valueToAdd);
            }
        }
    }

    private static void ensureAndroidEnglishLanguageConfig(File systemDir) throws IOException {
        if (systemDir == null) return;

        // UT99_ANDROID_V204_ENGLISH_ONLY_LANGUAGE:
        // The Android port intentionally uses the original INT/English localization.
        // Force every known engine INI to Language=int on every launch so stale
        // Russian/other language settings from old installs cannot re-enable a
        // localization the Android package does not ship.  Read/write through
        // ISO-8859-1 so every non-ASCII byte in legacy CP1251 INIs is preserved
        // exactly; only the ASCII Language= line is changed.
        String[] names = {"AndroidUT99.ini", "Default.ini", "UnrealTournament.ini", "DCUtil.ini", "DefaultDCUtil.ini"};
        for (String name : names) {
            File ini = new File(systemDir, name);
            if (!ini.isFile() || ini.length() == 0L) continue;

            String text = readBytePreservingText(ini);
            String updated = forceEnglishLanguageValue(text);
            if (!updated.equals(text)) {
                writeBytePreservingText(ini, updated);
                Log.i(TAG, "UT99_ANDROID_V204_ENGLISH_ONLY_LANGUAGE forced " + name + " Language=int");
            }
        }
    }

    private static String forceEnglishLanguageValue(String text) {
        if (text == null) text = "";
        final String section = "[Engine.Engine]";
        final String eol = text.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
        String normalized = text.replace("\r\n", "\n").replace('\r', '\n');
        String[] lines = normalized.split("\n", -1);
        StringBuilder out = new StringBuilder(normalized.length() + 80);
        boolean inEngineSection = false;
        boolean sawEngineSection = false;
        boolean foundLanguage = false;

        for (int i = 0; i < lines.length; i++) {
            String line = lines[i];
            String trimmed = line.trim();

            if (trimmed.startsWith("[") && trimmed.endsWith("]")) {
                if (inEngineSection && !foundLanguage) {
                    out.append("Language=int").append('\n');
                    foundLanguage = true;
                }
                inEngineSection = "[engine.engine]".equalsIgnoreCase(trimmed);
                if (inEngineSection) {
                    sawEngineSection = true;
                    foundLanguage = false;
                }
            }

            if (inEngineSection && isLanguageLine(trimmed)) {
                // Keep indentation only; the value itself is intentionally fixed.
                int firstNonWs = 0;
                while (firstNonWs < line.length() && Character.isWhitespace(line.charAt(firstNonWs))) firstNonWs++;
                line = line.substring(0, firstNonWs) + "Language=int";
                foundLanguage = true;
            }

            out.append(line);
            if (i < lines.length - 1) out.append('\n');
        }

        if (inEngineSection && !foundLanguage) {
            if (out.length() > 0 && out.charAt(out.length() - 1) != '\n') out.append('\n');
            out.append("Language=int").append('\n');
        } else if (!sawEngineSection) {
            if (out.length() > 0 && out.charAt(out.length() - 1) != '\n') out.append('\n');
            out.append('\n')
                    .append("; UT99_ANDROID_V204_ENGLISH_ONLY_LANGUAGE").append('\n')
                    .append(section).append('\n')
                    .append("Language=int").append('\n');
        }

        String result = out.toString();
        if ("\r\n".equals(eol)) result = result.replace("\n", "\r\n");
        return result;
    }

    private static boolean isLanguageLine(String trimmed) {
        return java.util.regex.Pattern
                .compile("(?i)^Language\\s*=.*$")
                .matcher(trimmed).find();
    }

    private static String readBytePreservingText(File file) throws IOException {
        if (file == null || !file.isFile()) return "";
        FileInputStream in = new FileInputStream(file);
        try {
            byte[] data = new byte[(int) file.length()];
            int pos = 0;
            while (pos < data.length) {
                int n = in.read(data, pos, data.length - pos);
                if (n < 0) break;
                pos += n;
            }
            return new String(data, 0, pos, Charset.forName("ISO-8859-1"));
        } finally {
            in.close();
        }
    }

    private static void writeBytePreservingText(File file, String text) throws IOException {
        File parent = file.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            throw new IOException("Cannot create folder: " + parent.getAbsolutePath());
        }
        FileOutputStream out = new FileOutputStream(file, false);
        try {
            out.write(text.getBytes(Charset.forName("ISO-8859-1")));
        } finally {
            out.close();
        }
    }

    private static void ensureAndroidLeanMenuConfig(File systemDir) throws IOException {
        if (systemDir == null) return;

        // UT99_ANDROID_V135_LEAN_MENU_NO_LAN_AUTOSCAN:
        // The compiled UMenu/UBrowser packages are still the original UT99 ones,
        // but the active INI can at least keep context help off and stop the LAN
        // browser from having active list factories.  Direct Multiplayer > Open
        // Location still works for manual IP entry.
        String leanBlock = "\n\n; UT99_ANDROID_V135_LEAN_MENU_NO_LAN_AUTOSCAN\n" +
                "[UMenu.UMenuMenuBar]\n" +
                "ShowHelp=False\n" +
                "\n" +
                "[UBrowserLAN]\n" +
                "ServerListTitle=LAN Servers\n" +
                "ListFactories[0]=\n" +
                "ListFactories[1]=\n" +
                "ListFactories[2]=\n" +
                "ListFactories[3]=\n" +
                "ListFactories[4]=\n" +
                "ListFactories[5]=\n" +
                "ListFactories[6]=\n" +
                "ListFactories[7]=\n" +
                "ListFactories[8]=\n" +
                "ListFactories[9]=\n";

        String[] names = {"AndroidUT99.ini", "Default.ini", "UnrealTournament.ini", "DCUtil.ini", "DefaultDCUtil.ini"};
        for (String name : names) {
            File ini = new File(systemDir, name);
            if (!ini.isFile() || ini.length() == 0L) continue;
            String text = readBytePreservingText(ini);
            if (text.indexOf("UT99_ANDROID_V135_LEAN_MENU_NO_LAN_AUTOSCAN") < 0) {
                writeBytePreservingText(ini, text + leanBlock);
            }
        }
    }

    private static void ensureOpenGLESRegistry(File systemDir) throws IOException {
        if (systemDir == null) return;

        // UT99_ANDROID_V133_OPENGL_ES_VIDEO_DRIVER_LABEL:
        // Preferences > Video builds the driver combo from System/*.int [Public]
        // object entries. Keep the Android renderer label user-facing and stable.
        File glesInt = new File(systemDir, "NOpenGLESDrv.int");
        String text = "; UT99_ANDROID_V133_OPENGL_ES_VIDEO_DRIVER_LABEL\n" +
                "[Public]\n" +
                "Object=(Name=NOpenGLESDrv.NOpenGLESRenderDevice,Class=Class,MetaClass=Engine.RenderDevice,Description=OpenGLES,Autodetect=)\n" +
                "Preferences=(Caption=\"Rendering\",Parent=\"Advanced Options\")\n" +
                "Preferences=(Caption=\"OpenGLES\",Parent=\"Rendering\",Class=NOpenGLESDrv.NOpenGLESRenderDevice,Immediate=True)\n" +
                "\n" +
                "[NOpenGLESRenderDevice]\n" +
                "ClassCaption=OpenGLES\n";
        if (!glesInt.isFile() || glesInt.length() == 0L || readUtf8(glesInt).indexOf("UT99_ANDROID_V133_OPENGL_ES_VIDEO_DRIVER_LABEL") < 0) {
            writeUtf8(glesInt, text);
        }
    }

    private static String readUtf8(File file) throws IOException {
        if (file == null || !file.isFile()) {
            return "";
        }
        FileInputStream in = new FileInputStream(file);
        try {
            byte[] data = new byte[(int) file.length()];
            int pos = 0;
            while (pos < data.length) {
                int n = in.read(data, pos, data.length - pos);
                if (n < 0) {
                    break;
                }
                pos += n;
            }
            return new String(data, 0, pos, Charset.forName("UTF-8"));
        } finally {
            in.close();
        }
    }

    private static void writeUtf8(File file, String text) throws IOException {
        File parent = file.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            throw new IOException("Cannot create folder: " + parent.getAbsolutePath());
        }
        FileOutputStream out = new FileOutputStream(file, false);
        try {
            out.write(text.getBytes(Charset.forName("UTF-8")));
        } finally {
            out.close();
        }
    }

    private static String buildAndroidIni() {
        return "[URL]\n" +
                "Protocol=unreal\n" +
                "ProtocolDescription=Unreal Protocol\n" +
                "Name=Player\n" +
                "Map=CityIntro.unr\n" +
                "LocalMap=CityIntro.unr\n" +
                "Host=\n" +
                "Portal=\n" +
                "MapExt=unr\n" +
                "SaveExt=usa\n" +
                "Port=7777\n" +
                "Class=Botpack.TMale1\n" +
                "\n" +
                "[FirstRun]\n" +
                "FirstRun=436\n" +
                "\n" +
                "[Engine.Engine]\n" +
                "GameRenderDevice=NOpenGLESDrv.NOpenGLESRenderDevice\n" +
                "WindowedRenderDevice=NOpenGLESDrv.NOpenGLESRenderDevice\n" +
                "RenderDevice=NOpenGLESDrv.NOpenGLESRenderDevice\n" +
                "AudioDevice=Audio.GenericAudioSubsystem\n" +
                "NetworkDevice=IpDrv.TcpNetDriver\n" +
                "Console=UTMenu.UTConsole\n" +
                "Language=int\n" +
                "GameEngine=Engine.GameEngine\n" +
                "EditorEngine=Editor.EditorEngine\n" +
                "DefaultGame=Botpack.DeathMatchPlus\n" +
                "DefaultServerGame=Botpack.DeathMatchPlus\n" +
                "ViewportManager=NSDLDrv.NSDLClient\n" +
                "Render=Render.Render\n" +
                "Input=Engine.Input\n" +
                "Canvas=Engine.Canvas\n" +
                "CdPath=../\n" +
                "\n" +
                "[Core.System]\n" +
                "PurgeCacheDays=30\n" +
                "SavePath=../Save\n" +
                "CachePath=../Cache\n" +
                "CacheExt=.uxx\n" +
                "Paths=../System/*.u\n" +
                "Paths=../Maps/*.unr\n" +
                "Paths=../Textures/*.utx\n" +
                "Paths=../Sounds/*.uax\n" +
                "Paths=../Music/*.umx\n" +
                "Suppress=DevLoad\n" +
                "Suppress=DevSave\n" +
                "Suppress=DevNetTraffic\n" +
                "\n" +
                "[Engine.GameEngine]\n" +
                "CacheSizeMegs=4\n" +
                "UseSound=True\n" +
                "ServerActors=IpDrv.UdpBeacon\n" +
                "ServerPackages=Core\n" +
                "ServerPackages=Engine\n" +
                "ServerPackages=Fire\n" +
                "ServerPackages=Botpack\n" +
                "\n" +
                "[NSDLDrv.NSDLClient]\n" +
                "DefaultDisplay=0\n" +
                "ScreenFlashes=True\n" +
                "NoDynamicLights=False\n" +
                "StartupFullscreen=True\n" +
                "UseJoystick=True\n" +
                "DeadZoneXYZ=0.100000\n" +
                "DeadZoneRUV=0.100000\n" +
                "ScaleXYZ=100.000000\n" +
                "ScaleRUV=100.000000\n" +
                "InvertY=False\n" +
                "InvertV=False\n" +
                "\n" +
                "[Engine.Engine.ViewportManager]\n" +
                "AndroidWidescreenFOV=False\n" +
                "\n" +
                "[NOpenGLESDrv.NOpenGLESRenderDevice]\n" +
                "HighDetailActors=True\n" +
                "Coronas=True\n" +
                "VolumetricLighting=True\n" +
                "ShinySurfaces=True\n" +
                "NoFiltering=False\n" +
                "Overbright=True\n" +
                "DetailTextures=False\n" +
                "UseVAO=False\n" +
                "UseBGRA=False\n" +
                "WidescreenFOV=False\n" +
                "\n" +
                "[Audio.GenericAudioSubsystem]\n" +
                "UseFilter=True\n" +
                "UseSurround=False\n" +
                "UseStereo=True\n" +
                "UseCDMusic=False\n" +
                "UseDigitalMusic=True\n" +
                "UseSpatial=False\n" +
                "UseReverb=False\n" +
                "Use3dHardware=False\n" +
                "LowSoundQuality=False\n" +
                "ReverseStereo=False\n" +
                "Latency=40\n" +
                "OutputRate=22050Hz\n" +
                "Channels=16\n" +
                "MusicVolume=192\n" +
                "SoundVolume=224\n" +
                "AmbientFactor=0.700000\n" +
                "\n" +
                "[IpDrv.TcpNetDriver]\n" +
                "AllowDownloads=False\n" +
                "ConnectionTimeout=15.0\n" +
                "InitialConnectTimeout=30.0\n" +
                "AckTimeout=1.0\n" +
                "KeepAliveTime=0.2\n" +
                "MaxClientRate=20000\n" +
                "SimLatency=0\n" +
                "RelevantTimeout=5.0\n" +
                "SpawnPrioritySeconds=1.0\n" +
                "ServerTravelPause=4.0\n" +
                "NetServerMaxTickRate=20\n" +
                "LanServerMaxTickRate=35\n";
    }

    private static String buildAndroidUserIni() {
        return "[DefaultPlayer]\n" +
                "Name=Player\n" +
                "Class=Botpack.TMale1\n" +
                "team=0\n" +
                "skin=CommandoSkins.cmdo\n" +
                "Face=CommandoSkins.Blake\n" +
                "\n" +
                "[Engine.Input]\n" +
"\n" +
"; UT99_ANDROID_GAMEPAD_BINDS_V39\n" +
"W=MoveForward\n" +
"S=MoveBackward\n" +
"A=StrafeLeft\n" +
"D=StrafeRight\n" +
"Space=Jump\n" +
"LeftMouse=Fire\n" +
"RightMouse=AltFire\n" +
"MouseX=Axis aMouseX Speed=1.0\n" +
"MouseY=Axis aMouseY Speed=1.0\n" +
                androidControllerBindingDefaultsV117() +
                "Aliases[0]=(Command=\"Button bFire | Fire\",Alias=Fire)\n" +
                "Aliases[1]=(Command=\"Button bAltFire | AltFire\",Alias=AltFire)\n" +
                "Aliases[2]=(Command=\"Axis aBaseY Speed=+300.0\",Alias=MoveForward)\n" +
                "Aliases[3]=(Command=\"Axis aBaseY Speed=-300.0\",Alias=MoveBackward)\n" +
                "Aliases[4]=(Command=\"Axis aBaseX Speed=-150.0\",Alias=TurnLeft)\n" +
                "Aliases[5]=(Command=\"Axis aBaseX Speed=+150.0\",Alias=TurnRight)\n" +
                "Aliases[6]=(Command=\"Axis aStrafe Speed=-300.0\",Alias=StrafeLeft)\n" +
                "Aliases[7]=(Command=\"Axis aStrafe Speed=+300.0\",Alias=StrafeRight)\n" +
                "Aliases[8]=(Command=\"Jump | Axis aUp Speed=+300.0\",Alias=Jump)\n" +
                "Aliases[9]=(Command=\"Button bDuck | Axis aUp Speed=-300.0\",Alias=Duck)\n" +
                "Escape=ShowMenu\n" +
                "Space=Jump\n" +
                "Enter=Fire\n" +
                "LeftMouse=Fire\n" +
                "RightMouse=AltFire\n" +
                "Up=MoveForward\n" +
                "Down=MoveBackward\n" +
                "Left=TurnLeft\n" +
                "Right=TurnRight\n";
    }

    static String dataMessage(Context context) {
        File preferred = preferredRoot(context);
        File legacy = legacyRoot(context);
        return "Unreal Tournament data missing\n\n" +
                "Install or copy the game folders to:\n" +
                preferred.getAbsolutePath() + "\n\n" +
                "Also accepted for older test builds:\n" +
                legacy.getAbsolutePath() + "\n\n" +
                "Required:\n" +
                "System, Maps, Textures, Sounds, Music\n\n" +
                "You can now select either the Unreal Tournament folder or a ZIP containing these folders.";
    }
}
