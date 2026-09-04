/* Matching body — 0x10006150 */
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>

extern int DAT_1021ce58;    /* slot[0].hMutex; slot stride 0x978 (0x25E ints) */

/* WHAT IT DOES: read one network slot's state word plus three of its status
 * bytes in a single locked operation, so the caller sees a consistent
 * snapshot rather than four separately-locked reads that could disagree. */
/* @implements 0x10006150 glide BrNetSlotGetF030 */
int BrNetSlotGetF030(int i, unsigned char *pb34, unsigned char *pb35,
                     unsigned char *pb36)
{
    int v;
    int off = i * 0x978;

    WaitForSingleObject(*(HANDLE *)((char *)&DAT_1021ce58 + off),
                        0xffffffff);
    v     = *(int *)((char *)&DAT_1021ce58 + off + 0x30);
    *pb34 = *(unsigned char *)((char *)&DAT_1021ce58 + off + 0x34);
    *pb35 = *(unsigned char *)((char *)&DAT_1021ce58 + off + 0x35);
    *pb36 = *(unsigned char *)((char *)&DAT_1021ce58 + off + 0x36);
    ReleaseMutex(*(HANDLE *)((char *)&DAT_1021ce58 + off));
    return v;
}

#endif /* BR_MATCHING_BUILD */
