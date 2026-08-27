/* 0x00401670 OpenMediaFile: graph + MultiByteToWideChar + RenderFile */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401670 bossrally.exe OpenMediaFile */

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
void DeleteContents(void);
int CreateGraph(void);

typedef HRESULT (__stdcall *RenderFile_t)(ComObj *, LPCWSTR, LPCWSTR);

int OpenMediaFile(const char *path)
{
    WCHAR wbuf[0x104];
    HRESULT hr;

    DeleteContents();
    if (CreateGraph() == 0)
        return 0;
    MultiByteToWideChar(0, 0, path, -1, wbuf, 0x104);
    SetCursor(LoadCursorA(0, IDC_WAIT));
    hr = ((RenderFile_t)gGraph->lpVtbl->slot[10])(gGraph, wbuf, 0);
    SetCursor(LoadCursorA(0, IDC_ARROW));
    return hr >= 0;
}

#endif
