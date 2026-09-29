/* bcopy.s -- libultra's memory copy (libc/bcopy.s).
 */
/* n64-cflags: -O1 -mips3 -32 */

/* WHAT IT DOES: Copy len bytes from src to dst, safe for overlap (backwards
 * when dst is above src and within the source): by words, halfwords or bytes
 * as the two addresses' alignment allows, in blocks where it can. */
/* @implements 0x802674D0 tgr bcopy */
/* registers by number: IDO's as has no names */
.set noreorder
.set noat

.text

.globl bcopy
.ent bcopy
bcopy:
    beqz    $6, .L8026753C
    move    $7, $5
    beq     $4, $5, .L8026753C
    slt     $1, $5, $4
    bnel    $1, $0, .L80267504
    slti    $1, $6, 0x10
    add     $2, $4, $6
    slt     $1, $5, $2
    beql    $1, $0, .L80267504
    slti    $1, $6, 0x10
    b       .L80267668
    slti    $1, $6, 0x10
    slti    $1, $6, 0x10
.L80267504:
    bnez    $1, .L8026751C
    nop
    andi    $2, $4, 3
    andi    $3, $5, 3
    beq     $2, $3, .L80267544
    nop
.L8026751C:
    beqz    $6, .L8026753C
    nop
    addu    $3, $4, $6
.L80267528:
    lb      $2, 0($4)
    addiu   $4, $4, 1
    addiu   $5, $5, 1
    bne     $4, $3, .L80267528
    sb      $2, -1($5)
.L8026753C:
    jr      $31
    move    $2, $7
.L80267544:
    beqz    $2, .L802675A8
    addiu   $1, $0, 1
    beq     $2, $1, .L8026758C
    addiu   $1, $0, 2
    beql    $2, $1, .L80267578
    lh      $2, 0($4)
    lb      $2, 0($4)
    addiu   $4, $4, 1
    addiu   $5, $5, 1
    addiu   $6, $6, -1
    b       .L802675A8
    sb      $2, -1($5)
    lh      $2, 0($4)
.L80267578:
    addiu   $4, $4, 2
    addiu   $5, $5, 2
    addiu   $6, $6, -2
    b       .L802675A8
    sh      $2, -2($5)
.L8026758C:
    lb      $2, 0($4)
    lh      $3, 1($4)
    addiu   $4, $4, 3
    addiu   $5, $5, 3
    addiu   $6, $6, -3
    sb      $2, -3($5)
    sh      $3, -2($5)
.L802675A8:
    slti    $1, $6, 0x20
    bnel    $1, $0, .L80267608
    slti    $1, $6, 0x10
    lw      $2, 0($4)
    lw      $3, 4($4)
    lw      $8, 8($4)
    lw      $9, 0xc($4)
    lw      $10, 0x10($4)
    lw      $11, 0x14($4)
    lw      $12, 0x18($4)
    lw      $13, 0x1c($4)
    addiu   $4, $4, 0x20
    addiu   $5, $5, 0x20
    addiu   $6, $6, -0x20
    sw      $2, -0x20($5)
    sw      $3, -0x1c($5)
    sw      $8, -0x18($5)
    sw      $9, -0x14($5)
    sw      $10, -0x10($5)
    sw      $11, -0xc($5)
    sw      $12, -8($5)
    b       .L802675A8
    sw      $13, -4($5)
.L80267604:
    slti    $1, $6, 0x10
.L80267608:
    bnel    $1, $0, .L80267644
    slti    $1, $6, 4
    lw      $2, 0($4)
    lw      $3, 4($4)
    lw      $8, 8($4)
    lw      $9, 0xc($4)
    addiu   $4, $4, 0x10
    addiu   $5, $5, 0x10
    addiu   $6, $6, -0x10
    sw      $2, -0x10($5)
    sw      $3, -0xc($5)
    sw      $8, -8($5)
    b       .L80267604
    sw      $9, -4($5)
.L80267640:
    slti    $1, $6, 4
.L80267644:
    bnez    $1, .L8026751C
    nop
    lw      $2, 0($4)
    addiu   $4, $4, 4
    addiu   $5, $5, 4
    addiu   $6, $6, -4
    b       .L80267640
    sw      $2, -4($5)
    slti    $1, $6, 0x10
.L80267668:
    add     $4, $4, $6
    bnez    $1, .L80267684
    add     $5, $5, $6
    andi    $2, $4, 3
    andi    $3, $5, 3
    beq     $2, $3, .L802676B4
    nop
.L80267684:
    beqz    $6, .L8026753C
    nop
    addiu   $4, $4, -1
    addiu   $5, $5, -1
    subu    $3, $4, $6
.L80267698:
    lb      $2, 0($4)
    addiu   $4, $4, -1
    addiu   $5, $5, -1
    bne     $4, $3, .L80267698
    sb      $2, 1($5)
    jr      $31
    move    $2, $7
.L802676B4:
    beqz    $2, .L80267718
    addiu   $1, $0, 3
    beq     $2, $1, .L802676FC
    addiu   $1, $0, 2
    beql    $2, $1, .L802676E8
    lh      $2, -2($4)
    lb      $2, -1($4)
    addiu   $4, $4, -1
    addiu   $5, $5, -1
    addiu   $6, $6, -1
    b       .L80267718
    sb      $2, 0($5)
    lh      $2, -2($4)
.L802676E8:
    addiu   $4, $4, -2
    addiu   $5, $5, -2
    addiu   $6, $6, -2
    b       .L80267718
    sh      $2, 0($5)
.L802676FC:
    lb      $2, -1($4)
    lh      $3, -3($4)
    addiu   $4, $4, -3
    addiu   $5, $5, -3
    addiu   $6, $6, -3
    sb      $2, 2($5)
    sh      $3, 0($5)
.L80267718:
    slti    $1, $6, 0x20
    bnel    $1, $0, .L80267778
    slti    $1, $6, 0x10
    lw      $2, -4($4)
    lw      $3, -8($4)
    lw      $8, -0xc($4)
    lw      $9, -0x10($4)
    lw      $10, -0x14($4)
    lw      $11, -0x18($4)
    lw      $12, -0x1c($4)
    lw      $13, -0x20($4)
    addiu   $4, $4, -0x20
    addiu   $5, $5, -0x20
    addiu   $6, $6, -0x20
    sw      $2, 0x1c($5)
    sw      $3, 0x18($5)
    sw      $8, 0x14($5)
    sw      $9, 0x10($5)
    sw      $10, 0xc($5)
    sw      $11, 8($5)
    sw      $12, 4($5)
    b       .L80267718
    sw      $13, 0($5)
.L80267774:
    slti    $1, $6, 0x10
.L80267778:
    bnel    $1, $0, .L802677B4
    slti    $1, $6, 4
    lw      $2, -4($4)
    lw      $3, -8($4)
    lw      $8, -0xc($4)
    lw      $9, -0x10($4)
    addiu   $4, $4, -0x10
    addiu   $5, $5, -0x10
    addiu   $6, $6, -0x10
    sw      $2, 0xc($5)
    sw      $3, 8($5)
    sw      $8, 4($5)
    b       .L80267774
    sw      $9, 0($5)
.L802677B0:
    slti    $1, $6, 4
.L802677B4:
    bnez    $1, .L80267684
    nop
    lw      $2, -4($4)
    addiu   $4, $4, -4
    addiu   $5, $5, -4
    addiu   $6, $6, -4
    b       .L802677B0
    sw      $2, 0($5)
.end bcopy
