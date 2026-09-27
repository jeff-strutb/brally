/* n64-cflags: -O2 -mips3 -32 */
/* mixer.s -- the software audio mixer's inner loops, hand-written MIPS III
 * (64-bit 32.32 sample-position accumulators), and the doubleword copy,
 * fill and float-to-int helpers assembled with them (one object, starting at
 * 0x80256270 after the preceding object's padding)
 */

/* WHAT IT DOES: Copy 16 bytes as two doublewords (both addresses 8-byte
 * aligned). */
/* @implements 0x80256270 tgr BrCopy16 */

/* WHAT IT DOES: Fill memory with a 32-bit value, 64 bytes at a time: the
 * length is rounded up to 64 (lengths under 64 KB), the value doubled into
 * a doubleword. */
/* @implements 0x80256290 tgr BrFill64 */

/* WHAT IT DOES: Convert a float to an int by truncation (trunc.w.s), without
 * the rounding-mode save and restore a C cast compiles to. */
/* @implements 0x802562E0 tgr BrFloatToInt */

/* WHAT IT DOES: Mix the music voices into a 16 KB ring of 16-bit stereo
 * samples: for each output pair, step every voice's 32.32 position and add
 * its 8-bit sample times the voice's volume, wrapping at the ring's end. */
/* @implements 0x802562F0 tgr BrMixMusic */

/* WHAT IT DOES: Mix one music voice (selected by its byte offset in the
 * voice table) into the ring: left and right volumes applied to its 8-bit
 * samples, 32.32 position stepped per sample pair. */
/* @implements 0x8025649C tgr BrMixMusicVoice */

/* WHAT IT DOES: Mix the sound-effect voices into the ring four 16-bit
 * samples at a time, masking each lane, a voice's optional second sample
 * source added to its first. */
/* @implements 0x8025658C tgr BrMixSfx */

/* registers by number: IDO's as has no names */
.set noreorder
.set noat

.text

.globl BrCopy16
.ent BrCopy16
BrCopy16:
    ld      $12, 0($5)
    ld      $14, 8($5)
    sd      $12, 0($4)
    sd      $14, 8($4)
    jr      $31
    nop
.end BrCopy16

.globl BrFill64
.ent BrFill64
BrFill64:
    addiu   $5, $5, 0x3f
    andi    $5, $5, 0xffc0
    addu    $5, $5, $4
    dsll    $7, $6, 0x10
    dsll    $7, $7, 0x10
    or      $6, $6, $7
    sd      $6, 0($4)
.L802562AC:
    sd      $6, 8($4)
    sd      $6, 0x10($4)
    sd      $6, 0x18($4)
    addiu   $4, $4, 0x40
    sd      $6, -0x20($4)
    sd      $6, -0x18($4)
    sd      $6, -0x10($4)
    sd      $6, -8($4)
    bnel    $5, $4, .L802562AC
    sd      $6, 0($4)
    jr      $31
    nop
.end BrFill64

.globl BrFloatToInt
.ent BrFloatToInt
BrFloatToInt:
    trunc.w.s $f12, $f12
    jr      $31
    mfc1    $2, $f12
.end BrFloatToInt

.globl BrMixMusic
.ent BrMixMusic
BrMixMusic:
    addiu   $29, $29, -0x30
    sw      $16, 0($29)
    sw      $17, 4($29)
    sw      $18, 8($29)
    sw      $19, 0xc($29)
    sw      $4, 0x10($29)
    sw      $20, 0x14($29)
    sw      $21, 0x18($29)
    sw      $22, 0x1c($29)
    sw      $23, 0x20($29)
    sw      $30, 0x24($29)
    sw      $28, 0x28($29)
    lui     $8, %hi(D_802A4790)
    addiu   $8, $8, %lo(D_802A4790)
    lhu     $6, 0($8)
    nop
    add     $7, $6, $5
    addi    $3, $7, -0x4000
    bgtz    $3, .L80256344
    add     $4, $4, $6
    lui     $3, 0
.L80256344:
    sub     $2, $5, $3
    ld      $5, 8($8)
    ld      $6, 0x10($8)
    lw      $16, 0x18($8)
    ld      $7, 0x38($8)
    ld      $9, 0x40($8)
    lw      $17, 0x48($8)
    ld      $10, 0x20($8)
    ld      $11, 0x28($8)
    lw      $18, 0x30($8)
    ld      $12, 0x50($8)
    ld      $13, 0x58($8)
    lw      $19, 0x60($8)
    ld      $20, 0x68($8)
    ld      $21, 0x70($8)
    lw      $22, 0x78($8)
    ld      $23, 0x80($8)
    ld      $30, 0x88($8)
    lw      $28, 0x90($8)
    beqz    $2, .L80256434
    nop
.L80256398:
    daddu   $5, $5, $6
    daddu   $7, $7, $9
    daddu   $10, $10, $11
    daddu   $12, $12, $13
    daddu   $20, $20, $21
    daddu   $23, $23, $30
    addi    $2, $2, -4
    dsra32  $25, $5, 0
    lb      $24, 0($25)
    mult    $16, $24
    mflo    $24
    dsra32  $14, $7, 0
    lb      $25, 0($14)
    mult    $17, $25
    mflo    $25
    add     $24, $24, $25
    dsra32  $14, $20, 0
    lb      $25, 0($14)
    mult    $22, $25
    mflo    $25
    add     $24, $24, $25
    sh      $24, 0($4)
    dsra32  $15, $10, 0
    lb      $15, 0($15)
    mult    $18, $15
    mflo    $15
    dsra32  $25, $12, 0
    lb      $25, 0($25)
    mult    $19, $25
    mflo    $25
    add     $25, $25, $15
    dsra32  $15, $23, 0
    lb      $15, 0($15)
    mult    $28, $15
    mflo    $15
    add     $25, $25, $15
    sh      $25, 2($4)
    bgtz    $2, .L80256398
    addi    $4, $4, 4
.L80256434:
    beqz    $3, .L80256448
    addi    $2, $3, 0
    addi    $4, $4, -0x4000
    j       .L80256398
    sub     $3, $3, $3
.L80256448:
    sd      $5, 8($8)
    sd      $7, 0x38($8)
    sd      $10, 0x20($8)
    sd      $12, 0x50($8)
    sd      $20, 0x68($8)
    sd      $23, 0x80($8)
    lw      $6, 0x10($29)
    lw      $16, 0($29)
    lw      $17, 4($29)
    sub     $4, $4, $6
    lw      $18, 8($29)
    lw      $19, 0xc($29)
    sh      $4, 0($8)
    lw      $20, 0x14($29)
    lw      $21, 0x18($29)
    lw      $22, 0x1c($29)
    lw      $23, 0x20($29)
    lw      $30, 0x24($29)
    lw      $28, 0x28($29)
    jr      $31
    addiu   $29, $29, 0x30
.end BrMixMusic

.globl BrMixMusicVoice
.ent BrMixMusicVoice
BrMixMusicVoice:
    addiu   $29, $29, -0x30
    sw      $16, 0($29)
    sw      $17, 4($29)
    sw      $18, 8($29)
    sw      $4, 0x10($29)
    sw      $28, 0x28($29)
    lui     $8, %hi(D_802A4790)
    addiu   $8, $8, %lo(D_802A4790)
    addu    $18, $6, $8
    lhu     $6, 0($8)
    nop
    add     $7, $6, $5
    addi    $3, $7, -0x4000
    bgtz    $3, .L802564DC
    add     $4, $4, $6
    lui     $3, 0
.L802564DC:
    sub     $2, $5, $3
    ld      $5, 0x98($18)
    ld      $6, 0xa0($18)
    lw      $16, 0xa8($18)
    ld      $7, 0xb0($18)
    ld      $9, 0xb8($18)
    lw      $17, 0xc0($18)
    beqz    $2, .L8025654C
    nop
.L80256500:
    daddu   $5, $5, $6
    daddu   $7, $7, $9
    addi    $2, $2, -4
    dsra32  $25, $5, 0
    lb      $24, 0($25)
    mult    $16, $24
    mflo    $24
    dsra32  $14, $7, 0
    lb      $25, 0($14)
    mult    $17, $25
    mflo    $25
    lh      $15, 0($4)
    add     $24, $15, $24
    sh      $24, 0($4)
    lh      $15, 2($4)
    add     $25, $15, $25
    sh      $25, 2($4)
    bgtz    $2, .L80256500
    addi    $4, $4, 4
.L8025654C:
    beqz    $3, .L80256560
    addi    $2, $3, 0
    addi    $4, $4, -0x4000
    j       .L80256500
    sub     $3, $3, $3
.L80256560:
    sd      $5, 0x98($18)
    sd      $7, 0xb0($18)
    lw      $6, 0x10($29)
    lw      $16, 0($29)
    lw      $17, 4($29)
    lw      $18, 8($29)
    sub     $4, $4, $6
    sh      $4, 0($8)
    lw      $28, 0x28($29)
    jr      $31
    addiu   $29, $29, 0x30
.end BrMixMusicVoice

.globl BrMixSfx
.ent BrMixSfx
BrMixSfx:
    addiu   $29, $29, -0x28
    sw      $16, 0($29)
    sw      $17, 4($29)
    sw      $18, 8($29)
    sw      $19, 0xc($29)
    sw      $4, 0x10($29)
    sw      $20, 0x14($29)
    sw      $21, 0x18($29)
    sw      $22, 0x1c($29)
    lui     $8, %hi(D_802A4918)
    addiu   $8, $8, %lo(D_802A4918)
    add     $7, $6, $5
    addi    $3, $7, -0x4000
    bgtz    $3, .L802565CC
    add     $4, $4, $6
    lui     $3, 0
.L802565CC:
    sub     $2, $5, $3
    ld      $5, 8($8)
    ld      $6, 0x10($8)
    lw      $16, 0x18($8)
    ld      $7, 0x20($8)
    ld      $9, 0x28($8)
    lw      $17, 0x30($8)
    ld      $10, 0x38($8)
    ld      $11, 0x40($8)
    lw      $18, 0x48($8)
    ld      $12, 0x50($8)
    ld      $13, 0x58($8)
    lw      $19, 0x60($8)
    ld      $20, 0x68($8)
    ld      $21, 0x70($8)
    lw      $22, 0x78($8)
    beqz    $2, .L802566CC
    nop
.L80256614:
    daddu   $5, $5, $6
    daddu   $7, $7, $9
    daddu   $10, $10, $11
    daddu   $12, $12, $13
    daddu   $20, $20, $21
    lw      $24, 0x98($8)
    ld      $15, 0($4)
    beqz    $24, .L80256640
    dsra32  $25, $5, 0
    addu    $24, $24, $25
    lb      $24, 0($24)
.L80256640:
    lb      $25, 0($25)
    add     $25, $24, $25
    mult    $16, $25
    mflo    $24
    dsra32  $25, $7, 0
    lb      $25, 0($25)
    mult    $17, $25
    mflo    $25
    add     $24, $24, $25
    dsra32  $25, $10, 0
    lb      $25, 0($25)
    mult    $18, $25
    mflo    $25
    add     $24, $24, $25
    dsra32  $25, $12, 0
    lb      $25, 0($25)
    mult    $19, $25
    mflo    $25
    add     $24, $24, $25
    dsra32  $25, $20, 0
    lb      $25, 0($25)
    mult    $22, $25
    mflo    $25
    add     $24, $24, $25
    ld      $14, 0($8)
    and     $24, $24, $14
    dsll32  $25, $24, 0
    daddu   $25, $25, $24
    and     $15, $15, $14
    daddu   $25, $25, $15
    and     $25, $25, $14
    addi    $2, $2, -8
    sd      $25, 0($4)
    bgtz    $2, .L80256614
    addi    $4, $4, 8
.L802566CC:
    beqz    $3, .L802566E0
    addi    $2, $3, 0
    addi    $4, $4, -0x4000
    j       .L80256614
    sub     $3, $3, $3
.L802566E0:
    sd      $5, 8($8)
    sd      $7, 0x20($8)
    sd      $10, 0x38($8)
    sd      $12, 0x50($8)
    sd      $20, 0x68($8)
    lw      $16, 0($29)
    lw      $17, 4($29)
    lw      $18, 8($29)
    lw      $19, 0xc($29)
    lw      $4, 0x10($29)
    lw      $20, 0x14($29)
    lw      $21, 0x18($29)
    lw      $22, 0x1c($29)
    jr      $31
    addiu   $29, $29, 0x28
.end BrMixSfx

