/* dplay.c: DirectPlay as seen from a machine with DirectPlay installed and no
 * network service provider: the game was not launched by a lobby, and the
 * connection list is empty. Single player never goes further than that.
 * A network implementation replaces the DirectPlay object here; the lobby
 * answers DPERR_NOTLOBBIED until something launches the game as a lobby
 * client. */
#include <stdlib.h>
#include <string.h>

#include "plat.h"
#include "dplay.h"

const GUID CLSID_DirectPlay = { 0xD1EB6D20, 0x8923, 0x11D0, { 0x9D, 0x97, 0x00, 0xA0, 0xC9, 0x0A, 0x43, 0xCB } };
const GUID CLSID_DirectPlayLobby = { 0x2FE8F810, 0xB2A5, 0x11D0, { 0xA7, 0x87, 0x00, 0x00, 0xF8, 0x03, 0xAB, 0xFC } };

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
    if (n <= 0)
        free(o);
    return (ULONG)(n < 0 ? 0 : n);
}

/* IDirectPlay4A */
static HRESULT dp_close(void *self) { (void)self; return DP_OK; }
static HRESULT dp_enum_connections(void *self, const GUID *app, void *cb, void *ctx, DWORD flags)
{
    (void)self; (void)app; (void)cb; (void)ctx; (void)flags;
    return DP_OK;           /* no service providers to offer */
}
static HRESULT dp_enum_sessions(void *self, DPSESSIONDESC2 *d, DWORD timeout, void *cb, void *ctx, DWORD flags)
{
    (void)self; (void)d; (void)timeout; (void)cb; (void)ctx; (void)flags;
    return DPERR_NOCONNECTION;
}
static HRESULT dp_receive(void *self, DPID *from, DPID *to, DWORD flags, void *data, DWORD *size)
{
    (void)self; (void)from; (void)to; (void)flags; (void)data; (void)size;
    return DPERR_NOCONNECTION;
}

/* IDirectPlayLobby3A */
static HRESULT dpl_get_connection_settings(void *self, DWORD app, void *data, DWORD *size)
{
    (void)self; (void)app; (void)data; (void)size;
    return DPERR_NOTLOBBIED;
}

static void *s_dp_vtbl[DP4_SLOTS], *s_dpl_vtbl[DPL3_SLOTS];

static void tables(void)
{
    int i;
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
    s_dp_vtbl[DP4_EnumConnections] = (void *)dp_enum_connections;
    s_dp_vtbl[DP4_EnumSessions] = (void *)dp_enum_sessions;
    s_dp_vtbl[DP4_Receive] = (void *)dp_receive;
    s_dpl_vtbl[DPL3_GetConnectionSettings] = (void *)dpl_get_connection_settings;
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
