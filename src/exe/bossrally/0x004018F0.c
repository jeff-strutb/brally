/* 0x004018F0 OnMediaPauseStop: Pause, rewind, GetState, Stop, state=1 */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: pause playback, or stop it if it was already paused. */
/* @implements 0x004018F0 bossrally.exe OnMediaPauseStop */

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
extern IID IID_IMediaControl;
extern IID IID_IMediaPosition;
int IsPlayingOrPaused(void);
void SetMediaState(int s);

typedef HRESULT (__stdcall *NoArg_t)(ComObj *);
typedef HRESULT (__stdcall *Put2_t)(ComObj *, int, int);
typedef HRESULT (__stdcall *GetState_t)(ComObj *, long, long *);

void OnMediaPauseStop(void)
{
    ComObj *pMC;
    ComObj *pMP;
    HRESULT hr;
    long state;

    if (IsPlayingOrPaused() == 0)
        return;
    hr = gGraph->lpVtbl->QueryInterface(gGraph, &IID_IMediaControl, (void **)&pMC);
    if (hr < 0)
        return;
    ((NoArg_t)pMC->lpVtbl->slot[5])(pMC);
    hr = gGraph->lpVtbl->QueryInterface(gGraph, &IID_IMediaPosition, (void **)&pMP);
    if (hr >= 0) {
        ((Put2_t)pMP->lpVtbl->slot[5])(pMP, 0, 0);
        pMP->lpVtbl->Release(pMP);
    }
    ((GetState_t)pMC->lpVtbl->slot[7])(pMC, -1, &state);
    ((NoArg_t)pMC->lpVtbl->slot[6])(pMC);
    pMC->lpVtbl->Release(pMC);
    SetMediaState(1);
}

#endif
