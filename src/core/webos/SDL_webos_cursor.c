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

#include "../../events/SDL_mouse_c.h"

/* The SDL2 fork dispatched this through a WebOSSetCursorVisibility hook that it
 * added to SDL_Mouse. That hook, and the wayland mouse backend behind it, land
 * with the wayland port; until then the entry point exists so that the public
 * API surface is complete, and reports that nothing implements it. */
bool SDL_webOSCursorVisibility(bool visible)
{
    (void)visible;

    return SDL_SetError("Failed to set cursor visibility: the current mouse driver does not support it");
}

#endif // SDL_PLATFORM_WEBOS
