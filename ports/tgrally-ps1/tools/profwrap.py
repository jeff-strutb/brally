#!/usr/bin/env python3
"""profwrap.py LIST OUT.s -- inclusive timing wrappers for the functions named
in LIST (one a line): link.sh renames each in the core object that defines it
to __prof_NAME, and NAME becomes a wrapper that notes root counter 2 (the
system clock / 8) on a shadow stack, calls the function on the caller's own
stack (so arguments passed on it still line up), and adds the time and a call
to the function's entry in prof_table, which perf_ps1.c prints."""
import sys

names = [l.split()[0] for l in open(sys.argv[1]) if l.strip() and not l.startswith('#')]
o = ['    .set noreorder', '    .set noat', '    .text']
for i, n in enumerate(names):
    o += ['    .globl %s' % n, '    .type %s,@function' % n, '%s:' % n,
          '    la $t0, prof_sp', '    lw $t1, 0($t0)',
          '    lui $t2, 0x1F80', '    lw $t2, 0x1120($t2)',
          '    sw $ra, 0($t1)', '    sw $t2, 4($t1)', '    addiu $t1, $t1, 8',
          '    jal __prof_%s' % n, '    sw $t1, 0($t0)',
          '    la $t0, prof_sp', '    lw $t1, 0($t0)', '    lui $t3, 0x1F80',
          '    lw $t3, 0x1120($t3)', '    addiu $t1, $t1, -8', '    sw $t1, 0($t0)',
          '    lw $ra, 0($t1)', '    lw $t2, 4($t1)', '    la $t4, prof_table + %d' % (12 * i + 4),
          '    subu $t3, $t3, $t2', '    andi $t3, $t3, 0xFFFF', '    lw $t5, 0($t4)', '    lw $t6, 4($t4)',
          '    addu $t5, $t5, $t3', '    addiu $t6, $t6, 1', '    sw $t5, 0($t4)', '    jr $ra', '    sw $t6, 4($t4)']
o += ['    .data', '    .globl prof_sp', 'prof_sp: .word prof_stack', 'prof_stack: .space 512',
      '    .globl prof_table', 'prof_table:']
for i, n in enumerate(names):
    o += ['    .word prof_name%d, 0, 0' % i]
o += ['    .globl prof_count', 'prof_count: .word %d' % len(names), '    .section .rodata']
for i, n in enumerate(names):
    o += ['prof_name%d: .asciz "%s"' % (i, n)]
open(sys.argv[2], 'w').write('\n'.join(o) + '\n')
