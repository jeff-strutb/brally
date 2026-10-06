/* br_rca.c -- Boss Rally .rca car definition loader (portable C99). See br_rca.h. */
#include "br_rca.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rd_u32le(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Reinterpret a 32-bit pattern as float without aliasing through a pointer. */
static float u32_to_f32(uint32_t v)
{
    float f;
    memcpy(&f, &v, sizeof(f));
    return f;
}

int BrRcaDecode(BrRca *pRca, const void *pvData, size_t cbData)
{
    const unsigned char *p = (const unsigned char *)pvData;
    size_t need = BR_RCA_PARAM_OFFSET + (size_t)BR_RCA_PARAM_COUNT * 4;
    int i;

    memset(pRca, 0, sizeof(*pRca));
    if (cbData < need)
        return 1;
    if (memcmp(p, "RCar", 4) != 0)
        return 1;

    /* name: NUL-terminated, zero-padded out to the parameter block */
    {
        size_t n = 0;
        const unsigned char *q = p + BR_RCA_NAME_OFFSET;
        while (n < sizeof(pRca->szName) - 1 &&
               (BR_RCA_NAME_OFFSET + n) < BR_RCA_PARAM_OFFSET &&
               q[n] != '\0') {
            pRca->szName[n] = (char)q[n];
            n++;
        }
        pRca->szName[n] = '\0';
    }

    for (i = 0; i < BR_RCA_PARAM_COUNT; i++) {
        uint32_t v = rd_u32le(p + BR_RCA_PARAM_OFFSET + i * 4);
        pRca->auParams[i] = v;
        pRca->afParams[i] = u32_to_f32(v);
    }
    /* gears live at +0x9C, i.e. two words into the parameter block */
    for (i = 0; i < BR_RCA_GEAR_COUNT; i++)
        pRca->gears[i] = pRca->afParams[2 + i];

    pRca->cbFile = cbData;
    return 0;
}

int BrRcaLoad(BrRca *pRca, const char *pszPath)
{
    FILE *f;
    long cb;
    void *buf;
    int rc;

    memset(pRca, 0, sizeof(*pRca));
    f = fopen(pszPath, "rb");
    if (f == NULL)
        return 1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 1; }
    cb = ftell(f);
    if (cb <= 0) { fclose(f); return 1; }
    rewind(f);
    buf = malloc((size_t)cb);
    if (buf == NULL) { fclose(f); return 1; }
    if (fread(buf, 1, (size_t)cb, f) != (size_t)cb) { free(buf); fclose(f); return 1; }
    fclose(f);
    rc = BrRcaDecode(pRca, buf, (size_t)cb);
    free(buf);
    return rc;
}
