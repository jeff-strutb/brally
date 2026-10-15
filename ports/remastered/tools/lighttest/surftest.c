/* surftest.c: light_surf_build (light_surf.h) on each line of LIST, "hash material path"
 * (a PAM, BR_LIGHT_TEXOUT's), each map written to OUTDIR/<hash>.bin: its size and
 * regions (int32 w, h, n), surfaces (8 bytes), mean colours (8 x 3 float), the map
 * (w*h*4), the variation (w*h*2).  surfparity.sh compares them with texregion.py. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "light_surf.h"
int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "surftest LIST OUTDIR\n");
        return 2;
    }
    FILE *l = fopen(argv[1], "r");
    char hs[64], path[1024], buf[4096];
    int mat, n = 0, made = 0;
    while (fscanf(l, "%63s %d %1023s", hs, &mat, path) == 3) {
        FILE *f = fopen(path, "rb");
        int w = 0, h = 0;
        size_t hl = 0;
        light_surf_map m;
        uint8_t *px;
        size_t got = fread(buf, 1, sizeof buf, f);
        char *e = strstr(buf, "ENDHDR\n");
        sscanf(strstr(buf, "WIDTH"), "WIDTH %d", &w);
        sscanf(strstr(buf, "HEIGHT"), "HEIGHT %d", &h);
        hl = (size_t)(e - buf) + 7;
        px = malloc((size_t)w * h * 4);
        fseek(f, (long)hl, SEEK_SET);
        fread(px, 1, (size_t)w * h * 4, f);
        fclose(f);
        (void)got;
        n++;
        if (light_surf_build(mat, px, w, h, &m)) {
            char o[1200];
            FILE *g;
            snprintf(o, sizeof o, "%s/%s.bin", argv[2], hs);
            g = fopen(o, "wb");
            fwrite(&m.w, 4, 1, g); fwrite(&m.h, 4, 1, g); fwrite(&m.n, 4, 1, g);
            fwrite(m.surf, 1, 8, g); fwrite(m.lin, 4, 24, g);
            fwrite(m.map, 1, (size_t)m.w * m.h * 4, g); fwrite(m.var, 1, (size_t)m.w * m.h * 2, g);
            fclose(g);
            light_surf_free(&m);
            made++;
        }
        free(px);
    }
    printf("%d textures, %d maps\n", n, made);
    return 0;
}
