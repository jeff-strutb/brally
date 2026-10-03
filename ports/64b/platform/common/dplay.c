/* dplay.c: DirectPlay 4 and DirectPlayLobby 3, over the network.
 *
 * What the game sees is a DirectX 6 machine with DirectPlay's four stock
 * service providers, never launched from a lobby (the model the brbox oracle
 * runs the original against, tools/brbox_com.py): it enumerates the
 * providers, opens or joins a session, creates its player, and exchanges
 * messages; the other machines' players arrive as DPSYS_CREATEPLAYERORGROUP
 * and leave as DPSYS_DESTROYPLAYERORGROUP. Which provider the player picks
 * makes no difference here: every one is this one transport.
 *
 * The transport is UDP on the local network (host.h). A host advertises its
 * session to a multicast group every quarter second; a joining machine sends
 * JOIN to the host, which answers with the session's players. From then on
 * the host is the hub: a member sends its announcements and messages to the
 * host, the host applies them and passes them to every other member. Several
 * copies on one machine see each other the same way.
 *
 * All state is one process-wide view, as each brbox box has one, guarded by
 * one lock: the game calls in from its main thread and its DirectPlay
 * thread, and a receive thread here applies what arrives and signals the
 * players' events. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plat.h"
#include "host.h"
#include "dplay.h"

const GUID CLSID_DirectPlay = { 0xD1EB6D20, 0x8923, 0x11D0, { 0x9D, 0x97, 0x00, 0xA0, 0xC9, 0x0A, 0x43, 0xCB } };
const GUID CLSID_DirectPlayLobby = { 0x2FE8F810, 0xB2A5, 0x11D0, { 0xA7, 0x87, 0x00, 0x00, 0xF8, 0x03, 0xAB, 0xFC } };

#define DPID_SERVERPLAYER       1
#define DPPLAYER_SERVERPLAYER   0x100
#define DPOPEN_JOIN             0x1
#define DPOPEN_CREATE           0x2
#define DPENUMPLAYERS_LOCAL     0x8
#define DPENUMPLAYERS_REMOTE    0x10
#define DPESC_TIMEDOUT          0x1
#define DPRECEIVE_TOPLAYER      0x2
#define DPRECEIVE_FROMPLAYER    0x4
#define DPRECEIVE_PEEK          0x8
#define DPPLAYERTYPE_PLAYER     1
#define DPSYS_CREATEPLAYERORGROUP  0x0003
#define DPSYS_DESTROYPLAYERORGROUP 0x0005

#define NET_GROUP  0xEFFF4252u          /* 239.255.66.82 */
#define NET_PORT   47624                /* DirectPlay's own; BR_NETPORT overrides */
#define MAXP       32
#define MAXS       16
#define MAXM       16
#define NAMELEN    64

/* ---- the view ---------------------------------------------------------------------- */
typedef struct player {
    DPID     id;
    int      used, local;
    DWORD    flags;
    HANDLE   event;
    char     sname[NAMELEN], lname[NAMELEN];
    GUID     session;               /* the session it was announced in */
} player;

typedef struct session {
    int            used;
    DPSESSIONDESC2 desc;            /* lpszSessionNameA / lpszPasswordA unused */
    char           name[NAMELEN];
    host_addr      host;            /* where its host listens */
} session;

typedef struct inmsg {
    struct inmsg *next;
    DPID          from, to;         /* from 0: a system message */
    int           sys_kind;
    player        sys_p;
    int           n;
    unsigned char data[1];
} inmsg;

static host_mutex *s_lock;
static int         s_up;
static host_sock  *s_mc, *s_uc;     /* the multicast group, our own port */
static int         s_open, s_host;
static session     s_sess;          /* the one we are in */
static player      s_pl[MAXP];
static session     s_known[MAXS];   /* sessions other machines host */
static host_addr   s_members[MAXM]; /* host: who has joined */
static inmsg      *s_inbox;
static unsigned    s_pidbase, s_next_pid;
static uint64_t    s_last_announce;

/* the discovery port: BR_NETPORT keeps a scripted pair to itself. The first
 * copy of a pair (its script has a `peer` line) picks one before the network
 * starts; the copy it spawns inherits it (script.c). */
static uint16_t net_port(void)
{
    const char *e = getenv("BR_NETPORT");
    if (!e && !getenv("BR_PEER") && getenv("BR_SCRIPT")) {
        FILE *f = fopen(getenv("BR_SCRIPT"), "r");
        char line[256];
        int pair = 0;
        while (f && fgets(line, sizeof line, f))
            if (!strncmp(line, "peer ", 5))
                pair = 1;
        if (f)
            fclose(f);
        if (pair) {
            static char env[32];
            snprintf(env, sizeof env, "BR_NETPORT=%u", 40000u + (unsigned)((host_ticks_ns() / 1000u) % 20000u));
            putenv(env);
            e = env + strlen("BR_NETPORT=");
        }
    }
    return e && atoi(e) > 0 ? (uint16_t)atoi(e) : NET_PORT;
}

static void lock(void)   { plat_vclock_import(); host_mutex_lock(s_lock); }
static void unlock(void) { host_mutex_unlock(s_lock); }

/* ---- packets ----------------------------------------------------------------------- */
enum { P_ANNOUNCE = 1, P_GONE, P_JOIN, P_PLAYER_ADD, P_PLAYER_DEL, P_MSG, P_LEAVE };

typedef struct wbuf { unsigned char b[1500]; int n; } wbuf;
static void w8(wbuf *w, unsigned v)  { if (w->n < (int)sizeof w->b) w->b[w->n++] = (unsigned char)v; }
static void w16(wbuf *w, unsigned v) { w8(w, v); w8(w, v >> 8); }
static void w32(wbuf *w, uint32_t v) { w16(w, v & 0xFFFF); w16(w, v >> 16); }
static void wraw(wbuf *w, const void *p, int n)
{
    if (n > 0 && w->n + n <= (int)sizeof w->b) {
        memcpy(w->b + w->n, p, (size_t)n);
        w->n += n;
    }
}
static void wstr(wbuf *w, const char *s) { int n = (int)strlen(s); w16(w, (unsigned)n); wraw(w, s, n); }

typedef struct rbuf { const unsigned char *b; int n, at, bad; } rbuf;
static unsigned r8(rbuf *r)  { if (r->at >= r->n) { r->bad = 1; return 0; } return r->b[r->at++]; }
static unsigned r16(rbuf *r) { unsigned a = r8(r); return a | r8(r) << 8; }
static uint32_t r32(rbuf *r) { uint32_t a = r16(r); return a | (uint32_t)r16(r) << 16; }
static void rraw(rbuf *r, void *p, int n)
{
    if (n < 0 || r->at + n > r->n) {
        r->bad = 1;
        memset(p, 0, n > 0 ? (size_t)n : 0);
        return;
    }
    memcpy(p, r->b + r->at, (size_t)n);
    r->at += n;
}
static void rstr(rbuf *r, char *out, int cap)
{
    int n = (int)r16(r), k = n < cap - 1 ? n : cap - 1;
    rraw(r, out, k);
    out[k > 0 ? k : 0] = 0;
    if (n > k)
        r->at += n - k;
}

static void head(wbuf *w, int type, const GUID *g)
{
    w->n = 0;
    wraw(w, "BRDP", 4);
    w8(w, 1);
    w8(w, (unsigned)type);
    w16(w, 0);
    if (g)
        wraw(w, g, 16);
    else {
        GUID z;
        memset(&z, 0, sizeof z);
        wraw(w, &z, 16);
    }
}

static void put_desc(wbuf *w, const DPSESSIONDESC2 *d, const char *name)
{
    w32(w, d->dwFlags);
    wraw(w, &d->guidApplication, 16);
    w32(w, d->dwMaxPlayers);
    w32(w, d->dwCurrentPlayers);
    w32(w, d->dwUser1);
    w32(w, d->dwUser2);
    w32(w, d->dwUser3);
    w32(w, d->dwUser4);
    wstr(w, name);
}

static void get_desc(rbuf *r, DPSESSIONDESC2 *d, char *name)
{
    d->dwFlags = r32(r);
    rraw(r, &d->guidApplication, 16);
    d->dwMaxPlayers = r32(r);
    d->dwCurrentPlayers = r32(r);
    d->dwUser1 = r32(r);
    d->dwUser2 = r32(r);
    d->dwUser3 = r32(r);
    d->dwUser4 = r32(r);
    rstr(r, name, NAMELEN);
}

static void put_player(wbuf *w, const player *p)
{
    w32(w, p->id);
    w32(w, p->flags);
    wstr(w, p->sname);
    wstr(w, p->lname);
}

static void send_to(const host_addr *a, const wbuf *w)
{
    if (s_uc && host_udp_send(s_uc, a, w->b, w->n) < 0) {
        static int warned;
        if (!warned++)
            PLOG("[%lu] dplay: cannot send to %u.%u.%u.%u:%u\n", (unsigned long)plat_time_ms(), a->ip >> 24, (a->ip >> 16) & 255,
                 (a->ip >> 8) & 255, a->ip & 255, a->port);
    }
}

static void send_group(const wbuf *w)
{
    host_addr g;
    g.ip = NET_GROUP;
    g.port = net_port();
    send_to(&g, w);
}

/* our announcement or message to the session: a member tells the host, the
 * host tells every member */
static void send_session(const wbuf *w, const host_addr *except)
{
    int i;
    if (!s_open)
        return;
    if (!s_host) {
        send_to(&s_sess.host, w);
        return;
    }
    for (i = 0; i < MAXM; i++)
        if (s_members[i].port &&
            !(except && s_members[i].ip == except->ip && s_members[i].port == except->port))
            send_to(&s_members[i], w);
}

/* ---- players and the inbox ------------------------------------------------------------ */
static player *find_player(DPID id)
{
    int i;
    for (i = 0; i < MAXP; i++)
        if (s_pl[i].used && s_pl[i].id == id)
            return &s_pl[i];
    return NULL;
}

static player *new_player(void)
{
    int i;
    for (i = 0; i < MAXP; i++)
        if (!s_pl[i].used) {
            memset(&s_pl[i], 0, sizeof s_pl[i]);
            s_pl[i].used = 1;
            return &s_pl[i];
        }
    return NULL;
}

/* the players of the session we are in: ours, and the others announced in it */
static int in_session(const player *p)
{
    return p->used && (p->local || (s_open && !memcmp(&p->session, &s_sess.desc.guidInstance, 16)));
}

static DWORD player_count(void)
{
    int i;
    DWORD n = 0;
    for (i = 0; i < MAXP; i++)
        n += in_session(&s_pl[i]) ? 1 : 0;
    return n;
}

static void inbox_add(inmsg *m)
{
    inmsg **pp = &s_inbox;
    while (*pp)
        pp = &(*pp)->next;
    m->next = NULL;
    *pp = m;
}

static void signal_all(void)
{
    int i;
    for (i = 0; i < MAXP; i++)
        if (s_pl[i].used && s_pl[i].local && s_pl[i].event)
            SetEvent(s_pl[i].event);
}

static void sys_msg(int kind, const player *p)
{
    inmsg *m = (inmsg *)calloc(1, sizeof *m);
    if (!m)
        return;
    m->sys_kind = kind;
    m->sys_p = *p;
    inbox_add(m);
    signal_all();
}

/* a message reaches each of our players it is addressed to, never its sender */
static void deliver(DPID from, DPID to, const void *data, int n)
{
    int i;
    if (getenv("BR_DPLOG"))
        PLOG("[%lu] dplay: deliver %x -> %x, %d bytes, %08x\n", (unsigned long)plat_time_ms(), (unsigned)from,
             (unsigned)to, n, n >= 4 ? *(const unsigned *)data : 0u);
    for (i = 0; i < MAXP; i++) {
        player *p = &s_pl[i];
        inmsg *m;
        if (!p->used || !p->local || p->id == from || (to != 0 && to != p->id))
            continue;
        m = (inmsg *)calloc(1, sizeof *m + (size_t)(n > 0 ? n : 0));
        if (!m)
            continue;
        m->from = from;
        m->to = p->id;
        m->n = n;
        if (n > 0)
            memcpy(m->data, data, (size_t)n);
        inbox_add(m);
        if (p->event)
            SetEvent(p->event);
    }
}

static void clear_inbox(void)
{
    while (s_inbox) {
        inmsg *m = s_inbox;
        s_inbox = m->next;
        free(m);
    }
}

/* ---- what arrives ----------------------------------------------------------------- */
static void member_add(const host_addr *a)
{
    int i, fr = -1;
    for (i = 0; i < MAXM; i++) {
        if (s_members[i].port && s_members[i].ip == a->ip && s_members[i].port == a->port)
            return;
        if (!s_members[i].port && fr < 0)
            fr = i;
    }
    if (fr >= 0)
        s_members[fr] = *a;
}

static void member_del(const host_addr *a)
{
    int i;
    for (i = 0; i < MAXM; i++)
        if (s_members[i].ip == a->ip && s_members[i].port == a->port)
            memset(&s_members[i], 0, sizeof s_members[i]);
}

static void announce_to(const host_addr *a);

static void receive(const unsigned char *b, int n, const host_addr *from)
{
    rbuf r;
    GUID g;
    int type, ours;
    r.b = b;
    r.n = n;
    r.at = 0;
    r.bad = 0;
    if (n < 24 || memcmp(b, "BRDP", 4) || b[4] != 1)
        return;
    type = b[5];
    r.at = 8;
    rraw(&r, &g, 16);
    ours = s_open && !memcmp(&g, &s_sess.desc.guidInstance, 16);

    switch (type) {
    case P_ANNOUNCE: {
        session tmp;
        int i, slot = -1;
        if (s_host && ours)
            break;                              /* our own advertisement */
        memset(&tmp, 0, sizeof tmp);
        tmp.desc.dwSize = sizeof tmp.desc;
        tmp.desc.guidInstance = g;
        tmp.host.ip = from->ip;
        tmp.host.port = (uint16_t)r16(&r);
        r16(&r);
        get_desc(&r, &tmp.desc, tmp.name);
        if (r.bad)
            break;
        tmp.used = 1;
        for (i = 0; i < MAXS; i++)
            if (s_known[i].used && !memcmp(&s_known[i].desc.guidInstance, &g, 16))
                slot = i;
        for (i = 0; slot < 0 && i < MAXS; i++)
            if (!s_known[i].used)
                slot = i;
        if (slot >= 0) {
            if (!s_known[slot].used)
                PLOG("[%lu] dplay: found session \"%s\" at %u.%u.%u.%u:%u\n", (unsigned long)plat_time_ms(), tmp.name, tmp.host.ip >> 24,
                     (tmp.host.ip >> 16) & 255, (tmp.host.ip >> 8) & 255, tmp.host.ip & 255, tmp.host.port);
            s_known[slot] = tmp;
        }
        if (ours && !s_host) {                 /* the host changed the description */
            s_sess.desc = tmp.desc;
            strcpy(s_sess.name, tmp.name);
        }
        break;
    }
    case P_GONE: {
        int i;
        for (i = 0; i < MAXS; i++)
            if (s_known[i].used && !memcmp(&s_known[i].desc.guidInstance, &g, 16))
                s_known[i].used = 0;
        break;
    }
    case P_JOIN:
        if (s_host && ours) {
            int i;
            wbuf w;
            member_add(from);
            announce_to(from);
            /* every player already in the session, as each was announced */
            for (i = 0; i < MAXP; i++)
                if (in_session(&s_pl[i])) {
                    head(&w, P_PLAYER_ADD, &s_sess.desc.guidInstance);
                    put_player(&w, &s_pl[i]);
                    send_to(from, &w);
                }
            PLOG("[%lu] dplay: a player joined from %u.%u.%u.%u:%u\n", (unsigned long)plat_time_ms(), from->ip >> 24, (from->ip >> 16) & 255,
                 (from->ip >> 8) & 255, from->ip & 255, from->port);
        }
        break;
    case P_LEAVE:
        if (s_host && ours)
            member_del(from);
        break;
    case P_PLAYER_ADD: {
        player p, *q;
        memset(&p, 0, sizeof p);
        p.id = r32(&r);
        p.flags = r32(&r);
        rstr(&r, p.sname, NAMELEN);
        rstr(&r, p.lname, NAMELEN);
        if (getenv("BR_DPLOG"))
            PLOG("[%lu] dplay: player %x \"%s\" %s\n", (unsigned long)plat_time_ms(), (unsigned)p.id, p.sname,
                 r.bad ? "bad" : !ours ? "not ours" : find_player(p.id) ? "known" : "added");
        if (r.bad || !ours || (q = find_player(p.id)) != NULL)
            break;
        if ((q = new_player()) == NULL)
            break;
        p.used = 1;
        p.session = g;
        *q = p;
        sys_msg(DPSYS_CREATEPLAYERORGROUP, q);
        if (s_host) {
            wbuf w;
            head(&w, P_PLAYER_ADD, &g);
            put_player(&w, q);
            send_session(&w, from);
        }
        break;
    }
    case P_PLAYER_DEL: {
        DPID id = r32(&r);
        player *q = find_player(id);
        if (r.bad || !ours || !q || q->local)
            break;
        sys_msg(DPSYS_DESTROYPLAYERORGROUP, q);
        q->used = 0;
        if (s_host) {
            wbuf w;
            head(&w, P_PLAYER_DEL, &g);
            w32(&w, id);
            send_session(&w, from);
        }
        break;
    }
    case P_MSG: {
        DPID f = r32(&r), t = r32(&r);
        int len = (int)r32(&r);
        if (r.bad || !ours || len < 0 || r.at + len > r.n)
            break;
        deliver(f, t, b + r.at, len);
        if (s_host) {
            /* passed on unchanged to the other members */
            wbuf w;
            w.n = n;
            memcpy(w.b, b, (size_t)n);
            send_session(&w, from);
        }
        break;
    }
    }
}

static void announce_to(const host_addr *a)
{
    wbuf w;
    if (!s_open || !s_host)
        return;
    s_sess.desc.dwCurrentPlayers = player_count();
    head(&w, P_ANNOUNCE, &s_sess.desc.guidInstance);
    w16(&w, s_uc ? host_udp_port(s_uc) : 0);
    w16(&w, 0);
    put_desc(&w, &s_sess.desc, s_sess.name);
    if (a)
        send_to(a, &w);
    else
        send_group(&w);
}

static void *net_thread(void *arg)
{
    unsigned char buf[1500];
    (void)arg;
    for (;;) {
        host_addr from;
        int n;
        if (s_mc && (n = host_udp_recv(s_mc, &from, buf, sizeof buf, 0)) > 0) {
            lock();
            receive(buf, n, &from);
            unlock();
            continue;
        }
        if (s_uc && (n = host_udp_recv(s_uc, &from, buf, sizeof buf, 10)) > 0) {
            lock();
            receive(buf, n, &from);
            unlock();
        }
        lock();
        if (s_open && s_host && host_ticks_ns() - s_last_announce > 250000000ull) {
            s_last_announce = host_ticks_ns();
            announce_to(NULL);
        }
        unlock();
    }
    return NULL;
}

static void net_up(void)
{
    if (s_up)
        return;
    s_up = 1;
    if (!s_lock)
        s_lock = host_mutex_new();
    s_mc = host_udp_open(net_port(), NET_GROUP);
    s_uc = host_udp_open(0, 0);
    s_pidbase = (unsigned)((host_ticks_ns() / 1000u) ^ (uintptr_t)&s_pidbase) & 0xFFFFu;
    if (s_pidbase == 0)
        s_pidbase = 1;
    PLOG("[%lu] dplay: network %s (port %u, discovery %u)\n", (unsigned long)plat_time_ms(), s_mc && s_uc ? "up" : "unavailable", s_uc ? host_udp_port(s_uc) : 0, net_port());
    if (s_mc || s_uc)
        host_thread_start(net_thread, NULL);
}

/* ---- the objects -------------------------------------------------------------------- */
typedef struct dpobj {
    void *const *vtbl;
    int refs;
} dpobj;

/* a method the game reaches that this machine cannot do */
static HRESULT unsupported(void *self) { (void)self; return DPERR_UNSUPPORTED; }

static HRESULT query(void *self, REFIID iid, void **out)
{
    (void)iid;
    ((dpobj *)self)->refs++;
    *out = self;
    return DP_OK;
}
static ULONG addref(void *self) { return (ULONG)++((dpobj *)self)->refs; }
static ULONG release(void *self)
{
    dpobj *o = (dpobj *)self;
    int n = --o->refs;
    return (ULONG)(n < 0 ? 0 : n);   /* left allocated: the game may still hold it */
}

/* a buffer the caller sized: DPERR_BUFFERTOOSMALL with the size when short */
static HRESULT give(void *dst, DWORD *size, const void *src, DWORD n)
{
    DWORD have = size ? *size : 0;
    if (size)
        *size = n;
    if (!dst || have < n)
        return DPERR_BUFFERTOOSMALL;
    memcpy(dst, src, n);
    return DP_OK;
}

/* DPNAME + its strings, laid out for a buffer at base */
static DWORD name_blob(unsigned char *out, const player *p, unsigned char *base)
{
    DPNAME nm;
    size_t a = strlen(p->sname) + 1, b = strlen(p->lname) + 1;
    memset(&nm, 0, sizeof nm);
    nm.dwSize = sizeof nm;
    nm.lpszShortNameA = (char *)base + sizeof nm;
    nm.lpszLongNameA = (char *)base + sizeof nm + a;
    if (out) {
        memcpy(out, &nm, sizeof nm);
        memcpy(out + sizeof nm, p->sname, a);
        memcpy(out + sizeof nm + a, p->lname, b);
    }
    return (DWORD)(sizeof nm + a + b);
}

/* IDirectPlay4A */
static HRESULT dp_close(void *self)
{
    int i;
    wbuf w;
    (void)self;
    lock();
    if (s_open) {
        for (i = 0; i < MAXP; i++)
            if (s_pl[i].used && s_pl[i].local) {
                head(&w, P_PLAYER_DEL, &s_sess.desc.guidInstance);
                w32(&w, s_pl[i].id);
                send_session(&w, NULL);
            }
        head(&w, s_host ? P_GONE : P_LEAVE, &s_sess.desc.guidInstance);
        if (s_host)
            send_group(&w);
        else
            send_session(&w, NULL);
    }
    s_open = s_host = 0;
    memset(s_pl, 0, sizeof s_pl);
    memset(s_members, 0, sizeof s_members);
    clear_inbox();
    unlock();
    return DP_OK;
}

typedef BOOL (WINAPI *conn_cb)(const GUID *sp, void *conn, DWORD size, const DPNAME *name, DWORD flags, void *ctx);

static HRESULT dp_enum_connections(void *self, const GUID *app, void *cb, void *ctx, DWORD flags)
{
    static const struct { GUID g; const char *name; } sp[4] = {
        { { 0x685BC400, 0x9D2C, 0x11CF, { 0xA9, 0xCD, 0x00, 0xAA, 0x00, 0x68, 0x86, 0xE3 } }, "IPX Connection For DirectPlay" },
        { { 0x36E95EE0, 0x8577, 0x11CF, { 0x96, 0x0C, 0x00, 0x80, 0xC7, 0x53, 0x4E, 0x82 } }, "Internet TCP/IP Connection For DirectPlay" },
        { { 0x44EAA760, 0xCB68, 0x11CF, { 0x9C, 0x4E, 0x00, 0xA0, 0xC9, 0x05, 0x42, 0x5E } }, "Modem Connection For DirectPlay" },
        { { 0x0F1D6860, 0x88D9, 0x11CF, { 0x9C, 0x4E, 0x00, 0xA0, 0xC9, 0x05, 0x42, 0x5E } }, "Serial Connection For DirectPlay" },
    };
    /* DPAID_ServiceProvider {07D916C0-E0AF-11CF-9C4E-00A0C905425E} */
    static const GUID aid = { 0x07D916C0, 0xE0AF, 0x11CF, { 0x9C, 0x4E, 0x00, 0xA0, 0xC9, 0x05, 0x42, 0x5E } };
    /* the game copies 200 bytes from the name: room for them */
    static char names[4][256];
    int i;
    (void)self; (void)app; (void)flags;
    net_up();
    for (i = 0; i < 4; i++) {
        unsigned char conn[36];
        DWORD sz = 16;
        DPNAME nm;
        snprintf(names[i], sizeof names[i], "%s", sp[i].name);
        memcpy(conn, &aid, 16);
        memcpy(conn + 16, &sz, 4);
        memcpy(conn + 20, &sp[i].g, 16);
        memset(&nm, 0, sizeof nm);
        nm.dwSize = sizeof nm;
        nm.lpszShortNameA = names[i];
        nm.lpszLongNameA = names[i];
        if (!((conn_cb)cb)(&sp[i].g, conn, sizeof conn, &nm, 1, ctx))   /* DPCONNECTION_DIRECTPLAY */
            break;
    }
    return DP_OK;
}

static HRESULT dp_initialize_connection(void *self, void *conn, DWORD flags)
{
    (void)self; (void)conn; (void)flags;
    net_up();
    return DP_OK;
}

typedef BOOL (WINAPI *sess_cb)(const DPSESSIONDESC2 *d, DWORD *timeout, DWORD flags, void *ctx);

static HRESULT dp_enum_sessions(void *self, DPSESSIONDESC2 *want, DWORD timeout, void *cb, void *ctx, DWORD flags)
{
    session list[MAXS];
    int i, n = 0;
    DWORD to = timeout;
    (void)self; (void)want; (void)flags;
    net_up();
    lock();
    for (i = 0; i < MAXS; i++)
        if (s_known[i].used)
            list[n++] = s_known[i];
    unlock();
    for (i = 0; i < n; i++) {
        DPSESSIONDESC2 d = list[i].desc;
        d.dwSize = sizeof d;
        d.lpszSessionNameA = list[i].name;
        d.lpszPasswordA = NULL;
        to = timeout;
        if (!((sess_cb)cb)(&d, &to, 0, ctx))
            return DP_OK;
    }
    to = timeout;
    ((sess_cb)cb)(NULL, &to, DPESC_TIMEDOUT, ctx);
    PLOG("[%lu] dplay: EnumSessions lists %d\n", (unsigned long)plat_time_ms(), n);
    return DP_OK;
}

static HRESULT dp_open(void *self, DPSESSIONDESC2 *d, DWORD flags, void *sec, void *cred)
{
    int i;
    HRESULT hr = DPERR_NOSESSIONS;
    (void)self; (void)sec; (void)cred;
    net_up();
    PLOG("[%lu] dplay: Open flags %x\n", (unsigned long)plat_time_ms(), (unsigned)flags);
    lock();
    if (flags & DPOPEN_CREATE) {
        uint64_t t = host_ticks_ns();
        memset(&s_sess, 0, sizeof s_sess);
        s_sess.desc = *d;
        s_sess.desc.dwSize = sizeof s_sess.desc;
        s_sess.desc.lpszSessionNameA = s_sess.desc.lpszPasswordA = NULL;
        /* guidInstance: DirectPlay makes one up */
        memcpy(&s_sess.desc.guidInstance, &t, 8);
        memcpy((char *)&s_sess.desc.guidInstance + 8, &s_pidbase, 4);
        memcpy((char *)&s_sess.desc.guidInstance + 12, "BRAL", 4);
        snprintf(s_sess.name, sizeof s_sess.name, "%s", d->lpszSessionNameA ? d->lpszSessionNameA : "");
        s_open = s_host = 1;
        s_last_announce = host_ticks_ns();
        announce_to(NULL);
        PLOG("[%lu] dplay: hosting \"%s\"\n", (unsigned long)plat_time_ms(), s_sess.name);
        hr = DP_OK;
    } else {
        for (i = 0; i < MAXS; i++)
            if (s_known[i].used && !memcmp(&s_known[i].desc.guidInstance, &d->guidInstance, 16)) {
                wbuf w;
                s_sess = s_known[i];
                s_open = 1;
                s_host = 0;
                head(&w, P_JOIN, &s_sess.desc.guidInstance);
                send_session(&w, NULL);
                PLOG("[%lu] dplay: joined \"%s\"\n", (unsigned long)plat_time_ms(), s_sess.name);
                hr = DP_OK;
                break;
            }
    }
    unlock();
    return hr;
}

static HRESULT dp_open3(void *self, DPSESSIONDESC2 *d, DWORD flags)
{
    return dp_open(self, d, flags, NULL, NULL);
}

static HRESULT dp_create_player(void *self, DPID *id, const DPNAME *name, HANDLE ev, void *data, DWORD n, DWORD flags)
{
    player *p;
    (void)self; (void)data; (void)n;
    lock();
    p = new_player();
    if (!p) {
        unlock();
        return DPERR_UNSUPPORTED;
    }
    if (flags & DPPLAYER_SERVERPLAYER)
        p->id = DPID_SERVERPLAYER;
    else
        p->id = (DPID)(s_pidbase << 8) + 0x10 + s_next_pid++;
    p->local = 1;
    p->flags = flags;
    p->event = ev;
    if (name) {
        snprintf(p->sname, sizeof p->sname, "%s", name->lpszShortNameA ? name->lpszShortNameA : "");
        snprintf(p->lname, sizeof p->lname, "%s", name->lpszLongNameA ? name->lpszLongNameA : "");
    }
    if (s_open) {
        wbuf w;
        p->session = s_sess.desc.guidInstance;
        head(&w, P_PLAYER_ADD, &s_sess.desc.guidInstance);
        put_player(&w, p);
        send_session(&w, NULL);
    }
    *id = p->id;
    unlock();
    return DP_OK;
}

static HRESULT dp_destroy_player(void *self, DPID id)
{
    player *p;
    (void)self;
    lock();
    p = find_player(id);
    if (p && p->local) {
        if (s_open) {
            wbuf w;
            head(&w, P_PLAYER_DEL, &s_sess.desc.guidInstance);
            w32(&w, id);
            send_session(&w, NULL);
        }
        p->used = 0;
    }
    unlock();
    return DP_OK;
}

typedef BOOL (WINAPI *player_cb)(DPID id, DWORD type, const DPNAME *name, DWORD flags, void *ctx);

static HRESULT dp_enum_players(void *self, const GUID *inst, void *cb, void *ctx, DWORD flags)
{
    player list[MAXP];
    int i, n = 0;
    (void)self; (void)inst;
    lock();
    if (!s_open) {
        unlock();
        return DPERR_NOSESSIONS;
    }
    for (i = 0; i < MAXP; i++)
        if (in_session(&s_pl[i]))
            list[n++] = s_pl[i];
    unlock();
    /* in DPID order, as the oracle's sorted walk */
    for (i = 1; i < n; i++) {
        player t = list[i];
        int j = i;
        while (j > 0 && list[j - 1].id > t.id) {
            list[j] = list[j - 1];
            j--;
        }
        list[j] = t;
    }
    for (i = 0; i < n; i++) {
        unsigned char blob[sizeof(DPNAME) + 2 * NAMELEN];
        DWORD fl;
        if (list[i].id == DPID_SERVERPLAYER && !(flags & DPPLAYER_SERVERPLAYER))
            continue;
        name_blob(blob, &list[i], blob);
        fl = (list[i].local ? DPENUMPLAYERS_LOCAL : DPENUMPLAYERS_REMOTE) |
             (list[i].id == DPID_SERVERPLAYER ? DPPLAYER_SERVERPLAYER : 0);
        if (!((player_cb)cb)(list[i].id, DPPLAYERTYPE_PLAYER, (const DPNAME *)blob, fl, ctx))
            break;
    }
    return DP_OK;
}

static HRESULT dp_get_session_desc(void *self, void *data, DWORD *size)
{
    unsigned char buf[sizeof(DPSESSIONDESC2) + NAMELEN];
    DPSESSIONDESC2 d;
    DWORD n;
    HRESULT hr;
    (void)self;
    lock();
    if (!s_open) {
        unlock();
        return DPERR_NOCONNECTION;
    }
    d = s_sess.desc;
    d.dwSize = sizeof d;
    d.dwCurrentPlayers = player_count();
    d.lpszSessionNameA = data ? (char *)data + sizeof d : NULL;
    d.lpszPasswordA = NULL;
    if (getenv("BR_DPLOG"))
        PLOG("[%lu] dplay: GetSessionDesc user %x %x %x %x flags %x\n", (unsigned long)plat_time_ms(),
             (unsigned)d.dwUser1, (unsigned)d.dwUser2, (unsigned)d.dwUser3, (unsigned)d.dwUser4, (unsigned)d.dwFlags);
    n = (DWORD)(sizeof d + strlen(s_sess.name) + 1);
    memcpy(buf, &d, sizeof d);
    memcpy(buf + sizeof d, s_sess.name, strlen(s_sess.name) + 1);
    unlock();
    hr = give(data, size, buf, n);
    return hr;
}

static HRESULT dp_set_session_desc(void *self, const DPSESSIONDESC2 *d, DWORD flags)
{
    GUID g;
    (void)self; (void)flags;
    lock();
    if (!s_open) {
        unlock();
        return DPERR_NOCONNECTION;
    }
    g = s_sess.desc.guidInstance;
    s_sess.desc = *d;
    s_sess.desc.guidInstance = g;
    s_sess.desc.lpszSessionNameA = s_sess.desc.lpszPasswordA = NULL;
    if (d->lpszSessionNameA && d->lpszSessionNameA[0])
        snprintf(s_sess.name, sizeof s_sess.name, "%s", d->lpszSessionNameA);
    if (s_host)
        announce_to(NULL);
    unlock();
    return DP_OK;
}

static HRESULT dp_get_player_name(void *self, DPID id, void *data, DWORD *size)
{
    unsigned char blob[sizeof(DPNAME) + 2 * NAMELEN];
    player *p;
    DWORD n;
    (void)self;
    lock();
    p = find_player(id);
    if (!p || !in_session(p)) {
        unlock();
        return DPERR_INVALIDPLAYER;
    }
    n = name_blob(blob, p, (unsigned char *)data);
    unlock();
    return give(data, size, blob, n);
}

static HRESULT dp_get_player_data(void *self, DPID id, void *data, DWORD *size, DWORD flags)
{
    player *p;
    (void)self; (void)data; (void)flags;
    lock();
    p = find_player(id);
    unlock();
    if (!p)
        return DPERR_INVALIDPLAYER;
    if (size)
        *size = 0;                  /* nobody sets player data */
    return DP_OK;
}

static HRESULT dp_player_query(void *self, DPID id)
{
    player *p;
    (void)self;
    lock();
    p = find_player(id);
    unlock();
    return p ? DP_OK : DPERR_INVALIDPLAYER;
}

/* the DPMSG_CREATE/DESTROYPLAYERORGROUP record of m, for a buffer at base */
static DWORD sys_body(unsigned char *out, const inmsg *m, unsigned char *base)
{
    unsigned char nb[sizeof(DPNAME) + 2 * NAMELEN];
    DWORD nn, hn;
    if (m->sys_kind == DPSYS_CREATEPLAYERORGROUP) {
        struct {
            DWORD dwType, dwPlayerType; DPID dpId; DWORD dwCurrentPlayers;
            void *lpData; DWORD dwDataSize; DPNAME dpnName; DPID dpIdParent; DWORD dwFlags;
        } c;
        memset(&c, 0, sizeof c);
        hn = sizeof c;
        nn = name_blob(nb, &m->sys_p, base + hn);
        c.dwType = DPSYS_CREATEPLAYERORGROUP;
        c.dwPlayerType = DPPLAYERTYPE_PLAYER;
        c.dpId = m->sys_p.id;
        c.dwCurrentPlayers = player_count();
        memcpy(&c.dpnName, nb, sizeof c.dpnName);
        if (out) {
            memcpy(out, &c, hn);
            memcpy(out + hn, nb + sizeof(DPNAME), nn - sizeof(DPNAME));
        }
    } else {
        struct {
            DWORD dwType, dwPlayerType; DPID dpId;
            void *lpLocalData; DWORD dwLocalDataSize; void *lpRemoteData; DWORD dwRemoteDataSize;
            DPNAME dpnName; DPID dpIdParent; DWORD dwFlags;
        } d;
        memset(&d, 0, sizeof d);
        hn = sizeof d;
        nn = name_blob(nb, &m->sys_p, base + hn);
        d.dwType = DPSYS_DESTROYPLAYERORGROUP;
        d.dwPlayerType = DPPLAYERTYPE_PLAYER;
        d.dpId = m->sys_p.id;
        memcpy(&d.dpnName, nb, sizeof d.dpnName);
        if (out) {
            memcpy(out, &d, hn);
            memcpy(out + hn, nb + sizeof(DPNAME), nn - sizeof(DPNAME));
        }
    }
    return hn + nn - (DWORD)sizeof(DPNAME);
}

static HRESULT dp_receive(void *self, DPID *from, DPID *to, DWORD flags, void *data, DWORD *size)
{
    inmsg **pp, *m;
    DPID want_to, want_from;
    (void)self;
    lock();
    want_to = (flags & DPRECEIVE_TOPLAYER) && to ? *to : 0;
    want_from = (flags & DPRECEIVE_FROMPLAYER) && from ? *from : 0;
    for (pp = &s_inbox; (m = *pp) != NULL; pp = &m->next) {
        DWORD n, have;
        if ((flags & DPRECEIVE_TOPLAYER) && m->to != want_to && m->from != 0)
            continue;
        if ((flags & DPRECEIVE_FROMPLAYER) && m->from != want_from)
            continue;
        n = m->sys_kind ? sys_body(NULL, m, (unsigned char *)data) : (DWORD)m->n;
        have = size ? *size : 0;
        if (size)
            *size = n;
        if (!data || have < n) {
            unlock();
            return DPERR_BUFFERTOOSMALL;
        }
        if (m->sys_kind)
            sys_body((unsigned char *)data, m, (unsigned char *)data);
        else if (n)
            memcpy(data, m->data, n);
        if (from)
            *from = m->from;
        if (to)
            *to = m->to;
        if (getenv("BR_DPLOG"))
            PLOG("[%lu] dplay: receive %x -> %x, %u bytes, %08x\n", (unsigned long)plat_time_ms(), (unsigned)m->from,
                 (unsigned)m->to, (unsigned)n, n >= 4 ? *(const unsigned *)data : 0u);
        if (!(flags & DPRECEIVE_PEEK)) {
            *pp = m->next;
            free(m);
        }
        unlock();
        return DP_OK;
    }
    unlock();
    return DPERR_NOMESSAGES;
}

static HRESULT dp_get_message_count(void *self, DPID id, DWORD *count)
{
    inmsg *m;
    DWORD n = 0;
    (void)self;
    lock();
    for (m = s_inbox; m; m = m->next)
        n += (m->to == id || m->from == 0) ? 1 : 0;
    unlock();
    *count = n;
    return DP_OK;
}

static HRESULT dp_send(void *self, DPID from, DPID to, DWORD flags, const void *data, DWORD n)
{
    (void)self; (void)flags;
    lock();
    if (getenv("BR_DPLOG"))
        PLOG("[%lu] dplay: send %x -> %x, %u bytes, %08x %08x%s\n", (unsigned long)plat_time_ms(), (unsigned)from,
             (unsigned)to, (unsigned)n, n >= 4 ? *(const unsigned *)data : 0u, n >= 8 ? ((const unsigned *)data)[1] : 0u, s_open ? "" : " (no session)");
    if (s_open) {
        wbuf w;
        head(&w, P_MSG, &s_sess.desc.guidInstance);
        w32(&w, from);
        w32(&w, to);
        w32(&w, n);
        wraw(&w, data, (int)n);
        send_session(&w, NULL);
        /* this machine's other players hear it too, as DirectPlay does */
        deliver(from, to, data, (int)n);
    }
    unlock();
    return DP_OK;
}

static HRESULT dp_send_ex(void *self, DPID from, DPID to, DWORD flags, const void *data, DWORD n,
                          DWORD prio, DWORD timeout, void *ctx, DWORD *msgid)
{
    (void)prio; (void)timeout; (void)ctx;
    if (msgid)
        *msgid = 0;
    return dp_send(self, from, to, flags, data, n);
}

/* IDirectPlayLobby3A */
static HRESULT dpl_get_connection_settings(void *self, DWORD app, void *data, DWORD *size)
{
    (void)self; (void)app; (void)data; (void)size;
    return DPERR_NOTLOBBIED;
}

/* each element as a DPADDRESS chunk {GUID; DWORD size} then its data */
static HRESULT dpl_create_compound_address(void *self, const DPCOMPOUNDADDRESSELEMENT *e, DWORD count,
                                           void *addr, DWORD *size)
{
    DWORD n = 0, i, have = size ? *size : 0;
    unsigned char *o = (unsigned char *)addr;
    (void)self;
    for (i = 0; i < count; i++)
        n += 20 + e[i].dwDataSize;
    if (size)
        *size = n;
    if (!addr || have < n)
        return DPERR_BUFFERTOOSMALL;
    for (i = 0; i < count; i++) {
        memcpy(o, &e[i].guidDataType, 16);
        memcpy(o + 16, &e[i].dwDataSize, 4);
        if (e[i].dwDataSize)
            memcpy(o + 20, e[i].lpData, e[i].dwDataSize);
        o += 20 + e[i].dwDataSize;
    }
    return DP_OK;
}

static void *s_dp_vtbl[DP4_SLOTS], *s_dpl_vtbl[DPL3_SLOTS];

static void tables(void)
{
    int i;
    if (!s_lock)
        s_lock = host_mutex_new();
    if (s_dp_vtbl[0])
        return;
    for (i = 0; i < DP4_SLOTS; i++)
        s_dp_vtbl[i] = (void *)unsupported;
    for (i = 0; i < DPL3_SLOTS; i++)
        s_dpl_vtbl[i] = (void *)unsupported;
    s_dp_vtbl[DP4_QueryInterface] = s_dpl_vtbl[DPL3_QueryInterface] = (void *)query;
    s_dp_vtbl[DP4_AddRef] = s_dpl_vtbl[DPL3_AddRef] = (void *)addref;
    s_dp_vtbl[DP4_Release] = s_dpl_vtbl[DPL3_Release] = (void *)release;
    s_dp_vtbl[DP4_Close] = (void *)dp_close;
    s_dp_vtbl[DP4_CreatePlayer] = (void *)dp_create_player;
    s_dp_vtbl[DP4_DestroyPlayer] = (void *)dp_destroy_player;
    s_dp_vtbl[DP4_EnumPlayers] = (void *)dp_enum_players;
    s_dp_vtbl[DP4_EnumSessions] = (void *)dp_enum_sessions;
    s_dp_vtbl[DP4_GetMessageCount] = (void *)dp_get_message_count;
    s_dp_vtbl[DP4_GetPlayerAddress] = (void *)dp_player_query;
    s_dp_vtbl[DP4_GetPlayerCaps] = (void *)dp_player_query;
    s_dp_vtbl[DP4_GetPlayerData] = (void *)dp_get_player_data;
    s_dp_vtbl[DP4_GetPlayerName] = (void *)dp_get_player_name;
    s_dp_vtbl[DP4_GetSessionDesc] = (void *)dp_get_session_desc;
    s_dp_vtbl[DP4_Open] = (void *)dp_open3;
    s_dp_vtbl[DP4_Receive] = (void *)dp_receive;
    s_dp_vtbl[DP4_Send] = (void *)dp_send;
    s_dp_vtbl[DP4_SetSessionDesc] = (void *)dp_set_session_desc;
    s_dp_vtbl[DP4_EnumConnections] = (void *)dp_enum_connections;
    s_dp_vtbl[DP4_InitializeConnection] = (void *)dp_initialize_connection;
    s_dp_vtbl[DP4_SecureOpen] = (void *)dp_open;
    s_dp_vtbl[DP4_GetPlayerAccount] = (void *)dp_player_query;
    s_dp_vtbl[DP4_GetPlayerFlags] = (void *)dp_player_query;
    s_dp_vtbl[DP4_SendEx] = (void *)dp_send_ex;
    s_dpl_vtbl[DPL3_GetConnectionSettings] = (void *)dpl_get_connection_settings;
    s_dpl_vtbl[DPL3_CreateCompoundAddress] = (void *)dpl_create_compound_address;
}

static HRESULT make(void *const *vtbl, LPVOID *out)
{
    dpobj *o = (dpobj *)calloc(1, sizeof *o);
    if (!o)
        return E_OUTOFMEMORY;
    o->vtbl = vtbl;
    o->refs = 1;
    *out = o;
    return DP_OK;
}

/* CoCreateInstance for the DirectPlay classes; E_FAIL (not handled) else */
HRESULT plat_dplay_create(REFCLSID clsid, LPVOID *out)
{
    tables();
    if (!memcmp(clsid, &CLSID_DirectPlay, sizeof(GUID)))
        return make(s_dp_vtbl, out);
    if (!memcmp(clsid, &CLSID_DirectPlayLobby, sizeof(GUID)))
        return make(s_dpl_vtbl, out);
    return E_FAIL;
}

HRESULT WINAPI DirectPlayLobbyCreateA(LPGUID g, LPVOID *out, LPUNKNOWN outer, LPVOID data, DWORD n)
{
    (void)g; (void)outer; (void)data; (void)n;
    tables();
    return make(s_dpl_vtbl, out);
}
