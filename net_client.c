#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

#include "net_client.h"

static int net_init_tap();
static int net_init_slirp();

static int (*const net_init_fun[NET_CLIENT_DRIVER_MAX])(const netdev_t *) = {
    [NET_CLIENT_DRIVER_TAP] = net_init_tap,
    [NET_CLIENT_DRIVER_USER] = net_init_slirp,
};

static const char *client_driver_lookup[] = {
    [NET_CLIENT_DRIVER_TAP] = "tap",
    [NET_CLIENT_DRIVER_USER] = "user",
};

static int find_net_dev_idx(const char *net_type, const char **netlookup)
{
    int i;
    if (!net_type)
        return -1;
    for (i = 0; i < NET_CLIENT_DRIVER_MAX; i++) {
        if (!strcmp(net_type, netlookup[i])) {
            return i;
        }
    }
    return -1;
}

static int net_init_tap(netdev_t *netdev)
{
    net_tap_options *tap;
    tap = &netdev->op.tap;
    tap->tap_fd = open("/dev/net/tun", O_RDWR);
    if (tap->tap_fd < 0) {
        fprintf(stderr, "failed to open TAP device: %s\n", strerror(errno));
        return false;
    }

    /* Specify persistent tap device */
    struct ifreq ifreq = {.ifr_flags = IFF_TAP | IFF_NO_PI};
    strncpy(ifreq.ifr_name, "tap%d", sizeof(ifreq.ifr_name));
    if (ioctl(tap->tap_fd, TUNSETIFF, &ifreq) < 0) {
        fprintf(stderr, "failed to allocate TAP device: %s\n", strerror(errno));
        return false;
    }

    fprintf(stderr, "allocated TAP interface: %s\n", ifreq.ifr_name);
    assert(fcntl(tap->tap_fd, F_SETFL,
                 fcntl(tap->tap_fd, F_GETFL, 0) | O_NONBLOCK) >= 0);
    return 0;
}

static int net_init_slirp(netdev_t *netdev)
{
    // TBD: create slirp client
    return 0;
}

int net_client_init(netdev_t *netdev, const char *net_type)
{
    int dev_idx = find_net_dev_idx(net_type, client_driver_lookup);
    if (dev_idx == -1)
        return false;
    else
        netdev->type = dev_idx;

    net_init_fun[netdev->type](netdev);

    return true;
}