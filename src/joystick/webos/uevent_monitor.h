#include "../../SDL_internal.h"

#ifndef SDL_webos_uevent_monitor_h_
#define SDL_webos_uevent_monitor_h_

/* Hotplug notifications straight from the kernel, via a
 * NETLINK_KOBJECT_UEVENT socket.
 *
 * There's no libudev in the webOS app jail, so the joystick and hidapi
 * backends otherwise fall back to rescanning /dev every 3 seconds and
 * diffing a presence bitmask. That's slow to react, and it can't see a
 * device that disconnects and reconnects on the same index between two
 * scans, because the bitmask comes out unchanged.
 *
 * The kernel broadcasts add/remove as uevents to anyone bound to group 1,
 * with no privilege and no libudev needed. Events are explicit and ordered,
 * so the same-index case stops being invisible, and they arrive in well
 * under 100ms.
 *
 * Each subsystem should open its own monitor. Netlink broadcasts a copy to
 * every bound socket, so two monitors don't compete; sharing one would mean
 * whichever side drained it first consumed the other's events.
 */

typedef enum SDL_webOSUeventAction
{
    SDL_WEBOS_UEVENT_ACTION_OTHER,
    SDL_WEBOS_UEVENT_ACTION_ADD,
    SDL_WEBOS_UEVENT_ACTION_REMOVE,
} SDL_webOSUeventAction;

typedef struct SDL_webOSUeventMonitor SDL_webOSUeventMonitor;

typedef struct SDL_webOSUevent
{
    SDL_webOSUeventAction action;

    /* "input", "hidraw", ... NULL if the event didn't carry a SUBSYSTEM. */
    const char *subsystem;

    /* Trailing name of the device node, e.g. "event14", "js7", "hidraw0".
     * NULL for events that don't describe a node (bus/class-level events).
     * Prefix with the right directory to get a path; don't infer ordering
     * from it, and key on the name itself. */
    const char *devname;

    /* Full DEVPATH under /sys, for logging. NULL if absent. */
    const char *devpath;
} SDL_webOSUevent;

/* Returns NULL if the socket can't be created or bound, which the caller
 * must treat as a normal outcome and handle by keeping the presence-flag
 * poll. Unprivileged bind is the long-standing default and is verified
 * working on webOS 10 (kernel 5.4), but it has not been confirmed on the
 * 3.10 kernels that older webOS versions ship. */
extern SDL_webOSUeventMonitor *SDL_webOSUeventMonitorOpen(void);

extern void SDL_webOSUeventMonitorClose(SDL_webOSUeventMonitor *monitor);

/* Reads one pending event, without blocking. Returns SDL_FALSE once the
 * socket is drained; call it in a loop, since the socket buffer is finite
 * and a burst can queue several events.
 *
 * The strings in `event` point into storage owned by the monitor and stay
 * valid only until the next call on the same monitor. */
extern SDL_bool SDL_webOSUeventMonitorPoll(SDL_webOSUeventMonitor *monitor, SDL_webOSUevent *event);

#endif /* SDL_webos_uevent_monitor_h_ */
