#include "slirp.h"
#include "mbuf.h"

/* Flags
 * - Reserved: 1bit
 * - Don't Fragment (DF): 1bit
 * - More Fragments (MF): 1bit
 */
#define IP_DF 0x4000 /* don't fragment flag */
#define IP_MF 0x2000 /* more fragments flag */
#define IP_OFFMASK 0x1fff /* mask for fragmenting bits */

static struct ip *ip_reass(Slirp *slirp, struct ip *ip, struct ipq *q)
{
    register struct mbuf *m = dtom(slirp, ip);
    

}

void ip_input(struct mbuf *m)
{
    Slirp *slirp = m->slirp;

    register struct iphdr *ip;
    int hlen;


    if (!slirp->in_enabled) {
        goto bad;
    }

    if (m->m_len < sizeof(struct iphdr)) {
        goto bad;
    }

    ip = mtod(m, struct iphdr *) ;

    /* For IPV4, this is always equal to 4 */
    if (ip->version != IPVERSION) {
        goto bad;
    }

    /* IPv4 header if variable in size  duo to the 
     * optional 14th field (Options). The IHL field 
     * contains the size of the IPV4 header. The minimum
     * value for this field is 5.
     */
    hlen = ip->ihl << 2;

    /* Check header length */
    if (hlen < sizeof(struct iphdr) || hlen > m->m_len) {
        goto bad;
    }

    /* Before sending a packet, the checksum is computed
     * as the 16-bit ones' complement of the ones' complement
     * sum of all 16-bit words in the header.
     */
    if (cksum(m, hlen)) {
        goto bad;
    }

    ip->tot_len = ntohs((uint16_t) ip->tot_len);
    if (ip->tot_len < hlen) {
        goto bad;
    }
    ip->id = ntohs((uint16_t) ip->id);
    ip->frag_off = ntohs((uint16_t) ip->frag_off);


    /* Check that the amount of data in the buffers 
     * is as at least nuch as the IP header.
     */
    if (m->m_len < ip->tot_len) {
        goto bad;
    }

    /* Trimming packets that are too long allows the
     * system to continue processing the packet, even
     * if its length is slightly inconsistent (this 
     * could be some insignificant trailing data of 
     * the packet).
     */
    if (m->m_len > ip->tot_len) {
        m_adj(m, ip->tot_len - m->m_len);
    }

    /* The router decrements the TTL field by one.
     * When the TTL field hits zero, the router discard
     * the packet and typically send a ICMP time exceed
     * message to the sender.
     */
    if (ip->ttl == 0) {
        // icmp_send_err();
        goto bad;
    }

    /* Handle fragmentation */

    /* If offset or OP_MF are set, must reassemble.
     * Otherwise, nothing need be done.
     *
     */
    if (ip->frag_off & ~IP_DF) {
        register struct ipq *q;
        struct qlink *l;
        /*
         * Look for queue of fragments
         * of this datagram.
         */
        for (l = slirp->ipq.ip_link.next; l != &slirp->ipq.ip_link; l = l->next) {
            // q = container_of(l, struct ipq, ip_link);
            if (ip->id == q->ipq_id  &&
                ip->saddr == q->ipq_src.s_addr &&
                ip->daddr == q->ipq_dst.s_addr &&
                ip->protocol = q->ipq_p)
                goto found;
        }
        q = NULL;
found:

        /*
         * Adjust ip_len to not reflect header,
         * set ip_mff
         */

        ip->tot_len -= hlen;
        if (ip->frag_off & IP_MF) {
            ip->tos |= 1;   // marked as having more fragments
        } else {
            ip->tos &= ~1;
        }

        ip->frag_off <<= 3; // Get fragment offset
        
        /*
         * If datagram marked as having more fragments
         * or if this is not the first fragment,
         * attempt reassembly; if it succeeds, proceed.
         */
        if (ip->tos & 1 || ip->frag_off) {
            ip = ip_reass(slirp, ip, q);
            if (ip == NULL)
                return;
            m = dtom(slirp, ip);
        } else if (q)
            ip_freef(slirp, q);

    }


bad:
    m_free(m);
}