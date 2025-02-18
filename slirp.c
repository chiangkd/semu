// #include "slirp.h"
#include "netdev.h"

struct in_addr loopback_addr;


static int udp_init(Slirp *slirp)
{
    slirp->udb.so_next = slirp->udb.so_prev = &slirp->udb;
    slirp->udp_last_so = &slirp->udb;
}

static int tcp_init(Slirp *slirp)
{
    slirp->tcb.so_next = slirp->tcb.so_prev = &slirp->tcb;
    slirp->tcp_last_so = &slirp->tcb;
}

static int icmp_init(Slirp *slirp)
{
    slirp->icmp.so_next = slirp->icmp.so_prev = &slirp->icmp;
    slirp->icmp_last_so = &slirp->icmp;

    return 0;
}

static int if_init(Slirp *slirp)
{
    slirp->if_fastq.qh_link = slirp->if_fastq.qh_rlink;
    slirp->if_batchq.qh_link = slirp->if_batchq.qh_link;

    return 0;
}

static int ip_init(Slirp *slirp)
{
    slirp->ipq.ip_link.next = slirp->ipq.ip_link.prev = &slirp->ipq.ip_link;
    udp_init(slirp);
    tcp_init(slirp);
    icmp_init(slirp);
}


Slirp *slirp_new(SlirpConfig *cfg)
{
    Slirp *slirp;

    if_init(slirp);
    ip_init(slirp);

    /* detail implementation */

    return slirp;
}

/* might no needed */
Slirp *net_slirp_init(/* void *opaque, */ net_user_options_t *slirp_opt)
{
    /* TODO: set network configuration to cfg */
    SlirpConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.version = 1;
    cfg.restricted = 0;
    cfg.in_enabled = 1;
    cfg.vnetwork = slirp_opt->vnetwork;    // 10.0.2.0
    cfg.vnetmask = slirp_opt->vnetmask;    // 255.255.255.0
    cfg.vhost = slirp_opt->vhost;
    // cfg.in6_enabled = 0;
    // cfg.vprefix_addr6 = vprefix_addr6;
    // cfg.vprefix_len = vprefix_len;
    // cfg.vhost6 = vhost6;
    // cfg.vhostname = vhostname;
    // cfg.tftp_server_name = tftp_server_name;
    // cfg.tftp_path = tftp_path;
    // cfg.bootfile = bootfile;
    // cfg.vdhcp_start = vdhcp_start;
    cfg.vnameserver = slirp_opt->vnameserver;  // 10.0.2.3
    // cfg.vnameserver6 = vnameserver6;
    // cfg.vdnssearch = vdnssearch;
    // cfg.vdomainname = vdomainname;


    slirp_new(&cfg);

}



#define ARPOP_REQUEST 1 /* ARP request */
#define ARPOP_REPLY 2 /* ARP reply   */

/* emulated hosts use the MAC addr 52:55:IP:IP:IP:IP */
static const uint8_t special_ethaddr[ETH_ALEN] = { 0x52, 0x55, 0x00,
                                                   0x00, 0x00, 0x00 };
static void arp_input(Slirp *slirp, const uint8_t *pkt, int pkt_len)
{
    const struct slirp_arphdr *ah =
        (const struct slirp_arphdr *)(pkt + ETH_HLEN);
    uint8_t arp_reply[MAX(2 + ETH_HLEN + sizeof(struct slirp_arphdr), 2 + 64)];
    struct ethhdr *reh = (struct ethhdr *)(arp_reply + 2);
    struct slirp_arphdr *rah = (struct slirp_arphdr *)(arp_reply + 2 + ETH_HLEN);
    int ar_op;
    struct gfwd_list *ex_ptr;

    if (!slirp->in_enabled) {
        return;
    }

    if (pkt_len < ETH_HLEN + sizeof(struct slirp_arphdr)) {
        return; /* packet too short */
    }

    ar_op = ntohs(ah->ar_op);
    switch (ar_op) {
    case ARPOP_REQUEST:
        if (ah->ar_tip == ah->ar_sip) {
            /* Gratuitous ARP */
            arp_table_add(slirp, ah->ar_sip, ah->ar_sha);
            return;
        }

        if ((ah->ar_tip & slirp->vnetwork_mask.s_addr) ==
            slirp->vnetwork_addr.s_addr) {
            if (ah->ar_tip == slirp->vnameserver_addr.s_addr ||
                ah->ar_tip == slirp->vhost_addr.s_addr)
                goto arp_ok;
            /* TODO: IPv6 */
            for (ex_ptr = slirp->guestfwd_list; ex_ptr;
                 ex_ptr = ex_ptr->ex_next) {
                if (ex_ptr->ex_addr.s_addr == ah->ar_tip)
                    goto arp_ok;
            }
            return;
        arp_ok:
            memset(arp_reply, 0, sizeof(arp_reply));

            arp_table_add(slirp, ah->ar_sip, ah->ar_sha);

            /* ARP request for alias/dns mac address */
            memcpy(reh->h_dest, pkt + ETH_ALEN, ETH_ALEN);
            memcpy(reh->h_source, special_ethaddr, ETH_ALEN - 4);
            memcpy(&reh->h_source[2], &ah->ar_tip, 4);
            reh->h_proto = htons(ETH_P_ARP);

            rah->ar_hrd = htons(1);
            rah->ar_pro = htons(ETH_P_IP);
            rah->ar_hln = ETH_ALEN;
            rah->ar_pln = 4;
            rah->ar_op = htons(ARPOP_REPLY);
            memcpy(rah->ar_sha, reh->h_source, ETH_ALEN);
            rah->ar_sip = ah->ar_tip;
            memcpy(rah->ar_tha, ah->ar_sha, ETH_ALEN);
            rah->ar_tip = ah->ar_sip;
            slirp_send_packet_all(slirp, arp_reply + 2, sizeof(arp_reply) - 2);
        }
        break;
    case ARPOP_REPLY:
        arp_table_add(slirp, ah->ar_sip, ah->ar_sha);
        break;
    default:
        break;
    }
}

void slirp_input(Slirp *slirp, const uint8_t *pkt, int pkt_len)
{
    /*
    Suppose no support VLAN (802.1Q)
    - Peamble and SFD is for physical layer
    | byte            | Offset (bytes) | Length (bytes) | Description     |
    | --------------- | -------------- | -------------- | -------------   |
    | Destination MAC | 0              | 6              | Source MAC      |
    | Source MAC      | 0              | 6              | Destination MAC |
    | EtherType       | 12             | 2              | Ethertype       |
    | Payload         | 14             | 42 - 1500      | Payload         |
    */
    struct  mbuf *m;
    int proto;

    if (pkt_len < ETH_HLEN)
        return;

    proto = (((uint16_t)pkt[12]) << 8) + pkt[13];

    switch (proto) {
    case ETH_P_ARP:
        arp_input(slirp, pkt, pkt_len);
        break;
    case ETH_P_IP:
    case ETH_P_IPV6:
        m = m_get(slirp);
        if (!m)
            return;
        /* Note: we add 2 to align the IP header on 8 bytes despite the ethernet
         * header, and add the margin for the tcpiphdr overhead  */
        if (M_FREEROOM(m) < pkt_len + TCPIPHDR_DELTA + 2) {
            m_inc(m, pkt_len + TCPIPHDR_DELTA + 2);
        }
        m->m_len = pkt_len + TCPIPHDR_DELTA + 2;
        memcpy(m->m_data + TCPIPHDR_DELTA + 2, pkt, pkt_len);

        m->m_data += TCPIPHDR_DELTA + 2 + ETH_HLEN;
        m->m_len -= TCPIPHDR_DELTA + 2 + ETH_HLEN;

        if (proto == ETH_P_IP) {
            ip_input(m);
        } else if (proto == ETH_P_IPV6) {
            ip6_input(m);
        }
        break;

    case ETH_P_NCSI:
        ncsi_input(slirp, pkt, pkt_len);
        break;

    default:
        break;
    }
    if (pkt_len < ETH_HLEN)
        return ;
    
    proto = (((uint16_t)pkt[12]) << 8) + pkt[13];

}