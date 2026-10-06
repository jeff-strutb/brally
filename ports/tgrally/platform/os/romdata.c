/* romdata.c: where the cartridge's data is (plat.h, tgr_romdata).
 *
 *   ROMDATA=embed  (link.sh's default) tools/assets.py assembled it into the
 *                  executable as tgr_romdata_blob
 *   ROMDATA=file   (a release) the release builder wrote it beside the game
 *                  as romdata.bin: in the app's Resources on macOS, beside the
 *                  exe on Windows (host_resource_dir); TGR_ROMDATA names
 *                  another file. Read once, whole, at start-up.
 */
#include <stdio.h>
#include <stdlib.h>
#include "host.h"
#include "plat.h"

const uint8_t *tgr_romdata, *tgr_romdata_end;

#ifdef TGR_ROMDATA_EMBED
extern const uint8_t tgr_romdata_blob[], tgr_romdata_blob_end[];

int tgr_romdata_load(void)
{
    tgr_romdata = tgr_romdata_blob;
    tgr_romdata_end = tgr_romdata_blob_end;
    return 1;
}
#else
int tgr_romdata_load(void)
{
    char path[1100];
    const char *e = getenv("TGR_ROMDATA");
    uint8_t *p;
    FILE *f;
    long n;
    if (e)
        snprintf(path, sizeof path, "%s", e);
    else
        snprintf(path, sizeof path, "%s/romdata.bin", host_resource_dir());
    if (!(f = fopen(path, "rb"))) {
        fprintf(stderr, "tgr: no game data at %s\n", path);
        return 0;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n != (long)TGR_ROMDATA_SIZE || !(p = malloc((size_t)n)) || fread(p, 1, (size_t)n, f) != (size_t)n) {
        fprintf(stderr, "tgr: %s is not Top Gear Rally's data (%ld bytes)\n", path, n);
        fclose(f);
        return 0;
    }
    fclose(f);
    tgr_romdata = p;
    tgr_romdata_end = p + n;
    return 1;
}
#endif
