/* slice6_76.h -- BRD3D.dll, packet 76 (slice 6).
 *
 * HOW THESE TARGETS WERE CHOSEN
 * =============================
 * Not by runtime demand: there is none to rank by.  `./build/brally` still
 * reports "stubs: none reached", and `-all` reaches a stub in none of the
 * sixteen screen builders, so the hit table the harness exists to fill is
 * empty.  Packet 74 recorded the same negative result.
 *
 * The ranking used instead is STATIC demand, measured twice and from two
 * independent directions:
 *
 *   1. call sites in the already-ported C tree, counted per stub name;
 *   2. call sites in the IMAGE, by disassembling every function in
 *      config/functions.csv and counting direct `call`/`jmp` targets.
 *      That sweep found 1,460 distinct call targets over 7,982 call sites.
 *
 * Measure (2) is the one quoted below, because it counts the demand the whole
 * original has rather than only the part of it that is ported so far.  The two
 * agree on the ordering at the top.
 *
 * THE RESULT WAS MOSTLY "ALREADY PORTED"
 * ======================================
 * CONVENTIONS' rule -- do not re-decompile a function that already exists
 * under another name; grep the ADDRESS, not the symbol -- turned out to
 * dominate this packet.  Cross-referencing all 131 stub addresses against
 * every address-tagged definition comment in port/src found TWENTY stub
 * addresses that already have a body, including the four highest-demand stubs
 * in the tree.  Twelve of them are adapted here; the rest are declined for
 * reasons given below and in the report, every one of them the same reason:
 * the existing body took a `this` or a state-struct parameter that the
 * original does not have, and choosing which instance to bind would be the
 * aliased-storage bug CONVENTIONS warns about.
 *
 * SCOPE -- 16 stub entries, 15 original addresses
 * ===============================================
 * ADAPTERS (body already exists; this packet only wires the stub name to it):
 *
 *   0x1002F900  33 calls  BrSub_1002F900   -> slice1_05 BrRdpSetCombineLERP
 *   0x10042AF0  16 calls  BrX10042AF0      -> slice5_61 BrGfx42AF0_1
 *   0x10042AF0     "      BrGfx42AF0_3     -> slice5_61 BrGfx42AF0_1
 *   0x10069490  13 calls  BrX10069490      -> slice5_62 BrSub_10069490
 *   0x1003E310   9 calls  BrExt_1003E310   -> slice5_63 BrSub1003E310
 *   0x1006A4A0   8 calls  BrExt_1006A4A0   -> slice4_53 BrSub1006A4A0
 *   0x10060E90   7 calls  BrX10060E90      -> slice5_61 BrTimeNow
 *   0x10069530   4 calls  BrX10069530      -> slice3_41 BrPool32Alloc
 *   0x10079550   4 calls  BrExt_10079550   -> slice1_10 BrFfbShutdown(&g_brFfb)
 *   0x100443E0   3 calls  BrExt_100443E0   -> slice2_25 BrOptOpen2950B
 *   0x10044280   2 calls  BrExt_10044280   -> slice2_25 BrOptOpen2950A
 *   0x10043BF0   2 calls  BrExt_10043BF0   -> slice4_50 BrSub10043BF0
 *
 * TRANSCRIBED HERE (no prior body at the address):
 *
 *   0x10060D90   6 calls  BrSub10060D90    the two volume-slider tables
 *   0x100193C0   4 calls  BrSub_100193C0   proportional text width
 *   0x10072580   3 calls  BrX10072580      stop one bank voice
 *   0x10005D30   2 calls  BrSub10005D30    read the local-slot index
 *
 * DECLINED, AND WHY
 * =================
 * All of these already have a body; none can be adapted without inventing
 * which instance of a lifted global the adapter should bind.  That is the
 * aliased-storage hazard, and packet 74 declined 0x1002BD50 for exactly this
 * reason -- the same judgement is applied here to five more:
 *
 *   0x1002BD50  BrModelVtxResolve  = slice1_05 BrVtxCacheResolve.  Already
 *               declined by packet 74; the cache at 0x1067554C/0x1067B54C was
 *               lifted into a BrVtxCache parameter and no instance exists.
 *   0x1003E1D0  BrSub1003E1D0      = slice1_06 BrPairBufReset.  The two
 *               buffers (0x10ACED34/0x10AD189C, static backing 0x10AF9890/
 *               0x10AF99DC) were lifted into a BrPairBuf parameter; no
 *               instance exists.
 *   0x10005FE0  BrSub10005FE0      = slice1_02 BrNetDropMatching.  The net
 *               state was lifted into a BrNetState parameter; no instance
 *               exists.  Note also the arity conflict below.
 *   0x1003DFC0  BrExt_1003DFC0     = slice2_23 BrUiFn1003DFC0.  The nine
 *               globals it assigns were lifted into a BrStartupState; the
 *               original takes NO arguments at all.
 *   0x1003E0E0  BrExt_1003E0E0     = slice2_23 BrUiFn1003E0E0.  Same shape:
 *               the original takes no arguments, slice2_23 gave it a
 *               BrActiveFlags *.
 *   0x10034C66  BrX10034C66        = slice1_05 BrHookSetC.  Original is a
 *               __thiscall member; slice2_17.c's declaration has dropped the
 *               `this`.  No BrHooks instance exists.
 *
 * Declined for other reasons:
 *
 *   0x1006F310  BrProbe1006F310, 7 calls, and it RETURNS FLOAT -- which means
 *               its stub is not merely absent but actively wrong (the
 *               generated stub returns integer 0 and leaves xmm0 untouched,
 *               so its callers read garbage today).  It is nevertheless left
 *               alone: it walks the collision grid at 0x11750338/0x117554A0,
 *               which CONVENTIONS records as the ONE aliased-storage instance
 *               that cannot be made a single object on this host.  Porting it
 *               would have to pick one of the two conflicting views.  This is
 *               the highest-value remaining stub and should be done as its own
 *               packet, after that adjudication.
 *   0x1006F4A0  arity conflict, see below -- the argument slice3_40.h passes
 *               is the one the original never reads.
 *   0x10072270  0x1003C520  0x1003C550  Win32/COM leaves (SetEvent,
 *               WaitForSingleObject, CoCreateInstance, vtable Release).  These
 *               belong to the platform layer, not the portable core.
 *   0x1003CC70  0x1003D0B0  0x1003D210  DirectPlay session plumbing over
 *               GlobalAlloc/GlobalLock; same reason.
 *   0x1005F5A0  7 calls, but it is a DirectDraw Blt through a surface vtable;
 *               the clamping logic is real game code and worth having, but it
 *               needs a surface model this packet does not own.
 *
 * SIGNATURE CONFLICTS FOUND (reported, never silently resolved)
 * ============================================================
 *   0x10042AF0  THREE host names, THREE different signatures, and all three
 *               are wrong about the return value.  The original is six bytes:
 *               `mov eax, 1 / ret`.  It reads NO argument and RETURNS 1.
 *                 slice2_17.c: void BrX10042AF0(void *, int, int)     3 args
 *                 slice2_18.h: void BrGfx42AF0_3(void *, int32_t, int32_t)
 *                 slice5_61.h: void BrGfx42AF0_1(void *)              1 arg
 *               Since no argument is read and no caller reads the result,
 *               every one of them happens to be harmless -- but the image
 *               settles the arity at ZERO and the return type at int.
 *
 *   0x10072580  slice2_17.c declares `void BrX10072580(int)`.  The original
 *               returns int (1 on every early-out, and `hr == 0` otherwise).
 *               The void declaration is honoured here and the result
 *               DISCARDED, following packet 74's handling of 0x10072AF0.  Its
 *               one caller ignores the value, so nothing observable changes.
 *
 *   0x1006F4A0  slice3_40.h declares `void BrSub1006F4A0(void *pCar164)` --
 *               ONE argument -- and calls it with CAR_AT(pCar, 0x164).  The
 *               original is cdecl with TWO arguments, and the FIRST one is
 *               dead: `esi` is loaded from it and then unconditionally
 *               overwritten by the switch on all four iterations.  The live
 *               object is argument TWO.  So the single argument slice3_40.h
 *               passes lands in the slot the original never reads, and the
 *               object it wants is never passed at all.  NOT resolved here:
 *               this needs the caller corrected, not an adapter.
 *
 *   0x10005FE0  slice2_13.c declares `void BrSub10005FE0(uint32_t idPlayer)`
 *               -- one argument -- while slice1_02.h declares the same
 *               address as `BrNetDropMatching(BrNetState *, int32_t)` -- two.
 *               Consistent with the lifted-global pattern above; the extra
 *               leading parameter is a modelling choice, not the original.
 *
 *   0x1002F900  slice2_15.h's `BrGfxCmd` and slice1_05.h's `BrGfxWords` are
 *               two names for the same `{uint32_t w0, w1;}` object.  Layout
 *               identical, so the adapter casts; recorded because a third
 *               name for this pair would be the point at which it stops being
 *               harmless.
 *
 * ORIGINAL BEHAVIOUR PRESERVED ON PURPOSE
 * =======================================
 *   0x10060D90  the argument handed to the volume setter is the FULL eax:
 *               `mov eax, [index]` then `mov al, table_byte` leaves the
 *               index's upper three bytes in place.  For the shipped range
 *               (0..9) they are zero, so the value equals the table byte --
 *               but the expression is transcribed, not simplified.
 *   0x10060D90  neither index is bounds-checked before it indexes a ten-entry
 *               table.  Preserved.
 *   0x10072580  the bank index is not bounds-checked either.  Preserved.
 *   0x100193C0  a `%X` directive whose THIRD character is NUL falls through
 *               and measures the '%' as a glyph, then re-processes 'X' as a
 *               glyph on the next iteration.  Preserved.
 *   0x100193C0  and the other `%X` path is an off-by-one in the original: it
 *               steps the cursor by 2 and then the shared advance steps it
 *               again, so `%X` followed by any character consumes THREE
 *               characters and measures none of them.  "%di" measures nothing
 *               at all, not even the 'i'.  `%%`, `%i` and `%n` consume two,
 *               which is correct.  Preserved -- the loop invariant (the
 *               character just loaded is the one the cursor points at) makes
 *               this unambiguous in the disassembly.
 *   0x100193C0  the divisor is written into the incoming argument-1 stack
 *               slot -- README's "scratch-in-arg-slot" pattern.  Harmless
 *               here because the scale is already in a register.
 *   0x10005D30  0x10094294 is 0xFFFFFFFF in the shipped image, i.e. -1, which
 *               is the "empty" sentinel.  It is NOT zero, and zero is a valid
 *               slot index.
 */
#ifndef SLICE6_76_H
#define SLICE6_76_H

#include <stdint.h>

/* ==========================================================================
 * Storage this packet owns
 * ========================================================================== */

/* 0x10094294 -- the local slot / palette index, read by 0x10005D30 -- is NOT
 * owned here.  slice4_50.c owns it and slice4_50.h declares it; this packet
 * aliases in.  The collision was found by the LINKER only because both
 * packets independently chose the name `g_br094294`; had either picked a
 * different one, the tree would now hold two objects for one address, which
 * is the aliased-storage bug CONVENTIONS describes and which produces no
 * duplicate symbols at all.
 *
 * One live defect fell out of it.  slice4_50.c initialised the global to 0.
 * The DLL image holds 0xFFFFFFFF at 0x10094294 -- read out, not assumed --
 * i.e. -1, the "empty" sentinel, and 0 is a VALID slot index that
 * slice5_62.c feeds straight to BrNetSlotGetF02C.  Corrected in slice4_50.c
 * with the reasoning recorded there.
 *
 * OWNERSHIP NOTE: if a later packet models the record at 0x10221328 together
 * with this index, the symbol must be aliased into that model rather than
 * given a second view.
 *
 * 0x11828F08 -- fifteen BrSndVoice pointers.  This is NOT slice1_08.h's
 * BrSndVoices, which lives at 0x100B5DF0 and is 24*18 entries: different
 * address, different size.  The extent here is pinned by the image -- the
 * next global any function references above 0x11828F08 is 0x11828F44, i.e.
 * 0x3C bytes == 15 pointers, which is also slice1_08.h's BR_SND_BANK_SLOTS.
 *
 * Declared void * rather than BrSndVoice * so this header stays includable
 * beside headers that model the voice object differently; the .c casts. */
#define BR_SND_BANK_VOICES 15
extern void *g_aBrSndBankVoice[BR_SND_BANK_VOICES];

/* ==========================================================================
 * The seam to 0x100029F0
 * ========================================================================== */

/* 0x10060D90 calls 0x100029F0 with one cdecl argument, the music volume.
 * br_audio.c has that body as BrAudioSetVolume(BrAudio *, int) -- it takes the
 * audio object explicitly, and the original reaches it through a global no
 * module defines.  Rather than invent which BrAudio instance is the one, this
 * packet exposes the call as a hook: a NULL hook simply skips it, exactly as
 * slice3_41.c's BrGfx69580 does for its 64-byte pool counter.
 *
 * DEVIATION, and the only one in this packet. */
extern void (*g_pfnBrMusicVolume0029F0)(int32_t volume);

/* ==========================================================================
 * Tables read out of BRD3D.dll for 0x100193C0
 * ==========================================================================
 *
 * Exported so the tests can check the transcription against the image values
 * rather than against numbers this port produced. */

/* 0x100A5FEF, indexed by the character itself.  Only 0x21..0x7F is ever
 * reached -- everything outside takes the fixed-width branch -- so the table
 * is stored for that range and indexed as [c - 0x21]. */
#define BR_TEXT_CLASS_LO   0x21
#define BR_TEXT_CLASS_HI   0x7F
#define BR_TEXT_CLASS_N    (BR_TEXT_CLASS_HI - BR_TEXT_CLASS_LO + 1)
extern const signed char BrTextClassMap[BR_TEXT_CLASS_N];

/* 0x100A6070 (large) and 0x100A6150 (small): cumulative glyph offsets.  A
 * glyph's width is table[class + 1] - table[class], which is why there are 55
 * entries for 54 classes.  Classes 0..26 are digits and punctuation, 28..53
 * are letters (upper and lower share a class); 27 is a gap the class map
 * never produces and reading it would cross the two runs. */
#define BR_TEXT_GLYPHS 55
extern const int32_t BrTextWidthLarge[BR_TEXT_GLYPHS];   /* 0x100A6070 */
extern const int32_t BrTextWidthSmall[BR_TEXT_GLYPHS];   /* 0x100A6150 */

/* The two divisors the widths are scaled by, and the size threshold that
 * chooses between them.  Read off the immediates at 0x100193E3/0x100193F2
 * and 0x100193DE. */
#define BR_TEXT_DIV_LARGE  0x28
#define BR_TEXT_DIV_SMALL  0x14
#define BR_TEXT_LARGE_MIN  0x19

/* ==========================================================================
 * The four transcribed functions
 * ==========================================================================
 *
 * Every one of them already has a declaration in the module that CALLS it,
 * and that declaration is the contract this packet has to satisfy.  Following
 * packet 74, this header does NOT re-declare them: one declaration per name
 * stays the single source of truth, and the .c copies each prototype verbatim
 * from the header that owns it.  The owners are:
 *
 *   BrSub10060D90    port/include/slice2_25.h:457
 *   BrSub_100193C0   port/include/slice6_70.h:374
 *   BrX10072580      port/src/slice2_17.c:95   (a file-local extern)
 *   BrSub10005D30    port/include/slice3_40.h:169
 *
 * The twelve adapters are likewise declared by their callers and not here. */

#endif /* SLICE6_76_H */
