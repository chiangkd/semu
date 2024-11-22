#pragma once

typedef enum {
    NET_CLIENT_DRIVER_TAP,
    NET_CLIENT_DRIVER_USER,
    NET_CLIENT_DRIVER_MAX,
} net_client_driver_t;

typedef struct {
    int tap_fd;
} net_tap_options;

typedef struct {
    /* TODO: Implement user option */
    int temp;
} net_user_options;

typedef struct {
    char *name;
    net_client_driver_t type;
    union {
        net_tap_options tap;
        net_user_options user;
    } op;
} netdev_t;

int netdev_init(netdev_t *nedtev, const char *net_type);