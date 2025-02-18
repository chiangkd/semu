#pragma once

/* Slirp part */
#include <stdint.h>
#include <linux/if.h>
#include <linux/in.h>

// ICMP
#include <linux/icmp.h>
// TCP
#include <linux/tcp.h>
// IP
#include <linux/ip.h>

#include "common.h"

struct slirp_qhead {
    struct slirp_qhead *qh_link;
    struct slirp_qhead *qh_rlink;
};

struct qlink {
    void *next, *prev;
};


PACKED(struct mbuf_ptr
{
    struct mbuf *mptr;
});


// Source: https://github.com/utmapp/libslirp/blob/ios-support/src/tcpip.h
/*
 * Tcp+ip header, after ip options removed.
 */
struct tcpiphdr {
    struct mbuf_ptr ih_mbuf; /* backpointer to mbuf */
    union {
        struct {
            struct in_addr ih_src; /* source internet address */
            struct in_addr ih_dst; /* destination internet address */
            uint8_t ih_x1; /* (unused) */
            uint8_t ih_pr; /* protocol */
        } ti_i4;
        struct {
            struct in6_addr ih_src;
            struct in6_addr ih_dst;
            uint8_t ih_x1;
            uint8_t ih_nh;
        } ti_i6;
    } ti;
    uint16_t ti_x0;
    uint16_t ti_len; /* protocol length */
    struct tcphdr ti_t; /* tcp header */
};
#define ti_mbuf ih_mbuf.mptr
#define ti_pr ti.ti_i4.ih_pr
#define ti_src ti.ti_i4.ih_src
#define ti_dst ti.ti_i4.ih_dst
#define ti_src6 ti.ti_i6.ih_src
#define ti_dst6 ti.ti_i6.ih_dst
#define ti_nh6 ti.ti_i6.ih_nh
#define ti_sport ti_t.th_sport
#define ti_dport ti_t.th_dport
#define ti_seq ti_t.th_seq
#define ti_ack ti_t.th_ack
#define ti_x2 ti_t.th_x2
#define ti_off ti_t.th_off
#define ti_flags ti_t.th_flags
#define ti_win ti_t.th_win
#define ti_sum ti_t.th_sum
#define ti_urp ti_t.th_urp

#define tcpiphdr2qlink(T) \
    ((struct qlink *)(((char *)(T)) - sizeof(struct qlink)))
#define qlink2tcpiphdr(Q) \
    ((struct tcpiphdr *)(((char *)(Q)) + sizeof(struct qlink)))
#define tcpiphdr_next(T) qlink2tcpiphdr(tcpiphdr2qlink(T)->next)
#define tcpiphdr_prev(T) qlink2tcpiphdr(tcpiphdr2qlink(T)->prev)
#define tcpfrag_list_first(T) qlink2tcpiphdr((T)->seg_next)
#define tcpfrag_list_end(F, T) (tcpiphdr2qlink(F) == (struct qlink *)(T))
#define tcpfrag_list_empty(T) ((T)->seg_next == (struct tcpiphdr *)(T))

/* This is the difference between the size of a tcpiphdr structure, and the
 * size of actual ip+tcp headers, rounded up since we need to align data.  */
#define TCPIPHDR_DELTA                                     \
    (MAX(0, (sizeof(struct tcpiphdr) - sizeof(struct iphdr) - \
             sizeof(struct tcphdr) + 3) &                  \
                ~3))

#define ETH_ALEN 6
#define ETH_ADDRSTRLEN 18 /* "xx:xx:xx:xx:xx:xx", with trailing NUL */
#define ETH_HLEN 14
#define ETH_P_IP (0x0800) /* Internet Protocol packet  */
#define ETH_P_ARP (0x0806) /* Address Resolution packet */
#define ETH_P_IPV6 (0x86dd)
#define ETH_P_NCSI (0x88f8)

PACKED(struct slirp_arphdr {
    unsigned short ar_hrd; /* format of hardware address */
    unsigned short ar_pro; /* format of protocol address */
    unsigned char ar_hln; /* length of hardware address */
    unsigned char ar_pln; /* length of protocol address */
    unsigned short ar_op; /* ARP opcode (command)       */

    /*
     *  Ethernet looks like this : This bit is variable sized however...
     */
    uint8_t ar_sha[ETH_ALEN]; /* sender hardware address */
    uint32_t ar_sip; /* sender IP address       */
    uint8_t ar_tha[ETH_ALEN]; /* target hardware address */
    uint32_t ar_tip; /* target IP address       */
});

/*
 * Ip reassembly queue structure.  Each fragment
 * being reassembled is attached to one of these structures.
 * They are timed out after ipq_ttl drops to 0, and may also
 * be reclaimed if memory becomes tight.
 */
struct ipq {
    struct qlink ip_link; /* to other reass headers */
    uint8_t ipq_ttl; /* time for reass q to live */
    uint8_t ipq_p; /* protocol of this fragment */
    uint16_t ipq_id; /* sequence id for reassembly */
    struct in_addr ipq_src, ipq_dst;
};

struct sbuf {
    uint32_t sb_cc; /* actual chars in buffer */
    uint32_t sb_datalen; /* Length of data  */
    char *sb_wptr; /* write pointer. points to where the next
                    * bytes should be written in the sbuf */
    char *sb_rptr; /* read pointer. points to where the next
                    * byte should be read from the sbuf */
    char *sb_data; /* Actual data */
};

struct socket {
    struct socket *so_next, *so_prev; /* For a linked list of sockets */


    uint8_t so_iptos; /* Type of service */
    uint8_t so_emu; /* Is the socket emulated? */

    uint8_t so_type; /* Protocol of the socket. May be 0 if loading old
                      * states. */
    int32_t so_state; /* internal state flags SS_*, below */

    struct tcpcb *so_tcpcb; /* pointer to TCP protocol control block */
    unsigned so_expire; /* When the socket will expire */

    int so_queued; /* Number of packets queued from this socket */
    int so_nqueued; /* Number of packets queued in a row
                     * Used to determine when to "downgrade" a session
                     * from fastq to batchq */

    struct sbuf so_rcv; /* Receive buffer */
    struct sbuf so_snd; /* Send buffer */
};

typedef struct SlirpConfig {
    /* Version must be provided */
    uint32_t version;
    /*
     * Fields introduced in SlirpConfig version 1 begin
     */
    /* Whether to prevent the guest from accessing the Internet */
    int restricted;
    /* Whether IPv4 is enabled */
    bool in_enabled;
    /* Virtual network for the guest */
    struct in_addr vnetwork;
    /* Mask for the virtual network for the guest */
    struct in_addr vnetmask;
    /* Virtual address for the host exposed to the guest */
    struct in_addr vhost;
    /* Whether IPv6 is enabled */
    bool in6_enabled;
    /* Virtual IPv6 network for the guest */
    struct in6_addr vprefix_addr6;
    /* Len of the virtual IPv6 network for the guest */
    uint8_t vprefix_len;
    /* Virtual address for the host exposed to the guest */
    struct in6_addr vhost6;
    /* Hostname exposed to the guest in DHCP hostname option */
    const char *vhostname;
    /* Hostname exposed to the guest in the DHCP TFTP server name option */
    const char *tftp_server_name;
    /* Path of the files served by TFTP */
    const char *tftp_path;
    /* Boot file name exposed to the guest via DHCP */
    const char *bootfile;
    /* Start of the DHCP range */
    struct in_addr vdhcp_start;
    /* Virtual address for the DNS server exposed to the guest */
    struct in_addr vnameserver;
    /* Virtual IPv6 address for the DNS server exposed to the guest */
    struct in6_addr vnameserver6;
    /* DNS search names exposed to the guest via DHCP */
    const char **vdnssearch;
    /* Domain name exposed to the guest via DHCP */
    const char *vdomainname;
    /* MTU when sending packets to the guest */
    /* Default: IF_MTU_DEFAULT */
    size_t if_mtu;
    /* MRU when receiving packets from the guest */
    /* Default: IF_MRU_DEFAULT */
    size_t if_mru;
    /* Prohibit connecting to 127.0.0.1:* */
    bool disable_host_loopback;
    /*
     * Enable emulation code (*warning*: this code isn't safe, it is not
     * recommended to enable it)
     */
    bool enable_emu;

    /*
     * Fields introduced in SlirpConfig version 2 begin
     */
    /* Address to be used when sending data to the Internet */
    struct sockaddr_in *outbound_addr;
    /* IPv6 Address to be used when sending data to the Internet */
    struct sockaddr_in6 *outbound_addr6;

    /*
     * Fields introduced in SlirpConfig version 3 begin
     */
    /* slirp will not redirect/serve any DNS packet */
    bool disable_dns;

    /*
     * Fields introduced in SlirpConfig version 4 begin
     */
    /* slirp will not reply to any DHCP requests */
    bool disable_dhcp;

    /*
     * Fields introduced in SlirpConfig version 5 begin
     */
    /* Manufacturer ID (IANA Private Enterprise number) */
    uint32_t mfr_id;
    /*
     * MAC address allocated for an out-of-band management controller, to be
     * retrieved through NC-SI.
     */
    uint8_t oob_eth_addr[6];
} SlirpConfig;


typedef struct {
    int cfg_version;

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
    bool disable_dhcp; /* slirp will not reply to any DHCP requests */
    struct in_addr vdhcp_startaddr;
    struct in_addr vnameserver_addr;
    struct in6_addr vnameserver_addr6;

    struct in_addr client_ipaddr;
    char client_hostname[33];

    int restricted;
    struct gfwd_list *guestfwd_list;

    int if_mtu;
    int if_mru;

    bool disable_host_loopback;

    uint32_t mfr_id;
    uint8_t oob_eth_addr[ETH_ALEN];

    /* mbuf states */
    struct slirp_qhead m_freelist;
    struct slirp_qhead m_usedlist;
    int mbuf_alloced;

    /* if states */
    struct slirp_quehead if_fastq; /* fast queue (for interactive data) */
    struct slirp_quehead if_batchq; /* queue for non-interactive data */
    bool if_start_busy; /* avoid if_start recursion */

    /* ip states */
    struct ipq ipq; /* ip reass. queue */
    uint16_t ip_id; /* ip packet ctr, for ids */

    /* bootp/dhcp states */
    BOOTPClient bootp_clients[NB_BOOTP_CLIENTS];
    char *bootp_filename;
    size_t vdnssearch_len;
    uint8_t *vdnssearch;
    char *vdomainname;

    /* tcp states */
    struct socket tcb;
    struct socket *tcp_last_so;
    tcp_seq tcp_iss; /* tcp initial send seq # */
    uint32_t tcp_now; /* for RFC 1323 timestamps */

    /* udp states */
    struct socket udb;
    struct socket *udp_last_so;

    /* icmp states */
    struct socket icmp;
    struct socket *icmp_last_so;

    /* tftp states */
    char *tftp_prefix;
    struct tftp_session tftp_sessions[TFTP_SESSIONS_MAX];
    char *tftp_server_name;

    ArpTable arp_table;
    NdpTable ndp_table;

    GRand *grand;
    void *ra_timer;

    bool enable_emu;

    const SlirpCb *cb;
    void *opaque;

    struct sockaddr_in *outbound_addr;
    struct sockaddr_in6 *outbound_addr6;
    bool disable_dns; /* slirp will not redirect/serve any DNS packet */
} Slirp;
