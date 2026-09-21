/* WHAT IT DOES: tears down the current session (net, handles, video) and
 * brings the renderer back up at 640x480x16 if the clock pair drifted. */
/* @implements 0x1002F282 glide BrSessionReinitVideo
 * @cpp_kind free
 * @cpp_symbol _BrSessionReinitVideo
 *
 * cdecl, no args, 142 B, an odd-address /Od function in the 0x1002Fxxx
 * debug stretch (frame pointer, memory-operand compares, every global
 * re-read).  Moved out of ghidra_batch.c on 2026-09-13: that batch is /O2,
 * and the C lane's struct-by-value spelling of the one thiscall
 * (`push 0x10b72f48; mov ecx,0x10b71290; call`) homes the struct in a frame
 * slot under /Od; a class method call spells it directly.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#endif

class Save1290 {
public:
    void Set(void *p);              /* 0x100634B0, thiscall, one stack arg */
};

extern "C" {
void FUN_1006c460(void);
void FUN_10072840(void);
void FUN_1006a320(void);
void FUN_10005cd0(void);
void FUN_1001cd50(void);
void FUN_10063970(int, int, int, int, int);
void FUN_1005a420(void);

int    DAT_106ec760;
int    DAT_10b71a68;
int    DAT_106e9a34;
int    DAT_10b71a6c;
int    DAT_10b72f48;
Save1290 DAT_10b71290;
int    DAT_10226a48;
HANDLE DAT_106ed6e0;

void BrSessionReinitVideo(void)
{
    if ((DAT_106ec760 != DAT_10b71a68) || (DAT_106e9a34 != DAT_10b71a6c)) {
        DAT_10b71290.Set(&DAT_10b72f48);
    }
    FUN_1006c460();
    FUN_10072840();
    if (DAT_10226a48 != 0) {
        if (DAT_10226a48 > 1) {
            FUN_1006a320();
        }
        FUN_10005cd0();
    }
    FUN_1001cd50();
    CloseHandle(DAT_106ed6e0);
    DAT_106ed6e0 = 0;
    FUN_10063970(3, 0x280, 0x1e0, 0x10, 0);
    FUN_1005a420();
}
}
