# crt0.s: the PS-EXE's entry.  The BIOS has loaded the image at 0x80010000
# and jumps here; clear .bss, set gp and the stack, run main.  main never
# returns on the console; if it does, spin.
    .set noreorder
    .section .text.start, "ax", @progbits
    .global _start
    .type _start, @function
_start:
    la      $t0, __bss_start
    la      $t1, __bss_end
1:  sltu    $t2, $t0, $t1
    beqz    $t2, 2f
    nop
    sw      $zero, 0($t0)
    b       1b
    addiu   $t0, $t0, 4
2:  la      $gp, _gp
    la      $sp, __stack_top
    jal     main
    move    $fp, $sp
3:  b       3b
    nop

    .text
# the BIOS's character output (A0h:3Ch, std_out_putchar): DuckStation logs it
    .global bios_putchar
    .type bios_putchar, @function
bios_putchar:
    li      $t2, 0xA0
    jr      $t2
    li      $t1, 0x3C

# PCDRV, the debugger's host-file channel: a BREAK with a function code, the
# arguments in a1..a3, v0 0 on success and v1 the handle or the count.  These
# take C arguments in a0..a2 and return v1, or -1.
    .macro  pcdrv_ret
    bnez    $v0, 9f
    nop
    jr      $ra
    move    $v0, $v1
9:  jr      $ra
    li      $v0, -1
    .endm

    .global pcdrv_init              # int pcdrv_init(void)
pcdrv_init:
    .word   (0x101 << 6) | 0x0D
    jr      $ra
    nop
    .global pcdrv_creat             # int pcdrv_creat(const char *name)
pcdrv_creat:
    move    $a1, $a0
    move    $a2, $zero
    .word   (0x102 << 6) | 0x0D
    pcdrv_ret
    .global pcdrv_open              # int pcdrv_open(const char *name, int mode)
pcdrv_open:
    move    $a2, $a1
    move    $a1, $a0
    .word   (0x103 << 6) | 0x0D
    pcdrv_ret
    .global pcdrv_close             # int pcdrv_close(int fd)
pcdrv_close:
    move    $a1, $a0
    .word   (0x104 << 6) | 0x0D
    pcdrv_ret
    .global pcdrv_read              # int pcdrv_read(int fd, void *buf, int len)
pcdrv_read:
    move    $a3, $a1
    move    $a1, $a0
    .word   (0x105 << 6) | 0x0D
    pcdrv_ret
    .global pcdrv_write             # int pcdrv_write(int fd, const void *buf, int len)
pcdrv_write:
    move    $a3, $a1
    move    $a1, $a0
    .word   (0x106 << 6) | 0x0D
    pcdrv_ret
    .global pcdrv_seek              # int pcdrv_seek(int fd, int off, int whence)
pcdrv_seek:
    move    $a3, $a2
    move    $a2, $a1
    move    $a1, $a0
    .word   (0x107 << 6) | 0x0D
    pcdrv_ret
