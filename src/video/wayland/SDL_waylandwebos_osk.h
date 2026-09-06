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

#ifndef SDL_waylandwebos_osk_h_
#define SDL_waylandwebos_osk_h_

#ifdef SDL_VIDEO_DRIVER_WAYLAND_WEBOS

#include "SDL_waylandvideo.h"
#include "SDL_waylandwindow.h"

extern void WaylandWebOS_DisplayInitTextModelFactory(SDL_VideoData *display, Uint32 id);
extern void WaylandWebOS_WindowDestroyed(SDL_VideoData *display, SDL_WindowData *wind);
extern void WaylandWebOS_SeatDestroyed(SDL_VideoData *display, struct SDL_WaylandSeat *seat);
extern void WaylandWebOS_QuitTextInput(SDL_VideoData *display);

/* Creates and activates the text model once a seat holding keyboard focus has a
 * window that wants text input, and deactivates it when nothing does. */
extern void WaylandWebOS_UpdateTextInput(SDL_VideoData *display);

extern bool WaylandWebOS_StartTextInput(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID props);
extern bool WaylandWebOS_StopTextInput(SDL_VideoDevice *_this, SDL_Window *window);
extern bool WaylandWebOS_UpdateTextInputArea(SDL_VideoDevice *_this, SDL_Window *window);
extern bool WaylandWebOS_ClearComposition(SDL_VideoDevice *_this, SDL_Window *window);
extern void WaylandWebOS_ShowScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID props);
extern void WaylandWebOS_HideScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window);

#endif // SDL_VIDEO_DRIVER_WAYLAND_WEBOS

#endif // SDL_waylandwebos_osk_h_
