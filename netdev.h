#pragma once

#include <stdbool.h>

/* clang-format off */
#define SUPPORTED_DEVICES   \
        _(tap)              \
        _(user)
/* clang-format on */

typedef enum {
#define _(dev) NETDEV_IMPL_##dev,
    SUPPORTED_DEVICES
#undef _
} netdev_impl_t;

typedef struct {
    int tap_fd;
} net_tap_options_t;

/* Slirp part*/
#include "slirp.h"




typedef struct {
    /* TODO: Implement user option */
    int slirp_fd;
    int timers;
    struct in_addr vnetwork;            // 10.0.2.0
    struct in_addr vnetmask;            // 255.255.255.0
    struct in_addr vhost;               // 10.0.2.2
    struct in_addr vdhcp_start;         // 10.0.2.15
    struct in_addr vnameserver;         // 10.0.2.3
    struct in_addr recommended_vguest;  // 10.0.2.100
} net_user_options_t;

typedef struct {
    char *name;
    netdev_impl_t type;
    void *op;
} netdev_t;

bool netdev_init(netdev_t *nedtev, const char *net_type);
