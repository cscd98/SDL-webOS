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

#ifndef SDL_webos_init_h_
#define SDL_webos_init_h_

#ifdef SDL_PLATFORM_WEBOS

#include "SDL_webos_libs.h"

typedef enum SDL_webOSPowerState
{
    SDL_WEBOS_POWER_STATE_UNKNOWN,
    SDL_WEBOS_POWER_STATE_ACTIVE,
    SDL_WEBOS_POWER_STATE_POWER_OFF,
    SDL_WEBOS_POWER_STATE_SCREEN_SAVER
} SDL_webOSPowerState;

// Hand libhelpers the LSHandle the application already owns, so that SDL does
// not open a second connection to the Luna bus.
extern bool SDL_webOSSetLSHandle(LSHandle *handle);

extern void SDL_webOSInitLSHandle(void);

extern bool SDL_webOSAppRegistered(void);

// Returns true on success; call SDL_GetError() for more information on failure.
extern bool SDL_webOSRegisterApp(void);

extern void SDL_webOSUnregisterApp(void);

extern SDL_webOSPowerState SDL_webOSGetPowerState(void);

extern void SDL_webOSTurnOnScreen(void);

#endif // SDL_PLATFORM_WEBOS

#endif // SDL_webos_init_h_
