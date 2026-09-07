/* br_sndvoicestereo.c -- audio.
 *
 * Turning a left/right level pair into a voice's pan and volume, then pushing
 * both at the DirectSound buffer.  It lives in its own file because it READS
 * the return value of BrSndVoiceApplyVolume / BrSndVoiceApplyPan (the DirectSound
 * HRESULT each leaves in eax), and those two are defined `void` in their own
 * modules (br_sndvoice.c / slice6_76.c) -- so it must declare them int-returning
 * here, which it cannot do in the same translation unit as either definition.
 */
#ifdef BR_MATCHING_BUILD

/* Set when the sound system is up: the DirectSound object, and the two device
 * caps the mixer checks before touching a buffer.  Any of them zero means the
 * call is a no-op that still reports success. */
extern int DAT_100b55f0;
extern int DAT_1184c458;
extern int DAT_1184c45c;

/* Both are thiscall-free cdecl leaves that return their DirectSound HRESULT in
 * eax; the tree defines them `void` (the return is a side effect there), so this
 * caller declares them the way it READS them. */
int BrSndVoiceApplyVolume(int pVoice);
int BrSndVoiceApplyPan(int pVoice);

/* WHAT IT DOES: positions one playing voice from a packed left/right level pair
 * (each 0..32).  The louder side sets the volume field (+0x14) and the quieter
 * side, scaled against the louder, sets the pan field (+0x10); then it pushes
 * volume and pan at the DirectSound buffer.  A null voice fails; a sound system
 * that is not up, or a zero level, reports success without touching the buffer.
 * Fails if either DirectSound call fails. */
/* @implements 0x1006B790 glide BrSndVoiceSetLR */
int BrSndVoiceSetLR(int pVoice, unsigned int levels)
{
  int iVar1;
  int lo;
  int hi;

  if ((DAT_100b55f0 != 0) && (DAT_1184c458 != 0) && (DAT_1184c45c != 0)) {
    if (pVoice == 0)
      goto RET0;
    hi = levels >> 0x10;
    lo = levels & 0xffff;
    if (0x20 < hi)
      hi = 0x20;
    if (0x20 < lo)
      lo = 0x20;
    if (hi > lo) {
      *(int *)(pVoice + 0x14) = (hi * 400) / 32;
      *(int *)(pVoice + 0x10) = 400;
      if (hi == 0)
        goto APPLY;
      iVar1 = ((lo - hi) * 400) / hi;
    }
    else {
      *(int *)(pVoice + 0x14) = (lo * 400) / 32;
      *(int *)(pVoice + 0x10) = 400;
      if (lo == 0)
        goto APPLY;
      iVar1 = ((lo - hi) * 400) / lo;
    }
    *(int *)(pVoice + 0x10) = iVar1 + 400;
APPLY:
    iVar1 = BrSndVoiceApplyVolume(pVoice);
    if ((iVar1 == 0) && (iVar1 = BrSndVoiceApplyPan(pVoice), iVar1 == 0)) {
      return 1;
    }
RET0:
    return 0;
  }
  return 1;
}

#endif /* BR_MATCHING_BUILD */
