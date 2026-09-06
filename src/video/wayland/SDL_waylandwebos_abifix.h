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

#ifndef SDL_waylandwebos_abifix_h_
#define SDL_waylandwebos_abifix_h_

#ifdef SDL_VIDEO_DRIVER_WAYLAND_WEBOS

/* The wl_webos_input_manager the compositor implements does not match the
 * published XML: request opcodes differ, so a request marshalled from the
 * generated code hits the wrong method and the compositor kills the client
 * with "invalid method N, object wl_webos_input_manager".
 *
 * LG's own client library carries the authoritative wl_interface, so load it
 * and resolve the opcodes by request name at runtime.
 *
 * The SDL2 branch solves this by rewriting the scanner's generated opcode
 * defines with an awk pass at build time; resolving here keeps the build
 * plain and confines the workaround to one file. */

struct wl_interface;
struct wl_seat;
struct wl_webos_input_manager;
struct wl_webos_seat;

extern bool WaylandWebOS_AbiFixInit(void);
extern void WaylandWebOS_AbiFixQuit(void);

/* The real wl_webos_input_manager interface, for wl_registry_bind(). NULL if
 * LG's library could not be loaded, in which case the caller must not bind. */
extern const struct wl_interface *WaylandWebOS_GetInputManagerInterface(void);

/* Marshals get_webos_seat with the opcode this compositor actually uses.
 * Returns NULL if the request is unavailable. */
extern struct wl_webos_seat *WaylandWebOS_GetWebOSSeat(struct wl_webos_input_manager *manager,
                                                       struct wl_seat *seat);

#endif // SDL_VIDEO_DRIVER_WAYLAND_WEBOS

#endif // SDL_waylandwebos_abifix_h_
