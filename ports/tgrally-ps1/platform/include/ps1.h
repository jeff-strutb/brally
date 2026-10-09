/* ps1.h: the console underneath the platform layer (hw/) */
#ifndef PS1_H
#define PS1_H
#include <stdint.h>
#include "host.h"

#define PS1_IO(a) (*(volatile uint32_t *)(0x1F800000u + (a)))
#define PS1_IO16(a) (*(volatile uint16_t *)(0x1F800000u + (a)))
#define PS1_I_STAT PS1_IO(0x1070)
#define PS1_I_MASK PS1_IO(0x1074)

uint64_t ps1_time_ns(void);         /* since start-up, from the retraces and root counter 2 */
uint32_t ps1_retraces(void);        /* VBlanks seen */
uint32_t ps1_gp(void);
void     ps1_fatal(const char *msg) __attribute__((noreturn));
int      ps1_pad_read(host_pad *p);
void     ps1_tty(const char *s);
void     ps1_log_open(const char *name);  /* stdout and stderr to a host file too */
void     ps1_hw_init(void);
void     ps1_exc_install(void);      /* the port's exception vector (hw/exc.s, hw/irq.c) */
void     ps1_flush_icache(void);
uint32_t ps1_vblanks(void);          /* VBlank interrupts so far */
void     ps1_on_vblank(void (*fn)(void));
void     ps1_pc_sample(uint32_t every);  /* print the interrupted pc every n VBlanks */

/* PCDRV (hw/crt0.s): the debugger's host files */
int pcdrv_init(void);
int pcdrv_creat(const char *name);
int pcdrv_open(const char *name, int mode);     /* 0 read, 1 write, 2 both */
int pcdrv_close(int fd);
int pcdrv_read(int fd, void *buf, int len);
int pcdrv_write(int fd, const void *buf, int len);
int pcdrv_seek(int fd, int off, int whence);
int bios_putchar(int c);
#endif
