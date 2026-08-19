/* slice2_22.h -- 0x1003BD50..0x1003DBC0, decompiled from BRD3D.dll.
 *
 * This whole address range is the game's DirectPlay multiplayer module.
 * Identification is solid: the vtable slot offsets used throughout match
 * IDirectPlay4A and IDirectPlayLobby3A exactly (see the "vtable map" note
 * below), and the error strings reached from these functions name the
 * operations ("Could not create DirectPlay object because of error 0x%08X",
 * "Could not host session ...", "Could not join session ...", "Could not
 * select service provider ...").
 *
 * Consequence: MOST of this packet cannot be ported. It is COM calls through
 * DirectPlay interfaces, Win32 GlobalAlloc/GlobalLock, SetTimer/KillTimer,
 * OutputDebugStringA and MessageBoxA. Only the pieces below carry logic that
 * exists independently of those APIs, so only those are here. See the report
 * for the full skipped list.
 *
 * ---------------------------------------------------------------------------
 * Vtable map recovered while reading this packet (record it, do not re-derive)
 *
 *   g_10277B40 is an IDirectPlay4A*, g_10A9BFD8 an IDirectPlayLobby3A*.
 *
 *   IDirectPlay4A   +0x18 CreatePlayer          +0x24 DestroyPlayer
 *                   +0x30 EnumPlayers           +0x34 EnumSessions
 *                   +0x48 GetPlayerAddress      +0x58 GetSessionDesc
 *                   +0x68 Send                  +0x7C SetSessionDesc
 *                   +0x8C EnumConnections       +0x98 InitializeConnection
 *                   +0x9C SecureOpen
 *   IDirectPlayLobby3A                          +0x38 CreateCompoundAddress
 *
 * That pins 0x1000C4D0 (slice1_03's BrComCallLocked68, "vtable slot 26") as a
 * critical-section-guarded IDirectPlay4A::Send(idFrom, idTo, flags, pData,
 * cbData). Every sender below is built on it.
 * ---------------------------------------------------------------------------
 */
#ifndef SLICE2_22_H
#define SLICE2_22_H

#include <stdint.h>

#include "br_slots.h"   /* BrSlotTable -- the table at 0x10AA2538 */

/* ==========================================================================
 * 1. The 27-bit multiplicative sequence  (0x1003BD50)
 * ========================================================================== */

/* 0x1003BD50  advance the generator at 0x10A9BFD0.
 *
 * The original is a shift/lea chain, not a multiply instruction; it works out
 * to exactly 16807 (8s-s = 7s, *5, *5, <<4, +s, *3, s + 2*that = 16807s),
 * then `and eax, 0x07FFFFFF`.
 *
 * GOTCHA: this is NOT the Lehmer "minimal standard" generator even though it
 * borrows its multiplier. There is no modulo 2^31-1 -- the modulus is 2^27.
 * Do not substitute a stock minimal-standard implementation; the sequences
 * are unrelated. (The machine's 32-bit truncation of the product is NOT
 * observable, because 2^32 is a multiple of 2^27, so plain uint32_t
 * arithmetic reproduces it exactly.)
 *
 * GOTCHA: seed 0 is absorbing (0 -> 0). The original never guards against it.
 * Every other state is fine: 16807 is odd, so multiplication by it is a
 * bijection modulo 2^27 -- the sequence never collides or short-cycles into
 * a fixed point other than 0.
 *
 * The return value is incidental in the original (the new state simply
 * happens to be in eax at `ret`); it is surfaced here for testability. */
uint32_t BrDPlayRandStep(uint32_t *pSeed);

/* ==========================================================================
 * 2. The service-provider table  (0x1003C430, 0x1003CFC0)
 * ========================================================================== */

#define BR_DPLAY_SP_MAX      16    /* (0x10A9CECC - 0x10A9C0CC) / 0xE0 */
#define BR_DPLAY_SP_NAMELEN  0xC8  /* fixed 200-byte copy, see below     */
#define BR_DPLAY_SP_KNOWN    4     /* the four hardcoded provider GUIDs  */

/* One row of the table based at 0x10A9BFF0. Stride 0xE0, confirmed twice:
 * 0x1003C430 forms the row offset as idx*7*32, and 0x1003CFC0 forms
 * 0x10A9C0B8 + idx*7*32 -- 0x10A9C0B8 being 0x10A9BFF0 + 0xC8, i.e. the GUID
 * field of the same row. */
typedef struct BrDPlaySp {
    char     aName[BR_DPLAY_SP_NAMELEN]; /* +0x000  0x10A9BFF0 */
    uint8_t  aGuid[16];                  /* +0x0C8  0x10A9C0B8 */
    uint32_t cbConn;                     /* +0x0D8  0x10A9C0C8 */
    void    *pConn;                      /* +0x0DC  0x10A9C0CC */
} BrDPlaySp;                             /* = 0xE0 bytes on a 32-bit build.
                                          * On a 64-bit host the trailing
                                          * pointer forces padding and the
                                          * struct grows to 0xE8; the field
                                          * ORDER and the 0xC8/0xD8 offsets
                                          * are what the original's address
                                          * arithmetic actually depends on. */

/* 0x1003C430 (head)  map a service-provider GUID to its table row.
 *
 * `aKnown` is the four 16-byte GUIDs the original has at 0x10090890,
 * 0x100908A0, 0x100908B0, 0x100908C0, in that address order. Returns the row
 * index, or -1 if the GUID is none of them.
 *
 * GOTCHA: the mapping is NOT the identity. Reading the original's compare
 * order, 0x10090890 -> row 0, 0x100908A0 -> row 1, 0x100908B0 -> row 3,
 * 0x100908C0 -> row 2. Rows 2 and 3 are swapped relative to the GUIDs'
 * address order. Do not "tidy" this. */
int BrDPlaySpClassify(const uint8_t *pGuid,
                      const uint8_t aKnown[BR_DPLAY_SP_KNOWN][16]);

/* 0x1003C430  the IDirectPlay4A::EnumConnections callback (stdcall,
 * `ret 0x18`, six arguments). Fills the row selected by BrDPlaySpClassify
 * and always returns 1 (keep enumerating) -- including when the GUID matches
 * nothing, in which case nothing at all is written.
 *
 * `pszName` is DPNAME::lpszShortNameA (the original reads lpName+8);
 * `pConn`/`cbConn` are the opaque connection blob and its size.
 *
 * GOTCHA (original bug, preserved in spirit): when the connection blob cannot
 * be allocated the original still stores the NULL pointer into the row but
 * does NOT update cbConn, leaving the previous row's size behind next to a
 * null pointer. Reproduced.
 *
 * DEVIATION: the original copies a flat 0xC8 bytes out of the NUL-terminated
 * short name with `rep movsd`, over-reading past the string. Here the copy
 * stops at the NUL and the remainder of the field is zeroed.
 * DEVIATION: GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT)+GlobalLock is malloc
 * plus an explicit zero-fill. Ownership matches (the row owns the block; the
 * original frees it in 0x1003CFE0). */
int BrDPlaySpEnumConn(BrDPlaySp *aTable, const uint8_t *pGuid,
                      const uint8_t aKnown[BR_DPLAY_SP_KNOWN][16],
                      const char *pszName,
                      const void *pConn, uint32_t cbConn);

/* 0x1003CFE0  release every row's connection blob and null the pointer.
 * The original walks 0x10A9C0CC in 0xE0 steps while < 0x10A9CECC, i.e. all
 * 16 rows, GlobalUnlock+GlobalFree each. cbConn is deliberately NOT cleared,
 * matching the original.
 * DEVIATION: free() instead of GlobalUnlock/GlobalFree. */
void BrDPlaySpFreeAll(BrDPlaySp *aTable);

/* 0x1003CFC0  hand back a pointer to the GUID of the currently selected row.
 * `idxSel` is the original's 0x10AA287C. Always returns 0; the result is
 * delivered through *ppGuid.
 *
 * GOTCHA: there is no bounds check on idxSel in the original. Preserved --
 * pass a sane index.
 * GOTCHA: the pointer is to the row's GUID field, not to the row. */
int BrDPlaySpSelectedGuid(BrDPlaySp *aTable, uint32_t idxSel,
                          uint8_t **ppGuid);

/* ==========================================================================
 * 3. The 8-slot table at 0x10AA2538  (0x1003C9F0, 0x1003CA70, 0x1003CB24)
 * ========================================================================== */

/* These three share br_slots.h's BrSlotTable. Field roles, read off the
 * offsets used here: `id` (+0) is the key and -1 means free (as br_slots.h
 * already documents), `b` (+8) is a per-pass "seen" mark, `a` (+4) is a flag
 * that is only ever set for id == 1.
 *
 * The protocol, from 0x1003CB10: clear every `b`, run an EnumPlayers pass
 * that calls BrDPlaySlotTouch for each player, then BrDPlaySlotsPurge to drop
 * whatever was not touched. It is a mark-and-sweep over the player list. */

/* 0x1003C9F0  mark `id` as present.
 *
 * If a row already holds `id`, only its `b` is set. Otherwise the FIRST free
 * row (the original records the first -1 it sees and ignores later ones) is
 * claimed: id = id, b = 1, and a = 1 only when id == 1, else a = 0.
 * If there is no free row nothing happens at all.
 *
 * GOTCHA: id 0 is a perfectly valid key here (br_slots.h says the same); only
 * -1 means free. GOTCHA: id == 1 is a reserved value that additionally sets
 * `a`. GOTCHA: the free-slot index is kept in a BYTE register initialised to
 * 0xFF and tested with a SIGNED compare, so the "no free slot" sentinel is
 * -1 -- with only 8 rows this can never collide, but that is why the table
 * size cannot grow past 127 without changing behaviour.
 *
 * The original returns whatever was left in eax (a table address on one path,
 * a stale index on another). No caller reads it, so this is void. */
void BrDPlaySlotTouch(BrSlotTable *pTable, int32_t id);

/* 0x1003CA70  free every row whose `b` is clear: id = -1, a = 0, b = 0.
 * Rows with b set are left exactly as they are (b is NOT cleared here; the
 * clearing is done separately, see BrDPlaySlotsClearMarks). */
void BrDPlaySlotsPurge(BrSlotTable *pTable);

/* 0x1003CB24..0x1003CB37, the inline loop at the head of 0x1003CB10 (whose
 * body is EnumPlayers and is not portable). Clears `b` on every row and
 * touches nothing else -- in particular it does NOT free the rows. */
void BrDPlaySlotsClearMarks(BrSlotTable *pTable);

/* ==========================================================================
 * 4. The message senders  (0x1003D950..0x1003DB50)
 * ========================================================================== */

/* The object at 0x10A9D008 that every sender is handed. Only these fields are
 * read anywhere in this packet. */
typedef struct BrDPlayLink {
    void    *pIface;  /* +0x00  IDirectPlay4A*                              */
    void    *f04;     /* +0x04  never read here                             */
    uint32_t f08;     /* +0x08  our DPID: passed as Send's idFrom, and as
                       *        DestroyPlayer's argument in 0x1003C550      */
    uint32_t f0C;     /* +0x0C  gates the extra call in BrDPlaySendTag8     */
} BrDPlayLink;

/* The tag that leads each payload. All seven live in the 0x6000000N space. */
#define BR_DPLAY_TAG2  0x60000002u  /* 0x1003D950 */
#define BR_DPLAY_TAG3  0x60000003u  /* 0x1003D9F0 */
#define BR_DPLAY_TAG4  0x60000004u  /* 0x1003DA40 */
#define BR_DPLAY_TAG5  0x60000005u  /* 0x1003D9A0 */
#define BR_DPLAY_TAG6  0x60000006u  /* 0x1003DA90 */
#define BR_DPLAY_TAG7  0x60000007u  /* 0x1003DB00 */
#define BR_DPLAY_TAG8  0x60000008u  /* 0x1003DB50 */

/* Common body of 0x1003D950 / 0x1003D9A0 / 0x1003D9F0 / 0x1003DA40 /
 * 0x1003DA90 / 0x1003DB00: build the eight-byte payload { tag, value } and
 * hand it to Send(pIface, f08, 0, 1, payload, 8) -- the literal 0 and 1 are
 * pushed by every one of them (DPID_ALLPLAYERS and DPSEND_GUARANTEED).
 * Returns 0 without sending if pLink or pLink->pIface is null, or if
 * `fGate` is non-zero.
 *
 * `fGate` is the original's 0x10AA288C, read straight from memory by four of
 * the six. NOTE that 0x10AA288C is the same dword br_slots.h calls
 * BrSlotTable::count -- so a non-empty slot table silently suppresses these
 * sends. Passed in rather than global so the coupling is visible.
 *
 * GOTCHA: only four of the six consult the gate. See the wrappers. */
int BrDPlaySendPair(const BrDPlayLink *pLink, int32_t fGate,
                    uint32_t tag, uint32_t value);

/* 0x1003D950  tag 2, gated. */
int BrDPlaySendTag2(const BrDPlayLink *pLink, int32_t fGate, uint32_t value);
/* 0x1003D9A0  tag 5, gated. */
int BrDPlaySendTag5(const BrDPlayLink *pLink, int32_t fGate, uint32_t value);
/* 0x1003DA40  tag 4, gated. */
int BrDPlaySendTag4(const BrDPlayLink *pLink, int32_t fGate, uint32_t value);

/* 0x1003D9F0  tag 3, gated, and it takes NO value.
 *
 * GOTCHA (original bug): it reserves the same 8-byte payload as its five
 * siblings, writes only the tag, and sends all 8 bytes -- so the second dword
 * is whatever was on the stack. DEVIATION: sent as 0 here, because leaving it
 * indeterminate is neither reproducible nor safe. Any receiver that reads the
 * second dword of a tag-3 message is reading garbage in the original. */
int BrDPlaySendTag3(const BrDPlayLink *pLink, int32_t fGate);

/* 0x1003DA90  tag 6 and 0x1003DB00  tag 7.
 *
 * GOTCHA: these two are byte-for-byte the gated senders MINUS the
 * 0x10AA288C test. They transmit even when the gate is set. That asymmetry is
 * in the original and is why they take no fGate argument. */
int BrDPlaySendTag6(const BrDPlayLink *pLink, uint32_t value);
int BrDPlaySendTag7(const BrDPlayLink *pLink, uint32_t value);

/* 0x1003DAE0  send tag 6 carrying our own DPID. Does nothing when pLink is
 * null or pLink->f08 is zero -- so DPID 0 is treated as "no player". */
void BrDPlaySendTag6Self(const BrDPlayLink *pLink);

/* 0x1003DB50  tag 8: a TWELVE-byte payload { tag, a, b }.
 *
 * GOTCHA: not gated by 0x10AA288C either, and unlike the others it has a
 * side channel -- when pLink->f0C is non-zero the same payload is first
 * handed to 0x10003580 (a local delivery path; slice1_03 knows it as the
 * `pfnMsg107` hook) before going out over the wire. So a tag-8 message can be
 * processed locally AND remotely. */
int BrDPlaySendTag8(const BrDPlayLink *pLink, uint32_t a, uint32_t b);

/* ==========================================================================
 * 5. 0x1003CE80 tail fragment
 * ========================================================================== */

/* 0x1003CF2A..0x1003CF64, the trailing loop of 0x1003CE80 (the rest of that
 * function is GetSessionDesc plus a pile of globals and is not ported).
 *
 * Starting from *pIdx, advance to the next index whose availability predicate
 * -- 0x1003F320, which slice1_06 ports as BrOptAvailB -- is non-zero,
 * wrapping 31 -> 0, and stop if the search comes all the way back round.
 *
 * GOTCHA: the range test is `cmp eax, 0x1F / jle`, so the legal range is
 * 0..31 inclusive and the wrap happens only after 31 is exceeded.
 * GOTCHA: if nothing in 0..31 is available the index is left back at its
 * starting value, not at -1; there is no failure signal.
 * GOTCHA: the starting index is tested first, so an already-available index
 * is left untouched.
 *
 * `pCaps` is slice1_06's BrOptCaps; the parameter is void* here so this
 * header does not have to drag slice1_06.h in. */
void BrDPlayAdvanceAvail(const void *pCaps, int32_t *pIdx);

#endif /* SLICE2_22_H */
