/* br_net.h: the game's network play (platform/common/br_net.c).
 *
 * The game opens a network object, lists the ways to connect, opens or
 * joins a session, makes its player and trades messages; the other
 * machines' players arrive and leave as system messages. Its records keep
 * the layout it was written for (DirectPlay's, with pointers at the host's
 * width). Results are 0 or one of BR_NET_* below.
 *
 * Force-included into the core. */
#ifndef BR_NET_H
#define BR_NET_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BR_NET_OK              0
#define BR_NET_BUFFERTOOSMALL  ((int32_t)0x8877001E)
#define BR_NET_INVALIDOBJECT   ((int32_t)0x88770082)
#define BR_NET_INVALIDPLAYER   ((int32_t)0x88770096)
#define BR_NET_NOCONNECTION    ((int32_t)0x887700AA)
#define BR_NET_NOMESSAGES      ((int32_t)0x887700BE)
#define BR_NET_NOSESSIONS      ((int32_t)0x887700D2)
#define BR_NET_NOTLOBBIED      ((int32_t)0x8877042E)
#define BR_NET_UNSUPPORTED     ((int32_t)0x80004001)

typedef struct BrNetGuid { uint32_t a; uint16_t b, c; uint8_t d[8]; } BrNetGuid;
typedef uint32_t BrNetId;

typedef struct BrNetName {
    uint32_t dwSize;
    uint32_t dwFlags;
    char *lpszShortNameA;
    char *lpszLongNameA;
} BrNetName;

typedef struct BrNetSessionDesc {
    uint32_t dwSize;
    uint32_t dwFlags;
    BrNetGuid guidInstance;
    BrNetGuid guidApplication;
    uint32_t dwMaxPlayers;
    uint32_t dwCurrentPlayers;
    char *lpszSessionNameA;
    char *lpszPasswordA;
    uint32_t dwReserved1;
    uint32_t dwReserved2;
    uint32_t dwUser1;
    uint32_t dwUser2;
    uint32_t dwUser3;
    uint32_t dwUser4;
} BrNetSessionDesc;

typedef struct BrNetAddrElem {
    BrNetGuid guidDataType;
    uint32_t dwDataSize;
    void *lpData;
} BrNetAddrElem;

typedef struct BrNetConnection {
    uint32_t           dwSize;
    uint32_t           dwFlags;          /* DPLCONNECTION_CREATESESSION 2, _JOINSESSION 1 */
    BrNetSessionDesc *lpSessionDesc;
    BrNetName         *lpPlayerName;
    BrNetGuid           guidSP;
    void           *lpAddress;
    uint32_t           dwAddressSize;
} BrNetConnection;


void   *br_net_new(void);                 /* the game's network object */
void   *br_net_lobby_new(void);           /* the lobby that never launched it */
void    br_net_release(void *o);
int32_t br_net_close(void *dp);
int32_t br_net_enum_connections(void *dp, const void *app, void *cb, void *ctx, uint32_t flags);
int32_t br_net_init_connection(void *dp, void *conn, uint32_t flags);
int32_t br_net_enum_sessions(void *dp, BrNetSessionDesc *want, uint32_t timeout, void *cb, void *ctx, uint32_t flags);
int32_t br_net_open(void *dp, BrNetSessionDesc *d, uint32_t flags);
int32_t br_net_get_session(void *dp, void *data, uint32_t *size);
int32_t br_net_set_session(void *dp, const BrNetSessionDesc *d, uint32_t flags);
int32_t br_net_create_player(void *dp, BrNetId *id, const BrNetName *name, void *event, void *data, uint32_t n, uint32_t flags);
int32_t br_net_destroy_player(void *dp, BrNetId id);
int32_t br_net_enum_players(void *dp, const void *inst, void *cb, void *ctx, uint32_t flags);
int32_t br_net_player_name(void *dp, BrNetId id, void *data, uint32_t *size);
int32_t br_net_player_data(void *dp, BrNetId id, void *data, uint32_t *size, uint32_t flags);
int32_t br_net_player_address(void *dp, BrNetId id, void *data, uint32_t *size);
int32_t br_net_message_count(void *dp, BrNetId id, uint32_t *count);
int32_t br_net_receive(void *dp, BrNetId *from, BrNetId *to, uint32_t flags, void *data, uint32_t *size);
int32_t br_net_send(void *dp, BrNetId from, BrNetId to, uint32_t flags, const void *data, uint32_t n);
int32_t br_net_lobby_settings(void *lobby, uint32_t app, void *data, uint32_t *size);
int32_t br_net_lobby_address(void *lobby, const BrNetAddrElem *e, uint32_t count, void *addr, uint32_t *size);

#ifdef __cplusplus
}
#endif
#endif
