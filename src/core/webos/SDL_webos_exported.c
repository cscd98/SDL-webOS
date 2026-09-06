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

/* The SDL2 fork dispatched these through WebOS* hooks that it added to
 * SDL_VideoDevice. Those hooks, and the wayland video backend behind them, land
 * with the wayland port; until then these entry points exist so that the public
 * API surface is complete, and report that nothing implements them. */

const char *SDL_webOSCreateExportedWindow(SDL_webOSExportedWindowType type)
{
    (void)type;

    SDL_SetError("Failed creating exported window: the current video driver does not support exported windows");
    return NULL;
}

bool SDL_webOSSetExportedWindow(const char *windowId, SDL_Rect *src, SDL_Rect *dst)
{
    (void)windowId;
    (void)src;
    (void)dst;

    return SDL_SetError("Failed to set exported window: the current video driver does not support exported windows");
}

bool SDL_webOSExportedSetCropRegion(const char *windowId, SDL_Rect *org, SDL_Rect *src, SDL_Rect *dst)
{
    (void)windowId;
    (void)org;
    (void)src;
    (void)dst;

    return SDL_SetError("Failed to set crop region: the current video driver does not support exported windows");
}

bool SDL_webOSExportedSetProperty(const char *windowId, const char *name, const char *value)
{
    (void)windowId;
    (void)name;
    (void)value;

    return SDL_SetError("Failed to set property: the current video driver does not support exported windows");
}

void SDL_webOSDestroyExportedWindow(const char *windowId)
{
    (void)windowId;

    SDL_SetError("Failed to destroy exported window: the current video driver does not support exported windows");
}

#endif // SDL_PLATFORM_WEBOS
