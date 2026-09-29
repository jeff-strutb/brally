/* xmloop.c -- where an XM module loops, in seconds, as libopenmpt plays it.
 *
 *   xmloop module.xm...   ->   one line per module: <file> <restart_s> <end_s>
 *
 * restart_s is the time at which the song's restart order begins (header
 * +0x42), end_s the length of one pass. The loop is [restart_s, end_s).
 * package_app.sh builds this against libopenmpt and ost_loops.py uses it to
 * know how long each piece's loop should be in a recording of it.
 */
#include <libopenmpt/libopenmpt.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    int i;
    for (i = 1; i < argc; i++) {
        FILE *f = fopen(argv[i], "rb");
        long n;
        unsigned char *d;
        openmpt_module *m;
        double end, rs;
        int restart;
        if (!f) { fprintf(stderr, "xmloop: %s: cannot open\n", argv[i]); return 1; }
        fseek(f, 0, SEEK_END);
        n = ftell(f);
        fseek(f, 0, SEEK_SET);
        d = malloc(n);
        if (fread(d, 1, n, f) != (size_t)n || n < 0x50) { fprintf(stderr, "xmloop: %s: short\n", argv[i]); return 1; }
        fclose(f);
        restart = d[0x42] | d[0x43] << 8;
        m = openmpt_module_create_from_memory2(d, n, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
        if (!m) { fprintf(stderr, "xmloop: %s: not a module\n", argv[i]); return 1; }
        end = openmpt_module_get_duration_seconds(m);
        rs = openmpt_module_set_position_order_row(m, restart, 0);
        printf("%s %.6f %.6f\n", argv[i], rs, end);
        openmpt_module_destroy(m);
        free(d);
    }
    return 0;
}
