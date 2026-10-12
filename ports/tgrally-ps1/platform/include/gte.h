/* gte.h: the geometry transformation engine (coprocessor 2) */
#ifndef PS1_GTE_H
#define PS1_GTE_H
#include <stdint.h>

/* data registers */
#define GTE_VXY0 0
#define GTE_VZ0  1
#define GTE_RGB2 22
#define GTE_IR1  9
#define GTE_IR2  10
#define GTE_IR3  11
#define GTE_SXY2 14
#define GTE_SZ3  19
#define GTE_MAC0 24
#define GTE_MAC1 25
#define GTE_MAC2 26
#define GTE_MAC3 27
/* control registers */
#define GTE_RT11RT12 0
#define GTE_RT13RT21 1
#define GTE_RT22RT23 2
#define GTE_RT31RT32 3
#define GTE_RT33     4
#define GTE_TRX      5
#define GTE_TRY      6
#define GTE_TRZ      7
#define GTE_L11L12   8
#define GTE_L13L21   9
#define GTE_L22L23   10
#define GTE_L31L32   11
#define GTE_L33      12
#define GTE_RBK      13
#define GTE_GBK      14
#define GTE_BBK      15
#define GTE_LR1LR2   16
#define GTE_LR3LG1   17
#define GTE_LG2LG3   18
#define GTE_LB1LB2   19
#define GTE_LB3      20
#define GTE_OFX      24
#define GTE_OFY      25
#define GTE_H        26
#define GTE_FLAG     31

#define gte_ctc(reg, v) __asm__ volatile("ctc2 %0, $%1\n\tnop\n\tnop" : : "r"((uint32_t)(v)), "i"(reg))
#define gte_mtc(reg, v) __asm__ volatile("mtc2 %0, $%1\n\tnop\n\tnop" : : "r"((uint32_t)(v)), "i"(reg))
#define gte_mfc(reg) ({ uint32_t v_; __asm__ volatile("mfc2 %0, $%1\n\tnop" : "=r"(v_) : "i"(reg)); v_; })
#define gte_cfc(reg) ({ uint32_t v_; __asm__ volatile("cfc2 %0, $%1\n\tnop" : "=r"(v_) : "i"(reg)); v_; })

/* commands (cop2 imm25) */
#define gte_rtps()  __asm__ volatile("nop\n\tnop\n\t.word 0x4A180001")    /* perspective: V0 */
#define gte_ncs()   __asm__ volatile("nop\n\tnop\n\t.word 0x4AC8041E")    /* light V0 (a normal): RGB2 */
#define gte_mvmva_rt_v0_tr_sf1() __asm__ volatile("nop\n\tnop\n\t.word 0x4A480012")

void gte_init(void);
#endif
