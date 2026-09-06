/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_WAYLAND_WEBOS

#include "SDL_waylandwebos.h"

#include "../../events/SDL_events_c.h"

#include "webos-shell-client-protocol.h"

#ifdef SDL_WEBOS_HAVE_LIBHELPER
#include "../../core/webos/SDL_webos_libs.h"
#endif

// Contributed by Mariotaku <mariotaku.lee@gmail.com>

static void webos_shell_handle_state(void *data, struct wl_webos_shell_surface *shell_surface, uint32_t state)
{
    SDL_WindowData *d = (SDL_WindowData *)data;

    if (state == WL_WEBOS_SHELL_SURFACE_STATE_MINIMIZED) {
#ifdef SDL_WEBOS_HAVE_LIBHELPER
        HELPERS_HProcessAppState(3, NULL);
#endif
        SDL_SendWindowEvent(d->sdlwindow, SDL_EVENT_WINDOW_MINIMIZED, 0, 0);
        SDL_SendAppEvent(SDL_EVENT_WILL_ENTER_BACKGROUND);
        SDL_SendAppEvent(SDL_EVENT_DID_ENTER_BACKGROUND);
    } else if (state == WL_WEBOS_SHELL_SURFACE_STATE_FULLSCREEN) {
        // Only a genuine return to the foreground, not a fullscreen re-assert.
        if (d->webos_shell_state < WL_WEBOS_SHELL_SURFACE_STATE_MAXIMIZED) {
            SDL_SendWindowEvent(d->sdlwindow, SDL_EVENT_WINDOW_RESTORED, 0, 0);
            SDL_SendAppEvent(SDL_EVENT_WILL_ENTER_FOREGROUND);
            SDL_SendAppEvent(SDL_EVENT_DID_ENTER_FOREGROUND);
#ifdef SDL_WEBOS_HAVE_LIBHELPER
            HELPERS_HProcessAppState(0, NULL);
#endif
        }
    }

    d->webos_shell_state = state;
}

static void webos_shell_handle_position(void *data, struct wl_webos_shell_surface *shell_surface, int32_t x, int32_t y)
{
    SDL_WindowData *d = (SDL_WindowData *)data;

    SDL_SendWindowEvent(d->sdlwindow, SDL_EVENT_WINDOW_MOVED, x, y);
}

static void webos_shell_handle_close(void *data, struct wl_webos_shell_surface *shell_surface)
{
    SDL_WindowData *d = (SDL_WindowData *)data;

    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    SDL_SendWindowEvent(d->sdlwindow, SDL_EVENT_WINDOW_CLOSE_REQUESTED, 0, 0);
}

static void webos_shell_handle_exposed(void *data, struct wl_webos_shell_surface *shell_surface, struct wl_array *rectangles)
{
}

static void webos_shell_handle_state_about_to_change(void *data, struct wl_webos_shell_surface *shell_surface, uint32_t state)
{
    if (state != WL_WEBOS_SHELL_SURFACE_STATE_MINIMIZED) {
        return;
    }

    /* Sent before the state actually changes, so the app gets a chance to save
     * before it is backgrounded. */
    SDL_SendAppEvent(SDL_EVENT_WILL_ENTER_BACKGROUND);
#ifdef SDL_WEBOS_HAVE_LIBHELPER
    HELPERS_HProcessAppState(1, NULL);
#endif
}

static const struct wl_webos_shell_surface_listener webos_shell_surface_listener = {
    webos_shell_handle_state,
    webos_shell_handle_position,
    webos_shell_handle_close,
    webos_shell_handle_exposed,
    webos_shell_handle_state_about_to_change
};

bool WaylandWebOS_SetupSurface(SDL_VideoDevice *_this, SDL_WindowData *data)
{
    struct wl_webos_shell_surface *surface = data->shell_surface.webos.webos;
    const char *appid = SDL_getenv("APPID");
    const char *hint;

    if (!surface) {
        return SDL_SetError("No webOS shell surface to set up");
    }

    if (!appid) {
        return SDL_SetError("APPID environment variable is not set");
    }

    wl_webos_shell_surface_add_listener(surface, &webos_shell_surface_listener, data);
    wl_webos_shell_surface_set_property(surface, "appId", appid);

    /* Each access policy hands one remote key to the app instead of letting the
     * system act on it. */
    if (SDL_GetHintBoolean(SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_BACK, false)) {
        wl_webos_shell_surface_set_property(surface, "_WEBOS_ACCESS_POLICY_KEYS_BACK", "true");
    }
    if (SDL_GetHintBoolean(SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_EXIT, false)) {
        wl_webos_shell_surface_set_property(surface, "_WEBOS_ACCESS_POLICY_KEYS_EXIT", "true");
    }
    if (SDL_GetHintBoolean(SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_HOME, false)) {
        wl_webos_shell_surface_set_property(surface, "_WEBOS_ACCESS_POLICY_KEYS_HOME", "true");
    }
    if (SDL_GetHintBoolean(SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_GUIDE, false)) {
        wl_webos_shell_surface_set_property(surface, "_WEBOS_ACCESS_POLICY_KEYS_GUIDE", "true");
    }
    if (SDL_GetHintBoolean(SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_META, false)) {
        wl_webos_shell_surface_set_property(surface, "_WEBOS_ACCESS_POLICY_KEYS_META", "true");
    }
    if (!SDL_GetHintBoolean(SDL_HINT_WEBOS_ACCESS_POLICY_RIBBON, true)) {
        wl_webos_shell_surface_set_property(surface, "_WEBOS_ACCESS_POLICY_RIBBON", "false");
    }

    if (SDL_GetHintBoolean(SDL_HINT_WEBOS_CURSOR_CALIBRATION_DISABLE, false)) {
        wl_webos_shell_surface_set_property(surface, "restore_cursor_position", "true");
    }
    if (SDL_GetHintBoolean(SDL_HINT_WEBOS_CLOUDGAME_ACTIVE, true)) {
        wl_webos_shell_surface_set_property(surface, "cloudgame_active", "true");
    }

    hint = SDL_GetHint(SDL_HINT_WEBOS_CURSOR_FREQUENCY);
    if (hint && SDL_atoi(hint) > 0) {
        wl_webos_shell_surface_set_property(surface, "cursor_fps", hint);
    }

    hint = SDL_GetHint(SDL_HINT_WEBOS_WINDOW_CLASS);
    if (hint && SDL_strcmp(hint, "1") == 0) {
        wl_webos_shell_surface_set_property(surface, "_WEBOS_WINDOW_CLASS", hint);
    }

    // The compositor scales the surface to the screen rather than letterboxing.
    wl_webos_shell_surface_set_property(surface, "_WEBOS_ACCESS_POLICY_FORCESTRETCH", "true");

    if (data->sdlwindow->title) {
        wl_webos_shell_surface_set_property(surface, "title", data->sdlwindow->title);
    }

    return true;
}

#endif // SDL_VIDEO_DRIVER_WAYLAND_WEBOS
