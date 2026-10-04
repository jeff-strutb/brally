/* peersync.c: two scripted copies of the game on one virtual timeline.
 *
 * A script with a `peer` line runs a second copy as the other machine on
 * the network (script.c). Under BR_VCLOCK each copy keeps its own virtual
 * clock, and the scripts are timed in frames, so without help one copy can
 * reach a menu long before the other has created the session it looks for.
 * The brbox oracle solves this with conservative synchronisation, and so
 * does this: each copy tells the other its virtual time, and neither may run
 * more than LEAD_US ahead of the other's last published time. A copy that
 * has exited (or stopped answering) no longer holds the other back.
 *
 * The two talk over UDP on the loopback interface: the first copy opens a
 * port and hands it to the second in BR_PEERSYNC. Without BR_VCLOCK, or
 * without a peer, none of this runs. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plat.h"
#include "host.h"

#define LEAD_US   20000u            /* brbox's LATENCY_MS */
#define QUIET_NS  10000000000ull    /* no word for 10 s: the other is gone */

static host_sock *s_sock;
static host_addr  s_other;
static int        s_known, s_done, s_tried;
static uint64_t   s_other_us, s_sent_us, s_heard_ns;

#define LOOPBACK 0x7F000001u

static void hear(uint32_t wait_ms)
{
    unsigned char b[16];
    host_addr from;
    int n;
    while ((n = host_udp_recv(s_sock, &from, b, sizeof b, wait_ms)) == 9) {
        uint64_t t;
        memcpy(&t, b + 1, 8);
        if (!s_known) {
            s_other = from;
            s_known = 1;
        }
        if (b[0] == 'X')
            s_done = 1;
        else if (t > s_other_us)
            s_other_us = t;
        s_heard_ns = host_ticks_ns();
        wait_ms = 0;
    }
}

static void tell(char kind, uint64_t us)
{
    unsigned char b[9];
    if (!s_known)
        return;
    b[0] = (unsigned char)kind;
    memcpy(b + 1, &us, 8);
    host_udp_send(s_sock, &s_other, b, sizeof b);
}

static void bye(void)
{
    if (s_sock)
        tell('X', 0);
}

/* this copy is done: the other runs on without waiting for it */
void plat_peersync_end(void) { bye(); }

/* the first copy: a port for the peer to report to (0: none) */
uint16_t plat_peersync_listen(void)
{
    if (!plat_vclock() || s_sock)
        return 0;
    s_sock = host_udp_open(0, 0);
    if (!s_sock)
        return 0;
    s_tried = 1;
    s_heard_ns = host_ticks_ns();
    atexit(bye);
    PLOG("peersync: listening on %u\n", host_udp_port(s_sock));
    return host_udp_port(s_sock);
}

static void join(void)
{
    const char *e = getenv("BR_PEERSYNC");
    s_tried = 1;
    if (!e || !plat_vclock())
        return;
    s_sock = host_udp_open(0, 0);
    if (!s_sock)
        return;
    s_other.ip = LOOPBACK;
    s_other.port = (uint16_t)atoi(e);
    s_known = 1;
    s_heard_ns = host_ticks_ns();
    atexit(bye);
    PLOG("peersync: reporting to %u\n", s_other.port);
}

/* the main thread's virtual clock now reads us: publish it, and wait while
 * it is too far ahead of the other copy's */
void plat_peersync(uint64_t us)
{
    if (!s_tried)
        join();
    if (!s_sock)
        return;
    hear(0);
    if (us >= s_sent_us + 1000u || us < s_sent_us) {
        tell('T', us);
        s_sent_us = us;
    }
    while (!s_done && us > s_other_us + LEAD_US) {
        if (host_ticks_ns() - s_heard_ns > QUIET_NS) {
            PLOG("peersync: the other copy is silent; running on alone\n");
            s_done = 1;
            break;
        }
        tell('T', us);
        hear(20);
    }
}
