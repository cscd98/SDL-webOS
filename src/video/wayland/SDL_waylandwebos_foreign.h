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

#ifndef SDL_waylandwebos_foreign_h_
#define SDL_waylandwebos_foreign_h_

#ifdef SDL_VIDEO_DRIVER_WAYLAND_WEBOS

#include "../SDL_sysvideo.h"

extern void WaylandWebOS_DisplayInitForeign(SDL_VideoData *display, Uint32 id);
extern void WaylandWebOS_QuitForeign(SDL_VideoData *display);

extern const char *WaylandWebOS_CreateExportedWindow(SDL_VideoDevice *_this, SDL_webOSExportedWindowType type);
extern bool WaylandWebOS_SetExportedWindow(SDL_VideoDevice *_this, const char *windowId, const SDL_Rect *src, const SDL_Rect *dst);
extern bool WaylandWebOS_ExportedSetCropRegion(SDL_VideoDevice *_this, const char *windowId, const SDL_Rect *org, const SDL_Rect *src, const SDL_Rect *dst);
extern bool WaylandWebOS_ExportedSetProperty(SDL_VideoDevice *_this, const char *windowId, const char *name, const char *value);
extern bool WaylandWebOS_DestroyExportedWindow(SDL_VideoDevice *_this, const char *windowId);

#endif // SDL_VIDEO_DRIVER_WAYLAND_WEBOS

#endif // SDL_waylandwebos_foreign_h_
