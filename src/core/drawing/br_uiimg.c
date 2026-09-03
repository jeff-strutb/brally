/* br_uiimg.c -- drawing/ : the UI image registry.  See br_uiimg.h for the
 * derivation; this file is the transcription.
 *
 * Read off BRGlide.dll, which is the reference build.  Every address in this
 * file is a GLIDE address unless it is labelled D3D.
 *
 *   0x10056279 .. 0x1005629D   clear the table and three 16-bit slots
 *   0x100562A3 .. 0x100581CC   145 x { operator new(0x104); strcpy(path) }
 *   0x10058299 .. 0x100582E0   the two save-file path buffers
 *
 * All three are stretches of ONE function, Glide 0x10056260 / D3D 0x1005D440,
 * whose other four concerns are in port/src/startup/br_uiboot.c.  The split is
 * by responsibility, not by address: what is here is the state the drawing
 * chain reads afterwards, what is there is the boot sequence itself.
 *
 * ==========================================================================
 * THE ALLOCATOR DOES NOT ZERO, AND slice1_06.c SAYS IT DOES
 * ==========================================================================
 *
 * slice1_06.c's banner states "0x1007DFE0 is calloc(n,1) (it tail-calls
 * 0x1007D370 with a second argument of 1)", and its BrUiAssetPathsInit --
 * the D3D reading of this same loop -- calls calloc(0x104, 1).
 *
 * That is wrong, and it is wrong in both builds:
 *
 *   Glide  `push 0x104 / call 0x10074572`, and 0x10074572 is six bytes,
 *          `jmp dword ptr [0x118F04E4]` == MSVCRT!??2@YAPAXI@Z, which is
 *          `operator new`.  MSVCRT's operator new is malloc; it does not zero.
 *
 *   D3D    `push 0x104 / call 0x1007DFE0`, and 0x1007DFE0 is sixteen bytes:
 *          `mov eax,[esp+4] / push 1 / push eax / call 0x1007D370 / add
 *          esp,8 / ret`.  The literal 1 is the SECOND parameter of
 *          _nh_malloc(size, nhFlag), not calloc's element count -- 0x1007D370
 *          reads it into edi and uses it for nothing but `test edi,edi / je
 *          fail` around `call 0x10082ED0` (_callnewh) and a retry loop.  The
 *          allocation itself is `push esi / call 0x1007D3C0` with ONE
 *          argument.  Nothing zeroes.
 *
 * CONVENTIONS.md has it right ("0x1007DFE0 is operator new (_nh_malloc(size,
 * 1)) and does not zero"); slice1_06.c contradicts it.  The consequence is not
 * theoretical: port/tests/test_slice1_06.c asserts
 * `apsz[i][BR_UIASSET_PATH_MAX - 1] == '\0'` with the comment "calloc'd: the
 * tail of each buffer is zero", which is a test certifying a property the
 * original does not have.  Corrected in that file by this pass.
 *
 * So: this module allocates 0x104 UNINITIALISED bytes and copies exactly
 * strlen+1 of them.  Everything past the NUL is whatever the heap held.
 * ==========================================================================
 */
#include "br_uiimg.h"

#include <string.h>

#include "slice1_06.h"   /* g_apszBrUiAssets, g_pszBrRallySeasonDat/Ghost */

/* ==========================================================================
 * Storage.  Checked against every other header in port/include before being
 * declared: nothing else in this tree defines 0x10AC53E8, 0x10AC5C2C,
 * 0x10AC5D50 or 0x10AC5D54.  br_uispr.h's BrUiSprite::iImage is an INDEX into
 * this table, not a second copy of it, and br_surf.h describes the walk
 * without owning the storage.
 * ========================================================================== */

BrUiImg  g_aBrUiImg[BR_UIIMG_COUNT];   /* 0x10AC53E8 */
uint16_t g_cBrUiImgLoaded;             /* 0x10AC5C2C */
uint16_t g_wBrUiImgAC5D50;             /* 0x10AC5D50 */
uint16_t g_wBrUiImgAC5D54;             /* 0x10AC5D54 */

/* The arithmetic that pins the count, stated as a claim about the ORIGINAL so
 * that it holds on a 64-bit host: the 0x122-dword `rep stosd` covers exactly
 * 145 eight-byte records, and the last path store lands at base + 144*8 + 4,
 * four bytes short of the end of the cleared block.  If someone "corrects"
 * the count, these fail and point at the banner rather than at a blitter. */
typedef char br_uiimg_assert_fill[
    (0x122u * 4u == BR_UIIMG_COUNT * BR_UIIMG_ORIG_STRIDE) ? 1 : -1];
typedef char br_uiimg_assert_last[
    ((BR_UIIMG_COUNT - 1) * BR_UIIMG_ORIG_STRIDE + 4u
        == 0x10AC586Cu - 0x10AC53E8u) ? 1 : -1];

/* And that the list this module copies really is 145 long. */
typedef char br_uiimg_assert_names[
    (BR_UIIMG_COUNT == BR_UIASSET_COUNT) ? 1 : -1];

/* ==========================================================================
 * 0x10056279 .. 0x1005629D
 *
 *     mov ecx,0x122 / xor eax,eax / mov edi,0x10AC53E8 / xor ebx,ebx
 *     rep stosd
 *     mov word ptr [0x10AC5C2C], bx
 *     mov word ptr [0x10AC5D50], bx
 *     mov word ptr [0x10AC5D54], bx
 *
 * `xor ebx,ebx` is not part of the fill: ebx stays zero for the whole
 * function and is the constant the gate compares every allocation against.
 * ========================================================================== */
void BrUiImgTableClear(void)
{
    int i;

    /* memset over the struct array rather than a byte count, because the
     * struct is wider than eight bytes on this host.  The ORIGINAL's byte
     * count is asserted above; it is not used here. */
    for (i = 0; i < BR_UIIMG_COUNT; ++i) {
        g_aBrUiImg[i].pSurf   = NULL;
        g_aBrUiImg[i].pszPath = NULL;
    }

    g_cBrUiImgLoaded  = 0;
    g_wBrUiImgAC5D50  = 0;
    g_wBrUiImgAC5D54  = 0;
}

/* ==========================================================================
 * 0x100562A3 .. 0x100581CC -- the 145 allocate-and-copy blocks
 *
 * One block, in the original's own order (the compiler interleaves the
 * strlen scan with the store, and in seven of the 145 blocks it emits the
 * store BEFORE the string load -- which is why pairing "the nearest earlier
 * string" mis-attributes exactly seven entries, and why br_uispr.h records
 * seven names it "could not pair".  Grouping by the `call` instead recovers
 * all 145; the seven are 16, 46, 63, 80, 97, 127 and 144.):
 *
 *     push 0x104
 *     call 0x10074572            ; operator new -- NO zeroing
 *     mov  edx, eax
 *     mov  edi, <literal>
 *     add  esp, 4                ; cdecl: the CALLER pops
 *     mov  [0x10AC53E8 + 8i + 4], edx
 *     or   ecx,-1 / xor eax,eax / repne scasb / not ecx / sub edi,ecx
 *     mov  esi, edi / mov edi, edx
 *     shr  ecx,2 / rep movsd / and ecx,3 / rep movsb
 *
 * i.e. the pointer is published BEFORE the string is copied, and the copy is
 * exactly strlen+1 bytes.  MSVC's inlined strcpy, recognised as the idiom.
 *
 * DEVIATION: the original never tests the allocation.  A NULL would be
 * published into the table and then written through by `rep movsd`, which is
 * a fault in the original and undefined behaviour here.  This port skips the
 * copy for a failed slot, leaves the slot NULL and CONTINUES.  Two reasons it
 * continues rather than aborting: nothing in 0x10056260 reads the result, so
 * the gate's return value must not depend on it (see br_uiboot.c -- getting
 * that wrong would invent a second way for the gate to fail); and NULL is a
 * state the reader already handles, since 0x100583C0 skips any entry with no
 * name.  The count of failures is returned so a caller can tell.
 * ========================================================================== */
int BrUiImgPathsInit(const BrUiImgAlloc *pAlloc)
{
    int i;
    int nFailed = 0;

    if (pAlloc == NULL || pAlloc->pfnAlloc == NULL) {
        return BR_UIIMG_COUNT;
    }

    for (i = 0; i < BR_UIIMG_COUNT; ++i) {
        const char *pszSrc = g_apszBrUiAssets[i];
        size_t      cb;
        char       *p;

        p = (char *)pAlloc->pfnAlloc(pAlloc->pUser, BR_UIIMG_PATH_MAX);

        /* Published before the copy, exactly as the original does. */
        g_aBrUiImg[i].pszPath = p;

        if (p == NULL) {
            ++nFailed;
            continue;                /* DEVIATION -- see the banner above. */
        }

        cb = strlen(pszSrc) + 1u;

        /* DEVIATION: the original copies strlen+1 with no bound; the longest
         * of the 145 is "images\carwnoshad2.bmp" at 22 bytes against a 0x104
         * buffer, so the bound cannot fire on the shipped data.  It is here
         * because the buffer size and the string list are separately
         * declared and a future edit could make them disagree. */
        if (cb > BR_UIIMG_PATH_MAX) {
            cb = BR_UIIMG_PATH_MAX;
            memcpy(p, pszSrc, cb);
            p[BR_UIIMG_PATH_MAX - 1u] = '\0';
        } else {
            memcpy(p, pszSrc, cb);
        }
    }

    return nFailed;
}

void BrUiImgPathsFree(const BrUiImgAlloc *pAlloc)
{
    int i;

    if (pAlloc == NULL || pAlloc->pfnFree == NULL) {
        return;
    }
    for (i = 0; i < BR_UIIMG_COUNT; ++i) {
        if (g_aBrUiImg[i].pszPath != NULL) {
            pAlloc->pfnFree(pAlloc->pUser, g_aBrUiImg[i].pszPath);
            g_aBrUiImg[i].pszPath = NULL;
        }
    }
}

/* ==========================================================================
 * 0x10058299 .. 0x100582E0 -- the two save-file path buffers
 *
 * Two more inlined strcpy's, from the .rdata literals at 0x100ACB70 and
 * 0x100ACB5C into the fixed buffers at 0x117A6030 and 0x117A5F28.  The
 * destinations are 0x108 apart in both builds (D3D: 0x11782CD0 and
 * 0x11782BC8), which is the one piece of evidence anywhere in the corpus
 * about how big they are -- and 0x108 > 0x104, so the 0x104 the rest of the
 * tree assumes for these buffers at least fits.
 *
 * The buffers are the caller's on purpose: br_save.h records that they are
 * already modelled twice and asks for a merge rather than a third model.
 * ========================================================================== */
static void BrUiImgCopyPath(char *pszDst, size_t cbDst, const char *pszSrc)
{
    size_t cb;

    if (pszDst == NULL || cbDst == 0u || pszSrc == NULL) {
        return;
    }
    cb = strlen(pszSrc) + 1u;
    if (cb > cbDst) {
        cb = cbDst;                  /* DEVIATION: the original is unbounded */
    }
    memcpy(pszDst, pszSrc, cb);
    pszDst[cb - 1u] = '\0';
}

/* @n64 0x80264B20 located */
void BrUiImgSavePathsInit(char *pszSeason, size_t cbSeason,
                          char *pszGhost,  size_t cbGhost)
{
    /* Season first, then ghost -- the original's order. */
    BrUiImgCopyPath(pszSeason, cbSeason, g_pszBrRallySeasonDat);
    BrUiImgCopyPath(pszGhost,  cbGhost,  g_pszBrRallyGhostDat);
}

/* ========================================================================== */

/* @n64 0x80200108 exact */
void BrUiImgResetForTest(void)
{
    BrUiImgTableClear();
}
