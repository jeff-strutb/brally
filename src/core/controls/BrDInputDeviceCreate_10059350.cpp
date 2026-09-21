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
 * PARKED at 103 diffs, ALL displacement shift from ONE cross-jump. The
 * first two error blocks are byte-identical instruction sequences (they
 * differ only in the pushed line number and the call displacement), and
 * our cl tail-merges them: `jge +7 / push 0xAC / jmp` into the second
 * block's `call BrDInputErrLine`, saving 21 bytes. The original keeps
 * three separate copies. Everything else -- all three COM calls, the
 * argument orders, the hWnd reloads from [esp+0x10] in blocks 1 and 2 and
 * the cached edi in block 3, both returns -- is byte-identical.
 * DO NOT RE-PROBE -- unchanged by: three separate `hr` locals, and flags
 * /O2 /Gy, /Gf, /Op, /Oy, /Ox /Ob0 /Gy, /Ot, /Ob0, /Og /Oi /Ot /Oy /Ob1
 * (all 103). The nested `if (hr >= 0) { ... }` form un-merges but moves
 * the success return inline and reorders the error blocks (192 B, 118) --
 * the original's layout is the flat early-return one kept here.
 *
 * Third emitter-level residue of this session (with the SIB and memset
 * entries in docs/VC5-IDIOMS.md). Unlike those two this one is a whole
 * optimisation our cl performs and the original's did not, so it is the
 * strongest of the three as evidence for the compiler patch-level lead.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

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
        BrDInputReport(hWnd, hr, BrDInputErrLine(0xAD));
        return 0;
    }

    hr = pDev->lpVtbl->SetCooperativeLevel(pDev, hWnd, 5);
    if (hr < 0) {
        BrDInputReport(hWnd, hr, BrDInputErrLine(0xAE));
        return 0;
    }

    return 1;
}
