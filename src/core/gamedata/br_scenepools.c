/* br_scenepools.c -- gamedata: wiring up a scene's model and animation pools.
 *
 * The one-shot setup that points the fixed table pointers at the loaded data
 * block and starts the two service loops that feed them. Filed out of
 * slice2_19.c's Ghidra-matched section; the declaration block it needs is
 * copied rather than moved, because functions left behind in the slice use
 * the same symbols.
 *
 * See slice2_19.h for the recovered layouts and the gotchas.
 */
#ifdef BR_MATCHING_BUILD
#include <windows.h>

extern int DAT_106b7ac8;
extern int DAT_106b8090;
extern int DAT_106b80a8;
extern char DAT_106e7730;
extern int DAT_106e7738;
extern char DAT_106e79b8;
extern unsigned char DAT_106e79ba;
extern unsigned char DAT_106e79bb;
extern int DAT_106e79d4;
extern int DAT_106e8200;
extern int DAT_106e869c;
extern int DAT_106ea1a0;
extern int DAT_106ea358;
extern int DAT_106ea410;
extern int DAT_106ea430;
extern char DAT_106ec508;
extern int DAT_106ec6c0;
extern int DAT_106ec794;
extern int DAT_106ed368;
extern int DAT_106ed370;
extern int DAT_106ed570;
extern int DAT_106ed5d0;
extern int DAT_106ed6e0;
extern int DAT_10b25794;
extern int _DAT_106ec770;
int BrPodNop();
int BrStubFalse();
int BrStubTrue();

/* The two service loops this hands to the pool starter; they live in
 * src/core/racing/br_idleloop.c. */
void BrIdleLoop_1002DD30(void);
void BrIdleLoop_1002DD9A(void);

/* WHAT IT DOES: set up the model and animation pools for a scene from the
 * loaded data block, wiring the fixed table pointers before anything reads
 * them. */
/* @implements 0x1002DEC3 glide FUN_1002dec3 */
/* auto-filed from ghidra --refine; transforms: as-is */

void FUN_1002dec3(void)

{
  struct {
    int result;
    char buf[24];
    unsigned int flags;
    int idx;
    char tmp[4];
  } local_28;

  DAT_106e79d4 = &DAT_106b80a8;
  BrPodNop(&DAT_106ed570,&DAT_106ec6c0,0x20);
  BrPodNop(&DAT_106ea430,&DAT_106ec794,1);
  BrPodNop(4,&DAT_106ea430,DAT_106e7738);
  BrPodNop(&DAT_106ea410,&DAT_106e869c,1);
  BrPodNop(9,&DAT_106ea410,DAT_106e7738);
  BrPodNop(&DAT_106ed5d0,&DAT_106ea1a0,1);
  BrPodNop(&DAT_106ed5d0,DAT_106e7738,1);
  BrPodNop(&DAT_106b7ac8,10,BrIdleLoop_1002DD30,DAT_10b25794,&DAT_106ed368,0x3c);
  BrPodNop(&DAT_106b7ac8);
  BrPodNop(&DAT_106ed370,0xb,BrIdleLoop_1002DD9A,DAT_10b25794,&DAT_106e8200,0x3c);
  BrPodNop(&DAT_106ed370);
  _DAT_106ec770 = 0x47371b00;
  BrPodNop(local_28.buf,local_28.tmp,1);
  BrPodNop(5,local_28.buf,1);
  BrStubTrue(local_28.buf,&local_28.flags,&DAT_106e79b8);
  BrPodNop(&DAT_106b8090,&DAT_106ea358,1);
  BrPodNop(5,&DAT_106b8090,0);
  for (local_28.idx = 0; local_28.idx < 4; local_28.idx = local_28.idx + 1) {
    (&DAT_106e7730)[local_28.idx] = 0;
    if (((int)(local_28.flags & 0xff) >> local_28.idx & 1) != 0) {
      if (((&DAT_106e79bb)[local_28.idx * 4] & 8) == 0) {
        if ((*(unsigned short *)(&DAT_106e79b8 + local_28.idx * 4) & 4) != 0) {
          if (((&DAT_106e79ba)[local_28.idx * 4] & 1) != 0) {
            local_28.result = BrStubFalse(&DAT_106b8090,&DAT_106ec508 + local_28.idx * 0x68,local_28.idx);
            if (local_28.result != 0) {
              if (local_28.result > 9) {
                if (local_28.result > 0xb) goto next;
                if (BrStubFalse(&DAT_106b8090,&DAT_106ec508 + local_28.idx * 0x68,local_28.idx) == 0) {
                  (&DAT_106e7730)[local_28.idx] = 1;
                  BrStubFalse(&DAT_106ec508 + local_28.idx * 0x68);
                }
              }
            }
          }
        }
      }
    }
next:
    ;
  }
  DAT_106ed6e0 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  return;
}
#endif /* BR_MATCHING_BUILD */
