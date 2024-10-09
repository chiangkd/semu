#pragma once
#include <arpa/inet.h>

#include <glib.h>
#include <libslirp.h>
#include "riscv.h"

/* opaque for SlirpCb */
struct slirp_data {
    int tapfd;
    GSList *timers;
};

struct semu_slirp_config {
    unsigned int mtu;
    struct in_addr vnetwork;     // 10.0.2.0
    struct in_addr vnetmask;     // 255.255.255.0
    struct in_addr vhost;        // 10.0.2.2
    struct in_addr vdhcp_start;  // 10.0.2.15
    struct in_addr vnameserver;  // 10.0.2.3
    struct in_addr
        recommended_vguest;  // 10.0.2.100 (slirp itself is unaware of vguest)
    bool enable_ipv6;
    bool disable_host_loopback;
    bool enable_sandbox;
    bool enable_seccomp;
#if SLIRP_CONFIG_VERSION_MAX >= 2
    bool enable_outbound_addr;
    struct sockaddr_in outbound_addr;
    bool enable_outbound_addr6;
    struct sockaddr_in6 outbound_addr6;
#endif
#if SLIRP_CONFIG_VERSION_MAX >= 3
    bool disable_dns;
#endif
    struct sockaddr vmacaddress;  // MAC address of interface
    int vmacaddress_len;          // MAC address byte length
};

Slirp *create_slirp(void *opaque);

int net_slirp_add_poll(int fd, int events, void *opaque);
int net_slirp_get_revents(int idx, void *opaque);
