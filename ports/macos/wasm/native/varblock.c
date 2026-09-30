/* varblock.c -- the game's state snapshots, reaching the real save and load.
 *
 * The game snapshots a table of scattered variables into one buffer
 * (BrVarSave, 0x100608F0) and later copies it back (BrVarLoad, 0x10060970).
 * Four one-line wrappers do it: the race's state at the start of the final
 * lap, which the instant replay restores and plays from, and a 64-byte block
 * saved and restored round a pause.
 *
 * WHY: br_objlife.c calls the two through local empty stand-ins
 * (BrExt_10067880, BrExt_10067900). The MSVC build is byte-exact because a
 * call's target is a relocation, but the translation inlines the empty
 * stand-ins, so nothing was ever saved or restored. The replay then began
 * from the finished race: every car already flagged as over the line, so
 * the replay's "until everyone has finished" step passed on its first frame
 * and the race-end timer sent the game back to the menu two seconds later.
 * Each wrapper forwards as the original does (table and buffer addresses
 * are the original's immediates).
 *
 * The save's one caller, BrRaceSaveLastLapInfo, is in the same file as its
 * wrapper, so the translation inlined the wrapper there too and the override
 * below never gets that call. The caller is replaced as well: it runs as
 * translated, and when it took the saving path -- the only path that sets
 * the saved-this-race latch it tests first (0x11773668, set just before the
 * wrapper call) -- the save is made here, into the same entrant's area.
 */
#include "w2c_native.h"
#include "host.h"

#define F_VARSAVE   0x100608F0u   /* BrVarSave(table, dst, cbAvail) */
#define F_VARLOAD   0x10060970u   /* BrVarLoad(table, src) */
#define T_LASTLAP   0x100B31B8u   /* the last-lap snapshot's table */
#define T_PAUSE     0x100B3270u   /* the pause block's table */
#define B_PAUSE     0x10B1CBA8u   /* the pause block's buffer */
#define G_SAVED     0x11773668u   /* the last-lap snapshot was taken this race */
#define A_LASTLAP   0x102066C8u   /* entrant 0's save area; entrant n is n areas below */
#define A_STRIDE    0x15F88u

/* @replaces 0x100609B0 BrWrap_10067940 */
void n_BrWrap_10067940(u32 p)
{
    W_TRACE("n_BrWrap_10067940");
    w_icall_iii_(F_VARSAVE, T_LASTLAP, p + 0x7080, 0x15F88);
}

/* @replaces 0x10060A30 BrRaceSaveLastLapInfo */
void n_BrRaceSaveLastLapInfo(u32 car)
{
    u32 saved = W_LD(u32, G_SAVED, 0);
    W_TRACE("n_BrRaceSaveLastLapInfo");
    W_ORIG_BrRaceSaveLastLapInfo(car);
    if (!saved && W_LD(u32, G_SAVED, 0))
        n_BrWrap_10067940(A_LASTLAP - W_LD(u32, car, 0x140) * A_STRIDE);
}

/* @replaces 0x100609D0 BrWrap_10067960 */
void n_BrWrap_10067960(u32 p)
{
    W_TRACE("n_BrWrap_10067960");
    w_icall_ii_(F_VARLOAD, T_LASTLAP, p + 0x7080);
}

/* @replaces 0x100609F0 BrWrap_10067980 */
void n_BrWrap_10067980(void)
{
    W_TRACE("n_BrWrap_10067980");
    w_icall_iii_(F_VARSAVE, T_PAUSE, B_PAUSE, 0x40);
}

/* @replaces 0x10060A10 BrWrap_100679A0 */
void n_BrWrap_100679A0(void)
{
    W_TRACE("n_BrWrap_100679A0");
    w_icall_ii_(F_VARLOAD, T_PAUSE, B_PAUSE);
}
