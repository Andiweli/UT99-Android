package com.ast.ut99;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.MotionEvent;
import android.widget.FrameLayout;

import com.ast.retrotouch.RetroTouchAdapter;
import com.ast.retrotouch.RetroTouchControl;
import com.ast.retrotouch.RetroTouchControllers;
import com.ast.retrotouch.RetroTouchLayout;
import com.ast.retrotouch.RetroTouchMode;
import com.ast.retrotouch.RetroTouchNavigation;
import com.ast.retrotouch.RetroTouchView;

import org.libsdl.app.SDLControllerManager;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * UT99-specific bridge between RetroTouch and the existing Android/Unreal input path.
 *
 * RetroTouch owns drawing, multitouch, editing and persistence. This bridge maps
 * its abstract actions to the native UT99 input queue and selects one of three
 * explicit UI states reported by the engine:
 *
 *   OFF        = CityIntro/flyby/loading, touch passes through to SDL/UT
 *   NAVIGATION = UWindow/UT menus, RetroTouch D-pad + OK + Back
 *   GAMEPLAY   = live map, RetroTouch FPS controls
 */
final class Ut99RetroTouchBridge {
    private static final String TAG = "UT99RetroTouch";

    private static final long STATE_POLL_MS = 50L;
    private static final long CONFIG_POLL_MS = 650L;
    private static final long CONTROLLER_POLL_MS = 200L;

    // Keep these IDs stable: RetroTouch stores edited layouts under them.
    private static final String GAMEPLAY_LAYOUT_ID = "ut99_android_retrotouch_beta4_gameplay_v2";
    private static final String NAVIGATION_LAYOUT_ID = "ut99_android_retrotouch_beta4_navigation_v1";

    private static final String ACTION_FIRE = "fire";
    private static final String ACTION_ALT_FIRE = "alt_fire";
    private static final String ACTION_JUMP = "jump";
    private static final String ACTION_CROUCH = "crouch";
    private static final String ACTION_NEXT_WEAPON = "weapon_next";
    private static final String ACTION_MENU = "menu";
    private static final String ACTION_MENU_OK = "menu_ok";
    private static final String ACTION_MENU_BACK = "menu_back";

    private final GameActivity host;
    private final RetroTouchView view;
    private final Handler handler = new Handler(Looper.getMainLooper());

    private boolean running;
    private boolean editing;
    private boolean cachedTouchControlsEnabled = true;
    private boolean cachedControllerConnected;
    private long lastConfigReadMs;
    private long lastControllerReadMs;
    private int lastNativeResetSerial;
    private int lastNativeUiState = -1;
    private RetroTouchMode appliedMode = RetroTouchMode.OFF;

    // Track only states asserted by RetroTouch itself. Hardware controller input
    // shares UT99's native bridge, so handover cleanup must never emit blanket
    // releases that can cancel a newly active physical controller state.
    private final Set<Integer> touchPressedKeys = new HashSet<Integer>();
    private float touchMoveX;
    private float touchMoveY;

    private final Runnable statePoll = new Runnable() {
        @Override
        public void run() {
            if (!running) {
                return;
            }
            refreshState(false);
            handler.postDelayed(this, STATE_POLL_MS);
        }
    };

    Ut99RetroTouchBridge(GameActivity host) {
        this.host = host;
        this.view = new RetroTouchView(host);

        registerActions();
        view.setGameplayLayout(buildGameplayLayout());
        view.setNavigationLayout(buildNavigationLayout());
        view.setListener(new RetroTouchAdapter() {
            @Override
            public void onAction(String actionId, boolean pressed) {
                handleAction(actionId, pressed);
            }

            @Override
            public void onMove(float x, float y) {
                if (!editing && appliedMode == RetroTouchMode.GAMEPLAY) {
                    // Existing UT99 native path turns these axes into the already
                    // configurable left-stick movement bindings.
                    touchMoveX = x;
                    touchMoveY = y;
                    host.ut99RetroTouchMove(x, y);
                }
            }

            @Override
            public void onLook(float deltaX, float deltaY) {
                if (editing || appliedMode != RetroTouchMode.GAMEPLAY) {
                    return;
                }
                // RetroTouch reports look delta normalized by the shorter view edge.
                // Convert it back to the proven v107/v103 UT99 relative-look gain.
                float minEdge = Math.max(1.0f, Math.min(view.getWidth(), view.getHeight()));
                float x = clamp(deltaX * minEdge * 0.0210f, -1.0f, 1.0f);
                float y = clamp(deltaY * minEdge * 0.0210f, -1.0f, 1.0f);
                host.ut99RetroTouchLook(x, y);
            }

            @Override
            public void onEditorStateChanged(boolean isEditing) {
                editing = isEditing;
                if (isEditing) {
                    // Never keep native movement/buttons held while controls are moved.
                    releaseHostInputState();
                }
            }
        });

        // Primary fire can remain held while the same finger continues to look.
        view.setLookWhileHoldingAction(ACTION_FIRE, true);

        // UT99 owns controller-presence suppression explicitly. Do not combine it
        // with RetroTouch's temporary "hide after controller input" feature.
        // RetroTouchControllers.isControllerConnected() is polled below so the
        // overlay disappears while a GAMEPAD/JOYSTICK exists and returns after
        // disconnect/disable without restarting the Activity.
        view.setAutoHideOnController(false);
        cachedControllerConnected = RetroTouchControllers.isControllerConnected();
        lastControllerReadMs = android.os.SystemClock.uptimeMillis();
        view.setOverlayEnabled(true);
        view.setMode(RetroTouchMode.OFF);
        lastNativeResetSerial = host.ut99RetroTouchInputResetSerial();
    }

    RetroTouchView getView() {
        return view;
    }

    void attach() {
        FrameLayout.LayoutParams fill = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT);
        host.addContentView(view, fill);
        view.bringToFront();
        start();
        Log.i(TAG, "RetroTouch 1.0.0-beta.4 bridge attached with OFF/NAVIGATION/GAMEPLAY state machine");
    }

    void start() {
        if (running) {
            refreshState(true);
            return;
        }
        running = true;
        handler.removeCallbacks(statePoll);
        refreshState(true);
        handler.postDelayed(statePoll, STATE_POLL_MS);
    }

    void pause() {
        running = false;
        handler.removeCallbacks(statePoll);
        resetRetroTouchAndHost();
        applyMode(RetroTouchMode.OFF);
    }

    void destroy() {
        pause();
        handler.removeCallbacksAndMessages(null);
    }

    void onHardwareControllerInput() {
        // A real GAMEPAD/JOYSTICK event is authoritative. Retroid-class handhelds
        // can remove and recreate their integrated controller while the Activity
        // stays alive. RetroTouch sees the new Android InputDevice immediately,
        // but SDL2's joystick list may still contain the old/disconnected state.
        // Force SDL's own device poll before hiding the overlay so analog sticks,
        // D-pad and SDL_CONTROLLER* events are available again without restart.
        final boolean wasConnected = cachedControllerConnected;
        cachedControllerConnected = true;
        lastControllerReadMs = android.os.SystemClock.uptimeMillis();
        Log.i(TAG, "hardware controller input observed; suppressing RetroTouch");

        // First drop only RetroTouch-owned held state, then refresh SDL's device
        // inventory. GameActivity dispatches the physical key/axis immediately
        // after this callback, so the new hardware event wins the handover.
        refreshState(false);
        if (!wasConnected) {
            rescanSdlControllers("hardware-input");
            scheduleSdlControllerRescan();
        }
    }

    void beforeHostTouch(MotionEvent event) {
        if (event == null || !running) {
            return;
        }
        int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
            // Query the native state synchronously before a new pointer is routed.
            // This prevents one stale OFF/GAMEPLAY frame at menu transitions.
            refreshState(true);
        }
    }

    private void registerActions() {
        view.registerAction(ACTION_FIRE, "Fire");
        view.registerAction(ACTION_ALT_FIRE, "Alt Fire");
        view.registerAction(ACTION_JUMP, "Jump");
        view.registerAction(ACTION_CROUCH, "Crouch");
        view.registerAction(ACTION_NEXT_WEAPON, "Next\nWeapon");
        view.registerAction(ACTION_MENU, "Menu");
        view.registerAction(ACTION_MENU_OK, "OK");
        view.registerAction(ACTION_MENU_BACK, "Back");
    }

    private RetroTouchLayout buildGameplayLayout() {
        List<RetroTouchControl> controls = new ArrayList<RetroTouchControl>();

        // Left: movement. Right: large invisible LOOK area in normal gameplay.
        controls.add(RetroTouchControl.moveStick("move", 0.16f, 0.77f, 0.27f));
        controls.add(RetroTouchControl.lookZone("look", 0.73f, 0.50f, 0.54f, 0.88f));

        // FIRE center remains inside LOOK so Beta 4 can aim while firing.
        controls.add(RetroTouchControl.button(
                "fire_button", ACTION_FIRE, "Fire", 0.90f, 0.63f, 0.145f));
        controls.add(RetroTouchControl.button(
                "alt_fire_button", ACTION_ALT_FIRE, "Alt Fire", 0.90f, 0.79f, 0.125f));
        controls.add(RetroTouchControl.button(
                "next_weapon_button", ACTION_NEXT_WEAPON, "Next\nWeapon", 0.90f, 0.45f, 0.115f));
        controls.add(RetroTouchControl.button(
                "jump_button", ACTION_JUMP, "Jump", 0.73f, 0.87f, 0.120f));
        controls.add(RetroTouchControl.button(
                "crouch_button", ACTION_CROUCH, "Crouch", 0.86f, 0.91f, 0.105f));
        controls.add(RetroTouchControl.button(
                "menu_button", ACTION_MENU, "Menu", 0.075f, 0.095f, 0.085f));

        return new RetroTouchLayout(GAMEPLAY_LAYOUT_ID, controls);
    }

    private RetroTouchLayout buildNavigationLayout() {
        List<RetroTouchControl> controls = new ArrayList<RetroTouchControl>();

        // Classic menu controls requested for UT99 touch operation. The D-pad emits
        // RetroTouchNavigation nav_* action IDs; OK and Back are normal actions.
        controls.add(RetroTouchControl.dPad("navigation", 0.18f, 0.73f, 0.30f));
        controls.add(RetroTouchControl.button(
                "menu_ok_button", ACTION_MENU_OK, "OK", 0.86f, 0.68f, 0.14f));
        controls.add(RetroTouchControl.button(
                "menu_back_button", ACTION_MENU_BACK, "Back", 0.74f, 0.83f, 0.11f));

        return new RetroTouchLayout(NAVIGATION_LAYOUT_ID, controls);
    }

    private void handleAction(String actionId, boolean pressed) {
        if (RetroTouchNavigation.UP.equals(actionId)) {
            sendTouchButton(19, pressed); // KEYCODE_DPAD_UP
            return;
        }
        if (RetroTouchNavigation.DOWN.equals(actionId)) {
            sendTouchButton(20, pressed); // KEYCODE_DPAD_DOWN
            return;
        }
        if (RetroTouchNavigation.LEFT.equals(actionId)) {
            sendTouchButton(21, pressed); // KEYCODE_DPAD_LEFT
            return;
        }
        if (RetroTouchNavigation.RIGHT.equals(actionId)) {
            sendTouchButton(22, pressed); // KEYCODE_DPAD_RIGHT
            return;
        }
        if (ACTION_MENU_OK.equals(actionId)) {
            sendTouchButton(96, pressed); // KEYCODE_BUTTON_A -> Enter in menu
            return;
        }
        if (ACTION_MENU_BACK.equals(actionId)) {
            sendTouchButton(82, pressed); // KEYCODE_MENU -> single Escape toggle
            if (pressed) scheduleImmediateStateRefresh();
            return;
        }

        // Gameplay-only actions. Ignore any stale callback that arrives during a
        // mode transition; applyMode() also resets all held state.
        if (appliedMode != RetroTouchMode.GAMEPLAY) {
            return;
        }

        if (ACTION_FIRE.equals(actionId)) {
            sendTouchButton(105, pressed); // KEYCODE_BUTTON_R2 / TriggerR
        } else if (ACTION_ALT_FIRE.equals(actionId)) {
            sendTouchButton(104, pressed); // KEYCODE_BUTTON_L2 / TriggerL
        } else if (ACTION_JUMP.equals(actionId)) {
            sendTouchButton(96, pressed);  // KEYCODE_BUTTON_A
        } else if (ACTION_CROUCH.equals(actionId)) {
            sendTouchButton(97, pressed);  // KEYCODE_BUTTON_B
        } else if (ACTION_NEXT_WEAPON.equals(actionId)) {
            sendTouchButton(103, pressed); // KEYCODE_BUTTON_R1
        } else if (ACTION_MENU.equals(actionId)) {
            sendTouchButton(82, pressed);  // KEYCODE_MENU / Escape toggle
            if (pressed) scheduleImmediateStateRefresh();
        }
    }

    private void scheduleImmediateStateRefresh() {
        handler.postDelayed(new Runnable() {
            @Override
            public void run() {
                if (running) {
                    refreshState(true);
                }
            }
        }, 24L);
    }

    private void refreshState(boolean forceLog) {
        if (!running) {
            return;
        }

        int resetSerial = host.ut99RetroTouchInputResetSerial();
        if (resetSerial != lastNativeResetSerial) {
            lastNativeResetSerial = resetSerial;
            // UInput::ResetInput() is used for respawn/map transitions. Drop every
            // RetroTouch pointer/latch at the same boundary to prevent stuck motion.
            resetRetroTouchAndHost();
            Log.i(TAG, "native input reset observed serial=" + resetSerial);
        }

        long now = android.os.SystemClock.uptimeMillis();
        if (forceLog || now - lastConfigReadMs >= CONFIG_POLL_MS) {
            lastConfigReadMs = now;
            cachedTouchControlsEnabled = host.ut99RetroTouchPreferenceEnabled();
        }

        boolean controllerChanged = false;
        if (forceLog || now - lastControllerReadMs >= CONTROLLER_POLL_MS) {
            lastControllerReadMs = now;
            boolean connected = RetroTouchControllers.isControllerConnected();
            controllerChanged = connected != cachedControllerConnected;
            cachedControllerConnected = connected;
        }

        final int nativeUiState = host.ut99RetroTouchUiState();
        final boolean touchCapable = host.ut99RetroTouchHasTouchscreen();

        RetroTouchMode wanted = RetroTouchMode.OFF;
        if (cachedTouchControlsEnabled && touchCapable && !cachedControllerConnected) {
            if (nativeUiState == 1) {
                wanted = RetroTouchMode.NAVIGATION;
            } else if (nativeUiState == 2) {
                wanted = RetroTouchMode.GAMEPLAY;
            }
        }

        if (forceLog || nativeUiState != lastNativeUiState || controllerChanged) {
            Log.i(TAG, "state native=" + nativeUiState
                    + " enabled=" + cachedTouchControlsEnabled
                    + " touchCapable=" + touchCapable
                    + " controller=" + cachedControllerConnected
                    + " wanted=" + wanted);
            lastNativeUiState = nativeUiState;
        }
        applyMode(wanted);

        if (controllerChanged) {
            // Handover ordering matters: release only RetroTouch-owned state first,
            // then ask SDL2 to remove/add its physical joystick. This prevents the
            // cleanup edge from overwriting the first state of a newly active pad.
            rescanSdlControllers(cachedControllerConnected ? "connect-poll" : "disconnect-poll");
            if (cachedControllerConnected) {
                scheduleSdlControllerRescan();
            }
        }
    }


    private void rescanSdlControllers(String reason) {
        try {
            SDLControllerManager.initialize();
            SDLControllerManager.pollInputDevices();
            Log.i(TAG, "SDL controller rescan reason=" + reason
                    + " connected=" + cachedControllerConnected);
        } catch (Throwable t) {
            Log.w(TAG, "SDL controller rescan failed reason=" + reason, t);
        }
    }

    private void scheduleSdlControllerRescan() {
        // Some handheld controller mode switches expose the new InputDevice a few
        // frames before all motion ranges/descriptors settle. Re-polling is safe:
        // SDLJoystickHandler_API16 de-duplicates already registered device IDs.
        handler.postDelayed(new Runnable() {
            @Override
            public void run() {
                if (running && cachedControllerConnected) {
                    rescanSdlControllers("connect-settle-150ms");
                }
            }
        }, 150L);
        handler.postDelayed(new Runnable() {
            @Override
            public void run() {
                if (running && cachedControllerConnected) {
                    rescanSdlControllers("connect-settle-500ms");
                }
            }
        }, 500L);
    }

    private void applyMode(RetroTouchMode wanted) {
        if (wanted == appliedMode) {
            return;
        }

        // Always release the old mode before activating the new one. This is
        // important for NAVIGATION <-> GAMEPLAY transitions and respawns.
        view.resetInputState();
        releaseHostInputState();
        view.setMode(wanted);
        appliedMode = wanted;
        view.bringToFront();
        view.invalidate();
        Log.i(TAG, "mode=" + wanted);
    }

    private void resetRetroTouchAndHost() {
        view.resetInputState();
        releaseHostInputState();
    }

    private void sendTouchButton(int keyCode, boolean pressed) {
        if (pressed) {
            touchPressedKeys.add(keyCode);
        } else {
            touchPressedKeys.remove(keyCode);
        }
        host.ut99RetroTouchButton(keyCode, pressed);
    }

    private void releaseHostInputState() {
        // Release only state RetroTouch itself asserted. The previous blanket
        // release wrote zero/up into the same native variables used by Android's
        // physical-controller bridge and could invalidate a live handover.
        if (touchMoveX != 0.0f || touchMoveY != 0.0f) {
            touchMoveX = 0.0f;
            touchMoveY = 0.0f;
            host.ut99RetroTouchMove(0.0f, 0.0f);
        }

        if (!touchPressedKeys.isEmpty()) {
            Integer[] keys = touchPressedKeys.toArray(new Integer[touchPressedKeys.size()]);
            touchPressedKeys.clear();
            for (Integer key : keys) {
                if (key != null) {
                    host.ut99RetroTouchButton(key.intValue(), false);
                }
            }
        }
    }

    private static float clamp(float value, float min, float max) {
        return value < min ? min : (value > max ? max : value);
    }
}
