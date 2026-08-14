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
