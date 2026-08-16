#include "uevent_monitor.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/netlink.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "SDL_error.h"
#include "SDL_log.h"

/* Big enough for any single uevent; the kernel caps the payload well below
 * this. A short read would only lose the tail of one message, so oversize. */
#define UEVENT_BUF_SIZE 8192

/* Kernel-originated uevents. Group 2 is the udev-processed stream, which
 * nothing produces here without udev running. */
#define UEVENT_GROUP_KERNEL 1

struct SDL_webOSUeventMonitor
{
    int fd;
    char buf[UEVENT_BUF_SIZE];
};

static int OpenUeventSocket(void);

static SDL_bool ParseUevent(char *buf, size_t len, SDL_webOSUevent *event);

static const char *TrailingName(const char *path);

SDL_webOSUeventMonitor *SDL_webOSUeventMonitorOpen(void)
{
    SDL_webOSUeventMonitor *monitor;
    int fd;

    fd = OpenUeventSocket();

    if (fd < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_INPUT,
                    "Unable to open netlink uevent socket, falling back to polling: %s",
                    strerror(errno));
        return NULL;
    }

    monitor = (SDL_webOSUeventMonitor *)SDL_calloc(1, sizeof(*monitor));

    if (monitor == NULL) {
        close(fd);
        SDL_OutOfMemory();
        return NULL;
    }

    monitor->fd = fd;

    return monitor;
}

void SDL_webOSUeventMonitorClose(SDL_webOSUeventMonitor *monitor)
{
    if (monitor == NULL) {
        return;
    }

    if (monitor->fd >= 0) {
        close(monitor->fd);
    }

    SDL_free(monitor);
}

SDL_bool SDL_webOSUeventMonitorPoll(SDL_webOSUeventMonitor *monitor, SDL_webOSUevent *event)
{
    if (monitor == NULL || monitor->fd < 0 || event == NULL) {
        return SDL_FALSE;
    }

    for (;;) {
        struct sockaddr_nl addr;
        struct iovec iov;
        struct msghdr msg;
        ssize_t bytes;

        iov.iov_base = monitor->buf;
        /* Leave room to terminate the buffer, so parsing can't run past it
         * if the kernel ever hands us an unterminated final field. */
        iov.iov_len = sizeof(monitor->buf) - 1;

        SDL_zero(addr);
        SDL_zero(msg);
        msg.msg_name = &addr;
        msg.msg_namelen = sizeof(addr);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        bytes = recvmsg(monitor->fd, &msg, MSG_DONTWAIT);

        if (bytes <= 0) {
            if (bytes < 0 && errno == EINTR) {
                continue;
            }
            /* EAGAIN/EWOULDBLOCK: drained, which is the usual way out. */
            return SDL_FALSE;
        }

        /* Only the kernel may hotplug devices. Any other process can bind a
         * netlink socket and send us a unicast message, so drop anything
         * that isn't from portid 0 and addressed to a multicast group. */
        if (msg.msg_namelen != sizeof(addr) || addr.nl_pid != 0 || addr.nl_groups == 0) {
            continue;
        }

        monitor->buf[bytes] = '\0';

        if (ParseUevent(monitor->buf, (size_t)bytes, event)) {
            return SDL_TRUE;
        }

        /* Not an event we can describe; keep draining rather than making the
         * caller poll again for it. */
    }
}

static int OpenUeventSocket(void)
{
    struct sockaddr_nl addr;
    int fd;
    int rcvbuf = 1024 * 1024;

    fd = socket(AF_NETLINK, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, NETLINK_KOBJECT_UEVENT);

    if (fd < 0 && (errno == EINVAL || errno == EPROTONOSUPPORT)) {
        /* Older kernels reject the socket type flags; set them separately. */
        fd = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);

        if (fd >= 0) {
            int flags = fcntl(fd, F_GETFL, 0);
            if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0 ||
                fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
                close(fd);
                return -1;
            }
        }
    }

    if (fd < 0) {
        return -1;
    }

    /* A burst of uevents (a hub with several interfaces) can outrun us
     * between detect ticks, and an overflowing netlink socket drops
     * messages silently. Best effort; the default is workable. */
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    SDL_zero(addr);
    addr.nl_family = AF_NETLINK;
    addr.nl_groups = UEVENT_GROUP_KERNEL;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        int saved_errno = errno;
        close(fd);
        errno = saved_errno;
        return -1;
    }

    return fd;
}

/* A kernel uevent is "ACTION@DEVPATH" followed by NUL-separated KEY=VALUE
 * fields, e.g.
 *
 *   "add@/devices/.../input/input21/event14\0"
 *   "ACTION=add\0DEVPATH=/devices/.../event14\0SUBSYSTEM=input\0"
 *   "DEVNAME=input/event14\0MAJOR=13\0MINOR=78\0..."
 *
 * Returns SDL_FALSE for anything we can't turn into an add/remove, which
 * includes the "change"/"bind"/"move" actions we have no use for. */
static SDL_bool ParseUevent(char *buf, size_t len, SDL_webOSUevent *event)
{
    const char *devname = NULL;
    size_t pos;

    /* libudev's own broadcasts carry a magic prefix instead of the header
     * line above. We bind the kernel group so they shouldn't reach us. */
    if (len >= 8 && SDL_memcmp(buf, "libudev", 8) == 0) {
        return SDL_FALSE;
    }

    SDL_zerop(event);

    /* Skip the header line; ACTION= and DEVPATH= repeat it as proper fields. */
    pos = SDL_strlen(buf) + 1;

    while (pos < len) {
        const char *field = &buf[pos];
        size_t field_len = SDL_strlen(field);

        if (SDL_strncmp(field, "ACTION=", 7) == 0) {
            const char *action = field + 7;
            if (SDL_strcmp(action, "add") == 0) {
                event->action = SDL_WEBOS_UEVENT_ACTION_ADD;
            } else if (SDL_strcmp(action, "remove") == 0) {
                event->action = SDL_WEBOS_UEVENT_ACTION_REMOVE;
            } else {
                return SDL_FALSE;
            }
        } else if (SDL_strncmp(field, "SUBSYSTEM=", 10) == 0) {
            event->subsystem = field + 10;
        } else if (SDL_strncmp(field, "DEVPATH=", 8) == 0) {
            event->devpath = field + 8;
        } else if (SDL_strncmp(field, "DEVNAME=", 8) == 0) {
            /* Present whenever the event describes an actual device node,
             * and more trustworthy than the sysfs path, which for some
             * subsystems ends in the parent rather than the node. */
            devname = field + 8;
        }

        pos += field_len + 1;
    }

    if (event->action == SDL_WEBOS_UEVENT_ACTION_OTHER) {
        return SDL_FALSE;
    }

    /* DEVNAME arrives relative to /dev and may be nested ("input/event14"),
     * so reduce either source to the trailing component. A remove event on
     * an older kernel can omit DEVNAME, hence the DEVPATH fallback. */
    if (devname != NULL) {
        event->devname = TrailingName(devname);
    } else if (event->devpath != NULL) {
        event->devname = TrailingName(event->devpath);
    }

    return SDL_TRUE;
}

static const char *TrailingName(const char *path)
{
    const char *slash = SDL_strrchr(path, '/');
    const char *name = slash != NULL ? slash + 1 : path;

    return *name != '\0' ? name : NULL;
}
