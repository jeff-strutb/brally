/* slice6_70.h -- Boss Rally (BRD3D.dll) decompilation, slice 6, packet 70.
 *
 * A "close the link" packet: twelve addresses that an already-ported module
 * calls through an `extern` it declared itself, and that nothing defines.
 * Every name and signature below was grepped out of port/include/ BEFORE a
 * line was written; none of them is invented here.
 *
 * ======================================================================
 * IN THIS FILE (6 of the 12 wanted)
 * ======================================================================
 *   0x1003C020  BrSub1003C020    slice2_25.h:425   void (void)
 *                 + BrExt_1003C020  slice2_26.h:252 -- SAME address, second
 *                   pre-existing name, same shape.  Provided as a forwarder.
 *   0x1003BF60  BrExt_1003BF60   slice2_26.h:251   void (void)
 *                 + BrSub1003BF60  slice2_25.h:424 -- ditto.
 *   0x1003E680  BrExt_1003E680   slice2_26.h:257   void (void)
 *                 + BrSub1003E680  slice2_25.h:439 -- ditto.
 *   0x1003DB00  BrExt_1003DB00   slice2_26.h:250   void (BrObjA9D008 *, void *)
 *   0x1003C150  BrExt_1003C150   slice2_26.h:253   void (void)
 *   0x100173F0  BrSub_100173F0   slice2_15.h:482   void (BrHudView *, int)
 *
 * ======================================================================
 * NOT IN THIS FILE, AND WHY  (the full argument is in slice6_70.c's tail)
 * ======================================================================
 *   0x1004CAC0  BrOptFn1004CAC0 -- ALREADY IMPLEMENTED.  slice3_33.c has this
 *               exact body as `BrExt_1004CAC0(BrUiBuildCtx *, BrUiPhase *)`.
 *               The contract forbids duplicating it; what is missing is an
 *               adapter, and the adapter cannot be written without deciding
 *               who owns the build-context globals.
 *   0x1004D1F0  BrExt_1004D1F0  } the remaining menu-screen builders.
 *   0x1004DB00  BrExt_1004DB00  } br_phase.h unblocks the PHASE half of the
 *   0x10053CF0  BrExt_10053CF0  } blocker but not the GLOBALS half -- see the
 *   0x10058750  BrOptFn10058750 } "THE FOUR REMAINING BUILDERS" note in the .c.
 *   0x10062C50  BrSub10062C50   -- 1909 bytes of entity initialisation that
 *               writes ~150 fields between +0x164 and +0xD80.  slice3_45.h's
 *               BrEnt (the DECLARED parameter type) models none of them; they
 *               all fall inside its `pad` arrays.  See the .c.
 *
 * ======================================================================
 * SIGNATURE / NAME CONFLICTS FOUND (reported, deliberately not "resolved")
 * ======================================================================
 * 1. 0x1003C150 already HAS a body: slice4_50.c:243 `void BrSub1003C150(void)`.
 *    slice2_26.h wants the same address as `BrExt_1003C150`.  Byte-compared
 *    against this packet's listing: identical.  Forwarded, not re-decompiled.
 *
 * 2. 0x1003DB00 already HAS a body: slice2_22.c:220
 *    `int BrDPlaySendTag7(const BrDPlayLink *, uint32_t)`.  slice2_26.h wants
 *    it as `void BrExt_1003DB00(BrObjA9D008 *, void *)`.  The two structs are
 *    the same object (slice2_22.h's BrDPlayLink is the longer view).
 *    Forwarded, not re-decompiled.
 *
 * 3. 0x1003CC70 -- slice5_63.c:107 declares it `void BrSub1003CC70(void *)`.
 *    0x1003C020 TESTS the returned eax with `jl` and gives up on a negative,
 *    so the function DOES return a value and the `void` form is the lossy one.
 *    Declared `int32_t` below.  The two declarations are incompatible and must
 *    not meet in one translation unit until one is fixed.
 *
 * 4. 0x10AA29D8 -- slice2_25.h types it `BrOptFlagObj *` and models only
 *    +0x1C.  0x1003BF60 also writes the BYTE at +0x2B64, i.e. the object is
 *    really an entity record (stride 0x2B68, a project-wide constant).  This
 *    file does NOT model it a sixth time: it re-declares the same symbol
 *    through the identical `struct BrOptFlagObj *` type and reaches both
 *    fields byte-wise.  Both offsets sit below any pointer member, so they are
 *    stable on LP64.
 *
 * 5. 0x100AA010 / 0x100BD3E0 are declared by slice2_25.h (g_br0AA010,
 *    g_br0BD3E0) and modelled as struct FIELDS by slice2_23.h / slice2_24.h.
 *    The slice2_25.h externs are reused; no storage is defined here for them.
 *
 * ======================================================================
 * GLOBALS THIS FILE DEFINES that another header already models as a FIELD
 * ======================================================================
 * These have no standalone owner today, so storage is defined in slice6_70.c.
 * Where some other header models the same address inside a struct, it is named
 * here so integration can alias rather than duplicate:
 *
 *   0x10277B44  g_br277B44   -- the CreateEventA handle.  No other model.
 *   0x10A9D004  g_brA9D004   -- the 0x1003C020 attempt counter.  No other model.
 *   0x10A9D068  g_brA9D068   } no other model
 *   0x10A9D06C  g_brA9D06C   }
 *   0x10AA2A04  g_brAA2A04   -- slice2_25.h declares 2A00 and 2A08, not 2A04.
 *   0x10AA28B0  g_brAA28B0   }
 *   0x10AA28B4  g_brAA28B4   } no other model
 *   0x10AA28BC  g_brAA28BC   }
 *   0x10AA28C0  g_brAA28C0   }
 *   0x10AA28D0  g_brAA28D0   = slice2_24.h  BrUiGlobals.gAA28D0
 *   0x10AA26E8  g_brAA26E8   = slice2_23.h  BrUiGlobals.tAA26E8 (a POINTER
 *                              there; 0x1003E680 stores a plain 0 dword)
 *   0x10A9DBD8  g_aBrA9DBD8  -- the 0x53-dword twin of g_aBrAA26F0
 *   0x10220B20  g_a220B20    -- slice2_20.c already declares exactly this
 *                              name and extent and defines no storage.
 *   0x100BD3E8  g_br0BD3E8   } no other model
 *   0x100BD3F8  g_br0BD3F8   }
 *   0x100A73C8  g_pszBr0A73C8 = "%%y1%s%d/%d"   read out of the DLL
 *   0x100A73D4  g_pszBr0A73D4 = "L"             read out of the DLL
 */
#ifndef SLICE6_70_H
#define SLICE6_70_H

#include <stddef.h>
#include <stdint.h>

/* BrHudView is in this file's public signature, so its owner comes with it.
 * slice2_15.h also brings BrScreenGet()/BrHudGetEnv(), which is where
 * 0x100173F0's iView / cViews / 0x1022AF1C / 0x106C2CF8 are reached. */
#include "slice2_15.h"

/* Objects reached only as opaque pointers. These are the SAME types
 * slice2_25.h / slice2_26.h / slice2_22.h declare: the struct TAG is shared,
 * so the declarations below are compatible with theirs without repeating the
 * definitions -- and therefore without re-typedef'ing, which C99 forbids. */
struct BrDPlay;
struct BrOptFlagObj;
struct BrObj29D4;
struct BrObjA9D008;
struct BrDPlayLink;

/* ==========================================================================
 * The six the packet asked for
 * ========================================================================== */

/* 0x1003C020  Tear the session timer down, re-enumerate the DirectPlay
 * service providers, initialise the chosen connection, and (unless the mode
 * selector says 2 or 3) restart the 1 Hz timer.
 *
 * GOTCHA: THE ERROR MESSAGE IS FORMATTED AND THROWN AWAY, exactly as in
 * 0x1003C150 / 0x1003C260 -- "Could not select service provider because of
 * error 0x%08X" goes into a 1KB stack buffer and nothing reads it. The one
 * HRESULT that is NOT reported is 0x88770118 (DPERR_USERCANCEL's sibling on
 * the enumeration path). Reproduced; do not "fix" it into a message box.
 *
 * GOTCHA: 0x10A9D004 is incremented AFTER 0x1003C520 returns but BEFORE its
 * result is tested, so a failed attempt still counts.
 *
 * GOTCHA: when 0x10277B40 comes back NULL the fall-through reports whatever
 * 0x1003C520 returned, which on that path is >= 0 -- i.e. a "success" code is
 * formatted into the error string. In the original, preserved. */
void BrSub1003C020(void);
/* 0x1003C020 under slice2_26.h's name for the same address. */
void BrExt_1003C020(void);

/* 0x1003BF60  Leave the session: reset the slot table, kill the timer, drop
 * the network object, and clear four mode globals.
 *
 * GOTCHA: the +0x2B64 byte and the +0x1C flag bit are cleared only when the
 * mode selector is NOT 2 and NOT 3. The four global clears at the end happen
 * unconditionally.
 *
 * GOTCHA: 0x10AA29D8 is re-read between the two writes. Nothing in between
 * can change it, so this is only a faithfulness detail. */
void BrExt_1003BF60(void);
/* 0x1003BF60 under slice2_25.h's name for the same address. */
void BrSub1003BF60(void);

/* 0x1003E680  Reset the whole options/session global block back to defaults.
 *
 * GOTCHA: 0x10AA289C is cleared TWICE -- once in the opening run of stores and
 * again after 0x1003E1D0 returns. Both are kept.
 *
 * GOTCHA: 0x10220B20's 0x118 bytes are zeroed and then dword 0 is set to
 * 0xFFFFFFFF. That is NOT slice2_20.h's BrInit220B20, which sets dword 0 to 8
 * and then calls 0x10035BD1. Two different initialisers for one buffer.
 *
 * GOTCHA: 0x10AA27E0 is written as a WORD (0x0102). slice5_63.h models the
 * address as ONE dword carrying TWO 16-bit masks, so only the low half is
 * touched here and the high half is preserved. */
void BrExt_1003E680(void);
/* 0x1003E680 under slice2_25.h's name for the same address. */
void BrSub1003E680(void);

/* 0x1003DB00  Send the tag-7 pair { 0x60000007, value } over DirectPlay.
 *
 * See CONFLICT 2 above: this is slice2_22.c's BrDPlaySendTag7 and forwards to
 * it. The original returns 0 on the two null checks and the Send HRESULT
 * otherwise; slice2_26.h declares the address void, so the result is dropped.
 *
 * DEVIATION: `p` is declared `void *` by slice2_26.h but the original stores
 * it as the SECOND DWORD of an eight-byte wire payload. It is narrowed to 32
 * bits here, which is what actually goes on the wire. The one call site
 * (slice2_26.c:200) passes pA9D008->f08, which slice2_22.h types uint32_t. */
void BrExt_1003DB00(struct BrObjA9D008 *pObj, void *p);

/* 0x1003C150  Host a session. See CONFLICT 1: the body is slice4_50.c's
 * BrSub1003C150 and this forwards to it. */
void BrExt_1003C150(void);

/* 0x100173F0  Draw the lap counter and the finishing-position readout.
 *
 * Returns immediately when 0x100AA010 == 3. `a2` is pushed by both call sites
 * and never read.
 *
 * GOTCHA: the lap half draws when (cSplits < nLaps) OR (cViews == 1), so on a
 * full-screen view it still draws after the last lap -- with the OTHER string
 * (id 0xE6), because the string choice re-tests cSplits >= nLaps.
 *
 * GOTCHA: the "L" at 0x100A73D4 is used INSTEAD of string 0xE5 only when
 * cViews == 2. Three or four views fall into the string-table branch.
 *
 * GOTCHA (position suffix nudge): the switch on the race block's +0x0FF8
 * yields an x-nudge of -3 for position 0, +1 for 1, 0 for 2 and +1 for
 * anything else. In the split-screen branch that nudge is first doubled and
 * then divided by three with truncation toward zero (the original's
 * 0x55555556 magic-multiply), so -3 becomes -2 and +1 becomes 0. In the
 * full-screen branch it is used raw. Reproduced exactly. */
void BrSub_100173F0(BrHudView *aViews, int a2);

/* ==========================================================================
 * Platform hooks. No Win32 in portable code (CONTRACT), and the precedent is
 * slice1_07's MessageBoxA hook and slice4_53's SetTimer hook.
 * ========================================================================== */

/* USER32 KillTimer(hWnd, idEvent). Both 0x1003C020 and 0x1003BF60 open with
 * it. While NULL the call is skipped. */
extern int32_t (*g_pfnBrPlatKillTimer)(void *hWnd, uint32_t idEvent);

/* KERNEL32 CreateEventA(NULL, FALSE, FALSE, NULL) -- all four arguments are
 * literal zero in the original. Same shape as slice2_13.h's
 * BrDPlayOs::pfnCreateEvent; integration should point both at one function.
 * While NULL the port behaves as a FAILED CreateEventA, which is the path
 * that yields 0x8007000E (E_OUTOFMEMORY). */
extern void *(*g_pfnBrPlatCreateEvent)(void);

/* 0x1003C020's `call [edx+0x98]` on 0x10277B40. Slot 38 of the IDirectPlay4A
 * vtable is InitializeConnection(lpConnection, dwFlags); the error string the
 * failure path formats ("Could not select service provider...") agrees.
 * slice2_25.h's BrDPlayVtbl deliberately models only slots 0..31 and this
 * header must not redefine it, so the call goes through a hook -- the same
 * device slice2_25.h uses for 0x10042AF0. While NULL it reports 0x8007000E. */
extern int32_t (*g_pfnBrDPlayInitConn)(struct BrDPlay *pThis,
                                       void *pConnection, uint32_t dwFlags);

/* ==========================================================================
 * Globals this file OWNS (storage in slice6_70.c)
 * ========================================================================== */

extern void    *g_br277B44;      /* 0x10277B44 */
extern int32_t  g_brA9D004;      /* 0x10A9D004 */
extern int32_t  g_brA9D068;      /* 0x10A9D068 */
extern int32_t  g_brA9D06C;      /* 0x10A9D06C */
extern int32_t  g_brAA2A04;      /* 0x10AA2A04 */
extern int32_t  g_brAA28B0;      /* 0x10AA28B0 */
extern int32_t  g_brAA28B4;      /* 0x10AA28B4 */
extern int32_t  g_brAA28BC;      /* 0x10AA28BC */
extern int32_t  g_brAA28C0;      /* 0x10AA28C0 */
extern int32_t  g_brAA28D0;      /* 0x10AA28D0 */
extern int32_t  g_brAA26E8;      /* 0x10AA26E8 */
extern int32_t  g_br0BD3E8;      /* 0x100BD3E8 */
extern int32_t  g_br0BD3F8;      /* 0x100BD3F8 */

#define BR70_AA26F0_COUNT  0x53   /* == slice2_25.h's BR_OPT_AA26F0_COUNT */
#define BR70_220B20_COUNT  0x46
extern int32_t  g_aBrA9DBD8[BR70_AA26F0_COUNT];   /* 0x10A9DBD8 */
extern uint32_t g_a220B20[BR70_220B20_COUNT];     /* 0x10220B20 -- the name
                                                   * slice2_20.c:113 already
                                                   * externs, unchanged. */

/* Read out of orig/BRD3D.dll .rdata, not assumed (CONTRACT). */
extern const char *g_pszBr0A73C8;   /* 0x100A73C8 "%%y1%s%d/%d" */
extern const char *g_pszBr0A73D4;   /* 0x100A73D4 "L"           */

/* [0x106C2CF8] + 0x0FF8 -- the finishing-position index 0x100173F0 switches
 * on. slice2_15.h owns 0x106C2CF8 as BrHudEnv::pRace, but its BrRace is
 * "logical, not byte-exact" and has no field there, and this file may not
 * extend that header.
 *
 * DEVIATION: injected as a POINTER, so the live race object stays the source
 * of truth rather than a copy going stale. While NULL it reads as 0, which
 * selects string id 0xB3 -- the same thing a zeroed race block would do. */
extern const int32_t *g_pBrRace0FF8;

/* ==========================================================================
 * Cross-slice callees. Each is already declared, with this exact signature,
 * by the header named beside it, EXCEPT the four that carry no name anywhere
 * (0x1003C520 / 0x1003C550 / 0x1003CC70 / 0x1003D480 / 0x10072270 /
 * 0x1003E1D0 / 0x10019240 / 0x10019250 / 0x100193C0). Those get a positional
 * name here; grep confirmed none of them has an existing one.
 * ========================================================================== */

/* XSLICE 0x10277B40 -- slice2_25.h  `BrDPlay *g_brP277B40` */
extern struct BrDPlay *g_brP277B40;
/* XSLICE 0x10AA29D8 -- slice2_25.h  `BrOptFlagObj *g_brPAA29D8`, see CONFLICT 4 */
extern struct BrOptFlagObj *g_brPAA29D8;
/* XSLICE 0x10AA29D4 -- slice2_25.h  `BrObj29D4 *g_brPAA29D4`. 0x1003C020 only
 * tests it for NULL, so it stays opaque here. */
extern struct BrObj29D4 *g_brPAA29D4;
/* XSLICE 0x10680584 -- slice2_25.h */
extern void   *g_brP680584;
/* XSLICE 0x10A9BFDC + the SetTimer hook -- slice4_53.h. Declared without the
 * BrPlatSetTimerFn typedef so the two headers can share a TU. */
extern uint32_t  g_brA9BFDC;
extern uint32_t (*g_pfnBrPlatSetTimer)(void *hWnd, uint32_t idEvent,
                                       uint32_t uElapseMs, void *pfnProc);

/* XSLICE -- slice2_25.h mode / session globals */
extern int32_t g_br0AA010;   /* 0x100AA010 */
extern int32_t g_br0AC648;   /* 0x100AC648 */
extern int32_t g_br0AC64C;   /* 0x100AC64C */
extern int32_t g_br0AC650;   /* 0x100AC650 */
extern int32_t g_br0AC654;   /* 0x100AC654 */
extern int32_t g_br0AC658;   /* 0x100AC658 */
extern int32_t g_br0BD3E0;   /* 0x100BD3E0 */
extern int32_t g_br22AF18;   /* 0x1022AF18 */
extern int32_t g_brA9CFFC;   /* 0x10A9CFFC */
extern int32_t g_brAA287C;   /* 0x10AA287C */
extern int32_t g_brAA2884;   /* 0x10AA2884 */
extern int32_t g_brAA2888;   /* 0x10AA2888 */
extern int32_t g_brAA289C;   /* 0x10AA289C */
extern int32_t g_brAA2A00;   /* 0x10AA2A00 */
extern int32_t g_brAA2A08;   /* 0x10AA2A08 */
extern int32_t g_aBrAA26F0[BR70_AA26F0_COUNT];   /* 0x10AA26F0 */

/* XSLICE -- slice4_50.h */
extern int32_t g_brAA28C8;   /* 0x10AA28C8 */

/* XSLICE -- slice5_63.h */
extern int32_t g_brAA2A10;   /* 0x10AA2A10 */
extern int32_t g_brAA2A14;   /* 0x10AA2A14 */
extern int32_t g_brAA28A0;   /* 0x10AA28A0 */
extern int32_t g_brAA28A4;   /* 0x10AA28A4 */
extern int32_t g_brAA28AC;   /* 0x10AA28AC */
extern int8_t  g_brAA28B8;   /* 0x10AA28B8 -- byte store, SIGNED elsewhere */
extern int32_t g_brAA28C4;   /* 0x10AA28C4 */
extern uint32_t g_brAA27E0;  /* 0x10AA27E0 -- ONE dword, TWO 16-bit masks */
#ifndef BR63_TEXT_MAX
#define BR63_TEXT_MAX 0x104
#endif
extern char g_aBrAA2518[BR63_TEXT_MAX];   /* 0x10AA2518 */
extern char g_aBrA9D618[BR63_TEXT_MAX];   /* 0x10A9D618 */
extern const char *g_pszBr0A73C4;         /* 0x100A73C4 "%d" */

/* XSLICE 0x1007C830 -- slice2_25.h:419 / slice4_50.h:103 */
extern int  BrSprintf(char *pszDest, const char *pszFmt, ...);
/* XSLICE 0x10074030 -- slice2_25.h:420 / slice3_33.h:231. Three names exist
 * for this address; BrStrGet is the one with two votes. */
extern const char *BrStrGet(int id);
/* XSLICE 0x100586A0 -- slice2_25.h:452 (br_slots.h's BrSlotsReset, argumentless
 * because the table is a global). */
extern void BrSub100586A0(void);
/* XSLICE 0x1003E510 -- slice4_50.h:311 (= slice2_26.h's BrExt_1003E510). */
extern void BrSub1003E510(void);
/* XSLICE 0x1003C150 -- slice4_50.h:335, the body BrExt_1003C150 forwards to. */
extern void BrSub1003C150(void);
/* XSLICE 0x1003DB00 -- slice2_22.h:255, the body BrExt_1003DB00 forwards to. */
extern int  BrDPlaySendTag7(const struct BrDPlayLink *pLink, uint32_t value);
/* XSLICE 0x100192A0 -- slice1_03.h:138 */
extern void BrTextSetColors(int a1, int a2, int a3, int a4, int a5, int a6);

/* -- positional names coined here; grep found no existing one -------------- */

/* 0x1003C550 -- the DirectPlay teardown 0x1003BF60 and 0x1003C020 share.
 * slice2_22.h notes it takes BrDPlayLink::f08 as DestroyPlayer's argument. */
extern void    BrSub1003C550(void);
/* 0x1003D480 -- enumerate/choose a service provider. Two out-parameters and an
 * HRESULT; only the FIRST out-parameter is read by 0x1003C020, and it is the
 * connection blob handed to InitializeConnection. */
extern int32_t BrSub1003D480(void **ppConn, void **ppOut2);
/* 0x1003C520 -- create the IDirectPlay4A and store it. Takes the ADDRESS of
 * 0x10277B40, not its value. Returns an HRESULT. */
extern int32_t BrSub1003C520(struct BrDPlay **ppDPlay);
/* 0x1003CC70 -- see CONFLICT 3. Returns an HRESULT that 0x1003C020 tests. */
extern int32_t BrSub1003CC70(struct BrDPlay *pDPlay);
/* 0x10072270 -- run only when 0x10AA2884 (networked session) is set. */
extern void    BrSub10072270(void);
/* 0x1003E1D0 -- the paired-scratch-buffer reset slice2_23.h mentions. */
extern void    BrSub1003E1D0(void);
/* 0x10019240 / 0x10019250 -- the text-state bracket 0x100173F0's position
 * half opens and closes with. slice2_15.h declares 0x10019260..0x100192F0 but
 * not these two. */
extern void    BrSub_10019240(void);
extern void    BrSub_10019250(void);
/* 0x100193C0 -- measure a string at a given scale. slice1_03.h reaches this
 * same address through BrTextState::pfnMeasure and gives it no free-standing
 * name; integration should point that hook at this function. */
extern int     BrSub_100193C0(const char *psz, int scale);

#endif /* SLICE6_70_H */
