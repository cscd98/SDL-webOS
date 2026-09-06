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

#include "SDL_scancode_webos_c.h"

// Extracted from /usr/share/X11/xkb/keycodes/lg
enum
{
    IR_KEY_STOP = 136,
    IR_KEY_REW = 176,
    IR_KEY_EXIT = 182,
    IR_KEY_PLAY = 215,
    IR_KEY_FF = 216,
    IR_KEY_GUIDE = 370,
    IR_KEY_RED = 406,
    IR_KEY_GREEN = 407,
    IR_KEY_YELLOW = 408,
    IR_KEY_BLUE = 409,
    IR_KEY_CH_UP = 410,
    IR_KEY_CH_DOWN = 411,
    IR_KEY_BACK = 420,
    IR_KEY_HOME = 781,
    IR_KEY_CURSOR_SHOW = 1206,
    IR_KEY_CURSOR_HIDE = 1207,
    IR_KEY_GOTOPREV = 821,
    IR_KEY_GOTONEXT = 822,
};

SDL_Scancode SDL_GetWebOSScancode(int keycode)
{
    switch (keycode) {
    case IR_KEY_BACK:
        return SDL_SCANCODE_WEBOS_BACK;
    case IR_KEY_EXIT:
        return SDL_SCANCODE_WEBOS_EXIT;
    case IR_KEY_GUIDE:
        return SDL_SCANCODE_WEBOS_GUIDE;
    case IR_KEY_HOME:
        return SDL_SCANCODE_WEBOS_HOME;
    case IR_KEY_RED:
        return SDL_SCANCODE_WEBOS_RED;
    case IR_KEY_GREEN:
        return SDL_SCANCODE_WEBOS_GREEN;
    case IR_KEY_YELLOW:
        return SDL_SCANCODE_WEBOS_YELLOW;
    case IR_KEY_BLUE:
        return SDL_SCANCODE_WEBOS_BLUE;
    case IR_KEY_CH_UP:
        return SDL_SCANCODE_WEBOS_CH_UP;
    case IR_KEY_CH_DOWN:
        return SDL_SCANCODE_WEBOS_CH_DOWN;
    case IR_KEY_CURSOR_SHOW:
        return SDL_SCANCODE_WEBOS_CURSOR_SHOW;
    case IR_KEY_CURSOR_HIDE:
        return SDL_SCANCODE_WEBOS_CURSOR_HIDE;
    case IR_KEY_PLAY:
        return SDL_SCANCODE_MEDIA_PLAY;
    case IR_KEY_STOP:
        return SDL_SCANCODE_MEDIA_STOP;
    case IR_KEY_GOTOPREV:
        return SDL_SCANCODE_MEDIA_PREVIOUS_TRACK;
    case IR_KEY_GOTONEXT:
        return SDL_SCANCODE_MEDIA_NEXT_TRACK;
    case IR_KEY_REW:
        return SDL_SCANCODE_MEDIA_REWIND;
    case IR_KEY_FF:
        return SDL_SCANCODE_MEDIA_FAST_FORWARD;
    default:
        return SDL_SCANCODE_UNKNOWN;
    }
}

#endif // SDL_VIDEO_DRIVER_WAYLAND_WEBOS
