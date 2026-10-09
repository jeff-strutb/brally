/* ps1hw.c: the console's clocks, the TTY, and stopping on a fatal error. */
#include <stdint.h>
#include <string.h>
#include "ps1.h"

#define RC1_VALUE PS1_IO(0x1110)
#define RC1_MODE  PS1_IO(0x1114)
#define HBLANK_NS 63556u                    /* NTSC: 15,734 lines a second */

/* root counter 1 counts the lines (HBlanks); its 16 bits wrap every 4 s and
 * are unwrapped whenever the time is read, and at least at every VBlank */
static uint64_t s_lines;
static uint32_t s_last;

static uint64_t lines(void)
{
    uint32_t v = RC1_VALUE & 0xFFFF;
    s_lines += (v - s_last) & 0xFFFF;
    s_last = v;
    return s_lines;
}

static void on_vblank(void) { lines(); }

void ps1_hw_init(void)
{
    RC1_MODE = 0x0100;                      /* free-running, counting HBlanks */
    s_last = RC1_VALUE & 0xFFFF;
    ps1_exc_install();
    ps1_on_vblank(on_vblank);
}

uint64_t ps1_time_ns(void)
{
    uint64_t n;
    uint32_t sr;
    __asm__ volatile("mfc0 %0, $12" : "=r"(sr));
    __asm__ volatile("mtc0 %0, $12; nop; nop" : : "r"(sr & ~1u));
    n = lines();
    __asm__ volatile("mtc0 %0, $12; nop; nop" : : "r"(sr));
    return n * HBLANK_NS;
}

uint32_t ps1_retraces(void)
{
    return ps1_vblanks();
}

uint32_t ps1_gp(void)
{
    uint32_t gp;
    __asm__ volatile("move %0, $gp" : "=r"(gp));
    return gp;
}

void ps1_tty(const char *s)
{
    while (*s)
        bios_putchar(*s++);
}

void ps1_fatal(const char *msg)
{
    int fd;
    ps1_tty("FATAL: ");
    ps1_tty(msg);
    ps1_tty("\n");
    if ((fd = pcdrv_creat("fatal.txt")) >= 0) {
        pcdrv_write(fd, msg, (int)strlen(msg));
        pcdrv_close(fd);
    }
    if ((fd = pcdrv_creat("done")) >= 0)
        pcdrv_close(fd);
    for (;;)
        ;
}

int ps1_pad_read(host_pad *p)
{
    memset(p, 0, sizeof *p);
    p->pov = -1;
    return 0;
}
