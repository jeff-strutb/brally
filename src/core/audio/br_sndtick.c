/* br_sndtick.c -- audio: per-frame apply of pending channel state.
 *
 * 0x1006BDD0 walks 15 slots. Neighbours already matched: BrSndChanBind,
 * BrSndVoiceSetPan, BrSndChanSetRatio, BrSndVoiceSetFreq, BrSfxChanStart. */
#ifdef BR_MATCHING_BUILD

#define _CRTIMP __declspec(dllimport)

int BrSndChanBind(int, int);
int BrSndVoiceSetPan(int, int);
int BrSndChanSetRatio(int, __int64);
int BrSndVoiceSetFreq(int, int);
int BrSfxChanStart(int, int, int);
int FUN_1006bf50(int);

extern int DAT_1184c454;
extern int DAT_100b55f8;
extern int DAT_1184c268;
extern int DAT_100b5d00;
extern int DAT_118eef40;
extern int DAT_1184c080;
extern int DAT_118eef48;
extern int DAT_1184c088;
extern int DAT_118eef4c;
extern int DAT_1184c08c;
extern int DAT_118eef54;
extern int DAT_1184c094;

/* WHAT IT DOES: one tick of the sound-channel table. For each of 15 slots,
 * if the bound voice changed, re-bind it (bank 0x19 or bank 0 depending on
 * a mode flag), centre the pan, push the pending pitch and frequency, and
 * start it. Then copy across any still-pending volume/ratio/freq, and if a
 * pitch is live but the voice reports it has stopped, clear that pitch. */
/* @implements 0x1006BDD0 glide FUN_1006bdd0 */
int FUN_1006bdd0(void)
{
  int z;
  int i;
  int *p;
  int off;
  int v;
  int a;
  int c;

  z = 0;
  i = 0;
  p = &DAT_100b5d00;
  off = 0;
  do {
    if (DAT_1184c454 != z) {
      v = (&DAT_100b55f8)[i];
      if (v != z && v == (&DAT_1184c268)[i]) {
        BrSndChanBind(0x19, i);
        BrSndVoiceSetPan(i, 1);
        BrSndChanSetRatio(i, *(__int64 *)((char *)&DAT_118eef48 + off));
        BrSndVoiceSetFreq(i, *(int *)((char *)&DAT_118eef54 + off));
        BrSfxChanStart(0x19, i, 1);
      }
    } else {
      v = *p;
      if (v != z && v == (&DAT_1184c268)[i]) {
        BrSndChanBind(z, i);
        BrSndVoiceSetPan(i, 1);
        BrSndChanSetRatio(i, *(__int64 *)((char *)&DAT_118eef48 + off));
        BrSndVoiceSetFreq(i, *(int *)((char *)&DAT_118eef54 + off));
        BrSfxChanStart(z, i, 1);
      }
    }
    a = *(int *)((char *)&DAT_118eef40 + off);
    if (*(int *)((char *)&DAT_1184c080 + off) != a) {
      *(int *)((char *)&DAT_1184c080 + off) = a;
    }
    if (*(__int64 *)((char *)&DAT_1184c088 + off)
        != *(__int64 *)((char *)&DAT_118eef48 + off)) {
      BrSndChanSetRatio(i, *(__int64 *)((char *)&DAT_118eef48 + off));
    }
    a = *(int *)((char *)&DAT_118eef54 + off);
    if (*(int *)((char *)&DAT_1184c094 + off) != a) {
      BrSndVoiceSetFreq(i, a);
    }
    a = *(int *)((char *)&DAT_118eef48 + off);
    c = *(int *)((char *)&DAT_118eef4c + off);
    if ((a | c) != 0) {
      if (FUN_1006bf50(i) == 0) {
        *(int *)((char *)&DAT_118eef48 + off) = z;
        *(int *)((char *)&DAT_1184c088 + off) = z;
        *(int *)((char *)&DAT_118eef4c + off) = z;
        *(int *)((char *)&DAT_1184c08c + off) = z;
      }
    }
    p = p + 1;
    i = i + 1;
    off = off + 0x18;
  } while ((int)p < 0x100b5d3c);
  return 1;
}

#endif /* BR_MATCHING_BUILD */
