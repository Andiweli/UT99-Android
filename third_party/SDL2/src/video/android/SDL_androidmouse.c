/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_ANDROID

#include "SDL_androidmouse.h"

#include "SDL_events.h"
#include "../../events/SDL_mouse_c.h"

#include "../../core/android/SDL_android.h"
#include <stdint.h>

/* See Android's MotionEvent class for constants */
#define ACTION_DOWN       0
#define ACTION_UP         1
#define ACTION_MOVE       2
#define ACTION_HOVER_MOVE 7
#define ACTION_SCROLL     8
#define BUTTON_PRIMARY    1
#define BUTTON_SECONDARY  2
#define BUTTON_TERTIARY   4
#define BUTTON_BACK       8
#define BUTTON_FORWARD    16

typedef struct
{
    int custom_cursor;
    int system_cursor;

} SDL_AndroidCursorData;

/* Last known Android mouse button state (includes all buttons) */
static int last_state;

/* UT99_ANDROID_CHROMEOS_MOUSE_SUBPIXEL_V210
 * ChromeOS pointer capture supplies floating-point relative deltas. Preserve
 * the fractional part across SDL's integer mouse events instead of truncating
 * every MotionEvent independently. */
static float relative_remainder_x;
static float relative_remainder_y;
static int ut99_v216_mouse_diag_count;

/* Blank cursor */
static SDL_Cursor *empty_cursor;

static SDL_Cursor *Android_WrapCursor(int custom_cursor, int system_cursor)
{
    SDL_Cursor *cursor;

    cursor = SDL_calloc(1, sizeof(*cursor));
    if (cursor) {
        SDL_AndroidCursorData *data = (SDL_AndroidCursorData *)SDL_calloc(1, sizeof(*data));
        if (data) {
            data->custom_cursor = custom_cursor;
            data->system_cursor = system_cursor;
            cursor->driverdata = data;
        } else {
            SDL_free(cursor);
            cursor = NULL;
            SDL_OutOfMemory();
        }
    } else {
        SDL_OutOfMemory();
    }

    return cursor;
}

static SDL_Cursor *Android_CreateDefaultCursor()
{
    return Android_WrapCursor(0, SDL_SYSTEM_CURSOR_ARROW);
}

static SDL_Cursor *Android_CreateCursor(SDL_Surface *surface, int hot_x, int hot_y)
{
    int custom_cursor;
    SDL_Surface *converted;

    converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ARGB8888, 0);
    if (converted == NULL) {
        return NULL;
    }
    custom_cursor = Android_JNI_CreateCustomCursor(converted, hot_x, hot_y);
    SDL_FreeSurface(converted);
    if (!custom_cursor) {
        SDL_Unsupported();
        return NULL;
    }
    return Android_WrapCursor(custom_cursor, 0);
}

static SDL_Cursor *Android_CreateSystemCursor(SDL_SystemCursor id)
{
    return Android_WrapCursor(0, id);
}

static void Android_FreeCursor(SDL_Cursor *cursor)
{
    SDL_AndroidCursorData *data = (SDL_AndroidCursorData *)cursor->driverdata;
    if (data->custom_cursor != 0) {
        Android_JNI_DestroyCustomCursor(data->custom_cursor);
    }
    SDL_free(cursor->driverdata);
    SDL_free(cursor);
}

static SDL_Cursor *Android_CreateEmptyCursor()
{
    if (empty_cursor == NULL) {
        SDL_Surface *empty_surface = SDL_CreateRGBSurfaceWithFormat(0, 1, 1, 32, SDL_PIXELFORMAT_ARGB8888);
        if (empty_surface) {
            SDL_memset(empty_surface->pixels, 0, (size_t)empty_surface->h * empty_surface->pitch);
            empty_cursor = Android_CreateCursor(empty_surface, 0, 0);
            SDL_FreeSurface(empty_surface);
        }
    }
    return empty_cursor;
}

static void Android_DestroyEmptyCursor()
{
    if (empty_cursor) {
        Android_FreeCursor(empty_cursor);
        empty_cursor = NULL;
    }
}

static int Android_ShowCursor(SDL_Cursor *cursor)
{
    if (cursor == NULL) {
        cursor = Android_CreateEmptyCursor();
    }
    if (cursor) {
        SDL_AndroidCursorData *data = (SDL_AndroidCursorData *)cursor->driverdata;
        if (data->custom_cursor) {
            if (!Android_JNI_SetCustomCursor(data->custom_cursor)) {
                return SDL_Unsupported();
            }
        } else {
            if (!Android_JNI_SetSystemCursor(data->system_cursor)) {
                return SDL_Unsupported();
            }
        }
        return 0;
    } else {
        /* SDL error set inside Android_CreateEmptyCursor() */
        return -1;
    }
}

static int Android_SetRelativeMouseMode(SDL_bool enabled)
{
    if (!Android_JNI_SupportsRelativeMouse()) {
        return SDL_Unsupported();
    }

    if (!Android_JNI_SetRelativeMouseEnabled(enabled)) {
        return SDL_Unsupported();
    }

    relative_remainder_x = 0.0f;
    relative_remainder_y = 0.0f;

    return 0;
}

void Android_InitMouse(void)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    mouse->CreateCursor = Android_CreateCursor;
    mouse->CreateSystemCursor = Android_CreateSystemCursor;
    mouse->ShowCursor = Android_ShowCursor;
    mouse->FreeCursor = Android_FreeCursor;
    mouse->SetRelativeMouseMode = Android_SetRelativeMouseMode;

    SDL_SetDefaultCursor(Android_CreateDefaultCursor());

    last_state = 0;
    relative_remainder_x = 0.0f;
    relative_remainder_y = 0.0f;
}

void Android_QuitMouse(void)
{
    Android_DestroyEmptyCursor();
}

/* Translate Android mouse button state to SDL mouse button */
static Uint8 TranslateButton(int state)
{
    if (state & BUTTON_PRIMARY) {
        return SDL_BUTTON_LEFT;
    } else if (state & BUTTON_SECONDARY) {
        return SDL_BUTTON_RIGHT;
    } else if (state & BUTTON_TERTIARY) {
        return SDL_BUTTON_MIDDLE;
    } else if (state & BUTTON_FORWARD) {
        return SDL_BUTTON_X1;
    } else if (state & BUTTON_BACK) {
        return SDL_BUTTON_X2;
    } else {
        return 0;
    }
}

static void Android_SendMouseMotionPreserveSubpixel(SDL_Window *window, SDL_bool relative, float x, float y)
{
    if (relative) {
        int dx;
        int dy;

        relative_remainder_x += x;
        relative_remainder_y += y;
        dx = (int)relative_remainder_x;
        dy = (int)relative_remainder_y;
        relative_remainder_x -= (float)dx;
        relative_remainder_y -= (float)dy;

        if (dx != 0 || dy != 0) {
            SDL_SendMouseMotion(window, 0, SDL_TRUE, dx, dy);
        }
    } else {
        relative_remainder_x = 0.0f;
        relative_remainder_y = 0.0f;
        SDL_SendMouseMotion(window, 0, SDL_FALSE, (int)x, (int)y);
    }
}

/* UT99_ANDROID_CHROMEOS_MOUSE_HIRES_EVENT_V210
 * SDL_MOUSEMOTION stores relative motion as integers. Queue a parallel 20.12
 * fixed-point event so NSDLViewport can aggregate the original ChromeOS delta
 * once per input tick without smoothing or adding latency. */
#define UT99_ANDROID_HIRES_MOUSE_EVENT (SDL_USEREVENT + 0x210)
#define UT99_ANDROID_HIRES_MOUSE_MAGIC ((Sint32)0x554D3231)
#define UT99_ANDROID_HIRES_MOUSE_SCALE 4096.0f

static void Android_SendUT99HighResRelativeMouse(float x, float y)
{
    SDL_Event event;
    const Sint32 fixed_x = (Sint32)(x * UT99_ANDROID_HIRES_MOUSE_SCALE);
    const Sint32 fixed_y = (Sint32)(y * UT99_ANDROID_HIRES_MOUSE_SCALE);

    if (fixed_x == 0 && fixed_y == 0) {
        return;
    }

    SDL_zero(event);
    event.type = UT99_ANDROID_HIRES_MOUSE_EVENT;
    event.user.type = UT99_ANDROID_HIRES_MOUSE_EVENT;
    event.user.code = UT99_ANDROID_HIRES_MOUSE_MAGIC;
    event.user.data1 = (void *)(intptr_t)fixed_x;
    event.user.data2 = (void *)(intptr_t)fixed_y;
    SDL_PushEvent(&event);
}

void Android_OnMouse(SDL_Window *window, int state, int action, float x, float y, SDL_bool relative)
{
    int changes;
    Uint8 button;

    if (window == NULL) {
        return;
    }

    if (ut99_v216_mouse_diag_count < 48) {
        SDL_Log("UT99MouseDiag V216 Android_OnMouse action=%d state=0x%x relative=%d x=%.3f y=%.3f",
                action, state, relative ? 1 : 0, x, y);
        ++ut99_v216_mouse_diag_count;
    }

    switch (action) {
    case ACTION_DOWN:
        changes = state & ~last_state;
        button = TranslateButton(changes);
        last_state = state;
        Android_SendMouseMotionPreserveSubpixel(window, relative, x, y);
        SDL_SendMouseButton(window, 0, SDL_PRESSED, button);
        break;

    case ACTION_UP:
        changes = last_state & ~state;
        button = TranslateButton(changes);
        last_state = state;
        Android_SendMouseMotionPreserveSubpixel(window, relative, x, y);
        SDL_SendMouseButton(window, 0, SDL_RELEASED, button);
        break;

    case ACTION_MOVE:
    case ACTION_HOVER_MOVE:
        if (relative) {
            Android_SendUT99HighResRelativeMouse(x, y);
        }
        Android_SendMouseMotionPreserveSubpixel(window, relative, x, y);
        break;

    case ACTION_SCROLL:
        SDL_SendMouseWheel(window, 0, x, y, SDL_MOUSEWHEEL_NORMAL);
        break;

    default:
        break;
    }
}

#endif /* SDL_VIDEO_DRIVER_ANDROID */

/* vi: set ts=4 sw=4 expandtab: */
