#pragma once
#include <arpa/inet.h>

#include <libslirp.h>
#include "riscv.h"

struct Slirp {
    unsigned time_fasttimo;
    unsigned last_slowtimo;
    bool do_slowtimo;

    bool in_enabled, in6_enabled;

    /* virtual network configuration */
    struct in_addr vnetwork_addr;
    struct in_addr vnetwork_mask;
    struct in_addr vhost_addr;
    struct in6_addr vprefix_addr6;
    uint8_t vprefix_len;
    struct in6_addr vhost_addr6;
    struct in_addr vdhcp_startaddr;
    struct in_addr vnameserver_addr;
    struct in6_addr vnameserver_addr6;

};



typedef struct {
  char* data;
  uint32_t len;
} FDArray_t;


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


void slirp_cfg_init(SlirpConfig *cfg);

struct slirp_data *create_slirp(SlirpConfig *cfg, int32_t tap_fd);