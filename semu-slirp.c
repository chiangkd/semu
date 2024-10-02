#include "slirp.h"
#include "libslirp.h"

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

/** TBD */
static ssize_t libslirp_send_packet();

static ssize_t slirp_guest_error();

static ssize_t slirp_clock_get_ns();

static ssize_t slirp_timer_new();

static ssize_t slirp_timer_free();

static ssize_t slirp_timer_mod();

static ssize_t slirp_register_poll_fd();

static ssize_t slirp_unregister_poll_fd();

static ssize_t slirp_notify();

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


static void slirp_init()
{
    SlirpConfig cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.version = 1;
    cfg.restricted = 0;
    cfg.in_enabled = 1;
    cfg.vnetwork = htonl(0x0a000200);   // 10.0.2.0
    cfg.vnetmask = htonl(0xffffff00);   // 255.255.255.0
    cfg.vhost = htonl(0x0a000202);      // 10.0.2.2
    cfg.in6_enabled = 0;
    inet_pton(AF_INET6, "fd00::", &cfg.vprefix_addr6);
    cfg_vprefix_len = 64;
    inet_pton(AF_INET6, "fd00::2", &cfg.vhost6);
    cfg.vhostname = NULL;
    cfg.tftp_server_name = NULL;
    cfg.tftp_path = NULL;
    cfg.bootfile = NULL;
    cfg.vdhcp_start = htonl(0x0a00020f);    // 10.0.2.15
    cfg.vnameserver = htonl(0x0a000203);    // 10.0.2.3
    inet_pton(AF_INET6, "fd00::3", &cfg.vnameserver6);
    cfg.vdnssearch = NULL;
    cfg.vdomainname = NULL;
    cfg.if_mtu = 65520;
    cfg.if_mru = 65520;
    cfg.disable_host_loopback = 1;

#if SLIRP_CONFIG_VERSION_MAX >= 2
    cfg.outbound_addr = NULL;
    cfg.outbound_addr6 = NULL;
    if (s4nn->enable_outbound_addr) {
        cfg.version = 2;
        cfg.outbound_addr = &s4nn->outbound_addr;
    }
    if (s4nn->enable_outbound_addr6) {
        cfg.version = 2;
        cfg.outbound_addr6 = &s4nn->outbound_addr6;
    }
#endif
#if SLIRP_CONFIG_VERSION_MAX >= 3
    if (s4nn->disable_dns) {
        cfg.version = 3;
        cfg.disable_dns = true;
    }
#endif
    slirp = slirp_new(&cfg, &libslirp_cb, opaque);
    if (slirp == NULL) {
        fprintf(stderr, "slirp_new failed\n");
    }
    return slirp;

    struct Slirp *slirp_instance = slirp_new();
}


Slirp *slirp_new(const SlirpConfig *cfg, const SlirpCb *callbacks, void *opaque)
{

}


void slirp_cleanup(Slirp *slirp)
{

}

slirp_input()

void slirp_pollfds_fill(Slirp *slirp, uint32_t *timeout, SlirpAddPollCb add_poll, void *opaque)
{
    
}


void slirp_pollfds_poll(Slirp *slipr, int select_error, SlirpGetREventsCb get_revents, void *opaque)
{

}
