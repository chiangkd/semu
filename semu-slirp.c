#include "device.h"
#include "slirp.h"
#include "utils.h"

// #include "libslirp.h"
#include <stdlib.h>
#include <poll.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#define SLIRP_POLLFD_SIZE_INCREASE 16


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

#define FRONTENDSIDE 0
#define BACKENDSIDE  1

struct slirp_timer {
    struct slirp_timer *next;
    uint64_t expire_time;
    SlirpTimerCb cb;
    void *cb_opaque;
};

/* opaque for SlirpCb */
struct slirp_data {
    Slirp *slirp;
    pthread_t daemon;
    int sv[2];
    int tapfd;
    int pfd_len;
    int pfd_size;
    struct pollfd *pfd;
    struct slirp_timer *timers;
};

static int semu_slirp_to_poll(int events)
{
    int ret = 0;
    if (events & SLIRP_POLL_IN)
        ret |= POLLIN;
    if (events & SLIRP_POLL_OUT)
        ret |= POLLOUT;
    if (events & SLIRP_POLL_PRI)
        ret |= POLLPRI;
    if (events & SLIRP_POLL_HUP)
        ret |= POLLHUP;

    return ret;
}

static int semu_poll_to_slirp(int events)
{
    int ret = 0;
    if (events & POLLIN)
        ret |= SLIRP_POLL_IN;
    if (events & POLLOUT)
        ret |= SLIRP_POLL_OUT;
    if (events & POLLPRI)
        ret |= SLIRP_POLL_PRI;
    if (events & POLLERR)
        ret |= SLIRP_POLL_ERR;
    if (events & POLLHUP)
        ret |= SLIRP_POLL_HUP;

    return ret;
}

int net_slirp_add_poll(int fd, int events, void *opaque)
{
    struct slirp_data *slirp = opaque;

    if(slirp->pfd_len >= slirp->pfd_size) {
        int newsize = slirp->pfd_size + SLIRP_POLLFD_SIZE_INCREASE;
        struct pollfd *newfd = realloc(slirp->pfd, newsize * sizeof(struct pollfd));
        if(newfd) {
            slirp->pfd = newfd;
            slirp->pfd_size = newsize;
        }
    }
    if (slirp->pfd_len < slirp->pfd_size) {
        int idx = slirp->pfd_len++;
        slirp->pfd[idx].fd = fd;
        slirp->pfd[idx].events = semu_slirp_to_poll(events);
    }

    return 1;
}


int net_slirp_get_revents(int idx, void *opaque)
{
    struct slirp_data *slirp = opaque;

    return semu_poll_to_slirp(slirp->pfd[idx].revents);
}


/** TBD */
static ssize_t slirp_send_packet(const void *pkt, size_t pkt_len, void *opaque)
{
    struct slirp_data *data = (struct slirp_data *) opaque;

    return write(data->sv[BACKENDSIDE], pkt, pkt_len);
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
    struct slirp_timer *t = (struct slirp_timer *) malloc(sizeof(struct slirp_timer) * 1);
    if(t) {
        t->next = data->timers;
        t->expire_time = -1;
        t->cb = cb;
        t->cb_opaque = cb_opaque;
        data->timers = t;
    }
}

static void slirp_timer_free(void *timer, void *opaque)
{
    struct slirp_data *data = (struct slirp_data *) opaque;
    struct slirp_timer *t = (struct slirp_timer *) timer;
    struct slirp_timer **tscan;

    for(tscan = &data->timers; *tscan != NULL && tscan != t; tscan = &(*tscan)->next)
        ;
    if (*tscan) {
        *tscan = t->next;
        free(t);
    }
}

static void slirp_timer_mod(void *timer, int64_t expire_time_msec, void *opaque)
{
    struct slirp_timer *t = (struct slirp_timer *) timer;
    t->expire_time = expire_time_msec;
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


static void update_ra_timeout(uint32_t *timeout_msec,
                              void *opaque)
{
    struct slirp_data *data = opaque;
    struct slirp_timer *timer;
    
    int64_t now_msec = slirp_clock_get_ns(data) / 1000000;

    for (timer = data->timers; timer != NULL; timer = timer->next) {
        if (timer->expire_time != -1UL) {
            int64_t diff = timer->expire_time - now_msec;
            if (diff < 0)
                diff = 0;
            if (diff < *timeout_msec)
                *timeout_msec = diff;
        }
    }
}

static void check_ra_timeout(void *opaque) {
	struct slirp_data *slirp = opaque;
	struct slirp_timer *timer;
	int64_t now_ms = slirp_clock_get_ns(opaque) / 1000000;
	for (timer = slirp->timers; timer != NULL; timer =  timer->next) {
		if (timer->expire_time != -1UL) {
			int64_t diff = timer->expire_time - now_ms;
			if (diff <= 0) {
				timer->expire_time = -1UL;
				timer->cb(timer->cb_opaque);
			}
		}
	}
}


static void *slirp_daemon(void *opaque) {

    struct slirp_data *data = (struct slirp_data *)opaque;

    net_slirp_add_poll(data->tapfd, SLIRP_POLL_IN | SLIRP_POLL_HUP, data);


    while (1) {
        int pollout;
        uint32_t timeout = -1;


        slirp_pollfds_fill(data->slirp, &timeout, net_slirp_add_poll, data);


        update_ra_timeout(&timeout, data);
        pollout = poll(data->pfd, data->pfd_len, timeout);


        if(data->pfd[0].revents) {
            uint8_t buf[65536];

            fprintf(stderr, "revevts = 0x%x\n", data->pfd[0].revents);

            size_t len = read(data->tapfd, buf, 65536);
            if (len <= 0)
                break;
            else 
                slirp_input(data->slirp, buf, len);
        }
        slirp_pollfds_poll(data->slirp, (pollout <= 0), net_slirp_get_revents, data);
        check_ra_timeout(data);
    }
    return NULL;
}

void slirp_cfg_init(SlirpConfig *cfg) 
{
    memset(cfg, 0, sizeof(*cfg));

    // Default setting
    cfg->version = 1;
    cfg->restricted = 0;
    cfg->bootfile = 1;
    inet_pton(AF_INET, "10.0.2.0", &(cfg->vnetwork));
    inet_pton(AF_INET, "255.255.255.0", &(cfg->vnetmask));
    inet_pton(AF_INET, "10.0.2.2", &(cfg->vhost));
    cfg->in6_enabled = 1;
    inet_pton(AF_INET6, "fd00::", &(cfg->vprefix_addr6));
    cfg->vprefix_len = 64;
    inet_pton(AF_INET6, "fd00::2", &(cfg->vhost6));
    cfg->vhostname = "slirp";
    cfg->tftp_server_name = NULL;
    cfg->tftp_path = NULL;
    cfg->bootfile = NULL;
    inet_pton(AF_INET,"10.0.2.15", &(cfg->vdhcp_start));
    inet_pton(AF_INET,"10.0.2.3", &(cfg->vnameserver));
    inet_pton(AF_INET6, "fd00::3", &cfg->vnameserver6);
    cfg->vdnssearch = NULL;
    cfg->vdomainname = NULL;
    cfg->if_mtu = 0; // IF_MTU_DEFAULT
    cfg->if_mru = 0; // IF_MTU_DEFAULT
}

struct slirp_data *create_slirp(SlirpConfig *cfg, int32_t tap_fd)
{
    struct slirp_data *data = (struct slirp_data *)malloc(sizeof(struct slirp_data));

    // Open slirp
    if(data) {
        socketpair(AF_LOCAL, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, data->sv);

        data->slirp = slirp_new(cfg, &libslirp_cb, data);
        if(data->slirp == NULL) {
            perror("slirp");
            free(data);
        }
        data->tapfd = tap_fd;

        fprintf(stderr, "[daemon][tapfd] = 0x%x\n",data->tapfd);


        pthread_create(&data->daemon, NULL, slirp_daemon, data);

    }
    else if (data->slirp == NULL) {
        fprintf(stderr, "slirp_new failed\n");
        return NULL;
    }
    
    return data;
}



