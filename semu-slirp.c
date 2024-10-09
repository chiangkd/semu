#include "device.h"
#include "slirp.h"
#include "utils.h"

// #include "libslirp.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#if defined(__APPLE__)
#define HAVE_MACH_TIMER
#include <mach/mach_time.h>
#elif !defined(_WIN32) && !defined(_WIN64)
#define HAVE_POSIX_TIMER

/*
 * Use a faster but less precise clock source because we need quick
 * timestamps rather than fine-grained precision.
 */
#ifdef CLOCK_MONOTONIC_COARSE
#define CLOCKID CLOCK_MONOTONIC_COARSE
#else
#define CLOCKID CLOCK_REALTIME_COARSE
#endif
#endif



typedef struct {
    SlirpTimerCb cb;
    void *cb_opaque;
    int64_t expire_timer_msec;
} slirp_timer_t;



static int slirp_poll_to_gio(int events)
{
    int ret = 0;

    if (events & SLIRP_POLL_IN) {
        ret |= G_IO_IN;
    }
    if (events & SLIRP_POLL_OUT) {
        ret |= G_IO_OUT;
    }
    if (events & SLIRP_POLL_PRI) {
        ret |= G_IO_PRI;
    }
    if (events & SLIRP_POLL_ERR) {
        ret |= G_IO_ERR;
    }
    if (events & SLIRP_POLL_HUP) {
        ret |= G_IO_HUP;
    }

    return ret;
}

int net_slirp_add_poll(int fd, int events, void *opaque)
{
    GArray *pollfds = opaque;
    GPollFD pfd = {
        .fd = fd,
        .events = slirp_poll_to_gio(events),
    };
    int idx = pollfds->len;
    g_array_append_val(pollfds, pfd);
    return idx;
}

static int slirp_gio_to_poll(int events)
{
    int ret = 0;

    if (events & G_IO_IN) {
        ret |= SLIRP_POLL_IN;
    }
    if (events & G_IO_OUT) {
        ret |= SLIRP_POLL_OUT;
    }
    if (events & G_IO_PRI) {
        ret |= SLIRP_POLL_PRI;
    }
    if (events & G_IO_ERR) {
        ret |= SLIRP_POLL_ERR;
    }
    if (events & G_IO_HUP) {
        ret |= SLIRP_POLL_HUP;
    }

    return ret;
}

int net_slirp_get_revents(int idx, void *opaque)
{
    GArray *pollfds = opaque;

    return slirp_gio_to_poll(g_array_index(pollfds, GPollFD, idx).revents);
}


/** TBD */
static ssize_t slirp_send_packet(const void *pkt, size_t pkt_len, void *opaque)
{
    struct slirp_data *data = (struct slirp_data *) opaque;
    return write(data->tapfd, pkt, pkt_len);
}

static void slirp_guest_error(const char *msg, void *opaque)
{
    fprintf(stderr, "libslirp: %s\n", msg);
}

static uint64_t slirp_clock_get_ns(void *opaque)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static void *slirp_timer_new(SlirpTimerCb cb, void *cb_opaque, void *opaque)
{
    struct slirp_data *data = (struct slirp_data *) opaque;
    slirp_timer_t *t = (slirp_timer_t *) malloc(sizeof(slirp_timer_t) * 1);
    t->cb = cb;
    t->cb_opaque = cb_opaque;
    t->expire_timer_msec = -1;
    data->timers = g_slist_append(data->timers, t);

    return t;
}

static void slirp_timer_free(void *timer, void *opaque)
{
    struct slirp_data *data = (struct slirp_data *) opaque;
    data->timers = g_slist_remove(data->timers, timer);
    g_free(timer);
}

static void slirp_timer_mod(void *timer, int64_t expire_time_msec, void *opaque)
{
    slirp_timer_t *t = (struct timer *) timer;
    t->expire_timer_msec = expire_time_msec;
}

static void slirp_register_poll_fd(int fd, void *opaque)
{
    // Nop
}

static void slirp_unregister_poll_fd(int fd, void *opaque)
{
    // Nop
}

static void slirp_notify(void *opaque)
{
    // Nop
}

static const SlirpCb libslirp_cb = {
    .send_packet = slirp_send_packet,
    .guest_error = slirp_guest_error,
    .clock_get_ns = slirp_clock_get_ns,
    .timer_new = slirp_timer_new,
    .timer_free = slirp_timer_free,
    .timer_mod = slirp_timer_mod,
    .register_poll_fd = slirp_register_poll_fd,
    .unregister_poll_fd = slirp_unregister_poll_fd,
    .notify = slirp_notify,
};



Slirp *create_slirp(void *opaque)
{
    Slirp *slirp = NULL;
    SlirpConfig cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.version = 1;
    cfg.restricted = 0;
    cfg.in_enabled = 1;
    cfg.vnetwork.s_addr = htonl(0x0a000200);  // 10.0.2.0
    cfg.vnetmask.s_addr = htonl(0xffffff00);  // 255.255.255.0
    cfg.vhost.s_addr = htonl(0x0a000202);     // 10.0.2.2
    cfg.in6_enabled = 0;
    inet_pton(AF_INET6, "fd00::", &cfg.vprefix_addr6);
    cfg.vprefix_len = 64;
    inet_pton(AF_INET6, "fd00::2", &cfg.vhost6);
    cfg.vhostname = NULL;
    cfg.tftp_server_name = NULL;
    cfg.tftp_path = NULL;
    cfg.bootfile = NULL;
    cfg.vdhcp_start.s_addr = htonl(0x0a00020f);  // 10.0.2.15
    cfg.vnameserver.s_addr = htonl(0x0a000203);  // 10.0.2.3
    inet_pton(AF_INET6, "fd00::3", &cfg.vnameserver6);
    cfg.vdnssearch = NULL;
    cfg.vdomainname = NULL;
    cfg.if_mtu = 65520;
    cfg.if_mru = 65520;
    cfg.disable_host_loopback = 1;

    // #if SLIRP_CONFIG_VERSION_MAX >= 2
    //     cfg.outbound_addr = NULL;
    //     cfg.outbound_addr6 = NULL;
    //     if (s4nn->enable_outbound_addr) {
    //         cfg.version = 2;
    //         cfg.outbound_addr = &s4nn->outbound_addr;
    //     }
    //     if (s4nn->enable_outbound_addr6) {
    //         cfg.version = 2;
    //         cfg.outbound_addr6 = &s4nn->outbound_addr6;
    //     }
    // #endif
    // #if SLIRP_CONFIG_VERSION_MAX >= 3
    //     if (s4nn->disable_dns) {
    //         cfg.version = 3;
    //         cfg.disable_dns = true;
    //     }
    // #endif
    slirp = slirp_new(&cfg, &libslirp_cb, opaque);
    if (slirp == NULL) {
        fprintf(stderr, "slirp_new failed\n");
    }
    return slirp;
}


// Slirp *slirp_new(const SlirpConfig *cfg, const SlirpCb *callbacks, void
// *opaque)
// {

// }


// void slirp_cleanup(Slirp *slirp)
// {

// }

// slirp_input()

// void slirp_pollfds_fill(Slirp *slirp, uint32_t *timeout, SlirpAddPollCb
// add_poll, void *opaque)
// {

// }


// void slirp_pollfds_poll(Slirp *slipr, int select_error, SlirpGetREventsCb
// get_revents, void *opaque)
// {

// }
