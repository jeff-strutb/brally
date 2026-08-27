/* 0x00401820 OnMediaPlay: IMediaControl::Run, state=3 */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401820 bossrally.exe OnMediaPlay */

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
int CanRun(void);
void SetMediaState(int s);

typedef HRESULT (__stdcall *NoArg_t)(ComObj *);

void OnMediaPlay(void)
{
    ComObj *pMC;
    HRESULT hr;

    if (CanRun() == 0)
        return;
    hr = gGraph->lpVtbl->QueryInterface(gGraph, &IID_IMediaControl, (void **)&pMC);
    if (hr < 0)
        return;
    hr = ((NoArg_t)pMC->lpVtbl->slot[4])(pMC);
    pMC->lpVtbl->Release(pMC);
    if (hr < 0)
        return;
    SetMediaState(3);
}

#endif
