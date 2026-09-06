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

#include "SDL_waylandwebos_abifix.h"
#include "SDL_waylanddyn.h"

#include "webos-input-manager-client-protocol.h"

// Contributed by Mariotaku <mariotaku.lee@gmail.com>

static SDL_SharedObject *webos_client_lib = NULL;
static const struct wl_interface *input_manager_interface = NULL;
static int opcode_get_webos_seat = -1;

static int FindOpcode(const struct wl_interface *iface, const char *request)
{
    int i;

    for (i = 0; i < iface->method_count; ++i) {
        if (SDL_strcmp(iface->methods[i].name, request) == 0) {
            return i;
        }
    }
    return -1;
}

bool WaylandWebOS_AbiFixInit(void)
{
    if (webos_client_lib) {
        return true;
    }

    webos_client_lib = SDL_LoadObject("libwayland-webos-client.so.1");
    if (!webos_client_lib) {
        // Not fatal: the caller simply won't bind the input manager.
        return false;
    }

    /* The interface is a data symbol, but SDL_LoadFunction is the only lookup
     * SDL offers and the cast is what the SDL2 branch does too. */
    input_manager_interface = (const struct wl_interface *)SDL_LoadFunction(webos_client_lib,
                                                                           "wl_webos_input_manager_interface");
    if (!input_manager_interface) {
        SDL_UnloadObject(webos_client_lib);
        webos_client_lib = NULL;
        return false;
    }

    opcode_get_webos_seat = FindOpcode(input_manager_interface, "get_webos_seat");

    return true;
}

void WaylandWebOS_AbiFixQuit(void)
{
    input_manager_interface = NULL;
    opcode_get_webos_seat = -1;

    if (webos_client_lib) {
        SDL_UnloadObject(webos_client_lib);
        webos_client_lib = NULL;
    }
}

const struct wl_interface *WaylandWebOS_GetInputManagerInterface(void)
{
    return input_manager_interface;
}

struct wl_webos_seat *WaylandWebOS_GetWebOSSeat(struct wl_webos_input_manager *manager,
                                                struct wl_seat *seat)
{
    if (!manager || opcode_get_webos_seat < 0) {
        return NULL;
    }

    /* Equivalent to the generated wl_webos_input_manager_get_webos_seat(),
     * but with the opcode this compositor actually uses. */
    return (struct wl_webos_seat *)wl_proxy_marshal_constructor((struct wl_proxy *)manager,
                                                                (uint32_t)opcode_get_webos_seat,
                                                                &wl_webos_seat_interface,
                                                                NULL, seat);
}

#endif // SDL_VIDEO_DRIVER_WAYLAND_WEBOS
