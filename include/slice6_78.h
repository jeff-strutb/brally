/* slice6_78.h -- BRD3D.dll, packet 78 (slice 6).
 *
 * HOW THESE TARGETS WERE CHOSEN
 * =============================
 * Not by runtime demand.  `./build/brally -all` reports "16/16 builders ran
 * clean" and reaches ZERO stubs, so the hit table the harness exists to fill
 * is empty and ranks nothing.  Packets 74, 76 and 77 all recorded the same
 * negative result.
 *
 * The ranking used instead is STATIC demand, measured twice:
 *
 *   1. call sites in the already-ported C tree, counted per stub name, with
 *      each name resolved to an ORIGINAL ADDRESS via the `XSLICE 0x...`
 *      markers the tree puts above every cross-slice declaration;
 *   2. call sites in the IMAGE: a byte sweep of `.text` (0x10001000,
 *      0x8E000 bytes) for E8/E9 with a target inside `.text`.  1,477 distinct
 *      targets.
 *
 * Measure (2) is the one quoted below.  The two agree at the top.
 *
 * A WARNING ABOUT MEASURE (1), earned here: matching a stub name to the
 * nearest preceding `XSLICE` marker is WRONG often enough to matter.  It put
 * `BrXAtExit` at 0x10069490 (the marker above it belongs to `BrX10069490`);
 * the comment two lines further down says 0x1007E8B0, the CRT's atexit.  Every
 * address quoted below was confirmed by disassembling it.
 *
 * THE RESULT WAS AGAIN MOSTLY "ALREADY PORTED"
 * ============================================
 * Seven of the sixteen entry points here are ADAPTERS.  The bodies existed;
 * the callers were reaching a stub that returns 0.  That is CONVENTIONS' rule
 * -- grep the ADDRESS, not the symbol -- paying out for the third packet in a
 * row, and it includes the two highest-demand stubs left in the tree.
 *
 * SCOPE -- 16 stub entries, 17 original functions
 * ===============================================
 * ADAPTERS (body already exists; this packet only wires the stub name to it):
 *
 *   0x1002F900  33 calls  BrGfx2F900         -> slice1_05 BrRdpSetCombineLERP
 *   0x100192F0  27 calls  BrTextSetSize      -> slice5_63 BrSub_100192F0
 *   0x1003BD50  20 calls  BrRand             -> slice4_52 BrRandom
 *   0x10019260  12 calls  BrTextFlag358Clear -> slice5_63 BrSub_10019260
 *   0x10019270   9 calls  BrTextAlignCentre  -> slice5_63 BrSub_10019270
 *   0x100192A0   3 calls  BrTextSetColor6    -> slice1_03 BrTextSetColors
 *   0x1002BA00   1 call   BrSwapRec8Array    -> slice2_16 BrSwapU16x4Array
 *
 * TRANSCRIBED HERE (no prior body at the address):
 *
 *   0x10008CC0  12 calls  BrErrorf             format-and-die
 *   0x10002FE0   6 calls  BrChkFReadOpen       CHK_FReadOpen
 *   0x10003290   6 calls  BrChkFClose          CHK_FClose
 *   0x10002910   4 calls  BrCdTrackGet         + its 0x10002490 backend
 *   0x10002F90   3 calls  BrChkFileSize        CHK_FileSize
 *   0x10008B90   2 calls  BrPodWriterMakeName  path -> basename
 *   0x1002B9D0   2 calls  BrSegSetFlag         one store
 *   0x10019240   1 call   BrSub_10019240       one store
 *   0x10019250   1 call   BrSub_10019250       one store
 *
 * WHAT WAS ACTIVELY WRONG BEFORE, NOT MERELY ABSENT
 * =================================================
 * Three of these were worse than missing:
 *
 *   0x10008CC0  BrErrorf TERMINATES the process (exit(1)).  The stub returned
 *               0 and let every caller carry on past a condition the original
 *               treats as fatal -- including br_pod's "Memory Corrupted!"
 *               path, which the original does not survive.
 *   0x1003BD50  BrRand fed a constant 0 to every consumer.  slice2_20.c uses
 *               it for particle placement and slice2_15.c for the weather
 *               system; a zero PRNG is not a subtle difference.
 *   0x10002FE0  BrChkFReadOpen returned NULL, and slice2_20.c hands the
 *               result straight to BrChkFRead, which dereferences it.
 *
 * No FLOAT-returning function is ported here, so the "stub leaves xmm0
 * untouched" hazard does not apply to this packet.  The one float-returning
 * stub in the tree (0x1006F310, 7 calls) is still declined -- see below.
 *
 * DECLINED, AND WHY
 * =================
 * 0x10048710  RESOLVED -- WIRED.  Left here because the reasoning below was
 *             right and is worth keeping; only the conclusion changed.
 *
 *             This packet declined it as "37 call sites, the highest static
 *             demand in the whole tree, and it already has a body" on the
 *             grounds that the body (slice6_73.c's `BrOptObjCtor`) writes
 *             br_phase.h's 13-field `BrPhase_` while slice2_26.h and
 *             slice3_31.c called through a 5-field `BrPhase` whose fifth
 *             member `f68` lands where `BrPhase_` has nPages/iPage/aPages[0].
 *             All of that was correct, and so was the verdict that the fix is
 *             an adjudication and not an adapter.
 *
 *             The adjudication has now landed, in br_phase.h's banner.  It
 *             turned out to be FIVE models of this one object, not two:
 *             slice2_26.h's `BrPhase`, slice2_25.h's `BrOptObj`,
 *             slice3_33.h's `BrUiPhase`, slice3_31.h's `BrPhaseVtblExt`
 *             overlay on the vtable, and `BrPhase_` itself.  All are aliases
 *             of `BrPhase_` now, or deleted.
 *
 *             It also turned out the stub was NOT the safe side of the trade.
 *             `BrPhaseCtor` was stubbed, but slice2_25.c and slice5_63.c call
 *             the constructor under its OTHER name, `BrOptObjCtor`, and so
 *             were already reaching the real body at the host link -- one
 *             allocating `sizeof(BrOptObj)` (216 bytes) and the other the raw
 *             `0xC8` literal (200), against a 304-byte object.  Those were
 *             live 88- and 104-byte heap overflows, not benign NULLs.  The
 *             name `BrPhaseCtor` is retired; one address now has one name.
 *
 *             GENERAL LESSON, since this cost the most to find: "the stub is
 *             the safe answer" is only true if the stub is on the path.  Grep
 *             the ADDRESS for every name it has before believing a function
 *             is unwired -- which is CONVENTIONS' aliased-storage rule applied
 *             to code rather than to data.
 *
 * 0x1002BA80  BrSwapRec24Array = slice2_16 `BrRcaFixupArray`, which takes a
 *             `const BrRcaFixup *` the original does not have: the five
 *             globals it reads were lifted into that context and NO instance
 *             of it exists anywhere in the tree.  Same judgement packet 76
 *             applied to five functions and packet 74 to 0x1002BD50.  Note
 *             this packet DOES land the sibling 0x1002BA00, because that one
 *             lifted nothing.
 *
 * 0x1002BF40  BrDlIsRegistered = slice2_17 `BrPtrListContains`, which takes a
 *             `const BrPtrList *`.  slice5_60.c owns `BrPtrList
 *             *g_pBrDlPtrList` -- a POINTER, initialised to NULL -- not the
 *             array at 0x1067B548/0x1067B550.  Binding the adapter to it
 *             would mean either dereferencing NULL or inventing a second
 *             storage for those two addresses.
 *
 * 0x1006F310  7 calls, and it RETURNS FLOAT, so its stub is actively wrong
 *             today.  Still declined for packet 76's reason, unchanged: it
 *             walks the collision grid at 0x11750338/0x117554A0, the one
 *             aliased-storage instance CONVENTIONS records as unresolvable on
 *             this host.
 *
 * PLATFORM LEAVES, declined as not belonging in the portable core:
 *   0x10005E70 (6)  WaitForSingleObject/ReleaseMutex around a net-slot name
 *   0x10035BD1 (3)  shutdown sequence, CloseHandle
 *   0x100027F0 (1)  0x10002870 (1)  MCI dispatch / PostMessageA
 *   0x1003D0B0 (8)  0x1003D210 (8)  0x1003CC70 (4)  DirectPlay over
 *                   GlobalAlloc/GlobalLock -- packet 76 declined these too
 *   0x1005F5A0 (7)  a DirectDraw Blt through a surface vtable
 *   0x1008C000 (22) BrItoa is the CRT's `_itoa`.  Above 0x1007CC40, so
 *                   CONVENTIONS forbids porting it; it belongs in br_crt.
 *   0x1007E8B0 (13) BrXAtExit is the CRT's `atexit`; same rule.
 *
 * DECLINED FOR SCOPE (real game logic, each large enough to be its own
 * packet): 0x100360F0 (4, 2,773 bytes, SEH), 0x10062C50 (3, 1,909 bytes),
 * 0x10075F10 (3, 1,295 bytes).
 *
 * SIGNATURE CONFLICTS FOUND (reported, never silently resolved)
 * ============================================================
 *   0x10002FE0 / 0x10002F90 / 0x10003290
 *       slice2_20.c declares the handle as `FILE **`.  It is really a
 *       TWO-FIELD heap block: `{ FILE *pFile; char *pszName; }`, and both
 *       0x10002F90 and 0x10003290 read the SECOND field.  `FILE **` is the
 *       original's own pun and is correct for field 0 only; on LP64 the
 *       second field is at +8, not +4, so nothing may index it as `ppFile[1]`.
 *       The declarations here are kept byte-for-byte as slice2_20.c has them
 *       so no translation unit can see two different prototypes; the struct
 *       is internal to the .c.  The block is allocated with `sizeof`, not
 *       with the original's literal 8.
 *
 *   0x10008B90
 *       slice2_12.h declares `BrPodWriterMakeName(void *pStream, const char *,
 *       char *)` -- three arguments, the first being the object.  The body
 *       reads TWO stack arguments and `ret 8`, and never touches `ecx`.  Both
 *       call sites (0x100085FD `add ecx,4`, 0x10008A23 `mov ecx,ebp`) DO set
 *       `ecx`, so it is a __thiscall whose `this` is dead -- the three-argument
 *       model is right and the object is simply never read.  Kept, with the
 *       parameter marked unused.
 *
 *   0x10002910
 *       br_audio.h says its replacement "returns -1 when nothing is selected".
 *       The ORIGINAL returns 0 in every not-playing case (`neg/sbb/and`), and
 *       0 is also a legitimate track number.  The two are not interchangeable;
 *       br_audio's contract is the port's, not the binary's.
 *
 * ORIGINAL DEFECTS PRESERVED
 * ==========================
 *   0x10008CC0  formats the message into a fresh 1 KB block and then calls
 *               exit(1) WITHOUT PRINTING IT.  The block leaks and the text is
 *               never seen.  Every "error" this function reports is therefore
 *               a silent exit(1) in the shipped build.  Preserved.
 *   0x10008B90  on an EMPTY source string the backward scan starts at
 *               `pszSrc - 1` and can never reach its `p == pszSrc` terminator,
 *               so it runs backwards through memory until it finds a 0x5C
 *               byte.  Preserved as far as an observable can be: see the
 *               DEVIATION on that function.
 *   0x10008B90  the scan examines `p[-1]`, never the last character, so a
 *               TRAILING backslash is not a separator: "dir\" yields "dir\",
 *               not "".  Not a bug exactly, but it is the opposite of what
 *               every basename routine does.  Pinned by test.
 *   0x100085F0  (not ported here, recorded because this packet proves it)
 *               CleanupName calls 0x10008B90 FIRST and uppercases the
 *               basename.  br_pod.c's `BrPodCleanupName` omits the basename
 *               step entirely.  That is a real divergence in a module the
 *               README calls verified; it is out of scope here and is in the
 *               report.
 *
 * STORAGE THIS PACKET OWNS, AND THE ALIAS TO WATCH
 * ================================================
 *   0x10675540  `g_br675540`.  slice2_16.h ALREADY models this address, as
 *               the `enable` field of its lifted `BrRcaFixup` context ("0 skips
 *               all the copying"), and the image agrees: 0x1002BB86, inside
 *               BrRcaFixupRecord, is `mov eax,[0x10675540] / test / je`.  No
 *               BrRcaFixup instance exists today, so there is no live alias --
 *               but the moment one is constructed, its `enable` MUST be read
 *               from this object rather than given a second storage.  That is
 *               the aliased-storage bug CONVENTIONS describes, written down
 *               before it happens rather than after.
 *   0x104B0360  `g_br4B0360`.  One byte, written by 0x10019240 (1) and
 *               0x10019250 (0) and read twice inside the glyph emitter at
 *               0x10018590 (0x10018672, 0x1001881C), where it selects between
 *               two display-list words.  No other packet models it.  Follows
 *               slice5_63.h's precedent for the sibling byte 0x104B0358.
 *   0x10220C3C  `g_brCdMediaOk`.  Read by 21 sites across the CD module as a
 *               plain non-zero gate.  br_data.c owns its four neighbours
 *               (0x10220C38/C44/CD0/CD4); this one had no storage because
 *               nothing that reads it was ported until now.  Genuine .bss in
 *               the original -- 0x10220C3C is past the end of .data's raw
 *               bytes -- so zero here is the image's value, not a default.
 */
#ifndef SLICE6_78_H
#define SLICE6_78_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* ==========================================================================
 * 1. Storage owned here.  See the banner for the alias each one carries.
 * ========================================================================== */

/* 0x10675540.  Non-zero enables the record copying in BrRcaFixupRecord. */
extern int32_t g_br675540;

/* 0x104B0360.  One byte; the glyph emitter's two-way mode select. */
extern uint8_t g_br4B0360;

/* 0x10220C3C.  Non-zero means the CD module has a usable medium/track map. */
extern int32_t g_brCdMediaOk;

/* ==========================================================================
 * 2. The 0x100024C0 seam
 *
 * 0x10002910 dispatches on `g_brCdEnabled == 1` to one of two backends.  The
 * FALSE arm (0x10002490) is portable and is transcribed below.  The TRUE arm
 * (0x100024C0) issues an MCI_STATUS through mciSendCommandA and is platform
 * code, so it is a hook rather than a body -- the same shape slice6_76.c uses
 * for the 0x100029F0 seam.
 *
 * NOTE WHICH ARM SHIPS: br_data.c has `g_brCdEnabled = 2`, read out of the
 * image (0x100940A4 is `02 00 00 00` in .data).  The test is `cmp ...,1 /
 * jne`, NOT a zero test, so the shipped build takes the PORTABLE arm and the
 * hook is never called.  slice5_63.h records the same fact for 0x100027C0.
 *
 * A NULL hook returns 0, which is also what the portable arm returns when it
 * declines.  See the br_audio.h conflict in the banner: 0, not -1.
 * ========================================================================== */
extern int (*g_pfnBrCdTrackGet0024C0)(void);

/* ==========================================================================
 * 3. Transcribed functions
 * ========================================================================== */

/* 0x10002490 -- the portable CD-track backend, exposed because it is the arm
 * the shipped configuration actually takes and is worth testing directly.
 *
 * Returns g_brCdTrackCur only when all three of g_brCdEnabled, g_brCdPlaying
 * and g_brCdMediaOk are non-zero; 0 otherwise.  The third test is the
 * `neg / sbb / and` idiom, i.e. a mask, not a branch -- so a non-zero gate
 * with a zero track still yields 0 and the two cases are indistinguishable. */
int BrCdTrackGetEar(void);

/* 0x10002910 -- dispatch.  `g_brCdEnabled == 1` selects the MCI hook, ANY
 * other value (including the shipped 2) selects BrCdTrackGetEar.  Both arms
 * are TAIL JUMPS in the original, so the dispatcher adds no behaviour. */
int BrCdTrackGet(void);

/* 0x10008CC0 -- FORMAT AND DIE.  Allocates 0x400 bytes, formats into them,
 * and calls exit(1).  It never prints, and the block leaks; see the banner.
 * DOES NOT RETURN.  slice2_13.c's declaration is matched exactly. */
void BrErrorf(const char *pszFmt, ...);

/* 0x10002FE0  CHK_FReadOpen.  Opens pPath "rb" and returns a two-field heap
 * handle whose FIRST field is the FILE *.  Exits on failure, after writing
 * `c:\RallyError.txt` -- see the note in the .c.
 *
 * The `FILE **` is the ORIGINAL'S OWN PUN and is kept so that no translation
 * unit sees two prototypes for this symbol; it is valid for field 0 only.
 * Never index it: field 1 is at +8 on LP64, not +4. */
FILE **BrChkFReadOpen(const char *pPath);

/* 0x10002F90  CHK_FileSize.  ftell / fseek(END) / ftell / fseek(back).
 *
 * The restore is a genuine round trip: the stream is left exactly where it
 * was, which is why callers may size an already-positioned file.  No error
 * check anywhere -- a ftell of -1 is returned as -1. */
int BrChkFileSize(FILE **ppFile);

/* 0x10003290  CHK_FClose.  fclose, then free the name, then free the handle.
 * Exits(1) if fclose reports EOF.  Note the ORDER: the name is freed before
 * the handle, and the diagnostic reads the name BEFORE either free. */
void BrChkFClose(FILE **ppFile);

/* 0x10008B90  Copy the part of pszSrc after its last '\\' into pszDst.
 *
 * pStream is the original's __thiscall `this`.  Both call sites set it and
 * the body never reads it; see the signature-conflict note in the banner.
 *
 * pszDst must have room for the whole of pszSrc: the routine copies the
 * entire string when there is no separator, and the original bounds nothing.
 *
 * The last character is never examined as a separator, so a trailing '\\' is
 * kept.  Pinned by test. */
void BrPodWriterMakeName(void *pStream, const char *pszSrc, char *pszDst);

/* 0x1002B9D0  g_br675540 = v.  The whole function. */
void BrSegSetFlag(uint32_t v);

/* 0x10019240  g_br4B0360 = 1 */
void BrSub_10019240(void);
/* 0x10019250  g_br4B0360 = 0 */
void BrSub_10019250(void);

/* ==========================================================================
 * 4. Adapters
 *
 * Prototypes are copied VERBATIM from the header that already declares each
 * stub name, so a later divergence surfaces as a compile error at the owner
 * rather than as silent disagreement here.  Each forwards to the ONE body
 * that already exists for the address -- never a second body.
 * ========================================================================== */

/* 0x1002F900 -- slice2_18.h:139.  Seventeen cdecl arguments. */
void BrGfx2F900(uint32_t *pCmd,
                int32_t a01, int32_t a02, int32_t a03, int32_t a04,
                int32_t a05, int32_t a06, int32_t a07, int32_t a08,
                int32_t a09, int32_t a10, int32_t a11, int32_t a12,
                int32_t a13, int32_t a14, int32_t a15, int32_t a16);

/* 0x100192F0 -- slice2_14.c:209. */
void BrTextSetSize(int size);
/* 0x10019260 -- slice2_14.c:192. */
void BrTextFlag358Clear(void);
/* 0x10019270 -- slice2_14.c:198. */
void BrTextAlignCentre(void);
/* 0x100192A0 -- slice2_14.c:188.  Two colour triples. */
void BrTextSetColor6(int a, int b, int c, int d, int e, int f);

/* 0x1003BD50 -- slice2_20.c:70.  The 16807 LCG modulo 2^27; the result never
 * has its sign bit set despite the int return type. */
int BrRand(void);

/* 0x1002BA00 -- slice2_20.c:32.  n 8-byte records of four u16s each. */
void BrSwapRec8Array(void *pv, int n);

#endif /* SLICE6_78_H */
