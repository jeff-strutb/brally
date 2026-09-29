/* maptlbrdb.s -- libultra's debugger window mapping (os/maptlbrdb.s).
 */
/* n64-cflags: -O1 -mips3 -32 */

/* WHAT IT DOES: Map TLB entry 31 so 0xC0000000 reaches the development
 * board's RDB port (physical 0x80000000, uncached), keeping the current
 * ASID. */
/* @implements 0x8026BB40 tgr osMapTLBRdb */
/* registers by number: IDO's as has no names */
.set noreorder
.set noat

.text

.globl osMapTLBRdb
.ent osMapTLBRdb
osMapTLBRdb:
    mfc0    $8, $10
    addiu   $9, $0, 0x1f
    mtc0    $9, $0
    mtc0    $0, $5
    addiu   $10, $0, 0x17
    lui     $9, 0xc000
    mtc0    $9, $10
    lui     $9, 0x8000
    srl     $11, $9, 6
    or      $11, $11, $10
    mtc0    $11, $2
    addiu   $9, $0, 1
    mtc0    $9, $3
    nop
    tlbwi
    nop
    nop
    nop
    nop
    mtc0    $8, $10
    jr      $31
    nop
.end osMapTLBRdb
