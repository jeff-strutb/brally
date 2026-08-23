/* slice2_20.c -- 0x100370D0-0x10039020 from BRD3D.dll.  See slice2_20.h.
 *
 * All field access here goes through memcpy-based helpers rather than casting
 * the file image to a struct pointer.  That is not defensive style, it is
 * required: the images are N64 data whose alignment the host cannot rely on,
 * and reading them by overlay would also make the byte order depend on the
 * host, which is exactly the bug this whole range exists to avoid.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <string.h>

#include "slice2_20.h"
#include "br_seg.h"
#include "br_bits.h"
#include "br_vec.h"

/* ==========================================================================
 * Cross-slice dependencies
 * ========================================================================== */

/* XSLICE 0x1002B9D0 */
/* Stores its argument to the global at 0x10675540.  Purpose unknown; called
 * once at the head of each fixup pass (0 for cars, 1 for tracks). */
extern void BrSegSetFlag(uint32_t v);

/* XSLICE 0x1002B9E0 */
/* Byte-swap n u16s in place.  n <= 0 is a no-op. */
extern void BrSwapU16Array(void *pv, int n);

/* XSLICE 0x1002BA00 */
/* Byte-swap n 8-byte records (four u16s each). */
extern void BrSwapRec8Array(void *pv, int n);

/* XSLICE 0x1002BA60 */
/* Byte-swap n Vec3s (stride 0x0C), i.e. n calls to BrSwapVec3. */
extern void BrSwapVec3Array(void *pv, int n);

/* XSLICE 0x1002BA80 */
/* Byte-swap and rebase n records of stride 0x24 (body at 0x1002BAA0). */
extern void BrSwapRec24Array(void *pv, int n);

/* XSLICE 0x1002BF40 */
/* Non-zero if pv is NULL or already in the registered display-list table at
 * 0x1067B550.  Callers use `== 0` to mean "not seen yet". */
extern int BrDlIsRegistered(const void *pv);

/* XSLICE 0x1002BF80 */
/* Register and byte-swap a display list. */
extern void BrDlRegister(void *pv);
extern void BrSegPtrFixup(uint32_t *p);

/* XSLICE 0x10074DC0 */
extern void BrSub10074DC0(int n);
/* XSLICE 0x10074E00 */
extern void BrSub10074E00(void);
/* XSLICE 0x1003445A */
extern void BrSub1003445A(void *pv);
/* XSLICE 0x10035BD1 */
extern void BrSub10035BD1(void);
/* XSLICE 0x10061010 */
extern void BrSub10061010(int iCar, int fPreview);
/* XSLICE 0x10037990 */
extern void BrSub10037990(const char *pszPath);

/* XSLICE 0x1003B170 */
/* One vector in, one float out.  Used here and at 0x10037B10 as `if (f != 0)
 * r = 1/f`, so almost certainly a length -- not verified in this packet. */
extern float BrVec3Len(const BrVec3 *pV);

/* XSLICE 0x1003BD50 */
extern int BrRand(void);

/* Checked stdio wrappers -- names and signatures as already declared by
 * slice1_01.h.  NOTE the FILE ** (not FILE *): the originals dereference it. */
/* XSLICE 0x10003170 */
extern void *BrChkFRead(void *pDst, size_t size, size_t count, FILE **ppFile);
/* XSLICE 0x10003320 */
extern int BrChkFileExists(const char *pPath);
/* XSLICE 0x10002FE0 */
extern FILE **BrChkFReadOpen(const char *pPath);
/* XSLICE 0x10002F90 */
extern int BrChkFileSize(FILE **ppFile);
/* XSLICE 0x10003290 */
extern void BrChkFClose(FILE **ppFile);
/* XSLICE 0x1007C830 */
extern int BrSprintf(char *pDst, const char *pszFmt, ...);
/* XSLICE 0x10008CF0 */
extern void BrFatal(const char *pszMsg);

/* Backend dispatch, three function pointers in the DLL's data segment.  All
 * cdecl.  Handles are kept as uint32_t because the originals are 32-bit
 * values living inside the file image. */
/* XSLICE 0x118AA084 */
extern uint32_t (*g_pfn18AA084)(uint32_t hCtx, uint32_t hSrc, void *pDesc);
/* XSLICE 0x118AA0C4 */
extern void (*g_pfn18AA0C4)(void *pv);
/* XSLICE 0x118AA0C8 */
extern void (*g_pfn18AA0C8)(void *pRec, int flag);
/* XSLICE 0x118AA0CC */
extern void (*g_pfn18AA0CC)(void *pTable, int cRecords);

/* Plain globals. */
/* XSLICE 0x106C7C3C */ extern void    *g_p6C7C3C;
/* XSLICE 0x106C661C */ extern int      g_i6C661C;
/* XSLICE 0x106C6624 */ extern int      g_i6C6624;
/* XSLICE 0x100AC300 */ extern int      g_i0AC300;
/* XSLICE 0x104BBE08 */ extern int      g_i4BBE08;
/* XSLICE 0x100B8C90 */ extern int      g_i0B8C90;
/* XSLICE 0x10AA3444 */ extern int      g_i10AA3444;
/* XSLICE 0x10AA3460 */ extern int      g_i10AA3460;
/* XSLICE 0x100C12A0 */ extern uint8_t  g_ab0C12A0[];
/* XSLICE 0x100B84F8 */ extern const char *const g_apszCarFiles[];
/* XSLICE 0x100B80B8 */ extern const char *const g_apszTrackFiles[];
/* XSLICE 0x10220B20 */ extern uint32_t g_a220B20[0x46];

/* Particle-style pool, see slice2_20.h. */
/* XSLICE 0x10A99BB8 */ extern BrPoolNode g_aPoolNodes[];
/* XSLICE 0x10A99BA8 */ extern uint16_t   g_uPoolFree;
/* XSLICE 0x10A99BB0 */ extern uint16_t   g_uPoolHead;
/* XSLICE 0x106C2CFC */ extern float      g_f6C2CFC;

/* ==========================================================================
 * Endian / unaligned helpers
 * ========================================================================== */

void BrSwap4(void *pv)
{
    uint8_t *p = (uint8_t *)pv;
    uint8_t t;
    t = p[0]; p[0] = p[3]; p[3] = t;
    t = p[1]; p[1] = p[2]; p[2] = t;
}

void BrSwap2(void *pv)
{
    uint8_t *p = (uint8_t *)pv;
    uint8_t t = p[0]; p[0] = p[1]; p[1] = t;
}

uint32_t BrRead32BE(const void *pv)
{
    const uint8_t *p = (const uint8_t *)pv;
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}

static uint32_t BrRd32(const void *pv)
{
    uint32_t v;
    memcpy(&v, pv, sizeof v);
    return v;
}

static void BrWr32(void *pv, uint32_t v)
{
    memcpy(pv, &v, sizeof v);
}

static uint16_t BrRd16(const void *pv)
{
    uint16_t v;
    memcpy(&v, pv, sizeof v);
    return v;
}

static void BrWr16(void *pv, uint16_t v)
{
    memcpy(pv, &v, sizeof v);
}

/* Big-endian u32 at pv, written back as a host dword.  This is the second of
 * the two swap idioms in the original; numerically identical to BrSwap4. */
static void BrLoad32BE(void *pv)
{
    BrWr32(pv, BrRead32BE(pv));
}

/* Big-endian u16 at pv, written back as a host word. */
static void BrLoad16BE(void *pv)
{
    const uint8_t *p = (const uint8_t *)pv;
    BrWr16(pv, (uint16_t)(((uint16_t)p[0] << 8) | p[1]));
}

/* ==========================================================================
 * Load environment
 * ========================================================================== */

BrLoadEnv g_BrLoad;

/* The segment map the original kept at 0x1057553C / 0x10575538. */
static BrSegMap s_seg;

void *BrLoadResolve(uint32_t uFixedUp)
{
    uint32_t off;

    if (uFixedUp == 0 || g_BrLoad.pImage == NULL)
        return NULL;
    if (uFixedUp < g_BrLoad.uBase32)
        return NULL;
    off = uFixedUp - g_BrLoad.uBase32;
    /* DEVIATION: the original dereferences whatever BrSegFixup produced. The
     * port refuses anything outside the image so that a truncated or hostile
     * file cannot walk off the end.  BrSegFixup already turns unresolvable
     * values into 0, so a NULL here reaches the caller's existing null path. */
    if (off >= g_BrLoad.cbImage)
        return NULL;
    return g_BrLoad.pImage + off;
}

/* Rebase the dword at pv in place, matching `push pv / call 0x1002B970`. */
static void BrFixupAt(void *pv)
{
    uint32_t v = BrRd32(pv);
    BrSegFixup(&s_seg, &v);
    BrWr32(pv, v);
}

/* The dword at pv, already rebased, as a host pointer. */
static void *BrPtrAt(const void *pv)
{
    return BrLoadResolve(BrRd32(pv));
}

/* ==========================================================================
 * 0x100370D0  BrRcaFixup
 * ========================================================================== */

/* WHAT IT DOES: makes a freshly loaded car file usable. Boss Rally's PC
 * version reads the N64's data files exactly as they are, so every number in
 * them is stored the wrong way round and every internal reference points at
 * an N64 address. This walks the whole car -- geometry, textures,
 * descriptors, transforms -- turning each field around and rewriting each
 * reference to point at where the data actually sits in memory now. */
/* @implements 0x100370D0 d3d BrRcaFixup */
void BrRcaFixup(void *pvFile, size_t cbFile)
{
    uint8_t *pFile = (uint8_t *)pvFile;
    uint8_t *pData = pFile + 0x8000;          /* edi: the N64 struct */
    uint8_t *p;
    int i, j;

    /* DEVIATION: the original passes the real address of pFile + 0x8000 as
     * the host base.  A 64-bit host address does not fit in the u32 that
     * BrSegFixup writes back into the image, so the port installs the
     * surrogate BR_LOAD_BASE32 and remembers the matching host pointer. */
    g_BrLoad.pImage  = pData;
    g_BrLoad.uBase32 = BR_LOAD_BASE32;
    g_BrLoad.cbImage = (cbFile > 0x8000u) ? (cbFile - 0x8000u) : 0u;

    BrSegSetBases(&s_seg, 0x803C8000u, BR_LOAD_BASE32);
    BrSegSetFlag(0);

    g_p6C7C3C = pFile;
    BrWr32(pFile + 0x7C, 0);

    BrLoad32BE(pData + 0x00);
    BrSwap4(pData + 0x04);  BrFixupAt(pData + 0x04);
    BrLoad32BE(pData + 0x08);
    BrSwap4(pData + 0x0C);  BrFixupAt(pData + 0x0C);
    BrLoad32BE(pData + 0x10);
    BrSwap4(pData + 0x14);  BrFixupAt(pData + 0x14);

    /* [+0x14] is a table of [+0x10] records, stride 0x24. */
    BrSwapRec24Array(BrPtrAt(pData + 0x14), (int)BrRd32(pData + 0x10));

    {
        /* Patch four u16s in the record selected by the byte at +0x11A. */
        uint8_t *pTbl = (uint8_t *)BrPtrAt(pData + 0x14);
        uint8_t *pDesc = NULL;

        if (pTbl != NULL)
            pDesc = (uint8_t *)BrLoadResolve(BrRd32(pTbl + pData[0x11A] * 0x24 + 4));

        if (pDesc != NULL) {
            uint8_t *q = pDesc + 0x18;
            for (i = 0; i < 4; ++i, q += 2) {
                if (g_i6C661C == 0 && g_i6C6624 == 0) {
                    /* DEVIATION: the original is `or byte [q+1], 1`, which on
                     * x86 sets bit 8 of the little-endian u16.  Expressed on
                     * the u16 so it means the same thing on any host. */
                    BrWr16(q, (uint16_t)(BrRd16(q) | 0x0100u));
                } else {
                    BrWr16(q, (uint16_t)(BrRd16(q) & 0xFEFFu));
                }
            }
        }
    }

    BrSub10074E00();

    /* Thirty display-list pointers at +0x18, walked as 3 x 10. */
    p = pData + 0x18;
    for (j = 0; j < 3; ++j) {
        for (i = 0; i < 10; ++i, p += 4) {
            void *pDl;
            BrSwap4(p);
            BrFixupAt(p);
            pDl = BrPtrAt(p);
            if (BrDlIsRegistered(pDl) == 0) {
                BrDlRegister(pDl);
                BrSub10074DC0(2);
                g_pfn18AA0C4(pDl);
            }
        }
    }

    /* GOTCHA (see the header): the swap is on +0x90, the rebase on +0x94. */
    BrSwap4(pData + 0x90);
    BrFixupAt(pData + 0x94);

    for (i = 0; i < 6; ++i)
        BrSwap4(pData + 0x98 + i * 4);          /* +0x98 .. +0xAF */
    for (i = 0; i < 3; ++i)
        BrSwap4(pData + 0xB0 + i * 4);          /* +0xB0 .. +0xBB */

    /* Nine more display-list pointers at +0xBC, walked as 3 x 3. */
    p = pData + 0xBC;
    for (j = 0; j < 3; ++j) {
        for (i = 0; i < 3; ++i, p += 4) {
            void *pDl;
            BrSwap4(p);
            BrFixupAt(p);
            pDl = BrPtrAt(p);
            if (BrDlIsRegistered(pDl) == 0) {
                BrDlRegister(pDl);
                BrSub10074DC0(2);
                g_pfn18AA0C4(pDl);
            }
        }
    }

    BrSub10074DC0(2);

    {
        uint8_t *pTbl = (uint8_t *)BrPtrAt(pData + 0x14);
        uint8_t *pRec;
        uint8_t *pDesc;

        /* Records 6, 3 and 5 of the same table, by byte offset. */
        if (pTbl != NULL) {
            g_pfn18AA0C8(pTbl + 0xD8, 0);
            g_pfn18AA0C8(pTbl + 0x6C, 0);
            g_pfn18AA0C8(pTbl + 0xB4, 0);
        }

        pRec  = (pTbl != NULL) ? pTbl + pData[0x11B] * 0x24 : NULL;
        pDesc = (pRec != NULL) ? (uint8_t *)BrLoadResolve(BrRd32(pRec + 4)) : NULL;

        if (pDesc != NULL && g_i0AC300 == 0) {
            /* The handle is read BEFORE the call and re-read after: the
             * callee is allowed to replace it, and both values are used. */
            uint32_t hOld = BrRd32(pRec);
            uint32_t hNew;

            g_pfn18AA0C8(pRec, 1);
            hNew = BrRd32(pRec);
            BrWr32(pFile + 0x80, hNew);

            if (g_i6C661C == 0 && g_i6C6624 == 0) {
                BrWr16(pDesc + 0x1E, 0x0190);
                BrWr16(pDesc + 0x14, 0x01A0);
            } else {
                BrWr16(pDesc + 0x1E, 0x0070);
                BrWr16(pDesc + 0x14, 0x8290);
            }

            BrWr16(pDesc + 0x1C, 0x0190);
            BrWr16(pDesc + 0x1A, BrRd16(pDesc + 0x1E));
            BrWr16(pDesc + 0x12, 0x01A0);
            BrWr16(pDesc + 0x10, BrRd16(pDesc + 0x14));
            BrWr16(pDesc + 0x18, 0x8179);
            BrWr16(pDesc + 0x0E, 0x4192);
            BrWr16(pDesc + 0x16, 0x6BAD);
            BrWr16(pDesc + 0x0C, 0x31C6);
            BrWr32(pFile + 0x84,
                   g_pfn18AA084(BrRd32(pFile + 0x80), hOld, pDesc));

            BrWr16(pDesc + 0x1C, 0x00C0);
            BrWr16(pDesc + 0x1A, 0x00C0);
            BrWr16(pDesc + 0x12, 0x04F9);
            BrWr16(pDesc + 0x10, 0x04F9);
            BrWr16(pDesc + 0x16, 0x6BAD);
            BrWr16(pDesc + 0x0C, 0x31C6);
            BrWr32(pFile + 0x88,
                   g_pfn18AA084(BrRd32(pFile + 0x80), hOld, pDesc));

            BrWr16(pDesc + 0x1C, 0x0190);
            BrWr16(pDesc + 0x1A, BrRd16(pDesc + 0x1E));
            BrWr16(pDesc + 0x12, 0x01A0);
            BrWr16(pDesc + 0x10, BrRd16(pDesc + 0x14));
            BrWr16(pDesc + 0x16, 0x38E7);
            BrWr16(pDesc + 0x0C, 0xFEFF);
            BrWr32(pFile + 0x8C,
                   g_pfn18AA084(BrRd32(pFile + 0x80), hOld, pDesc));

            BrWr16(pDesc + 0x12, 0x04F9);
            BrWr16(pDesc + 0x1C, 0x00C0);
            BrWr16(pDesc + 0x1A, 0x00C0);
            BrWr16(pDesc + 0x10, 0x04F9);
            BrWr16(pDesc + 0x16, 0x38E7);
            BrWr16(pDesc + 0x0C, 0xFEFF);
            BrWr32(pFile + 0x90,
                   g_pfn18AA084(BrRd32(pFile + 0x80), hOld, pDesc));
        } else {
            BrWr32(pFile + 0x90, 0);
            BrWr32(pFile + 0x8C, 0);
            BrWr32(pFile + 0x88, 0);
            BrWr32(pFile + 0x84, 0);
            BrWr32(pFile + 0x80, 0);
        }
    }

    /* Twelve dwords at +0xE0, walked as 4 x 3. */
    p = pData + 0xE0;
    for (j = 0; j < 4; ++j)
        for (i = 0; i < 3; ++i, p += 4)
            BrSwap4(p);

    BrSwap4(pData + 0x11C);
    BrFixupAt(pData + 0x11C);
}

/* ==========================================================================
 * 0x100378B0  BrFileReadInto
 * ========================================================================== */

/* WHAT IT DOES: reads a whole file into a buffer. Asking for a negative size
 * means "however long the file is". A positive size is used as given, with
 * no check against either the file's real length or the buffer's, and a
 * missing file is complained about and then read from anyway. */
/* @implements 0x100378B0 d3d BrFileReadInto */
void BrFileReadInto(void *pvDest, const char *pszPath, int cbMax)
{
    char szMsg[0x200];
    FILE **ppFile;
    int cb = cbMax;

    if (BrChkFileExists(pszPath) == 0) {
        BrSprintf(szMsg, "File %s missing", pszPath);
        BrFatal(szMsg);
    }

    ppFile = BrChkFReadOpen(pszPath);
    /* Negative means "however long the file is".  Non-negative is used as-is,
     * with no clamp against either the file or the destination. */
    if (cb < 0)
        cb = BrChkFileSize(ppFile);

    BrChkFRead(pvDest, 1, (size_t)cb, ppFile);
    BrChkFClose(ppFile);
}

/* ==========================================================================
 * 0x10037740  BrRcaLoadCar
 * ========================================================================== */

void BrRcaLoadCar(void *pvDest, size_t cbDest, int iCar)
{
    char szMsg[0x100];
    char szPath[0x400];
    int  fSaved = 0;
    int  fPreview;

    g_i10AA3444 = iCar;

    /* Identity test against the static scratch buffer, not a content test. */
    fPreview = ((uint8_t *)pvDest == g_ab0C12A0);
    if (!fPreview) {
        fSaved = g_i0B8C90;
        if (fSaved == 0)
            g_i0B8C90 = 1;
    }
    BrSub10061010(iCar, fPreview ? 1 : 0);

    g_i10AA3460 = 0;

    /* DEVIATION: the original builds this with inline strlen/movsd into a
     * 0x400-byte frame slot and cannot overflow it for any shipped name.
     * strcpy/strcat is the same operation; the length is not checked here
     * either, matching the original. */
    strcpy(szPath, "cars/");
    strcat(szPath, g_apszCarFiles[iCar]);
    strcat(szPath, ".rca");

    BrFileReadInto(pvDest, szPath, -1);

    if (memcmp(pvDest, "RCar", 4) != 0) {
        /* DEVIATION: the original passes the destination buffer as its own
         * %s argument -- both `lea`s produce esp+0x14 -- and that buffer is
         * uninitialised at this point.  That is undefined behaviour with no
         * portable equivalent; the port formats an empty string, which is
         * what the original prints whenever the first stack byte is 0. */
        BrSprintf(szMsg, "not a car file: %s", "");
        BrFatal(szMsg);
    }

    /* DEVIATION: cbDest is a port addition (the original is two-argument);
     * it exists only to give BrRcaFixup a bound for pointer resolution. */
    BrRcaFixup(pvDest, cbDest);

    if (!fPreview)
        g_i0B8C90 = fSaved;
}

/* ==========================================================================
 * 0x10031140 (Glide) / 0x10037A90 (D3D)  BrTrackLoadHandling
 * ========================================================================== */

/* BUILD DIVERGENCE -- THE EXTENSION, and the port had the wrong one.
 *
 * The two builds are the same routine (config/shared.csv pairs them, matched
 * by callsite) with ONE string changed, and each string exists in only one
 * image:
 *
 *     Glide  0x1003117B  mov edi, 0x100AA338   -> ".hnt"
 *     D3D    0x10037AC9  mov edi, 0x100AABA8   -> ".hnd"
 *
 * Searching each image for the OTHER literal finds nothing, so this is a real
 * edit between the builds and not one shared constant read twice.
 *
 * WHICH ONE IS RIGHT IS NOT A COIN FLIP -- THE DISC SETTLES IT.  The extracted
 * assets under testdata/tracks/ are `desert.hnt` and `coast.hnt`, and there is
 * no `.hnd` anywhere on the disc.  So the shipped data is what the Glide build
 * asks for, and a D3D build run against this disc would open a file that does
 * not exist.  Glide is this project's declared reference (CONVENTIONS.md,
 * "Source precedence"), the asset evidence agrees with it independently, and
 * this body therefore transcribes Glide and carries the Glide claim.
 *
 * THE CLAIM MOVED WITH THE STRING.  While this said ".hnd" it was an honest
 * transcription of D3D 0x10037A90 and was labelled as one; the defect was
 * which build the port had chosen, not a mislabelled body.  Saying ".hnt"
 * under a `d3d` tag would be a body that matches neither image. */
/* WHAT IT DOES: loads a track's handling file -- the physics settings for
 * driving on it. It builds the track's path, swaps the extension for the
 * handling one, and hands it on to be read. */
/* @implements 0x10031140 glide BrTrackLoadHandling */
void BrTrackLoadHandling(int iTrack)
{
    char szPath[0x400];
    char *pExt;

    BrSprintf(szPath, "%s%s", "tracks/", g_apszTrackFiles[iTrack]);

    pExt = strrchr(szPath, '.');
    /* DEVIATION: the original does not test for NULL and would write through
     * it.  Every shipped name has an extension, so the guard is unreachable
     * in practice. */
    if (pExt != NULL)
        strcpy(pExt, BR_TRACK_HANDLING_EXT);

    BrSub10037990(szPath);
}

/* ==========================================================================
 * 0x10038510  BrTrackHdrRead
 * ========================================================================== */

/* WHAT IT DOES: reads a track's header and turns it the right way round: the
 * counts and sizes get their bytes reversed, and every reference in it is
 * rebased onto real memory. One word in the middle is skipped entirely,
 * which is the only gap in the whole header and is in the original. */
/* @implements 0x10038510 d3d BrTrackHdrRead */
void BrTrackHdrRead(void *pvHdr, FILE **ppFile)
{
    static const uint16_t s_aFixup[] = {
        0x0C, 0x14, 0x1C, 0x20, 0x24, 0x50, 0x54, 0x58, 0x5C, 0x60,
        0x68, 0x6C, 0x70, 0x74, 0x78, 0x84, 0x8C, 0x90, 0x94
    };
    uint8_t *h = (uint8_t *)pvHdr;
    size_t   i;
    int      k;

    BrChkFRead(h, 1, 0x230, ppFile);

    BrLoad32BE(h + 0x00);
    BrLoad32BE(h + 0x04);
    BrLoad32BE(h + 0x08);
    BrSwap4   (h + 0x0C);
    BrLoad32BE(h + 0x10);
    BrSwap4   (h + 0x14);
    BrLoad32BE(h + 0x18);
    BrSwap4   (h + 0x1C);

    for (k = 0x20; k < 0x50; k += 4)     /* +0x20 .. +0x4C */
        BrSwap4(h + k);

    for (k = 0x50; k < 0x64; k += 4)     /* +0x50 .. +0x60 */
        BrSwap4(h + k);

    BrLoad32BE(h + 0x64);

    for (k = 0x68; k < 0x7C; k += 4)     /* +0x68 .. +0x78 */
        BrSwap4(h + k);

    BrLoad32BE(h + 0x7C);
    /* GOTCHA: +0x80 is skipped -- the original jumps straight from +0x7C to
     * +0x84.  It is the one untouched dword in +0x00..+0x164. */
    BrSwap4   (h + 0x84);
    BrLoad32BE(h + 0x88);
    BrSwap4   (h + 0x8C);
    BrSwap4   (h + 0x90);
    BrSwap4   (h + 0x94);

    /* Ten rows of five dwords: +0x98 .. +0x15F. */
    for (k = 0; k < 10; ++k) {
        int c;
        for (c = 0; c < 5; ++c)
            BrSwap4(h + 0x98 + k * 0x14 + c * 4);
    }

    BrLoad32BE(h + 0x160);

    for (i = 0; i < sizeof s_aFixup / sizeof s_aFixup[0]; ++i)
        BrFixupAt(h + s_aFixup[i]);
}

/* ==========================================================================
 * 0x10038380 / 0x10038410 / 0x100382A0 / 0x10038250 -- geometry
 * ========================================================================== */

void BrSwapRec28(void *pvRec)
{
    uint8_t *p = (uint8_t *)pvRec;
    BrSwapVec3(p + 0x00);
    BrSwapVec3(p + 0x0C);
    BrSwapVec3(p + 0x18);
    BrSwap4   (p + 0x24);
}

void BrTrackFixupList84(void *pvHdr)
{
    uint8_t *h = (uint8_t *)pvHdr;
    uint8_t *p = (uint8_t *)BrPtrAt(h + 0x84);
    int i;

    if (p == NULL)
        return;
    /* The count is re-read from the header every iteration in the original. */
    for (i = 0; i < (int)BrRd32(h + 0x88); ++i, p += 0x0C)
        BrSwapVec3(p);
}

void BrTrackFixupNode(void *pvNode)
{
    uint8_t *p = (uint8_t *)pvNode;
    uint8_t *q;
    int i, cRec;

    BrSwap4(p + 0x00); BrFixupAt(p + 0x00);
    BrSwap4(p + 0x04); BrFixupAt(p + 0x04);
    BrSwap4(p + 0x08); BrFixupAt(p + 0x08);
    BrSwap4(p + 0x0C); BrFixupAt(p + 0x0C);

    BrLoad16BE(p + 0x14);
    BrLoad16BE(p + 0x16);

    BrSwapRec28(p + 0x18);

    /* GOTCHA: the original's guard here is an unsigned "< 0" that can never
     * fire, and the loop bound is <=, so this runs count+1 times even when
     * the count is zero. */
    cRec = BrRd16(p + 0x14);
    q = p + 0x40;
    for (i = 0; i <= cRec; ++i, q += 0x28)
        BrSwapRec28(q);
}

void BrTrackFixupList78(void *pvHdr)
{
    uint8_t *h = (uint8_t *)pvHdr;
    uint8_t *p = (uint8_t *)BrPtrAt(h + 0x78);
    int i;

    if (p == NULL)
        return;
    for (i = 0; i < (int)BrRd32(h + 0x7C); ++i, p += 4) {
        void *pNode;
        BrSwap4(p);
        BrFixupAt(p);
        pNode = BrPtrAt(p);
        if (pNode != NULL)
            BrTrackFixupNode(pNode);
    }
}

/* ==========================================================================
 * 0x10038010 / 0x10037FE0 -- the 0x54-byte object records
 * ========================================================================== */

/* WHAT IT DOES: prepares one drawable object of a track: turns all its
 * numbers round, rebases the one reference it carries -- which points at the
 * object's drawing commands -- and then registers those commands and hands
 * them to the graphics backend.
 *
 * The original UNROLLS all eighteen dword reversals and six u16 swaps
 * inline -- no loop, no helper -- and re-reads the +0x44 slot for both the
 * register call and the texture scan (re-deref idiom, docs/VC5-IDIOMS.md).
 *
 * NOT MATCHING by 24 bytes in the u16 window +0x1C0..0x1F0: the original
 * loads each pair's LOW byte first, VC5 here loads the highs first.  Three
 * spellings (or-order, statement split) compile byte-identical, so the load
 * order is scheduler-canonical -- allocator-residue class, do not grind. */
/* @implements 0x100316D0 glide BrTrackFixupRec54 */
void BrTrackFixupRec54(void *pvRec)
{
    uint8_t *p = (uint8_t *)pvRec;
    uint8_t  t, u;

    t = p[0x00]; u = p[0x03]; p[0x03] = t; p[0x00] = u;
    t = p[0x01]; u = p[0x02]; p[0x02] = t; p[0x01] = u;
    t = p[0x04]; u = p[0x07]; p[0x07] = t; p[0x04] = u;
    t = p[0x05]; u = p[0x06]; p[0x06] = t; p[0x05] = u;
    t = p[0x08]; u = p[0x0B]; p[0x0B] = t; p[0x08] = u;
    t = p[0x09]; u = p[0x0A]; p[0x0A] = t; p[0x09] = u;
    t = p[0x0C]; u = p[0x0F]; p[0x0F] = t; p[0x0C] = u;
    t = p[0x0D]; u = p[0x0E]; p[0x0E] = t; p[0x0D] = u;
    t = p[0x10]; u = p[0x13]; p[0x13] = t; p[0x10] = u;
    t = p[0x11]; u = p[0x12]; p[0x12] = t; p[0x11] = u;
    t = p[0x14]; u = p[0x17]; p[0x17] = t; p[0x14] = u;
    t = p[0x15]; u = p[0x16]; p[0x16] = t; p[0x15] = u;
    t = p[0x18]; u = p[0x1B]; p[0x1B] = t; p[0x18] = u;
    t = p[0x19]; u = p[0x1A]; p[0x1A] = t; p[0x19] = u;
    t = p[0x1C]; u = p[0x1F]; p[0x1F] = t; p[0x1C] = u;
    t = p[0x1D]; u = p[0x1E]; p[0x1E] = t; p[0x1D] = u;
    t = p[0x20]; u = p[0x23]; p[0x23] = t; p[0x20] = u;
    t = p[0x21]; u = p[0x22]; p[0x22] = t; p[0x21] = u;
    t = p[0x24]; u = p[0x27]; p[0x27] = t; p[0x24] = u;
    t = p[0x25]; u = p[0x26]; p[0x26] = t; p[0x25] = u;
    t = p[0x28]; u = p[0x2B]; p[0x2B] = t; p[0x28] = u;
    t = p[0x29]; u = p[0x2A]; p[0x2A] = t; p[0x29] = u;
    t = p[0x2C]; u = p[0x2F]; p[0x2F] = t; p[0x2C] = u;
    t = p[0x2D]; u = p[0x2E]; p[0x2E] = t; p[0x2D] = u;
    t = p[0x30]; u = p[0x33]; p[0x33] = t; p[0x30] = u;
    t = p[0x31]; u = p[0x32]; p[0x32] = t; p[0x31] = u;
    t = p[0x34]; u = p[0x37]; p[0x37] = t; p[0x34] = u;
    t = p[0x35]; u = p[0x36]; p[0x36] = t; p[0x35] = u;
    t = p[0x38]; u = p[0x3B]; p[0x3B] = t; p[0x38] = u;
    t = p[0x39]; u = p[0x3A]; p[0x3A] = t; p[0x39] = u;
    t = p[0x3C]; u = p[0x3F]; p[0x3F] = t; p[0x3C] = u;
    t = p[0x3D]; u = p[0x3E]; p[0x3E] = t; p[0x3D] = u;
    t = p[0x40]; u = p[0x43]; p[0x43] = t; p[0x40] = u;
    t = p[0x41]; u = p[0x42]; p[0x42] = t; p[0x41] = u;
    t = p[0x44]; u = p[0x47]; p[0x47] = t; p[0x44] = u;
    t = p[0x45]; u = p[0x46]; p[0x46] = t; p[0x45] = u;

    BrSegPtrFixup((uint32_t *)(void *)(p + 0x44));

    *(uint16_t *)(p + 0x48) = (uint16_t)((p[0x48] << 8) | p[0x49]);
    *(uint16_t *)(p + 0x4A) = (uint16_t)((p[0x4A] << 8) | p[0x4B]);
    *(uint16_t *)(p + 0x4C) = (uint16_t)((p[0x4C] << 8) | p[0x4D]);
    *(uint16_t *)(p + 0x4E) = (uint16_t)((p[0x4E] << 8) | p[0x4F]);
    *(uint16_t *)(p + 0x50) = (uint16_t)((p[0x50] << 8) | p[0x51]);
    *(uint16_t *)(p + 0x52) = (uint16_t)((p[0x52] << 8) | p[0x53]);

    BrDlRegister(*(void **)(void *)(p + 0x44));
    BrSub1003445A(p);
    BrSub10074DC0(1);
    g_pfn18AA0C4(*(void **)(void *)(p + 0x44));
}

void BrTrackFixupList60(void *pvHdr)
{
    uint8_t *h = (uint8_t *)pvHdr;
    uint8_t *p = (uint8_t *)BrPtrAt(h + 0x60);
    int i;

    if (p == NULL)
        return;
    for (i = 0; i < (int)BrRd32(h + 0x64); ++i, p += 0x54)
        BrTrackFixupRec54(p);
}

/* ==========================================================================
 * 0x10038450  BrTexCopyRecords
 * ========================================================================== */

/* WHAT IT DOES: copies the actual texture pixels and colour palettes out of
 * the loaded track image and into the places the texture records point at,
 * for those records that ask for it. Records that fail any of half a dozen
 * checks are quietly skipped. */
/* @implements 0x10038450 d3d BrTexCopyRecords */
void BrTexCopyRecords(void *pvTable, int cRecords)
{
    uint8_t *pTable = (uint8_t *)pvTable;
    int i;

    if (cRecords <= 0)
        return;

    for (i = 0; i < cRecords; ++i) {
        uint8_t *pRec = pTable + (size_t)i * 0x24;
        uint8_t *pDst;
        uint8_t *pDesc;
        uint8_t *pSrc;
        uint32_t uFlags;
        uint32_t cb;

        pDst = (uint8_t *)BrPtrAt(pRec + 0x00);
        if (pDst == NULL)
            continue;

        uFlags = BrRd32(pRec + 0x20);
        if ((uFlags & 0x00100000u) == 0)
            continue;

        pDesc = (uint8_t *)BrPtrAt(pRec + 0x08);
        if (pDesc == NULL)
            continue;
        if (BrRd16(pDesc + 0x02) != 2)
            continue;
        if ((int32_t)BrRd32(pDesc + 0x08) != -1)
            continue;

        cb = uFlags & 0x0003FFFFu;
        if (cb == 0)
            continue;

        /* DEVIATION: the sources are byte offsets into the texture image and
         * the original adds them to a raw global base with no check.  The
         * port refuses a copy that would run past the declared image. */
        pSrc = g_BrLoad.pTexBase;
        if (pSrc == NULL)
            continue;
        {
            uint32_t off = BrRd32(pDesc + 0x0C);
            if ((size_t)off > g_BrLoad.cbTexBase
             || (size_t)cb  > g_BrLoad.cbTexBase - off)
                continue;
            memcpy(pDst, pSrc + off, cb);
        }

        pDst = (uint8_t *)BrPtrAt(pRec + 0x04);
        if (pDst == NULL)
            continue;

        {
            uint32_t uSel;
            uint32_t cbPal;
            uint32_t off = BrRd32(pDesc + 0x10);

            if (g_BrLoad.pTexFlags == NULL)
                continue;
            /* Parallel array, same index and stride, field +0x20. */
            uSel = BrRd32(g_BrLoad.pTexFlags + (size_t)i * 0x24 + 0x20);
            uSel &= 0x0F000000u;
            cbPal = (uSel == 0x01000000u) ? 0x20u : 0x200u;

            if ((size_t)off > g_BrLoad.cbTexBase
             || (size_t)cbPal > g_BrLoad.cbTexBase - off)
                continue;
            memcpy(pDst, g_BrLoad.pTexBase + off, cbPal);
        }
    }
}

/* ==========================================================================
 * 0x10037FA0 / 0x10037E10 -- track fixup
 * ========================================================================== */

void BrTrackF08FromMax(void *pvHdr)
{
    uint8_t *h = (uint8_t *)pvHdr;
    const uint8_t *pSeg = (const uint8_t *)BrPtrAt(h + 0x24);
    const uint8_t *pArr = (const uint8_t *)BrPtrAt(h + 0x20);
    int n = 0, best = 0, i;

    if (pSeg != NULL)
        n = BrRd16(pSeg + 0x2000);

    if (n > 1 && pArr != NULL) {
        for (i = 1; i < n; ++i) {
            int v = BrRd16(pArr + (size_t)i * 2);
            if (v > best)
                best = v;
        }
    }
    /* Index 0 is skipped and best starts at 0, so this is never less than 1. */
    BrWr32(h + 8, (uint32_t)(best + 1));
}

void BrTrackFixup(void *pvHdr)
{
    uint8_t *h = (uint8_t *)pvHdr;
    uint8_t *pSeg;
    uint8_t *pArr;
    int n, maxA = 0, maxB = 0, i;

    BrSwapVec3Array (BrPtrAt(h + 0x14), (int)BrRd32(h + 0x10));
    BrSwapRec24Array(BrPtrAt(h + 0x1C), (int)BrRd32(h + 0x18));

    pSeg = (uint8_t *)BrPtrAt(h + 0x24);
    BrSwapU16Array(pSeg, 0x1001);
    n = (pSeg != NULL) ? BrRd16(pSeg + 0x2000) : 0;
    BrSwapU16Array(BrPtrAt(h + 0x20), n);

    /* max over [+0x20][n-1 .. 1], counting DOWN in the original. */
    pArr = (uint8_t *)BrPtrAt(h + 0x20);
    if (n - 1 >= 1 && pArr != NULL) {
        for (i = n - 1; i >= 1; --i) {
            int v = BrRd16(pArr + (size_t)i * 2);
            if (v > maxA)
                maxA = v;
        }
    }

    BrSwapU16Array(BrPtrAt(h + 0x90), maxA + 1);

    pArr = (uint8_t *)BrPtrAt(h + 0x90);
    if (maxA >= 1 && pArr != NULL) {
        for (i = maxA; i >= 1; --i) {
            int v = BrRd16(pArr + (size_t)i * 2);
            if (v > maxB)
                maxB = v;
        }
    }

    /* Scan forward from maxB to the first zero entry; that index is the
     * number of u16s to swap in the table at +0x8C. */
    pArr = (uint8_t *)BrPtrAt(h + 0x8C);
    if (pArr != NULL) {
        i = maxB;
        while (BrRd16(pArr + (size_t)i * 2) != 0)
            ++i;
        BrSwapU16Array(BrPtrAt(h + 0x8C), i);
    }

    BrTrackF08FromMax(h);
    BrSwapRec8Array(BrPtrAt(h + 0x0C), (int)BrRd32(h + 0x08));

    BrDlRegister(BrPtrAt(h + 0x50));
    BrSub10074DC0(4);
    if (g_i4BBE08 == 3)
        BrTexCopyRecords(BrPtrAt(h + 0x1C), (int)BrRd32(h + 0x18));
    g_pfn18AA0C4(BrPtrAt(h + 0x50));

    BrTrackFixupList60(h);
    BrSub10074DC0(1);
    g_pfn18AA0CC(BrPtrAt(h + 0x1C), (int)BrRd32(h + 0x18));

    pSeg = (uint8_t *)BrPtrAt(h + 0x6C);
    BrSwapU16Array(pSeg, 0x1001);
    n = (pSeg != NULL) ? BrRd16(pSeg + 0x2000) : 0;
    BrSwapU16Array(BrPtrAt(h + 0x68), n);

    BrTrackFixupList78(h);
    BrTrackFixupList84(h);
}

/* ==========================================================================
 * 0x10038B20  BrTrackFixupCmds
 * ========================================================================== */

void BrTrackFixupCmds(void *pvHdr)
{
    uint8_t *h = (uint8_t *)pvHdr;
    int32_t  cCmds;
    int32_t  i;
    int32_t  cVerts = 0;      /* ebp: survives across records -- see tag 5 */

    BrLoad32BE(h + 0x224);
    cCmds = (int32_t)BrRd32(h + 0x224);
    if (cCmds <= 0)
        return;

    for (i = 0; i < cCmds; ++i) {
        uint8_t *pRec = h + 0x164 + (size_t)i * 0x0C;
        int      tag  = (int8_t)pRec[8];
        uint8_t *pVtx;
        int32_t  k;

        switch (tag) {
        case 0:
        case 1:
        case 2:
            BrSwap4(pRec + 0x00);
            BrSwap4(pRec + 0x04);
            break;

        case 4:
            BrSwap4(pRec + 0x00);
            BrFixupAt(pRec + 0x00);
            BrLoad32BE(pRec + 0x04);
            cVerts = (int32_t)BrRd32(pRec + 0x04);
            pVtx = (uint8_t *)BrPtrAt(pRec + 0x00);
            if (cVerts > 0 && pVtx != NULL) {
                for (k = 0; k < cVerts; ++k, pVtx += 0x0C)
                    BrSwapVec3(pVtx);
            }
            break;

        case 5:
            BrSwap4(pRec + 0x00);
            BrFixupAt(pRec + 0x00);
            pVtx = (uint8_t *)BrPtrAt(pRec + 0x00);
            /* GOTCHA: no count of its own -- reuses the last tag-4 count. */
            if (cVerts > 0 && pVtx != NULL) {
                for (k = 0; k < cVerts; ++k, pVtx += 0x0C)
                    BrSwapVec3(pVtx);
            }
            break;

        case 3:
        case 6:
        case 7:
            BrSwap4(pRec + 0x00);
            break;

        default:
            /* tag > 7 (the `ja` in the original is unsigned, so a negative
             * tag byte also lands here) -- nothing at all. */
            break;
        }

        /* The count is re-read from the header every iteration. */
        cCmds = (int32_t)BrRd32(h + 0x224);
    }
}

/* ==========================================================================
 * 0x10039000  BrInit220B20
 * ========================================================================== */

/* WHAT IT DOES: clears a block of state, plants an 8 in its first slot, and
 * calls on to further setup. What the block holds is not established here. */
/* @implements 0x10039000 d3d BrInit220B20 */
void BrInit220B20(void)
{
    memset(g_a220B20, 0, sizeof(uint32_t) * 0x46);
    g_a220B20[0] = 8;
    BrSub10035BD1();
}

/* ==========================================================================
 * 0x10039020  BrPoolEmit
 *
 * Constants read from .rdata; the addresses are the original's operands.
 * ========================================================================== */

#define BR_K_08F564  1.52587890625e-05f      /* 0x37800000, == 1/65536      */
#define BR_K_08F568 -1.0000000474974513e-03f /* 0xBA83126F                  */
#define BR_K_08F56C -1.0f                    /* 0xBF800000                  */
#define BR_K_08F570  0.25f                   /* 0x3E800000                  */
#define BR_K_08F574  1.0000000474974513e-03f /* 0x3A83126F                  */
#define BR_K_08F578 -1.5f                    /* 0xBFC00000                  */
#define BR_K_08F57C  1.5259021893143654e-05f /* 0x37800080                  */
#define BR_K_08F580  0.10000000149011612f    /* 0x3DCCCCCD                  */
#define BR_K_08F584  0.15000000596046448f    /* 0x3E19999A                  */
#define BR_K_08F588  1.0f                    /* 0x3F800000                  */
#define BR_K_08F58C  255.0f                  /* 0x437F0000                  */
#define BR_K_MULADD  0.20000000298023224f    /* 0x3E4CCCCD, inline operand  */

/* 0x1007C8A0 is __ftol: truncate toward zero, low dword before any clamp.
 * Only the low byte of the result is used at the one call site here. */
static int32_t BrFtol(float f)
{
    return (int32_t)f;
}

static float BrFldAt(const void *pv)
{
    float f;
    memcpy(&f, pv, sizeof f);
    return f;
}

static void BrFstAt(void *pv, float f)
{
    memcpy(pv, &f, sizeof f);
}

void BrPoolEmit(void *pvThis)
{
    uint8_t *pThis = (uint8_t *)pvThis;
    float    fE24  = BrFldAt(pThis + 0x0E24);
    float    f105C;
    float    fTmp1;
    float    fT;
    float    fU;
    BrVec3   vTmp;
    uint16_t idx;
    BrPoolNode *pNode;

    /* timer += ((rand & 0x1FFF)/65536 - fE24 * -0.001 - -1.0) * dt */
    f105C = BrFldAt(pThis + 0x105C)
          + (((float)(BrRand() & 0x1FFF) * BR_K_08F564 - fE24 * BR_K_08F568)
             - BR_K_08F56C) * g_f6C2CFC;
    BrFstAt(pThis + 0x105C, f105C);

    if (!(f105C > BR_K_08F570))
        return;                       /* fcom + test ah,0x41: <= or unordered */

    idx = g_uPoolFree;
    if (idx == 0)
        return;                       /* pool empty; index 0 is the sentinel */

    fTmp1 = fE24 * BR_K_08F574;
    BrFstAt(pThis + 0x105C, 0.0f);

    pNode = &g_aPoolNodes[idx];

    /* Unlink from the free list, link onto the live list. */
    {
        uint16_t uOldHead = g_uPoolHead;
        g_uPoolHead = idx;
        g_uPoolFree = pNode->uNext;
        pNode->uNext = uOldHead;
    }

    BrVec3Scale(&pNode->v0C, (const BrVec3 *)(const void *)pThis,
                BR_K_08F578 - fTmp1);

    BrVec3Sub(&vTmp, (const BrVec3 *)(const void *)(pThis + 0x00F0),
                     (const BrVec3 *)(const void *)pThis);
    BrVec3MulAdd(&vTmp, &vTmp,
                 (const BrVec3 *)(const void *)(pThis + 0x0020), BR_K_MULADD);
    BrVec3MulAdd(&vTmp, &vTmp,
                 (const BrVec3 *)(const void *)(pThis + 0x0010), BR_K_MULADD);

    fT = (float)(BrRand() & 0xFFFF) * BR_K_08F57C;

    BrVec3Sub(&pNode->v00,
              (const BrVec3 *)(const void *)(pThis + 0x1060), &vTmp);
    BrVec3MulAdd(&pNode->v00, &vTmp, &pNode->v00, fT * fT);

    fU = fTmp1 * BR_K_08F580 - BR_K_08F56C;

    /* The three dwords are copied as raw words in the original, but they are
     * the same BrVec3 that was just built. */
    BrFstAt(pThis + 0x1060 + 0, vTmp.x);
    BrFstAt(pThis + 0x1060 + 4, vTmp.y);
    BrFstAt(pThis + 0x1060 + 8, vTmp.z);

    pNode->f18 = fU * BR_K_08F584;
    pNode->b1E = (uint8_t)BrFtol(
        BR_K_08F588 / (BrVec3Len((const BrVec3 *)(const void *)(pThis + 0x1024))
                       + fU) * BR_K_08F58C);
    pNode->b1F = 0xFF;
}

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: walk an array of 0x54-byte track records and fixup each one. */
/* @implements 0x100316A0 glide BrTrackFixupAllRec54 */

int BrTrackFixupAllRec54(int param_1)

{
  int iVar1;
  int iVar2;
  
  iVar1 = *(int *)(param_1 + 0x60);
  iVar2 = 0;
  if (0 < *(int *)(param_1 + 100)) {
    do {
      BrTrackFixupRec54(iVar1);
      iVar1 = iVar1 + 0x54;
      iVar2 = iVar2 + 1;
    } while (iVar2 < *(int *)(param_1 + 100));
  }
  return;
}

/* WHAT IT DOES: walk an array of Vec3s in a track struct and byte-swap each one. */
/* @implements 0x10031A80 glide BrTrackSwapAllVec3 */

int BrTrackSwapAllVec3(int param_1)

{
  int iVar1;
  int iVar2;
  
  iVar1 = *(int *)(param_1 + 0x84);
  iVar2 = 0;
  if (0 < *(int *)(param_1 + 0x88)) {
    do {
      BrSwapVec3(iVar1);
      iVar1 = iVar1 + 0xc;
      iVar2 = iVar2 + 1;
    } while (iVar2 < *(int *)(param_1 + 0x88));
  }
  return;
}

#endif /* BR_MATCHING_BUILD */
