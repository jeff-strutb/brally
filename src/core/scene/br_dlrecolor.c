/* br_dlrecolor.c -- scene: the difficulty recolour pass over a model's
 * display list.
 *
 * Glide 0x1002D864 (D3D 0x100341B3). Sits in the /Od-compiled stretch with
 * br_scenepools.c's 0x1002DEC3: frame pointer, every local homed on the
 * stack in declaration order, the `for` layout with the jump over the
 * increment. The sweep picks the /Od variant per function.
 */
#ifdef BR_MATCHING_BUILD

extern int BrG_6C661C;      /* 0x106ED6AC */
extern int BrG_6C6624;      /* 0x106ED6B4 */
extern int DAT_106ed6a8;
extern int DAT_106ed6b0;
extern int DAT_106ed6fc;
extern char DAT_100aa048;
extern char DAT_100aa04c;
extern char DAT_100aa050;
extern char DAT_100aa054;
extern char DAT_100aa058;
extern char DAT_100aa05c;

/* WHAT IT DOES: walk a display list (8-byte words, first byte the command)
 * up to its end marker 0xB8 and recolour it for the current difficulty.
 * 0xB9 words that match one of six table rows take the row's replacement
 * pair for this difficulty (rows 3-5 set the return flag). In normal mode a
 * 0xFC word matching the one fixed row is replaced too; when the alternate
 * palette is on and armed, a 0xFC word that matches either of two colour
 * pairs arms the next 0xFA / 0xFB words to take fixed replacement values.
 * Clears the arming global on exit; returns whether a late row was hit. */
/* Three /Od facts this transcription carries (2026-09-09):
 * - the five locals are single letters declared in the original's slot
 *   order, top of frame down (a=-4 .. e=-0x14). With Ghidra's names in the
 *   same order VC5 /Od scattered them (local_14 at -4, iVar1 at -8, ...):
 *   the slot order follows declaration order only for names that hash in
 *   order, so short names in frame order are the safe spelling;
 * - the arms are laid out in SOURCE order (0xB9, 0xFC, 0xFA, 0xFB, 0xB8);
 * - the end-of-list exit is a two-hop jump: the 0xB8 arm jumps to a block
 *   AFTER the return that jumps back to the shared exit. That is a label on
 *   a `goto` after the `return` (`out: goto done;`); a direct `goto done`
 *   from the arm is one hop and 5 bytes off. */
/* @implements 0x1002D864 glide BrDlRecolor */

int BrDlRecolor(unsigned int *param_1,int param_2)

{
  int a;
  int b;
  int c;
  int d;
  int e;

  e = 0;
  c = (BrG_6C661C == 0) && (BrG_6C6624 == 0);
  a = DAT_106ed6a8 + 1 + (c == 0);
  b = 0;
  if (param_1 != 0) {
    for (;; param_1 = param_1 + 2) {
      switch ((unsigned char)(*param_1 >> 0x18)) {
      case 0xb9:
        for (d = 0; d < 6; d = d + 1) {
          if ((*param_1 == *(unsigned int *)(param_2 + d * 0x20)) &&
             (param_1[1] == *(unsigned int *)(param_2 + 4 + d * 0x20))) {
            *param_1 = *(unsigned int *)(param_2 + d * 0x20 + a * 8);
            param_1[1] = *(unsigned int *)(param_2 + d * 0x20 + 4 + a * 8);
            if (d >= 3) {
              e = 1;
            }
            break;
          }
        }
        break;
      case 0xfc:
        if (c) {
          for (d = 0; d < 1; d = d + 1) {
            if ((*param_1 == *(unsigned int *)(&DAT_100aa048 + d * 0x10)) &&
               (param_1[1] == *(unsigned int *)(&DAT_100aa04c + d * 0x10))) {
              *param_1 = *(unsigned int *)(&DAT_100aa050 + d * 0x10);
              param_1[1] = *(unsigned int *)(&DAT_100aa054 + d * 0x10);
              break;
            }
          }
        }
        if ((DAT_106ed6b0 != 0) && (DAT_106ed6fc != 0)) {
          for (d = 0; d < 2; d = d + 1) {
            if ((*param_1 == *(unsigned int *)(&DAT_100aa058 + d * 8)) &&
               (param_1[1] == *(unsigned int *)(&DAT_100aa05c + d * 8))) {
              break;
            }
          }
          if (d < 2) {
            b = 1;
          }
          else {
            b = 0;
          }
        }
        break;
      case 0xfa:
        if ((b) && (DAT_106ed6b0 != 0)) {
          param_1[1] = 0x60789000;
        }
        break;
      case 0xfb:
        if ((b) && (DAT_106ed6b0 != 0)) {
          param_1[1] = 0x8c9ca800;
        }
        break;
      case 0xb8:
        goto out;
      }
    }
  }
done:
  DAT_106ed6fc = 0;
  return e;
out:
  goto done;
}

#endif /* BR_MATCHING_BUILD */
