/* br_rca.h -- Boss Rally .rca car definition (portable).
 *
 * Layout established by diffing ce.rca ("TYPE-CE") against bb.rca
 * ("Beach Ball") from the retail disc:
 *
 *   +0x00  char  magic[4]      "RCar"
 *   +0x04  char  szName[]      NUL-terminated, zero-padded to 0x94
 *   +0x94  u32   unknown       differs per car (0x01000000 / 0x02010000)
 *   +0x98  u32   zero          in both cars
 *   +0x9C  f32   gears[6]      forward gear ratios, descending
 *   +0xB4  ...   parameters    handling/physics block, see BrRcaParams
 *   ...          geometry and RGBA5551 texture data (not yet decoded)
 *
 * Evidence for the gear block: six consecutive descending floats in both
 * cars, identical for gears 1-4 (3.23, 2.10, 1.46, 1.11) and differing only
 * in the top two -- exactly how a shared gearbox with different final ratios
 * would look. Anything below `gears` is exposed as raw indexed floats until
 * the physics code that consumes it is decompiled; guessing names for them
 * would be inventing knowledge we do not have.
 *
 * THAT PHYSICS CODE IS NOW DECOMPILED -- see br_cardata.h, which reads the
 * same block through 0x1006FD90's own offsets. Two of the guesses above are
 * confirmed and one is refined:
 *
 *   +0x98  is a SEVEN-dword array, not six: 0x1006FE37/0x1006FE54 copies
 *          `mov ecx,7` + `rep movsd` from there into car+0xE28. Entry 0 is
 *          0.0f in every shipped car and entries 1..6 are the six ratios
 *          this header found at +0x9C, so `gears[]` here is that array's
 *          tail.
 *   +0x94  the "unknown" dword is not one value: 0x1006FEAC and 0x1006FEEA
 *          read its bytes at +0x96 and +0x97 SEPARATELY, sign-extended,
 *          into two int fields of the car.
 *   +0xC8  four floats -- the CAR'S COLLISION BOX, straight into
 *          body+0x1DC..+0x1E8. ce.rca (3.5, 2.0, 0.8, 0.7).
 *
 * This module is left as it is: it is the byte-level survey the layout came
 * out of and its tests assert that survey. br_cardata.c is the loader the
 * physics uses, and it decodes from the same offsets rather than through
 * BrRca, because the two want different things -- raw indexed words here,
 * named fields there.
 */
#ifndef BR_RCA_H
#define BR_RCA_H

#include <stddef.h>
#include <stdint.h>

#define BR_RCA_NAME_OFFSET   4
#define BR_RCA_PARAM_OFFSET  0x94
#define BR_RCA_GEAR_COUNT    6
/* Number of 32-bit parameters decoded after the header, chosen to cover the
 * block that visibly differs between cars. */
#define BR_RCA_PARAM_COUNT   24

typedef struct BrRca {
    char     szName[64];                        /* e.g. "TYPE-CE" */
    float    gears[BR_RCA_GEAR_COUNT];
    /* Raw parameter words starting at +0x94, both interpretations kept since
     * the block mixes floats and small integers. */
    float    afParams[BR_RCA_PARAM_COUNT];
    uint32_t auParams[BR_RCA_PARAM_COUNT];
    size_t   cbFile;
} BrRca;

/* Returns 0 on success. */
int BrRcaLoad(BrRca *pRca, const char *pszPath);
int BrRcaDecode(BrRca *pRca, const void *pvData, size_t cbData);

#endif /* BR_RCA_H */
