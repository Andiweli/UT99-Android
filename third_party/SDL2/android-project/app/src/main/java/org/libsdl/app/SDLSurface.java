package org.libsdl.app;

import android.content.Context;
import android.content.pm.ActivityInfo;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Build;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Display;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;


/**
    SDLSurface. This is what we draw on, so we need to know when it's created
    in order to do anything useful.

    Because of this, that's where we set up the SDL thread
*/
public class SDLSurface extends SurfaceView implements SurfaceHolder.Callback,
    View.OnKeyListener, View.OnTouchListener, SensorEventListener  {

    // Sensors
    protected SensorManager mSensorManager;
    protected Display mDisplay;

    // Keep track of the surface size to normalize touch events
    protected float mWidth, mHeight;

    // UT99_ANDROID_V74_TOUCH_NATIVE_SCALE_FIX:
    // When SurfaceHolder uses a smaller fixed buffer (75%/50%) the SurfaceView
    // still occupies the full Android display.  MotionEvent coordinates are in
    // View/display coordinates, while SDL's mWidth/mHeight below are the
    // smaller render buffer.  Keep a separate touch normalization size so the
    // finger position maps to the same visible point after Android upscales the
    // buffer to fullscreen.
    protected float mUT99TouchWidth, mUT99TouchHeight;

    // Is SurfaceView ready for rendering
    public boolean mIsSurfaceReady;

    // Startup
    public SDLSurface(Context context) {
        super(context);
        getHolder().addCallback(this);

        setFocusable(true);
        setFocusableInTouchMode(true);
        requestFocus();
        setOnKeyListener(this);
        setOnTouchListener(this);

        mDisplay = ((WindowManager)context.getSystemService(Context.WINDOW_SERVICE)).getDefaultDisplay();
        mSensorManager = (SensorManager)context.getSystemService(Context.SENSOR_SERVICE);

        setOnGenericMotionListener(SDLActivity.getMotionListener());

        // Some arbitrary defaults to avoid a potential division by zero
        mWidth = 1.0f;
        mHeight = 1.0f;
        mUT99TouchWidth = 1.0f;
        mUT99TouchHeight = 1.0f;

        mIsSurfaceReady = false;
    }

    public void handlePause() {
        enableSensor(Sensor.TYPE_ACCELEROMETER, false);
    }

    public void handleResume() {
        setFocusable(true);
        setFocusableInTouchMode(true);
        requestFocus();
        setOnKeyListener(this);
        setOnTouchListener(this);
        enableSensor(Sensor.TYPE_ACCELEROMETER, true);
    }

    public Surface getNativeSurface() {
        return getHolder().getSurface();
    }

    // Called when we have a valid drawing surface
    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        Log.v("SDL", "surfaceCreated()");
        SDLActivity.onNativeSurfaceCreated();
    }

    // Called when we lose the surface
    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.v("SDL", "surfaceDestroyed()");

        // Transition to pause, if needed
        SDLActivity.mNextNativeState = SDLActivity.NativeState.PAUSED;
        SDLActivity.handleNativeState();

        mIsSurfaceReady = false;
        SDLActivity.onNativeSurfaceDestroyed();
    }

    // Called when the surface is resized
    @Override
    public void surfaceChanged(SurfaceHolder holder,
                               int format, int width, int height) {
        Log.v("SDL", "surfaceChanged()");

        if (SDLActivity.mSingleton == null) {
            return;
        }

        mWidth = width;
        mHeight = height;
        int nDeviceWidth = width;
        int nDeviceHeight = height;
        try
        {
            if (Build.VERSION.SDK_INT >= 17 /* Android 4.2 (JELLY_BEAN_MR1) */) {
                DisplayMetrics realMetrics = new DisplayMetrics();
                mDisplay.getRealMetrics( realMetrics );
                nDeviceWidth = realMetrics.widthPixels;
                nDeviceHeight = realMetrics.heightPixels;
            }
        } catch(Exception ignored) {
        }

        synchronized(SDLActivity.getContext()) {
            // In case we're waiting on a size change after going fullscreen, send a notification.
            SDLActivity.getContext().notifyAll();
        }

        int viewWidth = getWidth();
        int viewHeight = getHeight();
        if (viewWidth <= 0) {
            viewWidth = nDeviceWidth;
        }
        if (viewHeight <= 0) {
            viewHeight = nDeviceHeight;
        }
        // UT99_ANDROID_AAOS_TOUCH_SAFE_AREA_FIX:
        // MotionEvent.getX()/getY() are local to this SurfaceView. On Android
        // Automotive, getRealMetrics() includes vehicle-owned system-bar areas
        // that are deliberately outside the app window. Using the physical
        // display size there therefore shifts/scales menu touch coordinates.
        // Normalize AAOS touch against the actual visible SurfaceView instead.
        // Keep the established max(surface/view/device) behavior everywhere
        // else so phone/tablet/Retroid/OUYA input remains unchanged.
        boolean ut99Automotive = false;
        try {
            ut99Automotive = getContext().getPackageManager().hasSystemFeature("android.hardware.type.automotive");
        } catch (Throwable ignored) {
        }

        if (ut99Automotive) {
            mUT99TouchWidth = Math.max(1.0f, (float)viewWidth);
            mUT99TouchHeight = Math.max(1.0f, (float)viewHeight);
        } else {
            mUT99TouchWidth = Math.max(1.0f, (float)Math.max(width, Math.max(viewWidth, nDeviceWidth)));
            mUT99TouchHeight = Math.max(1.0f, (float)Math.max(height, Math.max(viewHeight, nDeviceHeight)));
        }
        Log.v("SDL", "Window size: " + width + "x" + height);
        Log.v("SDL", "Device size: " + nDeviceWidth + "x" + nDeviceHeight);
        Log.i("UT99SDL", "UT99_ANDROID_AAOS_TOUCH_SAFE_AREA_FIX surface=" + width + "x" + height
                + " view=" + viewWidth + "x" + viewHeight
                + " device=" + nDeviceWidth + "x" + nDeviceHeight
                + " touchNorm=" + (int)mUT99TouchWidth + "x" + (int)mUT99TouchHeight
                + " automotive=" + ut99Automotive);
        SDLActivity.nativeSetScreenResolution(width, height, nDeviceWidth, nDeviceHeight, mDisplay.getRefreshRate());
        SDLActivity.onNativeResize();

        // Prevent a screen distortion glitch,
        // for instance when the device is in Landscape and a Portrait App is resumed.
        boolean skip = false;
        int requestedOrientation = SDLActivity.mSingleton.getRequestedOrientation();

        if (requestedOrientation == ActivityInfo.SCREEN_ORIENTATION_PORTRAIT || requestedOrientation == ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT) {
            if (mWidth > mHeight) {
               skip = true;
            }
        } else if (requestedOrientation == ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE || requestedOrientation == ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE) {
            if (mWidth < mHeight) {
               skip = true;
            }
        }

        // Special Patch for Square Resolution: Black Berry Passport
        if (skip) {
           double min = Math.min(mWidth, mHeight);
           double max = Math.max(mWidth, mHeight);

           if (max / min < 1.20) {
              Log.v("SDL", "Don't skip on such aspect-ratio. Could be a square resolution.");
              skip = false;
           }
        }

        // Don't skip in MultiWindow.
        if (skip) {
            if (Build.VERSION.SDK_INT >= 24 /* Android 7.0 (N) */) {
                if (SDLActivity.mSingleton.isInMultiWindowMode()) {
                    Log.v("SDL", "Don't skip in Multi-Window");
                    skip = false;
                }
            }
        }

        if (skip) {
           Log.v("SDL", "Skip .. Surface is not ready.");
           mIsSurfaceReady = false;
           return;
        }

        /* If the surface has been previously destroyed by onNativeSurfaceDestroyed, recreate it here */
        SDLActivity.onNativeSurfaceChanged();

        /* Surface is ready */
        mIsSurfaceReady = true;

        SDLActivity.mNextNativeState = SDLActivity.NativeState.RESUMED;
        SDLActivity.handleNativeState();
    }

    // Key events
    @Override
    public boolean onKey(View v, int keyCode, KeyEvent event) {
        return SDLActivity.handleKeyEvent(v, keyCode, event, null);
    }

    // UT99_ANDROID_CHROMEOS_RESOLUTION_MOUSE_MAP_V218:
    // SurfaceHolder.setFixedSize() changes the render buffer but not this
    // fullscreen SurfaceView. ChromeOS mouse coordinates therefore remain in
    // View space and must be mapped into the active SDL/render-buffer space.
    // These helpers are called only by GameActivity's confirmed ChromeOS path;
    // Android Automotive and every other Android input path remain untouched.
    public float ut99MapChromeOSAbsoluteMouseXV218(float viewX) {
        return ut99MapChromeOSAbsoluteMouseAxisV218(
                viewX, (float)getWidth(), mWidth);
    }

    public float ut99MapChromeOSAbsoluteMouseYV218(float viewY) {
        return ut99MapChromeOSAbsoluteMouseAxisV218(
                viewY, (float)getHeight(), mHeight);
    }

    private static float ut99MapChromeOSAbsoluteMouseAxisV218(
            float value, float viewSize, float surfaceSize) {
        if (surfaceSize <= 1.0f) return value;
        if (viewSize > 1.0f && Math.abs(viewSize - surfaceSize) > 1.0f) {
            value = value * (surfaceSize / viewSize);
        }
        if (value < 0.0f) value = 0.0f;
        if (value > surfaceSize - 1.0f) value = surfaceSize - 1.0f;
        return value;
    }

    // Touch events
    @Override
    public boolean onTouch(View v, MotionEvent event) {
        /* Ref: http://developer.android.com/training/gestures/multi.html */
        int touchDevId = event.getDeviceId();
        final int pointerCount = event.getPointerCount();
        int action = event.getActionMasked();
        int pointerFingerId;
        int i = -1;
        float x,y,p;

        /*
         * Prevent id to be -1, since it's used in SDL internal for synthetic events
         * Appears when using Android emulator, eg:
         *  adb shell input mouse tap 100 100
         *  adb shell input touchscreen tap 100 100
         */
        if (touchDevId < 0) {
            touchDevId -= 1;
        }

        // 12290 = Samsung DeX mode desktop mouse
        // 12290 = 0x3002 = 0x2002 | 0x1002 = SOURCE_MOUSE | SOURCE_TOUCHSCREEN
        // 0x2   = SOURCE_CLASS_POINTER
        // UT99_ANDROID_CHROMEOS_TOUCHPAD_SOURCE_V214:
        // ChromeOS can append SOURCE_TOUCHPAD or other source bits. Exact
        // equality misroutes those events into SDL's finger/touch path.
        final int ut99V214Source = event.getSource();
        boolean ut99V214MouseLike =
                (ut99V214Source & InputDevice.SOURCE_MOUSE) == InputDevice.SOURCE_MOUSE
                || (Build.VERSION.SDK_INT >= 26
                && (ut99V214Source & InputDevice.SOURCE_MOUSE_RELATIVE)
                == InputDevice.SOURCE_MOUSE_RELATIVE)
                || (ut99V214Source & InputDevice.SOURCE_TOUCHPAD)
                == InputDevice.SOURCE_TOUCHPAD;
        if (!ut99V214MouseLike) {
            try {
                final boolean chromeOS = getContext().getPackageManager().hasSystemFeature(
                        "org.chromium.arc.device_management");
                final boolean controller =
                        (ut99V214Source & InputDevice.SOURCE_GAMEPAD)
                        == InputDevice.SOURCE_GAMEPAD
                        || (ut99V214Source & InputDevice.SOURCE_JOYSTICK)
                        == InputDevice.SOURCE_JOYSTICK;
                ut99V214MouseLike = chromeOS && !controller
                        && (((ut99V214Source & InputDevice.SOURCE_CLASS_POINTER)
                        == InputDevice.SOURCE_CLASS_POINTER)
                        || ((ut99V214Source & InputDevice.SOURCE_CLASS_POSITION)
                        == InputDevice.SOURCE_CLASS_POSITION));
            } catch (Throwable ignored) {
            }
        }
        if (!ut99V214MouseLike && pointerCount > 0) {
            try {
                ut99V214MouseLike = event.getToolType(0) == MotionEvent.TOOL_TYPE_MOUSE;
            } catch (Throwable ignored) {
            }
        }
        if (ut99V214MouseLike) {
            int mouseButton = 1;
            try {
                Object object = event.getClass().getMethod("getButtonState").invoke(event);
                if (object != null) {
                    mouseButton = (Integer) object;
                }
            } catch(Exception ignored) {
            }

            // We need to check if we're in relative mouse mode and get the axis offset rather than the x/y values
            // if we are.  We'll leverage our existing mouse motion listener
            SDLGenericMotionListener_API12 motionListener = SDLActivity.getMotionListener();
            x = motionListener.getEventX(event);
            y = motionListener.getEventY(event);

            final boolean relativeMouse = motionListener.inRelativeMode();

            boolean ut99ChromeOSV218 = false;
            try {
                ut99ChromeOSV218 = getContext().getPackageManager().hasSystemFeature(
                        "org.chromium.arc.device_management");
            } catch (Throwable ignored) {
            }

            // UT99_ANDROID_V79_OUYA_MOUSE_NATIVE_SCALE_FIX:
            // OUYA's touchpad/native mouse cursor reports coordinates in the
            // fullscreen Android view space even when UT99 renders to a smaller
            // fixed SurfaceHolder backbuffer (75%/50% native Res.).  Touchscreen
            // input already uses normalized view coordinates in the non-mouse
            // branch below, but SOURCE_MOUSE needs the same display->surface
            // mapping before SDL creates mouse motion/button events.
            // Example: 1920x1080 view, 1440x810 surface => x/y * 0.75.
            if (!relativeMouse && action != MotionEvent.ACTION_SCROLL
                    && ut99ChromeOSV218) {
                // Use the local SurfaceView dimensions on ChromeOS. Unlike real
                // display metrics, these are the coordinate space MotionEvent
                // actually uses (also correct for ARC window/fullscreen changes).
                x = ut99MapChromeOSAbsoluteMouseXV218(x);
                y = ut99MapChromeOSAbsoluteMouseYV218(y);
            } else if (!relativeMouse && action != MotionEvent.ACTION_SCROLL
                    && mWidth > 1.0f && mHeight > 1.0f
                    && mUT99TouchWidth > 1.0f && mUT99TouchHeight > 1.0f
                    && (mUT99TouchWidth > mWidth + 1.0f || mUT99TouchHeight > mHeight + 1.0f)) {
                x = x * (mWidth / mUT99TouchWidth);
                y = y * (mHeight / mUT99TouchHeight);
                if (x < 0.0f) { x = 0.0f; }
                if (y < 0.0f) { y = 0.0f; }
                if (x > mWidth - 1.0f) { x = mWidth - 1.0f; }
                if (y > mHeight - 1.0f) { y = mHeight - 1.0f; }
            }

            SDLActivity.onNativeMouse(mouseButton, action, x, y, relativeMouse);
        } else {
            switch(action) {
                case MotionEvent.ACTION_MOVE:
                    for (i = 0; i < pointerCount; i++) {
                        pointerFingerId = event.getPointerId(i);
                        x = event.getX(i) / mUT99TouchWidth;
                        y = event.getY(i) / mUT99TouchHeight;
                        p = event.getPressure(i);
                        if (p > 1.0f) {
                            // may be larger than 1.0f on some devices
                            // see the documentation of getPressure(i)
                            p = 1.0f;
                        }
                        SDLActivity.onNativeTouch(touchDevId, pointerFingerId, action, x, y, p);
                    }
                    break;

                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_DOWN:
                    // Primary pointer up/down, the index is always zero
                    i = 0;
                    /* fallthrough */
                case MotionEvent.ACTION_POINTER_UP:
                case MotionEvent.ACTION_POINTER_DOWN:
                    // Non primary pointer up/down
                    if (i == -1) {
                        i = event.getActionIndex();
                    }

                    pointerFingerId = event.getPointerId(i);
                    x = event.getX(i) / mUT99TouchWidth;
                    y = event.getY(i) / mUT99TouchHeight;
                    p = event.getPressure(i);
                    if (p > 1.0f) {
                        // may be larger than 1.0f on some devices
                        // see the documentation of getPressure(i)
                        p = 1.0f;
                    }
                    SDLActivity.onNativeTouch(touchDevId, pointerFingerId, action, x, y, p);
                    break;

                case MotionEvent.ACTION_CANCEL:
                    for (i = 0; i < pointerCount; i++) {
                        pointerFingerId = event.getPointerId(i);
                        x = event.getX(i) / mUT99TouchWidth;
                        y = event.getY(i) / mUT99TouchHeight;
                        p = event.getPressure(i);
                        if (p > 1.0f) {
                            // may be larger than 1.0f on some devices
                            // see the documentation of getPressure(i)
                            p = 1.0f;
                        }
                        SDLActivity.onNativeTouch(touchDevId, pointerFingerId, MotionEvent.ACTION_UP, x, y, p);
                    }
                    break;

                default:
                    break;
            }
        }

        return true;
   }

    @Override
    public boolean onHoverEvent(MotionEvent event) {
        // UT99_ANDROID_CHROMEOS_VIEW_HOVER_ROUTE_V216:
        // ARC may send cursor motion through View.dispatchHoverEvent() without
        // invoking the generic-motion listener used by SDL. Forward it through
        // that listener explicitly so menu hover and the first gameplay motion
        // reach Android_OnMouse.
        if (event != null) {
            try {
                if (SDLActivity.getMotionListener().onGenericMotion(this, event)) {
                    return true;
                }
            } catch (Throwable ignored) {
            }
        }
        return super.onHoverEvent(event);
    }

    // Sensor events
    public void enableSensor(int sensortype, boolean enabled) {
        // TODO: This uses getDefaultSensor - what if we have >1 accels?
        if (enabled) {
            mSensorManager.registerListener(this,
                            mSensorManager.getDefaultSensor(sensortype),
                            SensorManager.SENSOR_DELAY_GAME, null);
        } else {
            mSensorManager.unregisterListener(this,
                            mSensorManager.getDefaultSensor(sensortype));
        }
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {
        // TODO
    }

    @Override
    public void onSensorChanged(SensorEvent event) {
        if (event.sensor.getType() == Sensor.TYPE_ACCELEROMETER) {

            // Since we may have an orientation set, we won't receive onConfigurationChanged events.
            // We thus should check here.
            int newOrientation;

            float x, y;
            switch (mDisplay.getRotation()) {
                case Surface.ROTATION_90:
                    x = -event.values[1];
                    y = event.values[0];
                    newOrientation = SDLActivity.SDL_ORIENTATION_LANDSCAPE;
                    break;
                case Surface.ROTATION_270:
                    x = event.values[1];
                    y = -event.values[0];
                    newOrientation = SDLActivity.SDL_ORIENTATION_LANDSCAPE_FLIPPED;
                    break;
                case Surface.ROTATION_180:
                    x = -event.values[0];
                    y = -event.values[1];
                    newOrientation = SDLActivity.SDL_ORIENTATION_PORTRAIT_FLIPPED;
                    break;
                case Surface.ROTATION_0:
                default:
                    x = event.values[0];
                    y = event.values[1];
                    newOrientation = SDLActivity.SDL_ORIENTATION_PORTRAIT;
                    break;
            }

            if (newOrientation != SDLActivity.mCurrentOrientation) {
                SDLActivity.mCurrentOrientation = newOrientation;
                SDLActivity.onNativeOrientationChanged(newOrientation);
            }

            SDLActivity.onNativeAccel(-x / SensorManager.GRAVITY_EARTH,
                                      y / SensorManager.GRAVITY_EARTH,
                                      event.values[2] / SensorManager.GRAVITY_EARTH);


        }
    }

    // Captured pointer events for API 26.
    public boolean onCapturedPointerEvent(MotionEvent event)
    {
        int action = event.getActionMasked();

        float x, y;
        switch (action) {
            case MotionEvent.ACTION_SCROLL:
                x = event.getAxisValue(MotionEvent.AXIS_HSCROLL, 0);
                y = event.getAxisValue(MotionEvent.AXIS_VSCROLL, 0);
                SDLActivity.onNativeMouse(0, action, x, y, false);
                return true;

            case MotionEvent.ACTION_HOVER_MOVE:
            case MotionEvent.ACTION_MOVE:
                x = event.getX(0);
                y = event.getY(0);
                SDLActivity.onNativeMouse(0, action, x, y, true);
                return true;

            case MotionEvent.ACTION_BUTTON_PRESS:
            case MotionEvent.ACTION_BUTTON_RELEASE:

                // Change our action value to what SDL's code expects.
                if (action == MotionEvent.ACTION_BUTTON_PRESS) {
                    action = MotionEvent.ACTION_DOWN;
                } else { /* MotionEvent.ACTION_BUTTON_RELEASE */
                    action = MotionEvent.ACTION_UP;
                }

                x = event.getX(0);
                y = event.getY(0);
                int button = event.getButtonState();

                SDLActivity.onNativeMouse(button, action, x, y, true);
                return true;
        }

        return false;
    }

    @Override
    public void onPointerCaptureChange(boolean hasCapture) {
        super.onPointerCaptureChange(hasCapture);
    }
}
