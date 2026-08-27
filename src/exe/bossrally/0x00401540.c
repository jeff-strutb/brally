/* 0x00401540 SetVideoDrain: IVideoWindow::put_MessageDrain(hwnd) */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401540 bossrally.exe SetVideoDrain */

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
extern IID IID_IVideoWindow;

typedef HRESULT (__stdcall *PutLong_t)(ComObj *, long);

int SetVideoDrain(HWND hwnd)
{
    ComObj *pVW;
    HRESULT hr;

    hr = gGraph->lpVtbl->QueryInterface(gGraph, &IID_IVideoWindow, (void **)&pVW);
    if (hr < 0)
        return 0;
    hr = ((PutLong_t)pVW->lpVtbl->slot[28])(pVW, (long)hwnd);
    if (hr < 0) {
        pVW->lpVtbl->Release(pVW);
        return 0;
    }
    pVW->lpVtbl->Release(pVW);
    return 1;
}

#endif
