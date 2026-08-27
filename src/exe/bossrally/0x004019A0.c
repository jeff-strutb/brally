/* 0x004019A0 OnGraphNotify — CPlay nested SUCCEEDED + EC_FULLSCREEN_LOST */
#ifdef BR_MATCHING_BUILD
/* @implements 0x004019A0 bossrally.exe OnGraphNotify */

#include <windows.h>
#include <objbase.h>

typedef struct ComObj ComObj;
typedef struct ComVtbl {
    HRESULT (__stdcall *QueryInterface)(ComObj *, const IID *, void **);
    ULONG (__stdcall *AddRef)(ComObj *);
    ULONG (__stdcall *Release)(ComObj *);
    void *slot[40];
} ComVtbl;
struct ComObj { ComVtbl *lpVtbl; };

extern ComObj *gGraph;
extern IID IID_IMediaEvent;
void OnMediaPauseStop(void);
void OnMediaStop(void);
int SetFullScreen(void);

typedef HRESULT (__stdcall *GetEvent_t)(ComObj *, long *, long *, long *, long);

void OnGraphNotify(void)
{
    ComObj *pME;
    long lEventCode, lParam1, lParam2;

    if (SUCCEEDED(gGraph->lpVtbl->QueryInterface(gGraph, &IID_IMediaEvent, (void **)&pME))) {
        if (SUCCEEDED(((GetEvent_t)pME->lpVtbl->slot[5])(pME, &lEventCode, &lParam1, &lParam2, 0))) {
            if (lEventCode == 1) {
                OnMediaPauseStop();
            } else if (lEventCode == 2 || lEventCode == 3) {
                OnMediaStop();
            } else if (lEventCode == 0x12) {
                SetFullScreen();
            }
        }
        pME->lpVtbl->Release(pME);
    }
}

#endif
