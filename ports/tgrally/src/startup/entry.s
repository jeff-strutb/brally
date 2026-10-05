/* entry.s -- the boot entry: the code the IPL3 jumps to at 0x80200000
 */

/* WHAT IT DOES: Clear the whole .bss (0x802AC400, 0xD67B0 bytes) a
 * doubleword at a time, set the boot stack (its top at 0x803168D0) and jump
 * into BrBoot; never returns. */
/* @implements 0x80200000 tgr BrEntry */

/* registers by number: $8-$10 = t0-t2, $29 = sp (IDO as has no names) */
.set noreorder
.set noat

.text
.globl BrEntry
.ent BrEntry
BrEntry:
    lui     $8, %hi(D_802AC400)
    lui     $9, 0x000D
    addiu   $8, $8, %lo(D_802AC400)
    ori     $9, $9, 0x67B0
1:
    addi    $9, $9, -8
    sw      $0, 0($8)
    sw      $0, 4($8)
    bnez    $9, 1b
     addi   $8, $8, 8
    lui     $10, %hi(BrBoot)
    lui     $29, %hi(D_803168D0)
    addiu   $10, $10, %lo(BrBoot)
    jr      $10
     addiu  $29, $29, %lo(D_803168D0)
.end BrEntry
