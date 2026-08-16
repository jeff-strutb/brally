/* br_cardata.c -- the .rca car-data record.  See br_cardata.h for the whole
 * trail from the disc to body+0x1DC and for the misreading it corrects.
 *
 * Transcribed from orig/BRGlide.dll:
 *   0x10030DE0   340 B   the loader ("cars/" + name + ".rca", "RCar" check)
 *   0x100B7D00           the 16-entry car name table
 *   0x1006FD90's reads   the physics block at +0x96..+0xD8
 *   0x1005BCC0's reads   the mount block at +0x80EC..+0x80FC
 */
#include "br_cardata.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================================================================== */
/* 0x100B7D00 -- the name table                                          */
/*                                                                       */
/* Read out of .data at RVA 0x100B7D00 as sixteen pointers into the       */
/* string block at 0x100B83F4..0x100B8430.  Entry 16 is 0x2F786673, not   */
/* a pointer, so the table's length is pinned by its own content -- the   */
/* code does not carry a count.                                          */
/* ==================================================================== */
static const char *const s_aszCar[BR_CARDATA_CARS] = {
    "ce", "es", "ns", "rs", "sp", "ps", "m3", "ip",
    "ld", "hm", "mt", "cu", "bb", "pj", "tr", "mn"
};

const char *BrCarDataName(int iCar)
{
    if (iCar < 0 || iCar >= BR_CARDATA_CARS) {
        return NULL;
    }
    return s_aszCar[iCar];
}

/* ==================================================================== */
/* Readers                                                               */
/*                                                                       */
/* The parameter block below +0x8000 is LITTLE-ENDIAN and is not touched  */
/* by 0x10030770; the block at +0x80EC is inside the N64 image and is     */
/* BIG-ENDIAN in the file.  Two readers, named for which half they are    */
/* for, so a future edit cannot silently use the wrong one.               */
/* ==================================================================== */
static uint32_t BrCdU32Le(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t BrCdU32Be(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/* Reinterpret without aliasing through a pointer, as br_rca.c does. */
static float BrCdF32(uint32_t v)
{
    float f;
    memcpy(&f, &v, sizeof f);
    return f;
}

static float BrCdFloatLe(const unsigned char *p)
{
    return BrCdF32(BrCdU32Le(p));
}

/* ==================================================================== */
/* 0x10030DE0's tail                                                     */
/* ==================================================================== */

int BrCarDataDecode(BrCarData *pData, const void *pvFile, size_t cbFile)
{
    const unsigned char *p    = (const unsigned char *)pvFile;
    /* The last thing 0x1006FD90 reads is +0xD8, so that is the minimum a
     * buffer has to carry for the physics block to be real. */
    const size_t         need = (size_t)BR_CARDATA_O_D8 + 1u;
    int                  i;

    memset(pData, 0, sizeof *pData);
    if (pvFile == NULL || cbFile < need) {
        return 1;
    }
    /* 0x10030EDB: `strncmp(buf, "RCar", 4)`, and a mismatch only produces the
     * "not a car file: %s" complaint -- the original loads on regardless.
     * Here it is a hard failure, because a caller that gets a zeroed record
     * back cannot tell the two apart and the box would silently stay zero. */
    if (memcmp(p, BR_CARDATA_MAGIC, 4) != 0) {
        return 1;
    }

    /* The name, as br_rca.c reads it: NUL-terminated at +0x04, zero-padded
     * out to the parameter block. */
    {
        size_t n = 0;
        const unsigned char *q = p + 4;
        while (n < sizeof pData->szName - 1u && (4u + n) < BR_CARDATA_O_B96
               && q[n] != '\0') {
            pData->szName[n] = (char)q[n];
            n++;
        }
        pData->szName[n] = '\0';
    }

    /* 0x1006FEBF..0x1006FEDD -- the four the whole module exists for. */
    pData->boxX    = BrCdFloatLe(p + BR_CARDATA_O_BOX + 0u);
    pData->boxY    = BrCdFloatLe(p + BR_CARDATA_O_BOX + 4u);
    pData->boxZ    = BrCdFloatLe(p + BR_CARDATA_O_BOX + 8u);
    pData->boxOffZ = BrCdFloatLe(p + BR_CARDATA_O_BOX + 12u);

    /* 0x1006FE37/0x1006FE54: `mov ecx,7` + `rep movsd` from +0x98. */
    for (i = 0; i < BR_CARDATA_GEARS; ++i) {
        pData->gears[i] = BrCdFloatLe(p + BR_CARDATA_O_GEARS + (unsigned)i * 4u);
    }
    /* 0x1006FE56..0x1006FE8D, the five plain dwords at +0xB4..+0xC4. */
    for (i = 0; i < 5; ++i) {
        pData->param[i] = BrCdFloatLe(p + BR_CARDATA_O_PARAMS + (unsigned)i * 4u);
    }

    /* The three `movsx` bytes: 0x1006FEAC (+0x96), 0x1006FEEA (+0x97),
     * 0x1006FE99 (+0xD8).  SIGN-extended, which is the original's choice and
     * is not the same as the zero extension slice8_83.h documents elsewhere
     * in the car record. */
    pData->b96 = (int8_t)p[BR_CARDATA_O_B96];
    pData->b97 = (int8_t)p[BR_CARDATA_O_B97];
    pData->bD8 = (int8_t)p[BR_CARDATA_O_D8];

    /* The mount block, in the big-endian half.  Optional: a truncated file
     * still yields a usable physics block, and fMountValid says which. */
    if (cbFile >= (size_t)BR_CARDATA_O_MOUNT + 16u) {
        for (i = 0; i < 4; ++i) {
            pData->mount[i] =
                BrCdF32(BrCdU32Be(p + BR_CARDATA_O_MOUNT + (unsigned)i * 4u));
        }
        pData->fMountValid = 1;
    }

    pData->cbFile = cbFile;
    return 0;
}

/* ==================================================================== */
/* 0x10030F50 -- read the whole file                                     */
/* ==================================================================== */

int BrCarDataLoadFile(BrCarData *pData, const char *pszPath)
{
    FILE  *f;
    long   cb;
    void  *pv;
    int    rc;

    memset(pData, 0, sizeof *pData);
    if (pszPath == NULL) {
        return 1;
    }
    f = fopen(pszPath, "rb");
    if (f == NULL) {
        return 1;
    }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 1; }
    cb = ftell(f);
    if (cb <= 0)                    { fclose(f); return 1; }
    rewind(f);
    pv = malloc((size_t)cb);
    if (pv == NULL)                 { fclose(f); return 1; }
    if (fread(pv, 1, (size_t)cb, f) != (size_t)cb) {
        free(pv); fclose(f); return 1;
    }
    fclose(f);
    rc = BrCarDataDecode(pData, pv, (size_t)cb);
    free(pv);
    return rc;
}

int BrCarDataLoadIndex(BrCarData *pData, const char *pszDir, int iCar)
{
    const char *pszName = BrCarDataName(iCar);
    char        szPath[512];

    memset(pData, 0, sizeof *pData);
    if (pszName == NULL) {
        return 1;
    }
    if (pszDir == NULL) {
        pszDir = BrCarDataDir();
        if (pszDir == NULL) {
            return 1;
        }
    }
    /* 0x10030E2C..0x10030EC9 builds "cars/" + name + ".rca" with three
     * inlined strcpy/strcat pairs.  The prefix is a parameter here; see
     * br_cardata.h's third DEVIATION. */
    if ((size_t)snprintf(szPath, sizeof szPath, "%s/%s.rca", pszDir, pszName)
        >= sizeof szPath) {
        return 1;
    }
    return BrCarDataLoadFile(pData, szPath);
}

/* ==================================================================== */
/* Finding CARS/                                                         */
/* ==================================================================== */

const char *BrCarDataDir(void)
{
    /* -1 not searched yet, 0 searched and not found, 1 found. */
    static int         s_state;
    static const char *s_pszDir;
    static const char *const s_aszTry[] = {
        "testdata/cars", "cars", "CARS", "../testdata/cars"
    };
    BrCarData  probe;
    const char *pszEnv;
    size_t      i;

    if (s_state != 0) {
        return s_pszDir;
    }
    s_state = 1;                       /* searched, whatever the answer     */

    pszEnv = getenv("BR_CARS_DIR");
    if (pszEnv != NULL && BrCarDataLoadIndex(&probe, pszEnv, 0) == 0) {
        s_pszDir = pszEnv;
        return s_pszDir;
    }
    for (i = 0; i < sizeof s_aszTry / sizeof s_aszTry[0]; ++i) {
        if (BrCarDataLoadIndex(&probe, s_aszTry[i], 0) == 0) {
            s_pszDir = s_aszTry[i];
            return s_pszDir;
        }
    }
    s_pszDir = NULL;
    return NULL;
}

const BrCarData *BrCarDataDefault(void)
{
    static int       s_state;
    static BrCarData s_data;

    if (s_state == 0) {
        s_state = 1;
        if (BrCarDataLoadIndex(&s_data, NULL, 0) != 0) {
            s_state = 2;               /* absent; report it as NULL         */
        }
    }
    return (s_state == 1) ? &s_data : NULL;
}
