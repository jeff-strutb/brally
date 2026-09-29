/* WHAT IT DOES: create the DirectInput device, reporting the failure through
 * the error dialog with the line it came from if it cannot. */
/* @implements 0x10059350 glide BrDInputDeviceCreate_10059350
 * @cpp_kind method
 * @cpp_symbol ?CreateDevice@Input59350@@QAEHPAX@Z
 *
 * Thiscall, one stack arg (`ret 4`), 181 B. The DirectInput device
 * bring-up for one device: IDirectInput::CreateDevice (+0x0C) into the
 * +0x50 slot, then IDirectInputDevice::SetDataFormat (+0x2C) with the
 * static format at 0x10072AC0, then SetCooperativeLevel (+0x34) with the
 * window and DISCL_EXCLUSIVE|DISCL_FOREGROUND (5). Every step reports
 * through the same (window, hr, line-string) helper pair and returns 0;
 * all three succeeding returns 1.
 *
 * The COM calls are the C-style `pV->lpVtbl->Fn(pV, ...)` form -- stdcall
 * through the vtable with the interface as the first stack argument --
 * which is why only the outer function is a thiscall method.
 *
 * The second error block calls BrDInputReportTwin, a separately named twin
 * of BrDInputReport that the linker folded onto 0x100590A0.  The first two
 * error blocks are the same instructions apart from the pushed line number.
 * VC5's tail merge compares call targets by symbol, so with one name for
 * both it cross-jumps block 1 into block 2 (153 B).  The original keeps
 * three separate copies (181 B), which needs a second callee symbol.  The
 * same lever is used at 0x10009010.  The twin's real name cannot be
 * recovered.
 */
#define _CRTIMP __declspec(dllimport)

struct DIDev;

struct DIDevVtbl {
    void *pad00[11];                                        /* +0x00..+0x28 */
    int (__stdcall *SetDataFormat)(DIDev *, const void *);  /* +0x2C */
    void *pad30;                                            /* +0x30 */
    int (__stdcall *SetCooperativeLevel)(DIDev *, void *, unsigned int);
                                                            /* +0x34 */
};

struct DIDev {
    DIDevVtbl *lpVtbl;
};

struct DI;

struct DIVtbl {
    void *pad00[3];                                         /* +0x00..+0x08 */
    int (__stdcall *CreateDevice)(DI *, const void *, DIDev **, void *);
                                                            /* +0x0C */
};

struct DI {
    DIVtbl *lpVtbl;
};

class Input59350 {
public:
    char   pad[0x50];
    DIDev *pDev;        /* +0x50 */

    int CreateDevice(void *hWnd);
};

typedef char chk_pDev[(unsigned)&((Input59350 *)0)->pDev == 0x50 ? 1 : -1];

extern "C" {
DI  *g_pDInput;                 /* 0x118EEE88 */
char g_DeviceGuid[16];          /* 0x10078708 */
char g_DataFormat[24];          /* 0x10072AC0 */
char *BrDInputErrLine(int line);                    /* 0x1006D280 */
void  BrDInputReport(void *hWnd, int hr, char *s);  /* 0x100590A0 */
/* A separately named twin of BrDInputReport whose identical body the linker
 * folded onto 0x100590A0 (see the header note). */
void  BrDInputReportTwin(void *hWnd, int hr, char *s); /* 0x100590A0 */
}

int Input59350::CreateDevice(void *hWnd)
{
    int hr;

    hr = g_pDInput->lpVtbl->CreateDevice(g_pDInput, g_DeviceGuid, &pDev, 0);
    if (hr < 0) {
        BrDInputReport(hWnd, hr, BrDInputErrLine(0xAC));
        return 0;
    }

    hr = pDev->lpVtbl->SetDataFormat(pDev, g_DataFormat);
    if (hr < 0) {
        BrDInputReportTwin(hWnd, hr, BrDInputErrLine(0xAD));
        return 0;
    }

    hr = pDev->lpVtbl->SetCooperativeLevel(pDev, hWnd, 5);
    if (hr < 0) {
        BrDInputReport(hWnd, hr, BrDInputErrLine(0xAE));
        return 0;
    }

    return 1;
}
