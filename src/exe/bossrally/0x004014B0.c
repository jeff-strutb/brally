/* 0x004014B0 CreateGraph: CoCreate FilterGraph, GetEventHandle. */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: build the DirectShow filter graph that will decode and
 * display the video. */
/* @implements 0x004014B0 bossrally.exe CreateGraph */

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
extern HANDLE gGraphEvent;
extern IID IID_IMediaEvent;
extern IID IID_IGraphBuilder;
extern CLSID CLSID_FilterGraph;
void DeleteContents(void);

typedef HRESULT (__stdcall *GetEventHandle_t)(ComObj *, HANDLE *);

int CreateGraph(void)
{
    ComObj *pME;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_FilterGraph, 0, 1, &IID_IGraphBuilder, (void **)&gGraph);
    if (hr < 0) {
        gGraph = 0;
        return 0;
    }
    hr = gGraph->lpVtbl->QueryInterface(gGraph, &IID_IMediaEvent, (void **)&pME);
    if (hr < 0) {
        DeleteContents();
        return 0;
    }
    hr = ((GetEventHandle_t)pME->lpVtbl->slot[4])(pME, &gGraphEvent);
    pME->lpVtbl->Release(pME);
    if (hr < 0) {
        DeleteContents();
        return 0;
    }
    return 1;
}

#endif
