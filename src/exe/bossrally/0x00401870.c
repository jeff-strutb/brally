/* 0x00401870 OnMediaStop — CPlay OnMediaAbortStop + FROM_START, no MessageBox */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: stop playback and rewind to the beginning. */
/* @implements 0x00401870 bossrally.exe OnMediaStop */

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

void OnMediaStop(void)
{
    ComObj *pMC;
    ComObj *pMP;
    HRESULT hr;

    if (IsPlayingOrPaused()) {
        hr = gGraph->lpVtbl->QueryInterface(gGraph, &IID_IMediaControl, (void **)&pMC);
        if (SUCCEEDED(hr)) {
            ((NoArg_t)pMC->lpVtbl->slot[6])(pMC);
            pMC->lpVtbl->Release(pMC);
            hr = gGraph->lpVtbl->QueryInterface(gGraph, &IID_IMediaPosition, (void **)&pMP);
            if (SUCCEEDED(hr)) {
                ((Put2_t)pMP->lpVtbl->slot[5])(pMP, 0, 0);
                pMP->lpVtbl->Release(pMP);
            }
            if (SUCCEEDED(hr)) {
                SetMediaState(1);
                return;
            }
        }
    }
}

#endif
