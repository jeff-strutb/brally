/* irq.c: the console's interrupts.  The exception vector is the port's own
 * (hw/exc.s); the BIOS's handler is gone, so nothing of the BIOS that needs
 * interrupts is used (its TTY output and PCDRV do not). */
#include <stdint.h>
#include <string.h>
#include "ps1.h"

extern char ps1_exc_stub[], ps1_exc_stub_end[];
static volatile uint32_t s_vblank;
static void (*s_vblank_fn)(void);

static uint32_t sr_get(void) { uint32_t v; __asm__ volatile("mfc0 %0, $12" : "=r"(v)); return v; }
static void sr_set(uint32_t v) { __asm__ volatile("mtc0 %0, $12; nop; nop" : : "r"(v)); }

void ps1_exc_install(void)
{
    uint32_t sr = sr_get();
    sr_set(sr & ~1u);                       /* interrupts off while the vector changes */
    PS1_I_MASK = 0;
    PS1_I_STAT = 0;
    memcpy((void *)0x80000080, ps1_exc_stub, (size_t)(ps1_exc_stub_end - ps1_exc_stub));
    ps1_flush_icache();
    PS1_I_MASK = 1;                         /* the VBlank */
    sr_set((sr & ~0xFF00u) | 0x0401u);      /* IM2 (the interrupt controller), IEc */
}

uint32_t ps1_vblanks(void) { return s_vblank; }

/* a sample of where the CPU is, every n VBlanks (TGR_PCSAMPLE=n): the
 * interrupted PC and return address, for finding a hang without a debugger */
extern uint32_t exc_frame[];
static uint32_t s_sample;
void ps1_pc_sample(uint32_t every) { s_sample = every; }
static void hex(const char *name, uint32_t v);
void ps1_on_vblank(void (*fn)(void)) { s_vblank_fn = fn; }

void ps1_irq(uint32_t cause)
{
    uint32_t st = PS1_I_STAT & PS1_I_MASK;
    (void)cause;
    PS1_I_STAT = ~st;                       /* acknowledge what is handled */
    if (st & 1) {
        s_vblank++;
        if (s_vblank_fn)
            s_vblank_fn();
        if (s_sample && s_vblank % s_sample == 0) {
            hex("pc=", exc_frame[31]);
            hex("ra=", exc_frame[17]);
            hex("sp=", exc_frame[18]);
            ps1_tty("\n");
        }
    }
}

static void hex(const char *name, uint32_t v)
{
    static const char d[] = "0123456789ABCDEF";
    char b[16];
    int i;
    ps1_tty(name);
    for (i = 0; i < 8; i++)
        b[i] = d[(v >> (28 - 4 * i)) & 15];
    b[8] = ' ';
    b[9] = 0;
    ps1_tty(b);
}

/* a fault: where and why, the registers, then stop (written to fatal.txt too) */
void ps1_fault(uint32_t cause, uint32_t epc, uint32_t bad, const uint32_t *frame)
{
    static const char *names[] = {"at", "v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3", "t4", "t5",
                                  "t6", "t7", "t8", "t9", "ra", "sp", "gp", "s0", "s1", "s2", "s3", "s4", "s5",
                                  "s6", "s7", "fp"};
    int i;
    ps1_tty("\nFAULT ");
    hex("cause=", cause);
    hex("epc=", epc);
    hex("badvaddr=", bad);
    ps1_tty("\n");
    for (i = 0; i < 29; i++) {
        ps1_tty(names[i]);
        hex("=", frame[i]);
        if (i % 6 == 5)
            ps1_tty("\n");
    }
    ps1_fatal("fault (registers on the TTY)");
}
