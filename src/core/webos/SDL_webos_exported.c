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

#ifdef SDL_PLATFORM_WEBOS

#include "../../video/SDL_sysvideo.h"

#ifdef SDL_VIDEO_DRIVER_WAYLAND_WEBOS
#include "../../video/wayland/SDL_waylandwebos_foreign.h"

/* There is no vtable slot for these, so dispatch on the driver name, the same
 * check SDL_video.c uses for its own x11 special cases. */
static SDL_VideoDevice *GetWaylandVideoDevice(void)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();

    if (_this && SDL_strcmp(_this->name, "wayland") == 0) {
        return _this;
    }
    return NULL;
}
#endif

const char *SDL_webOSCreateExportedWindow(SDL_webOSExportedWindowType type)
{
#ifdef SDL_VIDEO_DRIVER_WAYLAND_WEBOS
    SDL_VideoDevice *_this = GetWaylandVideoDevice();

    if (_this) {
        return WaylandWebOS_CreateExportedWindow(_this, type);
    }
#else
    (void)type;
#endif

    SDL_SetError("Failed creating exported window: the current video driver does not support exported windows");
    return NULL;
}

bool SDL_webOSSetExportedWindow(const char *windowId, SDL_Rect *src, SDL_Rect *dst)
{
#ifdef SDL_VIDEO_DRIVER_WAYLAND_WEBOS
    SDL_VideoDevice *_this = GetWaylandVideoDevice();

    if (_this) {
        return WaylandWebOS_SetExportedWindow(_this, windowId, src, dst);
    }
#else
    (void)windowId;
    (void)src;
    (void)dst;
#endif

    return SDL_SetError("Failed to set exported window: the current video driver does not support exported windows");
}

bool SDL_webOSExportedSetCropRegion(const char *windowId, SDL_Rect *org, SDL_Rect *src, SDL_Rect *dst)
{
#ifdef SDL_VIDEO_DRIVER_WAYLAND_WEBOS
    SDL_VideoDevice *_this = GetWaylandVideoDevice();

    if (_this) {
        return WaylandWebOS_ExportedSetCropRegion(_this, windowId, org, src, dst);
    }
#else
    (void)windowId;
    (void)org;
    (void)src;
    (void)dst;
#endif

    return SDL_SetError("Failed to set crop region: the current video driver does not support exported windows");
}

bool SDL_webOSExportedSetProperty(const char *windowId, const char *name, const char *value)
{
#ifdef SDL_VIDEO_DRIVER_WAYLAND_WEBOS
    SDL_VideoDevice *_this = GetWaylandVideoDevice();

    if (_this) {
        return WaylandWebOS_ExportedSetProperty(_this, windowId, name, value);
    }
#else
    (void)windowId;
    (void)name;
    (void)value;
#endif

    return SDL_SetError("Failed to set property: the current video driver does not support exported windows");
}

void SDL_webOSDestroyExportedWindow(const char *windowId)
{
#ifdef SDL_VIDEO_DRIVER_WAYLAND_WEBOS
    SDL_VideoDevice *_this = GetWaylandVideoDevice();

    if (_this) {
        WaylandWebOS_DestroyExportedWindow(_this, windowId);
        return;
    }
#else
    (void)windowId;
#endif

    SDL_SetError("Failed to destroy exported window: the current video driver does not support exported windows");
}

#endif // SDL_PLATFORM_WEBOS
