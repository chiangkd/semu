#pragma once

#include "slirp.h"

/*
 * Macros for type conversion
 * mtod(m,t) -	convert mbuf pointer to data pointer of correct type
 */
#define mtod(m, t) ((t)(m)->m_data)
/* Ref: https://man.freebsd.org/cgi/man.cgi?query=mbuf */
struct mbuf {
    /* XXX should union some of these! */
    /* header at beginning of each mbuf: */
    struct mbuf *m_next; /* Linked list of mbufs */
    struct mbuf *m_prev;
    struct mbuf *m_nextpkt; /* Next packet in queue/record */
    struct mbuf *m_prevpkt; /* Flags aren't used in the output queue */
    int m_flags; /* Misc flags */

    int m_size; /* Size of mbuf, from m_dat or m_ext */
    struct socket *m_so;

    char *m_data; /* Current location of data */
    int m_len; /* Amount of data in this mbuf, from m_data */

    Slirp *slirp;
    bool resolution_requested;
    uint64_t expiration_date;
    char *m_ext;
    /* start of dynamic buffer area, must be last element */
    char m_dat[];
};

#define M_EXT 0x01 /* m_ext points to more (malloced) data */
#define M_FREELIST 0x02 /* mbuf is on free list */
#define M_USEDLIST 0x04 /* XXX mbuf is on used list (for dtom()) */
#define M_DOFREE                                      \
    0x08 /* when m_free is called on the mbuf, free() \
          * it rather than putting it on the free list */

/*
 * Given a pointer into an mbuf, return the mbuf
 * XXX This is a kludge, I should eliminate the need for it
 * Fortunately, it's not used often
 */
struct mbuf *dtom(Slirp *, void *);