/* data_image.c: the original's initialised data, read at start-up.
 *
 * A build linked with IMAGE=runtime (datalift.py --runtime-image) carries
 * none of BRGlide.dll's bytes: br_data_lift asks for them here. They come
 * from the BRGlide.dll on the game's CD root, through the file runs of the
 * sections that map the range, and must hash to what the build was made
 * from. Plain stdio: the core's fopen is the game's, with its path mapping. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host.h"

typedef struct { unsigned img, file, n; } plat_image_run;

const unsigned char *plat_data_image(const plat_image_run *runs, int nruns, unsigned size,
                                     long dll_size, unsigned long long digest)
{
    char path[1100];
    unsigned char *dll = NULL, *img = calloc(1, size);
    unsigned long long h = 0xcbf29ce484222325ull;
    long n = -1;
    unsigned i;
    FILE *f;
    snprintf(path, sizeof path, "%s/BRGlide.dll", host_cd_dir());
    if ((f = fopen(path, "rb")) != NULL) {
        fseek(f, 0, SEEK_END);
        n = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (n != dll_size || !(dll = malloc((size_t)n)) || fread(dll, 1, (size_t)n, f) != (size_t)n)
            n = -1;
        fclose(f);
    }
    if (n == dll_size && dll && img) {
        for (i = 0; i < (unsigned)nruns; i++)
            memcpy(img + runs[i].img, dll + runs[i].file, runs[i].n);
        for (i = 0; i < size; i++)
            h = (h ^ img[i]) * 0x100000001b3ull;
    }
    free(dll);
    if (h != digest) {
        fprintf(stderr, "datalift: %s is missing or not the BRGlide.dll this build reads\n", path);
        host_message_box("The game's data (disc/BRGlide.dll) is missing or damaged. "
                         "Build the game again with the release builder.", "Boss Rally");
        exit(1);
    }
    return img;
}
