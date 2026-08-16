/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Headless diagnostic for the netlink uevent hotplug monitor.
 *
 * Runs the netlink monitor and the /dev presence poll side by side over the
 * same hotplug activity, and reports whether netlink can replace the poll on
 * this device. No video, no window: it's meant to be run over SSH on a TV.
 *
 * The question it exists to answer is whether an unprivileged process can
 * receive kernel uevents on a given webOS version. That's confirmed on
 * webOS 10 (kernel 5.4) but not on the 3.10 kernels older versions ship, and
 * it's the one thing standing between the netlink monitor and dropping the
 * 3-second poll.
 *
 * Note that it can only answer the question if devices actually come and go
 * while it runs, which is why it reports INCONCLUSIVE rather than a pass when
 * nothing happened. Silence is not evidence that netlink works: an idle
 * system produces no uevents whether or not the socket is delivering them.
 *
 * Usage:
 *   testwebosuevent [--duration SECONDS] [--poll-interval MS] [--verbose]
 *
 * Plug and unplug a controller (USB or Bluetooth) a few times while it runs.
 * Reconnecting the same controller repeatedly is the interesting case, since
 * that's what the presence bitmask can't see.
 *
 * Exit status: 0 netlink usable, 1 fallback required, 2 inconclusive.
 */

/* SDL_internal.h must come first, as in testevdev.c: it installs the dynapi
 * renaming, so the public declarations that follow get renamed along with our
 * calls. Including SDL.h ahead of it leaves calls pointing at SDL_*_REAL with
 * no declaration in scope. */
#include "../src/SDL_internal.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SDL.h"

/* Compiled in rather than linked, since these are internal and unexported. */
#include "../src/joystick/webos/dev_presence.c"
#include "../src/joystick/webos/dev_presence.h"
#include "../src/joystick/webos/uevent_monitor.c"
#include "../src/joystick/webos/uevent_monitor.h"

/* The presence bitmask is 32 bits wide, so it structurally cannot represent a
 * device above this index. Netlink has no such limit, and reporting when we
 * cross it is worth doing. */
#define PRESENCE_MAX_INDEX 32

#define TICK_INTERVAL_MS 20

typedef enum
{
    NODE_OTHER = -1,
    NODE_EVDEV = 0,
    NODE_JS,
    NODE_HIDRAW,
    NODE_KIND_COUNT
} NodeKind;

typedef struct
{
    const char *label;
    const char *prefix;
    SDL_webOSDevicePresenceCheck check;

    Uint32 poll_flags;

    /* Indices netlink reported on since the last poll tick, and when it first
     * reported each, so we can tell what the poll would have missed and how
     * far ahead netlink was. */
    Uint32 netlink_touched;
    Uint32 netlink_time[PRESENCE_MAX_INDEX];

    /* How many transitions netlink reported per index since the last poll. A
     * bitmask diff can express at most one, so anything above that is a
     * change the poll structurally cannot recover -- a reconnect, or a whole
     * connect/disconnect cycle, that happened entirely between two scans. */
    Uint8 netlink_transitions[PRESENCE_MAX_INDEX];
} NodeClass;

static NodeClass node_classes[NODE_KIND_COUNT] = {
    { "evdev", "event", SDL_WEBOS_DEVICE_PRESENCE_CHECK_EVDEV, 0, 0, { 0 } },
    { "js", "js", SDL_WEBOS_DEVICE_PRESENCE_CHECK_JS, 0, 0, { 0 } },
    { "hidraw", "hidraw", SDL_WEBOS_DEVICE_PRESENCE_CHECK_HIDRAW, 0, 0, { 0 } },
};

/* Verdict inputs */
static int poll_changes = 0;         /* bitmask transitions the poll saw */
static int poll_changes_missed = 0;  /* ... that netlink never reported */
static int netlink_events = 0;       /* add/remove on a device node */
static int netlink_invisible = 0;    /* ... the bitmask diff could not express */
static int beyond_bitmask = 0;       /* nodes the 32-bit mask can't represent */
static int netlink_overflows = 0;    /* times the kernel dropped queued uevents */
static Uint32 latency_total = 0;     /* how far netlink led the poll, summed */
static int latency_samples = 0;

static int verbose = 0;
static volatile int keep_running = 1;
static Uint32 start_time;

static void OnSignal(int sig)
{
    (void)sig;
    keep_running = 0;
}

static Uint32 Elapsed(void)
{
    return SDL_GetTicks() - start_time;
}

static void Report(const char *fmt, ...) SDL_PRINTF_VARARG_FUNC(1);

static void Report(const char *fmt, ...)
{
    char msg[512];
    va_list ap;
    Uint32 ms = Elapsed();

    va_start(ap, fmt);
    SDL_vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    /* Straight to stdout rather than SDL_Log, so the output stays greppable
     * when this is piped over SSH. */
    printf("[%3u.%03u] %s\n", ms / 1000, ms % 1000, msg);
    fflush(stdout);
}

/* Splits "event14" into its class and index. Returns NODE_OTHER for anything
 * that isn't a numbered node we track, including "mice" and "mouse0". */
static NodeKind ClassifyNode(const char *devname, int *index)
{
    int kind;

    if (devname == NULL) {
        return NODE_OTHER;
    }

    for (kind = 0; kind < NODE_KIND_COUNT; kind++) {
        const char *prefix = node_classes[kind].prefix;
        size_t prefix_len = SDL_strlen(prefix);
        const char *suffix;
        char *endptr = NULL;
        long value;

        if (SDL_strncmp(devname, prefix, prefix_len) != 0) {
            continue;
        }

        suffix = devname + prefix_len;

        if (*suffix == '\0') {
            continue;
        }

        value = SDL_strtol(suffix, &endptr, 10);

        if (endptr == NULL || *endptr != '\0' || value < 0) {
            continue;
        }

        /* "js" is a prefix of nothing else here, but "event" vs "mouse" and
         * friends means we only get here on an exact prefix + digits match. */
        *index = (int)value;
        return (NodeKind)kind;
    }

    return NODE_OTHER;
}

static void HandleUevent(const SDL_webOSUevent *event)
{
    NodeKind kind;
    NodeClass *cls;
    int index = 0;

    kind = ClassifyNode(event->devname, &index);

    if (kind == NODE_OTHER) {
        if (verbose) {
            Report("netlink  %-6s %-8s %s (ignored)",
                   event->action == SDL_WEBOS_UEVENT_ACTION_ADD ? "add" : "remove",
                   event->subsystem ? event->subsystem : "-",
                   event->devname ? event->devname : "-");
        }
        return;
    }

    cls = &node_classes[kind];
    netlink_events++;

    Report("netlink  %-6s %s/%d",
           event->action == SDL_WEBOS_UEVENT_ACTION_ADD ? "add" : "remove",
           cls->label, index);

    if (index >= PRESENCE_MAX_INDEX) {
        /* The poll cannot see this device at all, on any timescale. */
        beyond_bitmask++;
        Report("         ^ index %d is outside the 32-bit presence bitmask; polling can never see it", index);
        return;
    }

    if (!(cls->netlink_touched & (1u << index))) {
        cls->netlink_time[index] = SDL_GetTicks();
    }

    cls->netlink_touched |= 1u << index;

    if (cls->netlink_transitions[index] < 255) {
        cls->netlink_transitions[index]++;
    }
}

/* `announce` is false for the initial seeding scan, where every attached
 * device shows up as a change against an empty bitmask. Counting or printing
 * those would report the entire existing device list as arrivals that netlink
 * failed to report. */
static void PollPresence(SDL_bool announce)
{
    int kind;

    for (kind = 0; kind < NODE_KIND_COUNT; kind++) {
        NodeClass *cls = &node_classes[kind];
        Uint32 flags = SDL_webOSGetDevicePresenceFlags(cls->check);
        Uint32 changed = flags ^ cls->poll_flags;
        int index;

        for (index = 0; announce && index < PRESENCE_MAX_INDEX; index++) {
            Uint32 bit = 1u << index;

            if (!(changed & bit)) {
                continue;
            }

            poll_changes++;

            if (cls->netlink_touched & bit) {
                Uint32 lead = SDL_GetTicks() - cls->netlink_time[index];
                latency_total += lead;
                latency_samples++;
                Report("poll     %-6s %s/%d (netlink was %ums ahead)",
                       (flags & bit) ? "add" : "remove", cls->label, index, lead);
            } else {
                poll_changes_missed++;
                Report("poll     %-6s %s/%d  *** netlink did not report this ***",
                       (flags & bit) ? "add" : "remove", cls->label, index);
            }
        }

        /* A bitmask diff carries at most one transition per index, so compare
         * what netlink reported against what the diff could express. Anything
         * over is lost for good: a same-index reconnect (2 transitions, 0
         * expressible) or a full connect/disconnect cycle landing between two
         * scans (3 transitions, 1 expressible). */
        for (index = 0; announce && index < PRESENCE_MAX_INDEX; index++) {
            int seen = cls->netlink_transitions[index];
            int expressible = (changed & (1u << index)) ? 1 : 0;

            if (seen > expressible) {
                netlink_invisible += seen - expressible;
                Report("         %s/%d: netlink saw %d transition(s), the bitmask could express %d",
                       cls->label, index, seen, expressible);
            }
        }

        cls->poll_flags = flags;
        cls->netlink_touched = 0;
        SDL_memset(cls->netlink_transitions, 0, sizeof(cls->netlink_transitions));
    }
}

static int PrintVerdict(SDL_bool monitor_opened)
{
    printf("\n");
    printf("=========================================================\n");
    printf(" netlink events on tracked nodes : %d\n", netlink_events);
    printf(" poll-observed changes           : %d\n", poll_changes);
    printf("   corroborated by netlink       : %d\n", poll_changes - poll_changes_missed);
    printf("   missed by netlink             : %d\n", poll_changes_missed);
    printf(" transitions the poll cannot see  : %d\n", netlink_invisible);
    printf(" nodes beyond the 32-bit mask    : %d\n", beyond_bitmask);
    printf(" socket overflows (events lost)  : %d\n", netlink_overflows);

    if (latency_samples > 0) {
        printf(" mean netlink lead over poll     : %ums over %d samples\n",
               latency_total / (Uint32)latency_samples, latency_samples);
    }

    printf("---------------------------------------------------------\n");

    if (!monitor_opened) {
        printf(" VERDICT: FALLBACK REQUIRED\n");
        printf("   The netlink socket could not be opened or bound, so this\n");
        printf("   webOS version must keep the presence poll.\n");
        printf("=========================================================\n");
        return 1;
    }

    if (netlink_events == 0 && poll_changes == 0) {
        printf(" VERDICT: INCONCLUSIVE\n");
        printf("   No device appeared or disappeared during the run, so this\n");
        printf("   says nothing about whether netlink delivers. Re-run and\n");
        printf("   plug/unplug a controller while it is running.\n");
        printf("=========================================================\n");
        return 2;
    }

    if (poll_changes_missed > 0) {
        printf(" VERDICT: FALLBACK REQUIRED\n");
        printf("   The poll saw %d change(s) netlink never reported, so the\n", poll_changes_missed);
        printf("   uevent stream is not reaching this process reliably.\n");
        printf("=========================================================\n");
        return 1;
    }

    printf(" VERDICT: NETLINK USABLE\n");
    printf("   Every change the poll detected was reported by netlink first.\n");

    if (netlink_invisible > 0) {
        printf("   netlink reported %d transition(s) more than the bitmask diff\n", netlink_invisible);
        printf("   could express -- reconnects the poll structurally cannot see.\n");
    }

    printf("=========================================================\n");
    return 0;
}

int main(int argc, char *argv[])
{
    SDL_webOSUeventMonitor *monitor;
    Uint32 duration_ms = 30000;
    Uint32 poll_interval_ms = 3000;
    Uint32 last_poll;
    int i;

    for (i = 1; i < argc; i++) {
        if (SDL_strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
            duration_ms = (Uint32)SDL_atoi(argv[++i]) * 1000;
        } else if (SDL_strcmp(argv[i], "--poll-interval") == 0 && i + 1 < argc) {
            poll_interval_ms = (Uint32)SDL_atoi(argv[++i]);
        } else if (SDL_strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else {
            fprintf(stderr, "Usage: %s [--duration SECONDS] [--poll-interval MS] [--verbose]\n", argv[0]);
            return 3;
        }
    }

    if (poll_interval_ms == 0) {
        fprintf(stderr, "--poll-interval must be greater than zero\n");
        return 3;
    }

    signal(SIGINT, OnSignal);
    signal(SIGTERM, OnSignal);

    /* No video, no joystick backend: this exercises the mechanism directly,
     * so it stays meaningful before the backends are wired up to it. */
    if (SDL_Init(0) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 3;
    }

    SDL_LogSetPriority(SDL_LOG_CATEGORY_INPUT, SDL_LOG_PRIORITY_VERBOSE);

    start_time = SDL_GetTicks();

    monitor = SDL_webOSUeventMonitorOpen();

    Report("netlink monitor: %s", monitor ? "bound to the kernel uevent group" : "UNAVAILABLE");

    /* Seed the poll state before announcing readiness, so devices that were
     * already attached don't register as arrivals on the first tick. The real
     * backend has to bind the socket before this scan for the same reason:
     * anything appearing in the gap would otherwise go unnoticed. */
    PollPresence(SDL_FALSE);

    last_poll = SDL_GetTicks();

    {
        int kind;
        for (kind = 0; kind < NODE_KIND_COUNT; kind++) {
            Report("already attached: %s %s", node_classes[kind].label,
                   node_classes[kind].poll_flags ? "yes" : "none");
        }
    }

    Report("running for %us — plug and unplug a controller now (Ctrl-C to stop early)",
           duration_ms / 1000);

    while (keep_running && Elapsed() < duration_ms) {
        if (monitor != NULL) {
            SDL_webOSUevent event;

            while (SDL_webOSUeventMonitorPoll(monitor, &event)) {
                HandleUevent(&event);
            }

            /* An overflow means the device list can be stale in a way no
             * later event corrects, so a backend has to resync here. Counted
             * separately because it invalidates the comparison rather than
             * being a netlink failure. */
            if (SDL_webOSUeventMonitorLostEvents(monitor)) {
                netlink_overflows++;
                Report("netlink  *** dropped events (buffer overflow) — a backend must rescan here ***");
            }
        }

        if (SDL_TICKS_PASSED(SDL_GetTicks(), last_poll + poll_interval_ms)) {
            PollPresence(SDL_TRUE);
            last_poll = SDL_GetTicks();
        }

        SDL_Delay(TICK_INTERVAL_MS);
    }

    /* A final poll, so a change in the last interval still gets cross-checked
     * instead of being dropped on the floor at exit. */
    PollPresence(SDL_TRUE);

    SDL_webOSUeventMonitorClose(monitor);
    SDL_Quit();

    return PrintVerdict(monitor != NULL);
}
