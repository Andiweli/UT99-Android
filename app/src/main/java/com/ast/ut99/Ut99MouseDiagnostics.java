package com.ast.ut99;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.os.SystemClock;
import android.provider.MediaStore;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/** Temporary file logger for diagnosing ChromeOS mouse delivery without ADB. */
public final class Ut99MouseDiagnostics {
    private static final Object LOCK = new Object();
    private static final String FILE_NAME = "UT99_ChromeOS_Mouse.txt";
    private static final String DOWNLOADS_RELATIVE =
            Environment.DIRECTORY_DOWNLOADS + "/UT99/";
    private static final int MAX_LINES = 20000;

    private static Context appContext;
    private static BufferedWriter writer;
    private static String displayLocation = "not initialized";
    private static int lineCount;
    private static long lastMoveLogMs;

    private Ut99MouseDiagnostics() {
    }

    public static void init(Context context) {
        synchronized (LOCK) {
            closeLocked();
            appContext = context != null ? context.getApplicationContext() : null;
            lineCount = 0;
            lastMoveLogMs = 0L;
            displayLocation = "logger unavailable";
            if (appContext == null) return;

            if (Build.VERSION.SDK_INT >= 29) {
                try {
                    if (Api29MediaStore.openDownloads()) {
                        displayLocation = "Downloads/UT99/" + FILE_NAME;
                    }
                } catch (Throwable ignored) {
                }
            }

            if (writer == null) {
                try {
                    File base = appContext.getExternalFilesDir("Diagnostics");
                    if (base != null) {
                        if (!base.exists()) base.mkdirs();
                        File file = new File(base, FILE_NAME);
                        writer = new BufferedWriter(new OutputStreamWriter(
                                new FileOutputStream(file, false), "UTF-8"));
                        displayLocation = file.getAbsolutePath()
                                + " (app-specific fallback)";
                    }
                } catch (Throwable ignored) {
                }
            }

            if (writer == null) return;
            rawLocked("============================================================");
            rawLocked("UT99 Android ChromeOS Mouse Diagnostic V218");
            rawLocked("Started: " + new SimpleDateFormat(
                    "yyyy-MM-dd HH:mm:ss.SSS", Locale.US).format(new Date()));
            rawLocked("Output: " + displayLocation);
            rawLocked("Android SDK=" + Build.VERSION.SDK_INT
                    + " release=" + Build.VERSION.RELEASE
                    + " manufacturer=" + Build.MANUFACTURER
                    + " model=" + Build.MODEL
                    + " device=" + Build.DEVICE
                    + " product=" + Build.PRODUCT);
            dumpInputDevicesLocked();
            rawLocked("============================================================");
        }
    }

    @android.annotation.TargetApi(29)
    private static final class Api29MediaStore {
        private Api29MediaStore() {
        }

        static boolean openDownloads() throws Exception {
            ContentResolver resolver = appContext.getContentResolver();
            Uri collection = MediaStore.Downloads.EXTERNAL_CONTENT_URI;
            Uri oldUri = findExisting(resolver, collection);
            if (oldUri != null) {
                try {
                    resolver.delete(oldUri, null, null);
                } catch (Throwable ignored) {
                }
            }

            ContentValues values = new ContentValues();
            values.put(MediaStore.MediaColumns.DISPLAY_NAME, FILE_NAME);
            values.put(MediaStore.MediaColumns.MIME_TYPE, "text/plain");
            values.put(MediaStore.MediaColumns.RELATIVE_PATH, DOWNLOADS_RELATIVE);
            Uri created = resolver.insert(collection, values);
            if (created == null) return false;

            OutputStream output = resolver.openOutputStream(created, "wt");
            if (output == null) {
                try {
                    resolver.delete(created, null, null);
                } catch (Throwable ignored) {
                }
                return false;
            }
            writer = new BufferedWriter(new OutputStreamWriter(output, "UTF-8"));
            return true;
        }

        private static Uri findExisting(ContentResolver resolver, Uri collection) {
            Cursor cursor = null;
            try {
                String[] projection = new String[] { MediaStore.MediaColumns._ID };
                String selection = MediaStore.MediaColumns.DISPLAY_NAME + "=? AND "
                        + MediaStore.MediaColumns.RELATIVE_PATH + "=?";
                String[] args = new String[] { FILE_NAME, DOWNLOADS_RELATIVE };
                cursor = resolver.query(collection, projection, selection, args, null);
                if (cursor != null && cursor.moveToFirst()) {
                    return Uri.withAppendedPath(collection,
                            Long.toString(cursor.getLong(0)));
                }
            } catch (Throwable ignored) {
            } finally {
                if (cursor != null) cursor.close();
            }
            return null;
        }
    }

    public static String getDisplayLocation() {
        synchronized (LOCK) {
            return displayLocation;
        }
    }

    public static void close() {
        synchronized (LOCK) {
            rawLocked("Logger closing");
            closeLocked();
        }
    }

    private static void closeLocked() {
        if (writer != null) {
            try {
                writer.flush();
            } catch (Throwable ignored) {
            }
            try {
                writer.close();
            } catch (Throwable ignored) {
            }
        }
        writer = null;
    }

    public static void log(String stage, String message) {
        synchronized (LOCK) {
            if (writer == null || lineCount >= MAX_LINES) return;
            rawLocked(stage + " | " + (message != null ? message : ""));
        }
    }

    public static void logMotion(String stage, MotionEvent event, View targetView) {
        if (event == null) {
            log(stage, "MotionEvent=null");
            return;
        }
        int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_MOVE || action == MotionEvent.ACTION_HOVER_MOVE) {
            long now = SystemClock.uptimeMillis();
            synchronized (LOCK) {
                if (now - lastMoveLogMs < 15L) return;
                lastMoveLogMs = now;
            }
        }

        InputDevice device = event.getDevice();
        int tool = event.getPointerCount() > 0
                ? event.getToolType(0) : MotionEvent.TOOL_TYPE_UNKNOWN;
        boolean captured = false;
        if (Build.VERSION.SDK_INT >= 26 && targetView != null) {
            try {
                captured = Api26View.hasPointerCapture(targetView);
            } catch (Throwable ignored) {
            }
        }
        String x = event.getPointerCount() > 0 ? f(event.getX(0)) : "n/a";
        String y = event.getPointerCount() > 0 ? f(event.getY(0)) : "n/a";
        log(stage, "action=" + motionActionName(action)
                + " buttons=0x" + Integer.toHexString(event.getButtonState())
                + " source=0x" + Integer.toHexString(event.getSource())
                + "[" + sourceNames(event.getSource()) + "]"
                + " tool=" + toolTypeName(tool)
                + " deviceId=" + event.getDeviceId()
                + " device=" + deviceName(device)
                + " x=" + x + " y=" + y
                + " relX=" + f(event.getAxisValue(MotionEvent.AXIS_RELATIVE_X))
                + " relY=" + f(event.getAxisValue(MotionEvent.AXIS_RELATIVE_Y))
                + " hScroll=" + f(event.getAxisValue(MotionEvent.AXIS_HSCROLL))
                + " vScroll=" + f(event.getAxisValue(MotionEvent.AXIS_VSCROLL))
                + " capture=" + captured);
    }

    public static void logPointerCapture(String stage, View view, boolean requested) {
        boolean actual = false;
        if (Build.VERSION.SDK_INT >= 26 && view != null) {
            try {
                actual = Api26View.hasPointerCapture(view);
            } catch (Throwable ignored) {
            }
        }
        log(stage, "requested=" + requested + " actual=" + actual
                + " focused=" + (view != null && view.hasFocus())
                + " view=" + (view != null ? view.getClass().getName() : "null"));
    }

    @android.annotation.TargetApi(26)
    private static final class Api26View {
        private Api26View() {
        }
        static boolean hasPointerCapture(View view) {
            return view.hasPointerCapture();
        }
    }

    private static void dumpInputDevicesLocked() {
        try {
            int[] ids = InputDevice.getDeviceIds();
            rawLocked("Input devices: " + ids.length);
            for (int id : ids) {
                InputDevice device = InputDevice.getDevice(id);
                if (device == null) continue;
                rawLocked("DEVICE id=" + id
                        + " name=" + deviceName(device)
                        + " sources=0x" + Integer.toHexString(device.getSources())
                        + "[" + sourceNames(device.getSources()) + "]"
                        + " keyboardType=" + device.getKeyboardType()
                        + " virtual=" + device.isVirtual());
            }
        } catch (Throwable t) {
            rawLocked("Input device enumeration failed: "
                    + t.getClass().getSimpleName() + ": " + t.getMessage());
        }
    }

    private static void rawLocked(String text) {
        if (writer == null || lineCount >= MAX_LINES) return;
        try {
            writer.write(String.format(Locale.US, "%9d ms | %s",
                    SystemClock.elapsedRealtime(), text != null ? text : ""));
            writer.newLine();
            writer.flush();
            lineCount++;
        } catch (Throwable ignored) {
        }
    }

    private static String deviceName(InputDevice device) {
        if (device == null) return "<none>";
        String name = device.getName();
        return name != null ? name.replace('|', '/') : "<unnamed>";
    }

    private static String motionActionName(int action) {
        switch (action) {
            case MotionEvent.ACTION_DOWN: return "DOWN";
            case MotionEvent.ACTION_UP: return "UP";
            case MotionEvent.ACTION_MOVE: return "MOVE";
            case MotionEvent.ACTION_CANCEL: return "CANCEL";
            case MotionEvent.ACTION_HOVER_MOVE: return "HOVER_MOVE";
            case MotionEvent.ACTION_SCROLL: return "SCROLL";
            case MotionEvent.ACTION_HOVER_ENTER: return "HOVER_ENTER";
            case MotionEvent.ACTION_HOVER_EXIT: return "HOVER_EXIT";
            case MotionEvent.ACTION_BUTTON_PRESS: return "BUTTON_PRESS";
            case MotionEvent.ACTION_BUTTON_RELEASE: return "BUTTON_RELEASE";
            default: return Integer.toString(action);
        }
    }

    private static String toolTypeName(int type) {
        switch (type) {
            case MotionEvent.TOOL_TYPE_FINGER: return "FINGER";
            case MotionEvent.TOOL_TYPE_STYLUS: return "STYLUS";
            case MotionEvent.TOOL_TYPE_MOUSE: return "MOUSE";
            case MotionEvent.TOOL_TYPE_ERASER: return "ERASER";
            default: return "UNKNOWN(" + type + ")";
        }
    }

    private static String sourceNames(int source) {
        StringBuilder sb = new StringBuilder();
        addSource(sb, source, InputDevice.SOURCE_KEYBOARD, "KEYBOARD");
        addSource(sb, source, InputDevice.SOURCE_DPAD, "DPAD");
        addSource(sb, source, InputDevice.SOURCE_GAMEPAD, "GAMEPAD");
        addSource(sb, source, InputDevice.SOURCE_JOYSTICK, "JOYSTICK");
        addSource(sb, source, InputDevice.SOURCE_MOUSE, "MOUSE");
        if (Build.VERSION.SDK_INT >= 26) {
            addSource(sb, source, InputDevice.SOURCE_MOUSE_RELATIVE, "MOUSE_RELATIVE");
        }
        addSource(sb, source, InputDevice.SOURCE_TOUCHSCREEN, "TOUCHSCREEN");
        addSource(sb, source, InputDevice.SOURCE_TOUCHPAD, "TOUCHPAD");
        if (sb.length() == 0) sb.append("unknown");
        return sb.toString();
    }

    private static void addSource(StringBuilder sb, int source, int flag, String name) {
        if ((source & flag) == flag) {
            if (sb.length() > 0) sb.append(',');
            sb.append(name);
        }
    }

    private static String f(float value) {
        return String.format(Locale.US, "%.3f", value);
    }
}
