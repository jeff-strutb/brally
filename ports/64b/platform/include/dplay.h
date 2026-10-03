/* dplay.h: the DirectPlay 4 / DirectPlayLobby 3 surface the game uses.
 *
 * Own definitions with the SDK's field order and the SDK's names, so the
 * records are laid out for whatever ABI the build targets (pointers at their
 * native width). The core reaches the interfaces through their vtable slot
 * numbers; platform/common/dplay.c implements them. */
#ifndef BR_DPLAY_H
#define BR_DPLAY_H

#include "win32.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DP_OK                  ((HRESULT)0)
#define DPERR_BUFFERTOOSMALL   ((HRESULT)0x8877001E)
#define DPERR_INVALIDOBJECT    ((HRESULT)0x88770082)
#define DPERR_INVALIDPLAYER    ((HRESULT)0x88770096)
#define DPERR_NOCONNECTION     ((HRESULT)0x887700AA)
#define DPERR_NOMESSAGES       ((HRESULT)0x887700BE)
#define DPERR_NOSESSIONS       ((HRESULT)0x887700D2)
#define DPERR_NOTLOBBIED       ((HRESULT)0x8877042E)
#define DPERR_UNSUPPORTED      ((HRESULT)0x80004001)   /* E_NOTIMPL */

typedef DWORD DPID;

typedef struct DPNAME {
    DWORD dwSize;
    DWORD dwFlags;
    char *lpszShortNameA;
    char *lpszLongNameA;
} DPNAME;

typedef struct DPSESSIONDESC2 {
    DWORD dwSize;
    DWORD dwFlags;
    GUID  guidInstance;
    GUID  guidApplication;
    DWORD dwMaxPlayers;
    DWORD dwCurrentPlayers;
    char *lpszSessionNameA;
    char *lpszPasswordA;
    DWORD dwReserved1;
    DWORD dwReserved2;
    DWORD dwUser1;
    DWORD dwUser2;
    DWORD dwUser3;
    DWORD dwUser4;
} DPSESSIONDESC2;

typedef struct DPCOMPOUNDADDRESSELEMENT {
    GUID  guidDataType;
    DWORD dwDataSize;
    void *lpData;
} DPCOMPOUNDADDRESSELEMENT;

typedef struct DPLCONNECTION {
    DWORD           dwSize;
    DWORD           dwFlags;          /* DPLCONNECTION_CREATESESSION 2, _JOINSESSION 1 */
    DPSESSIONDESC2 *lpSessionDesc;
    DPNAME         *lpPlayerName;
    GUID            guidSP;
    void           *lpAddress;
    DWORD           dwAddressSize;
} DPLCONNECTION;

/* IDirectPlay4A vtable slots */
enum {
    DP4_QueryInterface, DP4_AddRef, DP4_Release, DP4_AddPlayerToGroup, DP4_Close,
    DP4_CreateGroup, DP4_CreatePlayer, DP4_DeletePlayerFromGroup, DP4_DestroyGroup,
    DP4_DestroyPlayer, DP4_EnumGroupPlayers, DP4_EnumGroups, DP4_EnumPlayers,
    DP4_EnumSessions, DP4_GetCaps, DP4_GetGroupData, DP4_GetGroupName,
    DP4_GetMessageCount, DP4_GetPlayerAddress, DP4_GetPlayerCaps, DP4_GetPlayerData,
    DP4_GetPlayerName, DP4_GetSessionDesc, DP4_Initialize, DP4_Open, DP4_Receive,
    DP4_Send, DP4_SetGroupData, DP4_SetGroupName, DP4_SetPlayerData, DP4_SetPlayerName,
    DP4_SetSessionDesc, DP4_AddGroupToGroup, DP4_CreateGroupInGroup,
    DP4_DeleteGroupFromGroup, DP4_EnumConnections, DP4_EnumGroupsInGroup,
    DP4_GetGroupConnectionSettings, DP4_InitializeConnection, DP4_SecureOpen,
    DP4_SendChatMessage, DP4_SetGroupConnectionSettings, DP4_StartSession,
    DP4_GetGroupFlags, DP4_GetGroupParent, DP4_GetPlayerAccount, DP4_GetPlayerFlags,
    DP4_GetGroupOwner, DP4_SetGroupOwner, DP4_SendEx, DP4_GetMessageQueue,
    DP4_CancelMessage, DP4_CancelPriority, DP4_SLOTS
};

/* IDirectPlayLobby3A vtable slots */
enum {
    DPL3_QueryInterface, DPL3_AddRef, DPL3_Release, DPL3_Connect, DPL3_CreateAddress,
    DPL3_EnumAddress, DPL3_EnumAddressTypes, DPL3_EnumLocalApplications,
    DPL3_GetConnectionSettings, DPL3_ReceiveLobbyMessage, DPL3_RunApplication,
    DPL3_SendLobbyMessage, DPL3_SetConnectionSettings, DPL3_SetLobbyMessageEvent,
    DPL3_CreateCompoundAddress, DPL3_ConnectEx, DPL3_RegisterApplication,
    DPL3_UnregisterApplication, DPL3_WaitForConnectionSettings, DPL3_SLOTS
};

extern const GUID CLSID_DirectPlay;        /* D1EB6D20-8923-11D0-9D97-00A0C90A43CB */
extern const GUID CLSID_DirectPlayLobby;   /* 2FE8F810-B2A5-11D0-A787-0000F803ABFC */

#ifdef __cplusplus
}
#endif
#endif
