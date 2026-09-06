/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#include "SDL_waylandwebos_foreign.h"

#include "SDL_waylandvideo.h"
#include "SDL_waylandwindow.h"

#include "webos-foreign-client-protocol.h"

// Contributed by Mariotaku <mariotaku.lee@gmail.com>

/* The id an exported window is known by is only delivered in an event, so
 * creation has to wait for it rather than hand the app nothing. */
#define WEBOS_WINDOW_ID_TIMEOUT_NS SDL_MS_TO_NS(1000)

typedef struct SDL_WaylandExportedWindow
{
    struct wl_webos_exported *exported;
    struct SDL_WaylandExportedWindow *next;
    SDL_WindowID window;
    char window_id[32];
} SDL_WaylandExportedWindow;

static void exported_handle_window_id_assigned(void *data, struct wl_webos_exported *exported,
                                               const char *window_id, uint32_t exported_type)
{
    SDL_WaylandExportedWindow *window = (SDL_WaylandExportedWindow *)data;

    SDL_strlcpy(window->window_id, window_id, sizeof(window->window_id));
}

static const struct wl_webos_exported_listener webos_exported_listener = {
    exported_handle_window_id_assigned
};

static SDL_WaylandExportedWindow *WaylandWebOS_FindExportedWindow(SDL_VideoData *display, const char *windowId)
{
    SDL_WaylandExportedWindow *window;

    if (!display->webos_foreign.foreign) {
        SDL_SetError("The compositor does not implement wl_webos_foreign");
        return NULL;
    }
    if (!windowId) {
        SDL_SetError("Invalid window id");
        return NULL;
    }

    for (window = display->webos_foreign.windows; window; window = window->next) {
        if (SDL_strcmp(window->window_id, windowId) == 0) {
            return window;
        }
    }

    SDL_SetError("No exported window with id %s", windowId);
    return NULL;
}

static struct wl_region *WaylandWebOS_CreateRegion(SDL_VideoData *display, const SDL_Rect *rect)
{
    struct wl_region *region = wl_compositor_create_region(display->compositor);

    if (region) {
        wl_region_add(region, rect->x, rect->y, rect->w, rect->h);
    }
    return region;
}

/* wl_webos_exported takes no null regions - libwayland aborts the process
 * rather than marshal one - so "all of it" has to be spelled out. */
static bool WaylandWebOS_GetWholeWindowRect(SDL_WaylandExportedWindow *window, SDL_Rect *rect)
{
    SDL_Window *sdlwindow = SDL_GetWindowFromID(window->window);

    if (!sdlwindow) {
        return SDL_SetError("The window the exported window was created for is gone");
    }

    rect->x = 0;
    rect->y = 0;
    return SDL_GetWindowSizeInPixels(sdlwindow, &rect->w, &rect->h);
}

static void WaylandWebOS_DestroyRegion(struct wl_region *region)
{
    if (region) {
        wl_region_destroy(region);
    }
}

void WaylandWebOS_DisplayInitForeign(SDL_VideoData *display, Uint32 id)
{
    display->webos_foreign.foreign = wl_registry_bind(display->registry, id, &wl_webos_foreign_interface, 1);
}

void WaylandWebOS_QuitForeign(SDL_VideoData *display)
{
    SDL_WaylandExportedWindow *window = display->webos_foreign.windows;

    while (window) {
        SDL_WaylandExportedWindow *next = window->next;

        wl_webos_exported_destroy(window->exported);
        SDL_free(window);
        window = next;
    }
    display->webos_foreign.windows = NULL;

    if (display->webos_foreign.foreign) {
        wl_webos_foreign_destroy(display->webos_foreign.foreign);
        display->webos_foreign.foreign = NULL;
    }
}

const char *WaylandWebOS_CreateExportedWindow(SDL_VideoDevice *_this, SDL_webOSExportedWindowType type)
{
    SDL_VideoData *display = _this->internal;
    SDL_WaylandExportedWindow *exported_window;
    SDL_WindowData *wind;
    SDL_Window *window;
    Uint64 timeout;

    if (!display->webos_foreign.foreign) {
        SDL_SetError("Failed creating exported window: the compositor does not implement wl_webos_foreign");
        return NULL;
    }
    if ((Uint32)type > SDL_WEBOS_EXPORTED_WINDOW_TYPE_OPAQUE) {
        SDL_SetError("Failed creating exported window: Invalid type");
        return NULL;
    }

    window = SDL_GL_GetCurrentWindow();
    if (!window) {
        for (window = _this->windows; window; window = window->next) {
            if (window->internal) {
                break;
            }
        }
    }
    if (!window) {
        SDL_SetError("Failed creating exported window: No current window");
        return NULL;
    }

    // The compositor punches through the app's own plane, which has to be the GL one.
    if (!(window->flags & SDL_WINDOW_OPENGL)) {
        if (!SDL_RecreateWindow(window, window->flags | SDL_WINDOW_OPENGL)) {
            SDL_SetError("Failed creating exported window: Failed to recreate window with OpenGL");
            return NULL;
        }
    }

    wind = window->internal;
    if (!wind || !wind->surface) {
        SDL_SetError("Failed creating exported window: No surface for window");
        return NULL;
    }

    exported_window = SDL_calloc(1, sizeof(*exported_window));
    if (!exported_window) {
        return NULL;
    }
    exported_window->window = window->id;

    exported_window->exported = wl_webos_foreign_export_element(display->webos_foreign.foreign, wind->surface,
                                                               (uint32_t)type);
    if (!exported_window->exported) {
        SDL_free(exported_window);
        SDL_SetError("Failed creating exported window: Failed exporting the surface");
        return NULL;
    }
    wl_webos_exported_add_listener(exported_window->exported, &webos_exported_listener, exported_window);

    timeout = SDL_GetTicksNS() + WEBOS_WINDOW_ID_TIMEOUT_NS;
    while (!exported_window->window_id[0]) {
        if (WAYLAND_wl_display_roundtrip(display->display) < 0 || SDL_GetTicksNS() >= timeout) {
            wl_webos_exported_destroy(exported_window->exported);
            SDL_free(exported_window);
            SDL_SetError("Failed creating exported window: No window id was assigned");
            return NULL;
        }
    }

    exported_window->next = display->webos_foreign.windows;
    display->webos_foreign.windows = exported_window;

    return exported_window->window_id;
}

bool WaylandWebOS_SetExportedWindow(SDL_VideoDevice *_this, const char *windowId, const SDL_Rect *src,
                                    const SDL_Rect *dst)
{
    SDL_VideoData *display = _this->internal;
    SDL_WaylandExportedWindow *window = WaylandWebOS_FindExportedWindow(display, windowId);
    struct wl_region *src_region;
    struct wl_region *dst_region;
    SDL_Rect whole;

    if (!window) {
        return false;
    }
    if ((!src || !dst) && !WaylandWebOS_GetWholeWindowRect(window, &whole)) {
        return false;
    }

    src_region = WaylandWebOS_CreateRegion(display, src ? src : &whole);
    dst_region = WaylandWebOS_CreateRegion(display, dst ? dst : &whole);
    if (!src_region || !dst_region) {
        WaylandWebOS_DestroyRegion(src_region);
        WaylandWebOS_DestroyRegion(dst_region);
        return SDL_SetError("Failed creating a region");
    }

    wl_webos_exported_set_exported_window(window->exported, src_region, dst_region);
    wl_region_destroy(src_region);
    wl_region_destroy(dst_region);
    WAYLAND_wl_display_flush(display->display);

    return true;
}

bool WaylandWebOS_ExportedSetCropRegion(SDL_VideoDevice *_this, const char *windowId, const SDL_Rect *org,
                                        const SDL_Rect *src, const SDL_Rect *dst)
{
    SDL_VideoData *display = _this->internal;
    SDL_WaylandExportedWindow *window;
    struct wl_region *org_region;
    struct wl_region *src_region;
    struct wl_region *dst_region;

    if (!org || !src || !dst) {
        return SDL_SetError("Invalid crop region");
    }

    window = WaylandWebOS_FindExportedWindow(display, windowId);
    if (!window) {
        return false;
    }

    org_region = WaylandWebOS_CreateRegion(display, org);
    src_region = WaylandWebOS_CreateRegion(display, src);
    dst_region = WaylandWebOS_CreateRegion(display, dst);
    if (!org_region || !src_region || !dst_region) {
        WaylandWebOS_DestroyRegion(org_region);
        WaylandWebOS_DestroyRegion(src_region);
        WaylandWebOS_DestroyRegion(dst_region);
        return SDL_SetError("Failed creating a region");
    }

    wl_webos_exported_set_crop_region(window->exported, org_region, src_region, dst_region);
    wl_region_destroy(org_region);
    wl_region_destroy(src_region);
    wl_region_destroy(dst_region);
    WAYLAND_wl_display_flush(display->display);

    return true;
}

bool WaylandWebOS_ExportedSetProperty(SDL_VideoDevice *_this, const char *windowId, const char *name,
                                      const char *value)
{
    SDL_VideoData *display = _this->internal;
    SDL_WaylandExportedWindow *window;

    if (!name) {
        return SDL_SetError("Invalid property name");
    }
    if (!value) {
        return SDL_SetError("Invalid property value");
    }

    window = WaylandWebOS_FindExportedWindow(display, windowId);
    if (!window) {
        return false;
    }

    wl_webos_exported_set_property(window->exported, name, value);
    WAYLAND_wl_display_flush(display->display);

    return true;
}

bool WaylandWebOS_DestroyExportedWindow(SDL_VideoDevice *_this, const char *windowId)
{
    SDL_VideoData *display = _this->internal;
    SDL_WaylandExportedWindow *window = WaylandWebOS_FindExportedWindow(display, windowId);
    SDL_WaylandExportedWindow **prev;

    if (!window) {
        return false;
    }

    for (prev = &display->webos_foreign.windows; *prev != window; prev = &(*prev)->next) {
    }
    *prev = window->next;

    wl_webos_exported_destroy(window->exported);
    WAYLAND_wl_display_flush(display->display);
    SDL_free(window);

    return true;
}

#endif // SDL_VIDEO_DRIVER_WAYLAND_WEBOS
