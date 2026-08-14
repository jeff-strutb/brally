/* test_f3d.c -- validate the F3D walker against retail geometry. */
#include "br_f3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;
static void check(int c, const char *w)
{ printf("  [%s] %s\n", c ? "PASS" : "FAIL", w); if (!c) g_fail = 1; }

static void *slurp(const char *p, size_t *pcb)
{
    FILE *f = fopen(p, "rb"); long cb; void *b;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); cb = ftell(f); rewind(f);
    b = malloc((size_t)cb);
    if (b && fread(b, 1, (size_t)cb, f) != (size_t)cb) { free(b); b = NULL; }
    fclose(f); if (b) *pcb = (size_t)cb; return b;
}

static void run(const char *pszPath)
{
    size_t cb = 0;
    void *d = slurp(pszPath, &cb);
    BrF3dStats st;
    if (!d) { printf("  [FAIL] open %s\n", pszPath); g_fail = 1; return; }
    BrF3dScanFile(d, cb, &st);
    printf("  %-18s cmds=%-6u vtxloads=%-5u verts=%-6u tris=%-6u bad=%u unknown=%u\n",
           pszPath, st.cCommands, st.cVtxLoads, st.cVerticesLoaded,
           st.cTriangles, st.cBadIndices, st.cUnknownOps);
    /* Assert correctness invariants, not volume. Model complexity varies
     * legitimately -- ce.rca is a car, bb.rca is a sphere -- so a triangle
     * threshold would encode an expectation rather than a property. */
    check(st.cTriangles > 0 && st.cVtxLoads > 0, "found geometry");
    /* The decisive one: in a genuine F3D list every triangle index is even
     * (they are stored pre-doubled) and within the vertex cache. Random data
     * fails this immediately -- ~63/64 of random triples have an odd byte. */
    check(st.cBadIndices * 20 < st.cTriangles, "triangle indices valid");
    /* A clean walk should terminate at G_ENDDL, not run into foreign data. */
    check(st.cUnknownOps * 10 < st.cCommands, "walks terminate cleanly");
    /* Each G_VTX load must supply enough vertices for the triangles that
     * follow it; grossly fewer would mean the n field is misdecoded. */
    check(st.cVerticesLoaded >= st.cVtxLoads * 3, "vertex counts plausible");
    free(d);
}

int main(void)
{
    run("testdata/ce.rca");
    run("testdata/bb.rca");
    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
