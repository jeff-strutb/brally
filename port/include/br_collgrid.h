/* br_collgrid.h -- storage for the collision grid, and the track binding.
 *
 * ======================================================================
 * WHY THIS FILE EXISTS, AND WHAT IT CORRECTS
 * ======================================================================
 * CONVENTIONS.md records this as an open, blocking defect:
 *
 *     "0x11750338 / 0x117554A0 cannot be made one object on this host.
 *      BrFxRecord is 32 bytes but BrCollPlane holds three BrVec3 * and widens
 *      under LP64, so the 600-record array cannot carry both views.  The fix
 *      is to make BrCollPlane store vertex INDICES; until then
 *      g_pBrCollGrid stays NULL, which every consumer already guards."
 *
 * Half of that is right and half of it is not, and the distinction is what
 * this module is:
 *
 *   RIGHT.  0x11750338 really is TWO host objects -- slice3_42.c's
 *           `BrFxRecord g_BrFx1750338[600]` and the collision grid -- and on
 *           LP64 they cannot be made one, because BrCollPlane is 40 bytes
 *           with three pointers in it and BrFxRecord is 32.  That alias is
 *           STILL OPEN and this module does not close it.
 *
 *   WRONG.  "no wheel will touch ground until BrCollPlane stores INDICES."
 *           The pointers are not the obstacle.  They point into
 *           `g_pBrCollVerts`, which is a genuine host `BrVec3 *` aimed at the
 *           track image -- and br_track.c has already byte-swapped that image
 *           in place, so the vertices are host-order floats on a 12-byte
 *           stride, exactly BrVec3.  A pointer into a host array survives
 *           LP64 perfectly well.
 *
 *           What actually kept g_pBrCollGrid NULL is much duller: NOTHING IN
 *           port/src DEFINES IT.  Four test files declare their own storage
 *           and the host had none, so the one guard in br_phys.c's
 *           BrPhysProbeCell ("a NULL grid means no ground") was taken on
 *           every probe of every run.  Giving the two globals storage is the
 *           whole fix, and it is this file.
 *
 * So the ground probe works today with BrCollPlane exactly as slice1_08.h
 * declares it.  Making BrCollPlane index-based is still the right end state
 * -- it is what would let this storage BE g_BrFx1750338 rather than sit
 * beside it -- but it is a separate change with its own risk, and doing it
 * was not necessary to put a car on the ground.
 *
 * ======================================================================
 * WHERE THE FIVE SOURCE TABLES COME FROM
 * ======================================================================
 * slice6_73.c's BrCollGridCellAcquire (0x1006F720 / Glide 0x100686D0) builds
 * a cell out of five globals.  Every one of them is a field of the .TRK
 * HEADER, and that is not inferred -- the Glide disassembly names them
 * directly and the track header base is 0x106EECD8:
 *
 *   Glide 0x106EECE4 = base + 0x0C = BR_TRK_H_FACES      -> g_pBrCollTriIdx
 *   Glide 0x106EECEC = base + 0x14 = BR_TRK_H_VERTICES   -> g_pBrCollVerts
 *   Glide 0x106EED6C = base + 0x94 = BR_TRK_H_FACESEND   -> g_pBrCollTriFlags
 *   Glide 0x106EECF8 = base + 0x20 = BR_TRK_H_GRIDITEMS  -> g_pBrTriTable
 *   Glide 0x106EECFC = base + 0x24 = BR_TRK_H_GRIDSTART  -> g_pBrGrid64
 *
 * The last two are pinned by the D3D side as well: slice1_01.h records the
 * grid base as D3D 0x106C7C6C and the triangle table as 0x106C7C68, and the
 * D3D header base is 0x106C7C48, so the same two offsets fall out.  br_track.h
 * calls +0x94 "FACESEND -- butts against the end of the face array"; it is
 * the per-face SURFACE BYTE array, which is what
 * `g_pBrCollTriFlags[tri] & 7` reads.
 *
 * All five are relocated to FILE OFFSETS by br_track.c (its documented LP64
 * deviation), so binding is `image + offset` and needs no new decoding.
 */
#ifndef BR_COLLGRID_H
#define BR_COLLGRID_H

#include "br_track.h"
#include "slice2_11.h"   /* BrCollPlane, g_pBrCollGrid, g_pBrCollGridCount */
#include "slice6_73.h"   /* BR73_COLL_CELLS and the five source globals    */

/* Point the five source globals at pTrack and give g_pBrCollGrid /
 * g_pBrCollGridCount their storage.  Returns 0 on success; non-zero, with
 * every global left NULL, when any of the five header fields does not address
 * the image -- which is what a track without collision data looks like.
 *
 * The cell cache is invalidated (all four keys forced to a value no query can
 * produce and all four counts zeroed), because the keys are stale the instant
 * the geometry changes and BrCollGridCellAcquire would otherwise hand back a
 * cell built from the previous track. */
int  BrCollGridBind(const BrTrack *pTrack);

/* Drop the binding: every global back to NULL, cache invalidated.  Safe to
 * call when nothing is bound. */
void BrCollGridRelease(void);

/* How many of the four cells currently hold triangles, and how many
 * triangles in total.  Diagnostic only -- this is how a harness reports "the
 * probe is looking at real geometry" rather than asserting it. */
int  BrCollGridLoaded(int *pCells, int *pPlanes);

#endif /* BR_COLLGRID_H */
