/* env.m -- the Remastered environment's seam into the display lists (port code).
 *
 * The scene builder draws a track instance (a piece of scenery) with one
 * command: a jump into the instance's own list (G_DL), written either inline
 * or through BrObjDlBuild.  While the Remastered environment is on
 * (host_env.m), and only then:
 *
 *   - BrGbiDList, as the frame's list jumps into an instance that has cards
 *     replaced or models standing on it, draws those models (host_env.m) and
 *     points the jump at the instance's card-less copy.  The command it
 *     rewrites is in the frame's own list, built afresh every frame; the
 *     instance's list is never touched.
 *   - BrObjDlBuild, whose lighting path also walks the instance's list
 *     itself, is handed the card-less copy for the call (the record's list
 *     pointer is swapped and put back after it).
 *
 * Everything else is the original's, in the same order.  Without Remastered
 * both calls go straight to it.
 */
#include <stdlib.h>
#include <string.h>
#include "w2c_native.h"

int henv_active(void);
u32 henv_list(u32 idx, u32 orig);
int henv_owner(u32 addr);
void henv_draw(int idx);
void henv_note(int k);
void henv_stat_tick(void);

#define OBJ_TABLE     0x106EED38u    /* DAT_106eed38, the instance records (0x54 bytes) */
#define REC_LIST      0x44u          /* a record's display list */

/* WHY: the seam for the lighting path, which reads the list itself. */
/* @replaces 0x1000CBA0 BrObjDlBuild */
void n_BrObjDlBuild(u32 rects, u32 idx, u32 cls, u32 lit, u32 scene)
{
    u32 rec, orig, alt;
    if (!henv_active()) {
        W_ORIG_BrObjDlBuild(rects, idx, cls, lit, scene);
        return;
    }
    rec = W_LD(u32, OBJ_TABLE, 0) + idx * 0x54u;
    orig = W_LD(u32, rec, REC_LIST);
    alt = henv_list(idx, orig);
    if (alt) W_ST(u32, rec, REC_LIST, alt);
    W_ORIG_BrObjDlBuild(rects, idx, cls, lit, scene);
    if (alt) W_ST(u32, rec, REC_LIST, orig);
}

/* WHY: the seam every instance passes through, in every view. */
/* @replaces 0x10021020 BrGbiDList */
u32 n_BrGbiDList(u32 p)
{
    u32 target, alt;
    int idx;
    henv_stat_tick();
    if (henv_active() && (idx = henv_owner(target = W_LD(u32, p, 4))) >= 0) {
        henv_note(0);
        henv_draw(idx);
        alt = henv_list((u32)idx, W_LD(u32, W_LD(u32, OBJ_TABLE, 0) + (u32)idx * 0x54u, REC_LIST));
        if (alt && alt != target) W_ST(u32, p, 4, alt);
    }
    return W_ORIG_BrGbiDList(p);
}
