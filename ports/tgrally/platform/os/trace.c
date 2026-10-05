/* trace.c: what n64box logs at every retrace, for the lockstep comparison
 * (n64/tools/n64box.py --log): "frame kind value" lines. */
#include <stdarg.h>
#include <stdio.h>
#include "plat.h"

static FILE *s_f;

void tgr_trace(const char *kind, const char *fmt, ...)
{
    va_list ap;
    if (!g_tgr.trace)
        return;
    if (!s_f && !(s_f = fopen(g_tgr.trace, "w")))
        return;
    fprintf(s_f, "%u %s ", tgr_frame(), kind);
    va_start(ap, fmt);
    vfprintf(s_f, fmt, ap);
    va_end(ap);
    fputc('\n', s_f);
    fflush(s_f);
}

/* the retrace: the controllers sampled, the frame shown */
void tgr_vi_retrace(void)
{
    tgr_input_frame(tgr_frame());
    tgr_gfx_present();
}
