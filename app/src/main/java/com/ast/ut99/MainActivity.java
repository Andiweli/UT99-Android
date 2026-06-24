package com.ast.ut99;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ActivityNotFoundException;
import android.content.ContentResolver;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.DocumentsContract;
import android.view.Gravity;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import java.util.zip.ZipInputStream;

/**
 * Small Android-side data installer / preflight activity.
 *
 * Unreal Tournament itself is only launched once a readable data layout exists.
 * Accepted layouts are either:
 *   /Android/data/com.ast.ut99/files/UT99/System, Maps, Textures, Sounds, Music
 * or the older direct test layout:
 *   /Android/data/com.ast.ut99/files/System, Maps, Textures, Sounds, Music
 */
public class MainActivity extends Activity {
    private static final int REQ_SELECT_UT99_FOLDER = 3001;
    private static final int REQ_SELECT_UT99_ZIP = 3002;
    private static final String DEFAULT_ONLINE_ZIP_URL = "http://ouya.cweiske.de/apks/com.ast.ut99/UT99_v1.400.zip";

    private File selectedRoot;
    private String lastImportMessage;
    private ProgressBar installProgressBar;
    private TextView installProgressText;
    private TextView installMessageText;
    private long launchRequestedAtMs;
    private boolean launchedLegacySafeMode;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        hideSystemUi();
        continueStartup();
    }


    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        hideSystemUi();

        // UT99_ANDROID_V87_RELIABLE_RELAUNCH:
        // If the launcher is tapped again while the installer is still handing
        // off to the SDL process, do not start a second native engine instantly.
        // If an old launcher task is revived later, reset the guard and do a
        // normal preflight again so one tap is enough.
        if (launchInProgress && launchRequestedAtMs > 0) {
            long elapsed = android.os.SystemClock.uptimeMillis() - launchRequestedAtMs;
            if (elapsed < 6000L) {
                android.util.Log.i("UT99Installer", "v87 duplicate launcher intent ignored during active handoff elapsed=" + elapsed);
                return;
            }
        }

        launchInProgress = false;
        launchRequestedAtMs = 0L;
        android.util.Log.i("UT99Installer", "v87 launcher intent resumed installer; retrying startup preflight");
        continueStartup();
    }

    @Override
    protected void onResume() {
        super.onResume();
        hideSystemUi();

        // UT99_ANDROID_V75_OUYA_ENGINE_HANDOFF_GUARD:
        // On Android 4.1/OUYA, if the SDL activity aborts during native startup,
        // finishing the installer would drop the user straight back to the system.
        // Keep this activity alive and turn the return into a visible retry state.
        if (launchInProgress && Build.VERSION.SDK_INT <= 17 && launchRequestedAtMs > 0) {
            long elapsed = android.os.SystemClock.uptimeMillis() - launchRequestedAtMs;
            if (elapsed > 1800L) {
                launchInProgress = false;
                selectedRoot = UT99Paths.resolveDataRoot(this);
                if (UT99Paths.hasUsableGameData(selectedRoot)) {
                    UT99Paths.rememberDataRoot(this, selectedRoot);
                    android.util.Log.i("UT99Installer", "legacy game activity returned; closing installer instead of showing install screen root=" + selectedRoot.getAbsolutePath());
                    finish();
                    return;
                }
                lastImportMessage = t(
                        "Engine wurde beendet. Die Daten konnten danach nicht erneut geprüft werden.",
                        "Engine ended. The installed data could not be verified afterwards.");
                showMissingDataScreen();
            }
        }
    }

    private void continueStartup() {
        selectedRoot = UT99Paths.resolveDataRoot(this);
        UT99Paths.ensureSkeleton(UT99Paths.installRoot(this));
        UT99Paths.normalizeInstalledDataRoot(selectedRoot);

        if (UT99Paths.hasLaunchableGameData(selectedRoot)) {
            UT99Paths.rememberDataRoot(this, selectedRoot);
            android.util.Log.i("UT99Installer", "data check OK root=" + selectedRoot.getAbsolutePath());
            launchGame(selectedRoot);
            return;
        }

        android.util.Log.w("UT99Installer", "data check failed root=" + selectedRoot.getAbsolutePath());
        if (UT99Paths.hasUsableGameData(selectedRoot)) {
            lastImportMessage = t(
                    "Spieldaten sind teilweise vorhanden, aber Core.u, Engine.u, Botpack.u oder Maps/CityIntro.unr fehlen bzw. haben eine unpassende Groß-/Kleinschreibung.",
                    "Game data is partially present, but Core.u, Engine.u, Botpack.u or Maps/CityIntro.unr are missing or have incompatible letter casing.");
        }
        showMissingDataScreen();
    }

    private boolean launchInProgress;

    private void launchGame(final File root) {
        if (launchInProgress) {
            android.util.Log.i("UT99Installer", "launch already in progress, ignoring duplicate request");
            return;
        }
        launchInProgress = true;

        final File verifiedRoot = root != null ? root : UT99Paths.resolveDataRoot(this);
        UT99Paths.normalizeInstalledDataRoot(verifiedRoot);
        if (UT99Paths.hasLaunchableGameData(verifiedRoot)) {
            UT99Paths.rememberDataRoot(this, verifiedRoot);
        }
        if (!UT99Paths.hasLaunchableGameData(verifiedRoot)) {
            android.util.Log.e("UT99Installer", "launch refused, required launch files missing root=" + verifiedRoot.getAbsolutePath());
            launchInProgress = false;
            lastImportMessage = t(
                    "Installierte Daten gefunden, aber für den Start fehlen Core.u, Engine.u, Botpack.u oder Maps/CityIntro.unr.",
                    "Installed data found, but Core.u, Engine.u, Botpack.u or Maps/CityIntro.unr are missing for launch.");
            showMissingDataScreen();
            return;
        }

        launchedLegacySafeMode = isLegacyOuyaLikeDevice();
        launchRequestedAtMs = android.os.SystemClock.uptimeMillis();
        android.util.Log.i("UT99Installer", "launchGame root=" + verifiedRoot.getAbsolutePath()
                + " sdk=" + Build.VERSION.SDK_INT
                + " legacySafe=" + launchedLegacySafeMode);

        // UT99_ANDROID_V166H_DIRECT_GAME_START:
        // Do not show the Android-side "Starting Unreal Tournament" / OUYA
        // compatibility busy screen when valid data is already installed.  The
        // installer remains alive behind GameActivity on legacy devices, but
        // the game activity is handed off immediately so the user goes straight
        // into the native SDL/UE1 startup path.
        android.util.Log.i("UT99Installer", "UT99_ANDROID_V166H_DIRECT_GAME_START no installer busy screen");

        final long finishDelay = Build.VERSION.SDK_INT <= 17 ? -1L : 450L;
        try {
            Intent intent = new Intent(MainActivity.this, GameActivity.class);
            // UT99_ANDROID_V87_RELIABLE_RELAUNCH:
            // Clear any stale GameActivity instance from the task before
            // starting a fresh SDL/native run.  This avoids the intermittent
            // "tap launcher twice until it loads" behaviour when Android
            // revived an old :game task.
            intent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION | Intent.FLAG_ACTIVITY_CLEAR_TOP);
            intent.putExtra(UT99Paths.EXTRA_DATA_ROOT, verifiedRoot.getAbsolutePath());
            intent.putExtra(UT99Paths.EXTRA_LEGACY_SAFE_MODE, launchedLegacySafeMode);
            startActivity(intent);
            android.util.Log.i("UT99Installer", "GameActivity start requested direct legacySafe=" + launchedLegacySafeMode);
        } catch (Throwable ex) {
            android.util.Log.e("UT99Installer", "GameActivity launch failed", ex);
            launchInProgress = false;
            lastImportMessage = t("Spielstart fehlgeschlagen: ", "Game launch failed: ") + ex.getMessage();
            showMissingDataScreen();
            return;
        }

        if (finishDelay >= 0L) {
            new android.os.Handler(android.os.Looper.getMainLooper()).postDelayed(() -> {
                try {
                    finish();
                } catch (Throwable ignored) {
                }
            }, finishDelay);
        } else {
            android.util.Log.i("UT99Installer", "legacy device: keeping installer activity alive behind GameActivity");
        }
    }

    private boolean isLegacyOuyaLikeDevice() {
        if (Build.VERSION.SDK_INT <= 17) {
            return true;
        }
        String model = String.valueOf(Build.MODEL).toLowerCase(Locale.US);
        String manufacturer = String.valueOf(Build.MANUFACTURER).toLowerCase(Locale.US);
        String product = String.valueOf(Build.PRODUCT).toLowerCase(Locale.US);
        return model.contains("ouya") || manufacturer.contains("ouya") || product.contains("ouya");
    }

    private boolean isOuyaDevice() {
        String model = String.valueOf(Build.MODEL).toLowerCase(Locale.US);
        String manufacturer = String.valueOf(Build.MANUFACTURER).toLowerCase(Locale.US);
        String product = String.valueOf(Build.PRODUCT).toLowerCase(Locale.US);
        return model.contains("ouya") || manufacturer.contains("ouya") || product.contains("ouya");
    }

    private void hideSystemUi() {
        Window window = getWindow();
        if (window == null) return;
        View decor = window.getDecorView();
        if (decor == null) return;

        if (Build.VERSION.SDK_INT >= 30) {
            WindowInsetsController controller = decor.getWindowInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            decor.setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_FULLSCREEN |
                            View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
                            View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                            View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
                            View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        }
    }

    private Locale currentLocale() {
        if (Build.VERSION.SDK_INT >= 24) {
            return getResources().getConfiguration().getLocales().get(0);
        }
        return getResources().getConfiguration().locale;
    }

    private boolean isGermanUi() {
        Locale locale = currentLocale();
        return locale != null && "de".equalsIgnoreCase(locale.getLanguage());
    }

    private String t(String de, String en) {
        return isGermanUi() ? de : en;
    }

    private Button button(String label, View.OnClickListener listener) {
        Button button = new Button(this);
        button.setText(label);
        button.setAllCaps(false);
        button.setOnClickListener(listener);
        return button;
    }

    private void showMissingDataScreen() {
        LinearLayout body = new LinearLayout(this);
        body.setOrientation(LinearLayout.VERTICAL);
        body.setGravity(Gravity.CENTER);
        body.setPadding(48, 36, 48, 36);

        final boolean hasLaunchData = selectedRoot != null && UT99Paths.hasLaunchableGameData(selectedRoot);
        final boolean ouyaMode = isOuyaDevice();

        TextView title = new TextView(this);
        title.setText(hasLaunchData
                ? t("Unreal Tournament bereit", "Unreal Tournament ready")
                : t("Unreal Tournament-Daten fehlen", "Unreal Tournament data not found"));
        title.setTextSize(24.0f);
        title.setGravity(Gravity.CENTER);
        body.addView(title);

        String extra = "";
        if (lastImportMessage != null && lastImportMessage.length() > 0) {
            extra = t("\n\nLetzte Meldung:\n", "\n\nLast message:\n") + lastImportMessage;
        }

        TextView message = new TextView(this);
        if (hasLaunchData) {
            message.setText(t(
                    "Spieldaten gefunden unter:\n" + selectedRoot.getAbsolutePath() + extra,
                    "Game data found at:\n" + selectedRoot.getAbsolutePath() + extra));
        } else if (ouyaMode) {
            message.setText(t(
                    "Es wurde kein vollständiger UT99-Datenordner gefunden.\n\n" +
                            "Auf OUYA kannst du die Spieldaten direkt online herunterladen oder eine lokale ZIP-Datei importieren.\n\n" +
                            "Installationsziel:\n" + UT99Paths.installRoot(this).getAbsolutePath() + "\n\n" +
                            "Benötigt werden mindestens:\nSystem, Maps, Textures, Sounds, Music" + extra,
                    "No complete UT99 data folder was found.\n\n" +
                            "On OUYA you can download the game data directly or import a local ZIP file.\n\n" +
                            "Install target:\n" + UT99Paths.installRoot(this).getAbsolutePath() + "\n\n" +
                            "Required folders:\nSystem, Maps, Textures, Sounds, Music" + extra));
        } else {
            message.setText(t(
                    "Es wurde kein vollständiger UT99-Datenordner gefunden.\n\n" +
                            "Du kannst jetzt entweder den Unreal Tournament-Ordner auswählen oder eine ZIP-Datei importieren.\n" +
                            "Auf Android 4.x wird dafür ein eingebauter Datei-/Ordnerbrowser verwendet.\n\n" +
                            "Installationsziel:\n" + UT99Paths.installRoot(this).getAbsolutePath() + "\n\n" +
                            "Benötigt werden mindestens:\nSystem, Maps, Textures, Sounds, Music" + extra,
                    "No complete UT99 data folder was found.\n\n" +
                            "Select the Unreal Tournament folder or import a ZIP file containing the game data.\n" +
                            "On Android 4.x an internal file/folder browser is used.\n\n" +
                            "Install target:\n" + UT99Paths.installRoot(this).getAbsolutePath() + "\n\n" +
                            "Required folders:\nSystem, Maps, Textures, Sounds, Music" + extra));
        }
        message.setTextSize(16.0f);
        message.setGravity(Gravity.CENTER);
        message.setPadding(0, 24, 0, 24);
        body.addView(message);

        Button firstFocusButton = null;
        if (hasLaunchData) {
            firstFocusButton = button(t("Unreal Tournament starten", "Start Unreal Tournament"), v -> launchGame(selectedRoot));
            body.addView(firstFocusButton);
        }

        if (ouyaMode) {
            Button onlineButton = button(t("Online-ZIP herunterladen", "Download online ZIP"), v -> showOnlineZipDialog());
            body.addView(onlineButton);
            if (firstFocusButton == null) firstFocusButton = onlineButton;

            Button localZipButton = button(t("Lokales ZIP auswählen", "Select local ZIP"), v -> openZipPicker());
            body.addView(localZipButton);
            if (firstFocusButton == null) firstFocusButton = localZipButton;
        } else {
            Button folderButton = button(t("UT99-Ordner auswählen", "Select UT99 folder"), v -> openFolderPicker());
            body.addView(folderButton);
            if (firstFocusButton == null) firstFocusButton = folderButton;

            Button zipButton = button(t("UT99-ZIP auswählen", "Select UT99 ZIP"), v -> openZipPicker());
            body.addView(zipButton);
            if (firstFocusButton == null) firstFocusButton = zipButton;
        }

        Button checkButton = button(t("Erneut prüfen", "Check again"), v -> continueStartup());
        body.addView(checkButton);
        if (firstFocusButton == null) firstFocusButton = checkButton;

        final Button focusTarget = firstFocusButton;
        ScrollView scrollView = new ScrollView(this);
        scrollView.addView(body);
        setContentView(scrollView);
        hideSystemUi();
        restoreControllerFocus(focusTarget);
    }

    private void restoreControllerFocus(final View focusTarget) {
        if (focusTarget == null) return;
        focusTarget.setFocusable(true);
        focusTarget.setFocusableInTouchMode(true);
        focusTarget.postDelayed(() -> {
            try {
                focusTarget.requestFocus();
            } catch (Throwable ignored) {
            }
        }, 80L);
    }

    private void showBusyScreen(String titleText, String messageText) {
        LinearLayout body = new LinearLayout(this);
        body.setOrientation(LinearLayout.VERTICAL);
        body.setGravity(Gravity.CENTER);
        body.setPadding(48, 36, 48, 36);

        TextView title = new TextView(this);
        title.setText(titleText);
        title.setTextSize(24.0f);
        title.setGravity(Gravity.CENTER);
        body.addView(title);

        installMessageText = new TextView(this);
        installMessageText.setText(messageText);
        installMessageText.setTextSize(16.0f);
        installMessageText.setGravity(Gravity.CENTER);
        installMessageText.setPadding(0, 24, 0, 16);
        body.addView(installMessageText);

        installProgressBar = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        installProgressBar.setIndeterminate(false);
        installProgressBar.setMax(100);
        installProgressBar.setProgress(0);
        int progressWidth = Math.max(240, getResources().getDisplayMetrics().widthPixels / 2);
        LinearLayout.LayoutParams progressParams = new LinearLayout.LayoutParams(progressWidth, LinearLayout.LayoutParams.WRAP_CONTENT);
        progressParams.gravity = Gravity.CENTER_HORIZONTAL;
        body.addView(installProgressBar, progressParams);

        installProgressText = new TextView(this);
        installProgressText.setText("0%");
        installProgressText.setTextSize(16.0f);
        installProgressText.setGravity(Gravity.CENTER);
        installProgressText.setPadding(0, 8, 0, 0);
        body.addView(installProgressText);

        setContentView(body);
        hideSystemUi();
    }

    private void updateInstallMessage(final String message) {
        runOnUiThread(() -> {
            if (installMessageText != null) installMessageText.setText(message);
        });
    }

    private void updateInstallProgress(final String phase, final int percent) {
        final int safePercent = Math.max(0, Math.min(100, percent));
        runOnUiThread(() -> {
            if (installProgressBar != null) installProgressBar.setProgress(safePercent);
            if (installProgressText != null) {
                if (phase != null && phase.length() > 0) {
                    installProgressText.setText(phase + " " + safePercent + "%");
                } else {
                    installProgressText.setText(safePercent + "%");
                }
            }
        });
    }

    private void openFolderPicker() {
        if (Build.VERSION.SDK_INT < 21) {
            openLegacyFolderPicker(legacyStartDir());
            return;
        }

        try {
            Intent intent = new Intent("android.intent.action.OPEN_DOCUMENT_TREE");
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION |
                    Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION |
                    Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
            startActivityForResult(intent, REQ_SELECT_UT99_FOLDER);
        } catch (ActivityNotFoundException ex) {
            android.util.Log.w("UT99Installer", "SAF folder picker not available, using legacy picker", ex);
            openLegacyFolderPicker(legacyStartDir());
        }
    }

    private void openZipPicker() {
        if (Build.VERSION.SDK_INT < 19) {
            openLegacyZipPicker(legacyStartDir());
            return;
        }

        try {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            intent.putExtra(Intent.EXTRA_LOCAL_ONLY, true);
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
            startActivityForResult(Intent.createChooser(intent, t("UT99-ZIP auswählen", "Select UT99 ZIP")), REQ_SELECT_UT99_ZIP);
        } catch (ActivityNotFoundException ex) {
            android.util.Log.w("UT99Installer", "SAF zip picker not available, using legacy picker", ex);
            openLegacyZipPicker(legacyStartDir());
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != RESULT_OK || data == null || data.getData() == null) {
            showMissingDataScreen();
            return;
        }

        Uri uri = data.getData();
        tryPersistPermission(data, uri);

        if (requestCode == REQ_SELECT_UT99_FOLDER) {
            installFromFolder(uri);
        } else if (requestCode == REQ_SELECT_UT99_ZIP) {
            installFromZip(uri);
        }
    }

    private void tryPersistPermission(Intent data, Uri uri) {
        if (Build.VERSION.SDK_INT < 19 || uri == null) return;
        try {
            int flags = data.getFlags() & (Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            getContentResolver().takePersistableUriPermission(uri, flags & Intent.FLAG_GRANT_READ_URI_PERMISSION);
        } catch (Throwable ignored) {
            // Some providers do not support persisted grants. The current one-shot import still works.
        }
    }

    private void installFromFolder(final Uri treeUri) {
        showBusyScreen(t("Installiere UT99-Daten", "Installing UT99 data"),
                t("Ordner wird kopiert …", "Copying folder …"));
        new Thread(() -> {
            final String result;
            try {
                InstallStats stats = importFolderTree(treeUri, UT99Paths.installRoot(this));
                result = t("Ordnerimport abgeschlossen: ", "Folder import complete: ") + stats.files + " files";
            } catch (Throwable ex) {
                android.util.Log.e("UT99Installer", "folder import failed", ex);
                runOnUiThread(() -> {
                    lastImportMessage = t("Ordnerimport fehlgeschlagen: ", "Folder import failed: ") + ex.getMessage();
                    showMissingDataScreen();
                });
                return;
            }
            runOnUiThread(() -> {
                lastImportMessage = result;
                Toast.makeText(this, result, Toast.LENGTH_LONG).show();
                continueStartup();
            });
        }, "UT99FolderInstaller").start();
    }

    private void installFromZip(final Uri zipUri) {
        showBusyScreen(t("Installiere UT99-Daten", "Installing UT99 data"),
                t("ZIP wird vorbereitet …", "Preparing ZIP …"));
        new Thread(() -> {
            final String result;
            try {
                InstallStats stats = importZip(zipUri, UT99Paths.installRoot(this), (phase, percent) -> updateInstallProgress(phase, percent));
                result = t("ZIP-Import abgeschlossen: ", "ZIP import complete: ") + stats.files + " files";
            } catch (Throwable ex) {
                android.util.Log.e("UT99Installer", "zip import failed", ex);
                final String msg = ex.getMessage() != null ? ex.getMessage() : ex.toString();
                runOnUiThread(() -> {
                    lastImportMessage = t("ZIP-Import fehlgeschlagen: ", "ZIP import failed: ") + msg;
                    showMissingDataScreen();
                });
                return;
            }
            runOnUiThread(() -> {
                lastImportMessage = result;
                Toast.makeText(this, result, Toast.LENGTH_LONG).show();
                continueStartup();
            });
        }, "UT99ZipInstaller").start();
    }

    private void showOnlineZipDialog() {
        final EditText input = new EditText(this);
        input.setSingleLine(true);
        input.setText(DEFAULT_ONLINE_ZIP_URL);
        input.setSelectAllOnFocus(false);
        input.setSelection(input.getText().length());

        AlertDialog dialog = new AlertDialog.Builder(this)
                .setTitle(t("Online-ZIP herunterladen", "Download online ZIP"))
                .setMessage(t("Download-URL:", "Download URL:"))
                .setView(input)
                .setPositiveButton(t("Download starten", "Start download"), (d, which) -> {
                    String url = input.getText() != null ? input.getText().toString().trim() : "";
                    if (url.length() == 0) {
                        lastImportMessage = t("Keine Download-URL eingegeben.", "No download URL entered.");
                        showMissingDataScreen();
                        return;
                    }
                    installFromOnlineZip(url);
                })
                .setNegativeButton(t("Abbrechen", "Cancel"), (d, which) -> showMissingDataScreen())
                .create();

        dialog.setOnCancelListener(d -> showMissingDataScreen());
        dialog.setOnShowListener(d -> {
            try {
                Button positive = dialog.getButton(AlertDialog.BUTTON_POSITIVE);
                if (positive != null) {
                    positive.setFocusable(true);
                    positive.requestFocus();
                }
            } catch (Throwable ignored) {
            }
        });
        dialog.show();
    }

    private void installFromOnlineZip(final String urlText) {
        showBusyScreen(t("Installiere UT99-Daten", "Installing UT99 data"),
                t("Online-ZIP wird heruntergeladen …", "Downloading online ZIP …"));
        new Thread(() -> {
            File tmp = null;
            final String result;
            try {
                tmp = File.createTempFile("ut99-online-import", ".zip", getCacheDir());
                updateInstallMessage(t("Online-ZIP wird heruntergeladen …", "Downloading online ZIP …"));
                downloadOnlineZipToFile(urlText, tmp);
                if (!looksLikeZipFile(tmp)) {
                    throw new IOException("Downloaded file is not a ZIP archive. Check the URL or server redirect.");
                }
                updateInstallMessage(t("ZIP wird entpackt und geprüft …", "Extracting and verifying ZIP …"));
                InstallStats stats = extractZipFile(tmp, UT99Paths.installRoot(this), (phase, percent) -> updateInstallProgress(phase, percent));
                result = t("Online-ZIP-Import abgeschlossen: ", "Online ZIP import complete: ") + stats.files + " files";
            } catch (Throwable ex) {
                android.util.Log.e("UT99Installer", "online zip import failed", ex);
                final String msg = ex.getMessage() != null ? ex.getMessage() : ex.toString();
                runOnUiThread(() -> {
                    lastImportMessage = t("Online-ZIP-Import fehlgeschlagen: ", "Online ZIP import failed: ") + msg;
                    showMissingDataScreen();
                });
                if (tmp != null && tmp.exists() && !tmp.delete()) tmp.deleteOnExit();
                return;
            }

            if (tmp != null && tmp.exists() && !tmp.delete()) tmp.deleteOnExit();
            runOnUiThread(() -> {
                lastImportMessage = result;
                Toast.makeText(this, result, Toast.LENGTH_LONG).show();
                continueStartup();
            });
        }, "UT99OnlineZipInstaller").start();
    }

    private void downloadOnlineZipToFile(String urlText, File outFile) throws IOException {
        URL url = new URL(urlText);
        for (int redirect = 0; redirect < 5; redirect++) {
            String protocol = url.getProtocol();
            if (protocol == null) throw new IOException("Download URL has no protocol.");
            protocol = protocol.toLowerCase(Locale.US);
            if (!"http".equals(protocol) && !"https".equals(protocol)) {
                throw new IOException("Only HTTP/HTTPS URLs are supported.");
            }
            if (isOuyaDevice() && "https".equals(protocol)) {
                throw new IOException("OUYA cannot download HTTPS URLs. Use a direct HTTP URL without HTTPS redirect.");
            }

            HttpURLConnection connection = (HttpURLConnection) url.openConnection();
            connection.setConnectTimeout(20000);
            connection.setReadTimeout(60000);
            connection.setInstanceFollowRedirects(false);
            connection.setRequestProperty("User-Agent", "UT99-Android-Installer/1.7.1");
            connection.connect();

            int code = connection.getResponseCode();
            if (code >= 300 && code < 400) {
                String location = connection.getHeaderField("Location");
                connection.disconnect();
                if (location == null || location.trim().length() == 0) {
                    throw new IOException("Server redirected without Location header.");
                }
                URL next = new URL(url, location);
                String nextProtocol = next.getProtocol() != null ? next.getProtocol().toLowerCase(Locale.US) : "";
                if (isOuyaDevice() && "https".equals(nextProtocol)) {
                    throw new IOException("Server redirects to HTTPS, which OUYA cannot download. Use a direct HTTP mirror.");
                }
                url = next;
                continue;
            }

            if (code < 200 || code >= 300) {
                connection.disconnect();
                throw new IOException("HTTP error " + code + " while downloading ZIP.");
            }

            long totalBytes = connection.getContentLength();
            InputStream input = connection.getInputStream();
            try {
                FileOutputStream output = new FileOutputStream(outFile, false);
                try {
                    copyWithProgress(input, output, totalBytes, t("Download", "Download"), 0, 45);
                } finally {
                    output.close();
                }
            } finally {
                input.close();
                connection.disconnect();
            }
            if (outFile.length() <= 0) throw new IOException("Downloaded file is empty.");
            updateInstallProgress(t("Download", "Download"), 45);
            return;
        }
        throw new IOException("Too many redirects while downloading ZIP.");
    }

    private boolean looksLikeZipFile(File file) {
        if (file == null || !file.exists() || file.length() < 4) return false;
        FileInputStream in = null;
        try {
            in = new FileInputStream(file);
            int b0 = in.read();
            int b1 = in.read();
            int b2 = in.read();
            int b3 = in.read();
            return b0 == 'P' && b1 == 'K' && (b2 == 3 || b2 == 5 || b2 == 7) && (b3 == 4 || b3 == 6 || b3 == 8);
        } catch (Throwable ignored) {
            return false;
        } finally {
            if (in != null) {
                try { in.close(); } catch (IOException ignored) {}
            }
        }
    }

    private File legacyStartDir() {
        File external = null;
        try {
            external = Environment.getExternalStorageDirectory();
        } catch (Throwable ignored) {
            // Fall through to /sdcard below.
        }
        if (external != null && external.exists() && external.canRead()) return external;

        File sdcard = new File("/sdcard");
        if (sdcard.exists() && sdcard.canRead()) return sdcard;

        File mntSdcard = new File("/mnt/sdcard");
        if (mntSdcard.exists() && mntSdcard.canRead()) return mntSdcard;

        File storage = new File("/storage");
        if (storage.exists() && storage.canRead()) return storage;

        return new File("/");
    }

    private void openLegacyFolderPicker(File startDir) {
        final File dir = normalizeLegacyDir(startDir);
        final List<LegacyChoice> choices = legacyDirectoryChoices(dir, false);
        if (choices.isEmpty()) {
            lastImportMessage = t("Ordner kann nicht gelesen werden: ", "Cannot read folder: ") + dir.getAbsolutePath();
            showMissingDataScreen();
            return;
        }

        String[] labels = new String[choices.size()];
        for (int i = 0; i < choices.size(); i++) labels[i] = choices.get(i).label;

        new AlertDialog.Builder(this)
                .setTitle(t("UT99-Ordner auswählen", "Select UT99 folder") + "\n" + dir.getAbsolutePath())
                .setItems(labels, (dialog, which) -> {
                    LegacyChoice choice = choices.get(which);
                    if (choice.kind == LegacyChoice.KIND_SELECT_FOLDER) {
                        installFromLegacyFolder(dir);
                    } else if (choice.kind == LegacyChoice.KIND_DIRECTORY) {
                        openLegacyFolderPicker(choice.file);
                    } else if (choice.kind == LegacyChoice.KIND_CANCEL) {
                        showMissingDataScreen();
                    }
                })
                .setNegativeButton(t("Abbrechen", "Cancel"), (dialog, which) -> showMissingDataScreen())
                .show();
    }

    private void openLegacyZipPicker(File startDir) {
        final File dir = normalizeLegacyDir(startDir);
        final List<LegacyChoice> choices = legacyDirectoryChoices(dir, true);
        if (choices.isEmpty()) {
            lastImportMessage = t("Ordner kann nicht gelesen werden: ", "Cannot read folder: ") + dir.getAbsolutePath();
            showMissingDataScreen();
            return;
        }

        String[] labels = new String[choices.size()];
        for (int i = 0; i < choices.size(); i++) labels[i] = choices.get(i).label;

        new AlertDialog.Builder(this)
                .setTitle(t("UT99-ZIP auswählen", "Select UT99 ZIP") + "\n" + dir.getAbsolutePath())
                .setItems(labels, (dialog, which) -> {
                    LegacyChoice choice = choices.get(which);
                    if (choice.kind == LegacyChoice.KIND_ZIP_FILE) {
                        installFromLegacyZipFile(choice.file);
                    } else if (choice.kind == LegacyChoice.KIND_DIRECTORY) {
                        openLegacyZipPicker(choice.file);
                    } else if (choice.kind == LegacyChoice.KIND_CANCEL) {
                        showMissingDataScreen();
                    }
                })
                .setNegativeButton(t("Abbrechen", "Cancel"), (dialog, which) -> showMissingDataScreen())
                .show();
    }

    private File normalizeLegacyDir(File dir) {
        if (dir == null) return legacyStartDir();
        if (dir.isFile()) dir = dir.getParentFile();
        if (dir == null) return new File("/");
        try {
            return dir.getCanonicalFile();
        } catch (IOException ignored) {
            return dir.getAbsoluteFile();
        }
    }

    private List<LegacyChoice> legacyDirectoryChoices(File dir, boolean includeZipFiles) {
        ArrayList<LegacyChoice> out = new ArrayList<>();
        out.add(new LegacyChoice(t("Abbrechen", "Cancel"), null, LegacyChoice.KIND_CANCEL));

        if (!includeZipFiles) {
            out.add(new LegacyChoice(t("Diesen Ordner verwenden", "Use this folder"), dir, LegacyChoice.KIND_SELECT_FOLDER));
        }

        File parent = dir.getParentFile();
        if (parent != null) {
            out.add(new LegacyChoice("..", parent, LegacyChoice.KIND_DIRECTORY));
        }

        File[] files = dir.listFiles();
        if (files == null) return out;

        ArrayList<File> directories = new ArrayList<>();
        ArrayList<File> zips = new ArrayList<>();
        for (File file : files) {
            if (file == null || file.isHidden() || !file.canRead()) continue;
            if (file.isDirectory()) {
                directories.add(file);
            } else if (includeZipFiles && file.isFile() && file.getName().toLowerCase(Locale.US).endsWith(".zip")) {
                zips.add(file);
            }
        }

        Comparator<File> byName = (a, b) -> a.getName().compareToIgnoreCase(b.getName());
        Collections.sort(directories, byName);
        Collections.sort(zips, byName);

        for (File child : directories) {
            out.add(new LegacyChoice(child.getName() + "/", child, LegacyChoice.KIND_DIRECTORY));
        }
        for (File zip : zips) {
            out.add(new LegacyChoice(zip.getName(), zip, LegacyChoice.KIND_ZIP_FILE));
        }
        return out;
    }

    private void installFromLegacyFolder(final File folder) {
        showBusyScreen(t("Installiere UT99-Daten", "Installing UT99 data"),
                t("Ordner wird kopiert …", "Copying folder …"));
        new Thread(() -> {
            final String result;
            try {
                InstallStats stats = importLegacyFolder(folder, UT99Paths.installRoot(this));
                result = t("Ordnerimport abgeschlossen: ", "Folder import complete: ") + stats.files + " files";
            } catch (Throwable ex) {
                android.util.Log.e("UT99Installer", "legacy folder import failed", ex);
                runOnUiThread(() -> {
                    lastImportMessage = t("Ordnerimport fehlgeschlagen: ", "Folder import failed: ") + ex.getMessage();
                    showMissingDataScreen();
                });
                return;
            }
            runOnUiThread(() -> {
                lastImportMessage = result;
                Toast.makeText(this, result, Toast.LENGTH_LONG).show();
                continueStartup();
            });
        }, "UT99LegacyFolderInstaller").start();
    }

    private void installFromLegacyZipFile(final File zipFile) {
        showBusyScreen(t("Installiere UT99-Daten", "Installing UT99 data"),
                t("ZIP wird vorbereitet …", "Preparing ZIP …"));
        new Thread(() -> {
            final String result;
            try {
                InstallStats stats = importZipFile(zipFile, UT99Paths.installRoot(this), (phase, percent) -> updateInstallProgress(phase, percent));
                result = t("ZIP-Import abgeschlossen: ", "ZIP import complete: ") + stats.files + " files";
            } catch (Throwable ex) {
                android.util.Log.e("UT99Installer", "legacy zip import failed", ex);
                final String msg = ex.getMessage() != null ? ex.getMessage() : ex.toString();
                runOnUiThread(() -> {
                    lastImportMessage = t("ZIP-Import fehlgeschlagen: ", "ZIP import failed: ") + msg;
                    showMissingDataScreen();
                });
                return;
            }
            runOnUiThread(() -> {
                lastImportMessage = result;
                Toast.makeText(this, result, Toast.LENGTH_LONG).show();
                continueStartup();
            });
        }, "UT99LegacyZipInstaller").start();
    }

    private InstallStats importLegacyFolder(File selectedFolder, File targetRoot) throws IOException {
        if (selectedFolder == null || !selectedFolder.exists() || !selectedFolder.isDirectory()) {
            throw new IOException("Selected folder does not exist.");
        }
        UT99Paths.ensureSkeleton(targetRoot);

        File source = findLegacyGameDataFolder(selectedFolder, 2);
        if (source == null) {
            throw new IOException("Selected folder does not contain System, Maps, Textures, Sounds and Music.");
        }

        if (sameCanonicalFile(source, targetRoot)) {
            InstallStats stats = new InstallStats();
            if (!UT99Paths.hasUsableGameData(targetRoot)) {
                throw new IOException("Selected folder is the install target, but required UT99 files were not found.");
            }
            return stats;
        }

        InstallStats stats = new InstallStats();
        copyLegacyChildren(source, targetRoot, targetRoot, stats);
        if (!UT99Paths.hasUsableGameData(targetRoot)) {
            throw new IOException("Import finished, but required UT99 files were not found in " + targetRoot.getAbsolutePath());
        }
        return stats;
    }

    private File findLegacyGameDataFolder(File root, int depthLeft) {
        if (root == null || !root.exists() || !root.isDirectory() || !root.canRead()) return null;
        if (legacyFolderHasRequiredFolders(root)) return root;
        if (depthLeft <= 0) return null;

        File[] children = root.listFiles();
        if (children == null) return null;
        for (File child : children) {
            if (child != null && child.isDirectory() && child.canRead()) {
                File hit = findLegacyGameDataFolder(child, depthLeft - 1);
                if (hit != null) return hit;
            }
        }
        return null;
    }

    private boolean legacyFolderHasRequiredFolders(File folder) {
        return new File(folder, "System").isDirectory() &&
                new File(folder, "Maps").isDirectory() &&
                new File(folder, "Textures").isDirectory() &&
                new File(folder, "Sounds").isDirectory() &&
                new File(folder, "Music").isDirectory();
    }

    private void copyLegacyChildren(File sourceDir, File targetDir, File targetRoot, InstallStats stats) throws IOException {
        if (!targetDir.exists() && !targetDir.mkdirs()) {
            throw new IOException("Cannot create " + targetDir.getAbsolutePath());
        }
        File[] children = sourceDir.listFiles();
        if (children == null) return;
        for (File child : children) {
            if (child == null || child.isHidden()) continue;
            if (isTargetInsideLegacySource(child, targetRoot)) continue;
            String safeName = safeFileName(child.getName());
            if (safeName.length() == 0) continue;
            File out = new File(targetDir, safeName);
            if (child.isDirectory()) {
                copyLegacyChildren(child, out, targetRoot, stats);
            } else if (child.isFile()) {
                copyLegacyFile(child, out, stats);
            }
        }
    }

    private void copyLegacyFile(File source, File out, InstallStats stats) throws IOException {
        File parent = out.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            throw new IOException("Cannot create " + parent.getAbsolutePath());
        }
        FileInputStream input = new FileInputStream(source);
        try {
            FileOutputStream output = new FileOutputStream(out, false);
            try {
                stats.bytes += copy(input, output);
                stats.files++;
            } finally {
                output.close();
            }
        } finally {
            input.close();
        }
    }

    private boolean sameCanonicalFile(File a, File b) {
        try {
            return a.getCanonicalPath().equals(b.getCanonicalPath());
        } catch (IOException ignored) {
            return a.getAbsolutePath().equals(b.getAbsolutePath());
        }
    }

    private boolean isTargetInsideLegacySource(File possibleParent, File targetRoot) {
        try {
            String parentPath = possibleParent.getCanonicalPath();
            String targetPath = targetRoot.getCanonicalPath();
            return targetPath.equals(parentPath) || targetPath.startsWith(parentPath + File.separator);
        } catch (IOException ignored) {
            String parentPath = possibleParent.getAbsolutePath();
            String targetPath = targetRoot.getAbsolutePath();
            return targetPath.equals(parentPath) || targetPath.startsWith(parentPath + File.separator);
        }
    }

    private InstallStats importZipFile(File zipFile, File targetRoot) throws IOException {
        return importZipFile(zipFile, targetRoot, null);
    }

    private InstallStats importZipFile(File zipFile, File targetRoot, ProgressCallback progress) throws IOException {
        if (zipFile == null || !zipFile.exists() || !zipFile.isFile()) {
            throw new IOException("Selected ZIP does not exist.");
        }
        return extractZipFile(zipFile, targetRoot, progress);
    }

    private InstallStats importFolderTree(Uri treeUri, File targetRoot) throws IOException {
        if (Build.VERSION.SDK_INT < 21) throw new IOException("Folder import requires Android 5.0 or newer.");
        UT99Paths.ensureSkeleton(targetRoot);

        Uri rootDocument = DocumentsContract.buildDocumentUriUsingTree(treeUri, DocumentsContract.getTreeDocumentId(treeUri));
        Uri sourceDocument = findGameDataDocument(treeUri, rootDocument);
        if (sourceDocument == null) {
            throw new IOException("Selected folder does not contain System, Maps, Textures, Sounds and Music.");
        }

        InstallStats stats = new InstallStats();
        copyDocumentChildren(treeUri, sourceDocument, targetRoot, stats);
        if (!UT99Paths.hasUsableGameData(targetRoot)) {
            throw new IOException("Import finished, but required UT99 files were not found in " + targetRoot.getAbsolutePath());
        }
        return stats;
    }

    private Uri findGameDataDocument(Uri treeUri, Uri documentUri) throws IOException {
        if (documentHasRequiredFolders(treeUri, documentUri)) return documentUri;
        for (DocumentEntry child : listDocumentChildren(treeUri, documentUri)) {
            if (child.directory && documentHasRequiredFolders(treeUri, child.uri)) {
                return child.uri;
            }
        }
        return null;
    }

    private boolean documentHasRequiredFolders(Uri treeUri, Uri documentUri) throws IOException {
        Set<String> names = new HashSet<>();
        for (DocumentEntry child : listDocumentChildren(treeUri, documentUri)) {
            if (child.directory) names.add(child.name.toLowerCase(Locale.US));
        }
        return names.contains("system") && names.contains("maps") && names.contains("textures") &&
                names.contains("sounds") && names.contains("music");
    }

    private void copyDocumentChildren(Uri treeUri, Uri parentDocument, File targetDir, InstallStats stats) throws IOException {
        if (!targetDir.exists() && !targetDir.mkdirs()) {
            throw new IOException("Cannot create " + targetDir.getAbsolutePath());
        }
        for (DocumentEntry child : listDocumentChildren(treeUri, parentDocument)) {
            String safeName = safeFileName(child.name);
            if (safeName.length() == 0) continue;
            File out = new File(targetDir, safeName);
            if (child.directory) {
                copyDocumentChildren(treeUri, child.uri, out, stats);
            } else {
                copyContentUriToFile(child.uri, out, stats);
            }
        }
    }

    private java.util.List<DocumentEntry> listDocumentChildren(Uri treeUri, Uri documentUri) throws IOException {
        java.util.ArrayList<DocumentEntry> entries = new java.util.ArrayList<>();
        if (Build.VERSION.SDK_INT < 21) return entries;

        String documentId = DocumentsContract.getDocumentId(documentUri);
        Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, documentId);
        Cursor cursor = null;
        try {
            cursor = getContentResolver().query(childrenUri,
                    new String[] {
                            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                            DocumentsContract.Document.COLUMN_MIME_TYPE
                    }, null, null, null);
            if (cursor == null) return entries;
            while (cursor.moveToNext()) {
                String childId = cursor.getString(0);
                String name = cursor.getString(1);
                String mime = cursor.getString(2);
                if (name == null || name.length() == 0) continue;
                Uri childUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, childId);
                boolean directory = DocumentsContract.Document.MIME_TYPE_DIR.equals(mime);
                entries.add(new DocumentEntry(childUri, name, directory));
            }
        } catch (Exception ex) {
            throw new IOException("Could not read selected folder.", ex);
        } finally {
            if (cursor != null) cursor.close();
        }
        return entries;
    }

    private void copyContentUriToFile(Uri source, File out, InstallStats stats) throws IOException {
        File parent = out.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            throw new IOException("Cannot create " + parent.getAbsolutePath());
        }
        InputStream input = getContentResolver().openInputStream(source);
        if (input == null) throw new IOException("Cannot open " + source);
        try {
            FileOutputStream output = new FileOutputStream(out, false);
            try {
                stats.bytes += copy(input, output);
                stats.files++;
            } finally {
                output.close();
            }
        } finally {
            input.close();
        }
    }

    private InstallStats importZip(Uri zipUri, File targetRoot) throws IOException {
        return importZip(zipUri, targetRoot, null);
    }

    private InstallStats importZip(Uri zipUri, File targetRoot, ProgressCallback progress) throws IOException {
        File tmp = File.createTempFile("ut99-import", ".zip", getCacheDir());
        try {
            InputStream in = getContentResolver().openInputStream(zipUri);
            if (in == null) throw new IOException("Cannot open selected ZIP.");
            try {
                FileOutputStream out = new FileOutputStream(tmp, false);
                try {
                    copyWithProgress(in, out, -1, t("ZIP lesen", "Reading ZIP"), 0, 10);
                } finally {
                    out.close();
                }
            } finally {
                in.close();
            }
            if (!looksLikeZipFile(tmp)) {
                throw new IOException("Selected file is not a ZIP archive.");
            }
            return extractZipFile(tmp, targetRoot, progress);
        } finally {
            if (!tmp.delete()) tmp.deleteOnExit();
        }
    }

    private InstallStats extractZipFile(File zipSource, File targetRoot) throws IOException {
        return extractZipFile(zipSource, targetRoot, null);
    }

    private InstallStats extractZipFile(File zipSource, File targetRoot, ProgressCallback progress) throws IOException {
        UT99Paths.ensureSkeleton(targetRoot);

        String prefix;
        ZipFile zipFile = new ZipFile(zipSource);
        try {
            prefix = findZipGameDataPrefix(zipFile);
        } finally {
            zipFile.close();
        }
        if (prefix == null) {
            throw new IOException("ZIP does not contain System, Maps, Textures, Sounds and Music.");
        }

        File stagingRoot = new File(getCacheDir(), "ut99-extract-" + android.os.SystemClock.uptimeMillis());
        File stagingData = new File(stagingRoot, "UT99");
        InstallStats stats = new InstallStats();
        try {
            if (!stagingData.mkdirs() && !stagingData.isDirectory()) {
                throw new IOException("Cannot create temporary install folder.");
            }
            extractZipFileDirect(zipSource, stagingData, prefix, stats, progress);
            if (!UT99Paths.hasUsableGameData(stagingData)) {
                throw new IOException("ZIP extracted to temporary folder, but required UT99 files were not found. Extracted files=" + stats.files + ", bytes=" + stats.bytes);
            }
            if (progress != null) progress.onProgress(t("Kopieren", "Copying"), 90);
            replaceDirectoryContents(stagingData, targetRoot);
            if (progress != null) progress.onProgress(t("Fertig", "Done"), 100);
        } finally {
            deleteRecursive(stagingRoot);
        }

        if (!UT99Paths.hasUsableGameData(targetRoot)) {
            throw new IOException("ZIP copied, but required UT99 files were not found in " + targetRoot.getAbsolutePath());
        }
        return stats;
    }

    private void extractZipFileDirect(File zipSource, File targetRoot, String prefix, InstallStats stats, ProgressCallback progress) throws IOException {
        final int totalExtractFiles = countExtractableZipFiles(zipSource, prefix);
        if (progress != null) progress.onProgress(t("Installation", "Installing"), 45);

        ZipInputStream zipInput = new ZipInputStream(new BufferedInputStream(new FileInputStream(zipSource)));
        try {
            ZipEntry entry;
            while ((entry = zipInput.getNextEntry()) != null) {
                String name = normalizeZipName(entry.getName());
                if (name.length() == 0 || !name.startsWith(prefix)) {
                    zipInput.closeEntry();
                    continue;
                }
                String relative = name.substring(prefix.length());
                if (relative.length() == 0 || shouldSkipZipEntry(relative)) {
                    zipInput.closeEntry();
                    continue;
                }

                File out = safeZipOutputFile(targetRoot, relative);
                if (entry.isDirectory() || relative.endsWith("/")) {
                    if (!out.exists() && !out.mkdirs()) throw new IOException("Cannot create " + out.getAbsolutePath());
                } else {
                    File parent = out.getParentFile();
                    if (parent != null && !parent.exists() && !parent.mkdirs()) throw new IOException("Cannot create " + parent.getAbsolutePath());
                    FileOutputStream fileOut = new FileOutputStream(out, false);
                    try {
                        stats.bytes += copy(zipInput, fileOut);
                        stats.files++;
                        if (progress != null && totalExtractFiles > 0) {
                            int percent = 45 + (int) Math.min(45, (stats.files * 45L) / totalExtractFiles);
                            progress.onProgress(t("Installation", "Installing"), percent);
                        }
                    } finally {
                        fileOut.close();
                    }
                }
                zipInput.closeEntry();
            }
        } finally {
            zipInput.close();
        }
    }

    private int countExtractableZipFiles(File zipSource, String prefix) throws IOException {
        int count = 0;
        ZipFile zipFile = new ZipFile(zipSource);
        try {
            java.util.Enumeration<? extends ZipEntry> entries = zipFile.entries();
            while (entries.hasMoreElements()) {
                ZipEntry entry = entries.nextElement();
                String name = normalizeZipName(entry.getName());
                if (name.length() == 0 || !name.startsWith(prefix)) continue;
                String relative = name.substring(prefix.length());
                if (relative.length() == 0 || shouldSkipZipEntry(relative)) continue;
                if (!entry.isDirectory() && !relative.endsWith("/")) count++;
            }
        } finally {
            zipFile.close();
        }
        return count;
    }

    private void replaceDirectoryContents(File sourceDir, File targetDir) throws IOException {
        if (sourceDir == null || !sourceDir.isDirectory()) throw new IOException("Source folder is not readable.");
        if (!targetDir.exists() && !targetDir.mkdirs()) throw new IOException("Cannot create " + targetDir.getAbsolutePath());
        clearDirectory(targetDir);
        copyLegacyChildren(sourceDir, targetDir, targetDir, new InstallStats());
    }

    private void clearDirectory(File dir) throws IOException {
        File[] children = dir.listFiles();
        if (children == null) return;
        for (File child : children) {
            deleteRecursive(child);
        }
    }

    private void deleteRecursive(File file) throws IOException {
        if (file == null || !file.exists()) return;
        if (file.isDirectory()) {
            File[] children = file.listFiles();
            if (children != null) {
                for (File child : children) deleteRecursive(child);
            }
        }
        if (!file.delete() && file.exists()) {
            throw new IOException("Cannot delete " + file.getAbsolutePath());
        }
    }

    private String findZipGameDataPrefix(ZipFile zipFile) throws IOException {
        Set<String> lowerNames = new HashSet<>();
        java.util.Enumeration<? extends ZipEntry> entries = zipFile.entries();
        while (entries.hasMoreElements()) {
            String name = normalizeZipName(entries.nextElement().getName()).toLowerCase(Locale.US);
            if (name.length() > 0) lowerNames.add(name);
        }

        Set<String> candidates = new HashSet<>();
        for (String name : lowerNames) {
            int idx = name.indexOf("system/");
            if (idx >= 0) candidates.add(name.substring(0, idx));
        }
        candidates.add("");

        for (String prefix : candidates) {
            if (zipHasPath(lowerNames, prefix + "system/") &&
                    zipHasPath(lowerNames, prefix + "maps/") &&
                    zipHasPath(lowerNames, prefix + "textures/") &&
                    zipHasPath(lowerNames, prefix + "sounds/") &&
                    zipHasPath(lowerNames, prefix + "music/")) {
                return prefix;
            }
        }
        return null;
    }

    private boolean zipHasPath(Set<String> names, String path) {
        for (String name : names) {
            if (name.equals(path) || name.startsWith(path)) return true;
        }
        return false;
    }

    private boolean shouldSkipZipEntry(String relative) {
        String lower = relative.toLowerCase(Locale.US);
        return lower.startsWith("__macosx/") || lower.endsWith("/.ds_store") || lower.equals(".ds_store");
    }

    private String normalizeZipName(String raw) {
        if (raw == null) return "";
        String name = raw.replace('\\', '/');
        while (name.startsWith("/")) name = name.substring(1);
        while (name.startsWith("./")) name = name.substring(2);
        return name;
    }

    private File safeZipOutputFile(File targetRoot, String relative) throws IOException {
        String normalized = normalizeZipName(relative);
        if (normalized.contains("../") || normalized.equals("..") || normalized.startsWith("../")) {
            throw new IOException("Unsafe ZIP entry: " + relative);
        }
        File out = new File(targetRoot, normalized);
        String rootPath = targetRoot.getCanonicalPath() + File.separator;
        String outPath = out.getCanonicalPath();
        if (!outPath.startsWith(rootPath)) {
            throw new IOException("Unsafe ZIP entry path: " + relative);
        }
        return out;
    }

    private String safeFileName(String name) {
        if (name == null) return "";
        String cleaned = name.replace('/', '_').replace('\\', '_').trim();
        if (cleaned.equals(".") || cleaned.equals("..")) return "";
        return cleaned;
    }

    private long copy(InputStream input, FileOutputStream output) throws IOException {
        return copyWithProgress(input, output, -1, null, 0, 0);
    }

    private long copyWithProgress(InputStream input, FileOutputStream output, long totalBytes, String phase, int startPercent, int spanPercent) throws IOException {
        byte[] buffer = new byte[128 * 1024];
        long total = 0;
        int read;
        int lastPercent = -1;
        while ((read = input.read(buffer)) != -1) {
            output.write(buffer, 0, read);
            total += read;
            if (phase != null && totalBytes > 0 && spanPercent > 0) {
                int percent = startPercent + (int) Math.min(spanPercent, (total * spanPercent) / totalBytes);
                if (percent != lastPercent) {
                    lastPercent = percent;
                    updateInstallProgress(phase, percent);
                }
            }
        }
        output.flush();
        if (phase != null && spanPercent > 0 && totalBytes > 0) {
            updateInstallProgress(phase, startPercent + spanPercent);
        }
        return total;
    }

    private interface ProgressCallback {
        void onProgress(String phase, int percent);
    }

    private static final class LegacyChoice {
        static final int KIND_CANCEL = 0;
        static final int KIND_SELECT_FOLDER = 1;
        static final int KIND_DIRECTORY = 2;
        static final int KIND_ZIP_FILE = 3;

        final String label;
        final File file;
        final int kind;

        LegacyChoice(String label, File file, int kind) {
            this.label = label;
            this.file = file;
            this.kind = kind;
        }
    }

    private static final class DocumentEntry {
        final Uri uri;
        final String name;
        final boolean directory;

        DocumentEntry(Uri uri, String name, boolean directory) {
            this.uri = uri;
            this.name = name;
            this.directory = directory;
        }
    }

    private static final class InstallStats {
        int files;
        long bytes;
    }
}
