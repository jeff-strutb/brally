/* br_save.c -- the ".BRF" championship-season save file.  See br_save.h.
 *
 * Read off the writer (0x100709A0 D3D / 0x10069930 Glide) and the reader
 * (0x10070610 / 0x100695C0), both of which state the layout in full; the two
 * agree on every field, including which of the six adjacent option dwords is
 * left out.  No part of this was inferred from a file image -- there is no
 * retail .BRF to infer from.
 *
 * Everything that crosses the file boundary is decoded byte-wise.  The image
 * is never overlaid on a struct, and BrBrfSeason is a host object whose size
 * is nobody's business but this host's.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "br_save.h"
#include "slice1_01.h"   /* BrAdler32 -- 0x10001000, zlib adler32 verbatim */

/* Compile-time check that the layout constants still compose to the file the
 * two functions describe.  C99 has no _Static_assert here; the tree uses the
 * negative-array-size trick (port/tests/test_layout.c). */
typedef char br_save_assert_size[(BR_BRF_FILE_SIZE == 0x29C) ? 1 : -1];
typedef char br_save_assert_tail[(BR_BRF_TAIL_FROM_END == 0x94) ? 1 : -1];
typedef char br_save_assert_blk[(BR_SEASON_BLOCK_SIZE == 0x200) ? 1 : -1];

/* ==========================================================================
 * byte-wise integer access
 *
 * The original fwrite()s and fread()s a dword directly, so the file carries
 * the 32-bit x86 build's own layout: little-endian.  Spelled out rather than
 * memcpy'd so the convention is stated once, here, and not implied by the
 * host's.
 * ========================================================================== */

static uint32_t BrBrfRd32(const unsigned char *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static void BrBrfWr32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v & 0xFFu);
    p[1] = (unsigned char)((v >> 8) & 0xFFu);
    p[2] = (unsigned char)((v >> 16) & 0xFFu);
    p[3] = (unsigned char)((v >> 24) & 0xFFu);
}

/* ==========================================================================
 * the checksum
 * ==========================================================================
 *
 * The original is two calls, not one:
 *
 *     push 0 / push 0 / push 0 / call adler32     -> the seed, which is 1
 *     push 0x200 / push block / push seed / call adler32
 *
 * The first call is the documented "give me the initial value" form -- pBuf
 * NULL returns 1 and ignores the adler argument entirely (slice1_01.h).  It is
 * kept rather than folded to a literal 1 because that is the fact the original
 * relies on, and BrAdler32 is where it is established.
 */
/* @n64 0x8023DF4C located */
uint32_t BrBrfChecksum(const unsigned char *pBlock, size_t cbBlock)
{
    unsigned long seed;

    if (pBlock == NULL) {
        return 0u;
    }
    seed = BrAdler32(0uL, NULL, 0u);
    return (uint32_t)BrAdler32(seed, pBlock, (unsigned int)cbBlock);
}

/* ==========================================================================
 * the layout
 * ========================================================================== */

int BrBrfEncode(unsigned char *pOut, size_t cbOut, const BrBrfSeason *pIn)
{
    size_t off;
    int    i;

    if (pOut == NULL || pIn == NULL || cbOut < (size_t)BR_BRF_FILE_SIZE) {
        return BR_BRF_EARG;
    }

    /* Four bytes, no terminator: the original fwrite()s 4 out of a global
     * that happens to hold a five-byte C string. */
    memcpy(pOut + BR_BRF_MAGIC_OFF, BR_BRF_MAGIC, BR_BRF_MAGIC_SIZE);

    /* The sum precedes the data it covers, and covers nothing else. */
    BrBrfWr32(pOut + BR_BRF_SUM_OFF,
              BrBrfChecksum(pIn->aBlock, (size_t)BR_SEASON_BLOCK_SIZE));

    memcpy(pOut + BR_BRF_BLOCK_OFF, pIn->aBlock, (size_t)BR_SEASON_BLOCK_SIZE);

    off = (size_t)BR_BRF_BLOCK_OFF + (size_t)BR_SEASON_BLOCK_SIZE;
    for (i = 0; i < BR_BRF_OPT_COUNT; ++i) {
        BrBrfWr32(pOut + off + (size_t)i * 4u, pIn->aOpt[i]);
    }
    off += (size_t)BR_BRF_OPT_COUNT * 4u;

    memcpy(pOut + off, pIn->szName, (size_t)BR_SEASON_TAIL_SIZE);

    return BR_BRF_FILE_SIZE;
}

int BrBrfDecode(const unsigned char *pImage, size_t cbImage, BrBrfSeason *pOut)
{
    BrBrfSeason tmp;
    size_t      tail;
    uint32_t    stored;
    int         i;

    if (pImage == NULL || pOut == NULL) {
        return BR_BRF_EARG;
    }

    /* The reader's own minimum: it reads 4 + 4 + 0x200 forwards and then
     * seeks back 0x94 from the end, so anything shorter than the larger of
     * those two demands cannot be parsed.  On a real file they coincide. */
    if (cbImage < (size_t)BR_BRF_BLOCK_OFF + (size_t)BR_SEASON_BLOCK_SIZE
        || cbImage < (size_t)BR_BRF_TAIL_FROM_END) {
        return BR_BRF_ETRUNC;
    }

    /* `strncmp(buf, "RSea", 4)` -- four bytes, no terminator involved. */
    if (memcmp(pImage + BR_BRF_MAGIC_OFF, BR_BRF_MAGIC, BR_BRF_MAGIC_SIZE)
            != 0) {
        return BR_BRF_EMAGIC;
    }

    stored = BrBrfRd32(pImage + BR_BRF_SUM_OFF);
    memcpy(tmp.aBlock, pImage + BR_BRF_BLOCK_OFF,
           (size_t)BR_SEASON_BLOCK_SIZE);

    if (BrBrfChecksum(tmp.aBlock, (size_t)BR_SEASON_BLOCK_SIZE) != stored) {
        return BR_BRF_ECHECKSUM;
    }

    /* THE TAIL IS LOCATED FROM THE END.  The original does
     *     fseek(END); ftell(); fseek(n - 0x94); read 5 dwords
     *     fseek(END); ftell(); fseek(n - 0x80); read 0x80
     * -- two independent seeks, both from the end, so the option dwords are
     * at END-0x94 and the name at END-0x80 whatever lies between the payload
     * and them.  Reproduced: a file with slack after the payload decodes
     * exactly as the original decodes it, not as a sequential reader would. */
    tail = cbImage - (size_t)BR_BRF_TAIL_FROM_END;
    for (i = 0; i < BR_BRF_OPT_COUNT; ++i) {
        tmp.aOpt[i] = BrBrfRd32(pImage + tail + (size_t)i * 4u);
    }
    memcpy(tmp.szName, pImage + (cbImage - (size_t)BR_SEASON_TAIL_SIZE),
           (size_t)BR_SEASON_TAIL_SIZE);

    *pOut = tmp;
    return BR_BRF_OK;
}

/* ==========================================================================
 * the host seam
 *
 * Win32 in the original only in the sense that it is MSVCRT stdio: fopen /
 * fread / fseek / ftell / fclose, with the mode strings "rb" (0x1007B0E0 in
 * Glide) and "wb" (0x1007B600).  Nothing here needs a Win32 type.
 * ========================================================================== */

int BrBrfReadFile(const char *pszPath, BrBrfSeason *pOut)
{
    BrBrfSeason   tmp;
    unsigned char aSum[BR_BRF_SUM_SIZE];
    unsigned char aMagic[BR_BRF_MAGIC_SIZE];
    unsigned char aOpt[BR_BRF_OPT_COUNT * 4];
    uint32_t      stored;
    long          end;
    FILE         *pf;
    int           i;

    if (pszPath == NULL || pOut == NULL) {
        return BR_BRF_EARG;
    }

    pf = fopen(pszPath, "rb");
    if (pf == NULL) {
        return BR_BRF_EIO;   /* original: return (arg & 0xFF) != 0 */
    }

    /* The three CHECKED reads, in the original's order and with the original's
     * counts.  Each failure fcloses and returns; see BrBrfReaderFailReturn. */
    if (fread(aMagic, 1, sizeof aMagic, pf) != sizeof aMagic) {
        fclose(pf);
        return BR_BRF_ETRUNC;
    }
    if (memcmp(aMagic, BR_BRF_MAGIC, BR_BRF_MAGIC_SIZE) != 0) {
        fclose(pf);
        return BR_BRF_EMAGIC;
    }
    if (fread(aSum, 1, sizeof aSum, pf) != sizeof aSum) {
        fclose(pf);
        return BR_BRF_ETRUNC;
    }
    if (fread(tmp.aBlock, 1, (size_t)BR_SEASON_BLOCK_SIZE, pf)
            != (size_t)BR_SEASON_BLOCK_SIZE) {
        fclose(pf);
        return BR_BRF_ETRUNC;
    }

    stored = BrBrfRd32(aSum);
    if (BrBrfChecksum(tmp.aBlock, (size_t)BR_SEASON_BLOCK_SIZE) != stored) {
        fclose(pf);
        return BR_BRF_ECHECKSUM;
    }

    /* Tail pass one: five dwords at END - 0x94.  The original re-measures the
     * file rather than remembering the length, and does the five reads as five
     * separate 4-byte fread()s -- contiguous, so one read of 20 is the same
     * bytes.  It checks NONE of them. */
    if (fseek(pf, 0L, SEEK_END) != 0) {
        fclose(pf);
        return BR_BRF_EIO;
    }
    end = ftell(pf);
    /* DEVIATION (memory safety): the original computes `ftell() - 0x94` and
     * seeks there with no test, so a file shorter than 0x94 bytes seeks to a
     * negative offset and then reads whatever fseek's failure left the cursor
     * on.  Refused here. */
    if (end < (long)BR_BRF_TAIL_FROM_END) {
        fclose(pf);
        return BR_BRF_ETRUNC;
    }
    if (fseek(pf, end - (long)BR_BRF_TAIL_FROM_END, SEEK_SET) != 0) {
        fclose(pf);
        return BR_BRF_EIO;
    }
    (void)fread(aOpt, 1, sizeof aOpt, pf);      /* unchecked, as in the original */
    for (i = 0; i < BR_BRF_OPT_COUNT; ++i) {
        tmp.aOpt[i] = BrBrfRd32(aOpt + (size_t)i * 4u);
    }

    /* Tail pass two: the name at END - 0x80.  A SECOND measure-and-seek, not
     * a continuation of the first -- which is what makes the two offsets
     * independent of everything before them. */
    if (fseek(pf, 0L, SEEK_END) != 0) {
        fclose(pf);
        return BR_BRF_EIO;
    }
    end = ftell(pf);
    if (end < (long)BR_SEASON_TAIL_SIZE) {
        fclose(pf);
        return BR_BRF_ETRUNC;
    }
    if (fseek(pf, end - (long)BR_SEASON_TAIL_SIZE, SEEK_SET) != 0) {
        fclose(pf);
        return BR_BRF_EIO;
    }
    (void)fread(tmp.szName, 1, (size_t)BR_SEASON_TAIL_SIZE, pf);  /* unchecked */

    fclose(pf);
    *pOut = tmp;
    return BR_BRF_OK;
}

int BrBrfWriteFile(const char *pszPath, const BrBrfSeason *pIn)
{
    unsigned char aImage[BR_BRF_FILE_SIZE];
    FILE         *pf;
    int           n;

    if (pszPath == NULL || pIn == NULL) {
        return BR_BRF_EARG;
    }
    n = BrBrfEncode(aImage, sizeof aImage, pIn);
    if (n != BR_BRF_FILE_SIZE) {
        return n;
    }

    pf = fopen(pszPath, "wb");
    if (pf == NULL) {
        return BR_BRF_EIO;
    }
    if (fwrite(aImage, 1, sizeof aImage, pf) != sizeof aImage) {
        fclose(pf);
        return BR_BRF_EIO;
    }
    fclose(pf);
    return BR_BRF_OK;
}

/* ==========================================================================
 * what the load screen's file list is built from
 * ========================================================================== */

int BrBrfReadName(const char *pszPath, char szName[BR_SEASON_TAIL_SIZE])
{
    FILE *pf;
    long  end;

    if (pszPath == NULL || szName == NULL) {
        return BR_BRF_EARG;
    }

    /* 0x1005CF20 zeroes its 0x104-byte scratch before every match, so a short
     * or unreadable file leaves the slot empty rather than stale. */
    memset(szName, 0, (size_t)BR_SEASON_TAIL_SIZE);

    pf = fopen(pszPath, "rb");
    if (pf == NULL) {
        return BR_BRF_EIO;
    }
    if (fseek(pf, 0L, SEEK_END) != 0) {
        fclose(pf);
        return BR_BRF_EIO;
    }
    end = ftell(pf);
    /* DEVIATION (memory safety): the original seeks to ftell()-0x80 unchecked.
     * Note what it does NOT do: it never reads the magic and never checks the
     * checksum, so a corrupt save still contributes a name to the list. */
    if (end < (long)BR_SEASON_TAIL_SIZE) {
        fclose(pf);
        return BR_BRF_EIO;
    }
    if (fseek(pf, end - (long)BR_SEASON_TAIL_SIZE, SEEK_SET) != 0) {
        fclose(pf);
        return BR_BRF_EIO;
    }
    (void)fread(szName, 1, (size_t)BR_SEASON_TAIL_SIZE, pf);   /* unchecked */
    fclose(pf);
    return BR_BRF_OK;
}

int BrBrfSlotIndex(const char *pszFileName, const char *pszPrefix)
{
    size_t cbPrefix;

    if (pszFileName == NULL || pszPrefix == NULL) {
        return -1;
    }

    /* `strlen(prefix)` then `filename + that`, with no check that the prefix
     * is actually there and no bound -- the original trusts the mask that
     * produced the name.  A filename shorter than the prefix would walk off
     * the end in the original; clamped here, which lands on the terminator
     * and so yields atoi("") == 0, the same slot the original's own
     * non-numeric case picks. */
    cbPrefix = strlen(pszPrefix);
    if (cbPrefix > strlen(pszFileName)) {
        cbPrefix = strlen(pszFileName);
    }

    /* atoi, not strtol: 0x1007DDF0.  "12.brf" -> 12, and a tail with no
     * leading digits -> 0.  Neither the sign nor the range is checked, and
     * the caller uses the result as an index into a 100-slot array. */
    return atoi(pszFileName + cbPrefix);
}

int BrBrfFileName(char *pszOut, size_t cbOut, const char *pszPrefix, int slot)
{
    int n;

    if (pszOut == NULL || pszPrefix == NULL) {
        return -1;
    }
    /* DEVIATION: strcpy + _itoa + strcat + strcat in the original, into a
     * 0x104-byte stack buffer, unbounded.  The bytes produced are the same. */
    n = snprintf(pszOut, cbOut, "%s%d%s", pszPrefix, slot, BR_BRF_EXT);
    return n;
}
