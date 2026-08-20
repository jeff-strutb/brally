/* br_ui.h -- THE canonical layout of the 0x348-byte PAGE and the 0x1E214-byte
 * CONTROL, the two objects one level below br_phase.h's BrPhase_.
 *
 * WHY THIS FILE EXISTS
 *
 * br_phase.h ended the same argument one level up, and its own closing note
 * named this as the remaining work. Five headers independently modelled these
 * two objects and disagreed:
 *
 *   PAGE (0x348, ctor 0x10048470, vtable 0x1008F6F8)
 *     slice3_33.h  BrUiScreen  {f10, cCtl, apCtl[200], fX, fY, pOwner, cSel}
 *     slice3_32.h  BrUiPage    the same PLUS pVtbl, pfn04/08/0C, f16, f346
 *     slice6_72.h  BrUiPage_   slice3_33's fields, apCtl retyped BrUi72Ctl *
 *     slice6_73.h  BrUiPage_   slice3_33's fields PLUS pVtbl, pfn04, pfn08
 *
 *   CONTROL (0x1E214, ctor 0x100476C0, vtable 0x1008F6B8)
 *     slice2_23.h  BrUiObj     a BYTE ARRAY plus BR_UI_OFF_* constants, on the
 *                              stated grounds that the offsets "OVERLAP under
 *                              any single struct layout"
 *     slice3_32.h  BrUiObj     the same byte array plus BR_SCR_UI_* constants
 *                              and a port-only pointer-slot array
 *     slice3_33.h  BrUiCtl     16 fields
 *     slice6_71.h  BrUiCtlX    BrUiCtl + 7 more (a declared strict extension)
 *     slice6_72.h  BrUi72Ctl   19 fields, item block nested
 *     slice6_73.h  BrUiCtl_    28 fields, item block flat
 *
 * TWO of those spell the page `struct BrUiPage_`, so slice6_72.h and
 * slice6_73.h cannot be included in one translation unit. port/host/br_wire72.c
 * is already paying for that: it cannot include br_uictl.h and hand-declares
 * `BrUiCtlCtor` instead, then casts between two partial views of one object.
 *
 * WHAT THIS HEADER OWNS
 *
 * `struct BrUiPage_` -- br_phase.h forward-declares it and leaves it incomplete
 * on purpose, because its aPages[] needs it. slice6_72.h and slice6_73.h each
 * COMPLETED that forward declaration, differently. This header is the owner;
 * those two definitions are the ones to delete at migration.
 *
 * Consequence, stated plainly rather than hidden: br_ui.h cannot share a
 * translation unit with slice6_72.h or slice6_73.h. Neither can they with each
 * other today, so nothing gets worse, and migration is mechanical -- delete the
 * struct definitions from those two headers and #include this one.
 *
 * The old headers stay exactly as they are for the modules already compiled
 * against them. No sixth model is coined: every field below is a field some
 * existing header already named, a field an existing SHARED type already owns,
 * or a region the constructor establishes.
 *
 * ---------------------------------------------------------------------------
 * BYTE OFFSETS ARE 32-BIT-ONLY. READ THIS BEFORE ASSERTING ANYTHING.
 *
 * The comments give the ORIGINAL offsets, which hold on a 32-bit host. On LP64
 * every pointer widens and everything after it shifts, so
 * `offsetof(BrUiCtl_, w1E20C)` is NOT 0x1E20C there. Nothing may overlay this
 * struct on a file image or a foreign buffer, and no allocation may use the
 * 0x348 / 0x1E214 literals -- use BR_UI_PAGE_ALLOC_SIZE / BR_UI_CTL_ALLOC_SIZE.
 *
 * What IS asserted at the bottom, and holds on both: field ORDER, the integer
 * arithmetic that pins each array's element count, and that each object still
 * covers the original's allocation.
 * ---------------------------------------------------------------------------
 */
#ifndef BR_UI_H
#define BR_UI_H

#include <stddef.h>
#include <stdint.h>

#include "br_phase.h"    /* BrPhase_, and the BrUiPage_ forward declaration  */
#include "slice3_39.h"   /* BrTextBox (0x438) / BrTextList (0x1A9D4) -- the
                          * two objects EMBEDDED in the control; see ADJ-1
                          * and ADJ-6. Reused, never re-modelled.            */

/* ===========================================================================
 * ADJUDICATIONS
 *
 * Every disagreement below is settled from the disassembly, not by averaging
 * and not by taking the biggest model. Each one records what would make it
 * wrong, in the manner of br_phase.h's 20-vs-22 note.
 *
 * The single most useful fact, because five separate adjudications fall out of
 * it: the CONSTRUCTOR AT 0x100476C0 INITIALISES THE CONTROL IN ADDRESS ORDER
 * AND EVERY REGION IT TOUCHES ABUTS THE NEXT. There is not one byte of slack
 * between +0x60 and +0x128, +0x2978 and +0x2A40, +0x2A40 and +0x2AA4, +0x2AB6
 * and +0x2AE8, +0x2AF0 and +0x2B54, +0x2B5C and +0x3804, or +0x3838 and
 * +0x1E20C. When a `rep stosd` count and the next field's offset agree to the
 * byte six times running, the counts are the element counts.
 *
 * ---------------------------------------------------------------------------
 * ADJ-1. The block at control +0x2B5C is BrTextBox aText[3], not one item.
 *
 * slice6_71.h, slice6_72.h and slice6_73.h all model ONE item there.
 * slice2_23.h saw an array of stride 0x438 and could not bound it.
 *
 * The constructor settles it outright:
 *
 *     push 0x1005B0C0            ; dtor
 *     push 0x1005B050            ; ctor
 *     push 3                     ; COUNT
 *     push 0x438                 ; SIZE
 *     lea eax,[esi+0x2B5C] / push eax
 *     call 0x1007F680
 *
 * 0x1007F680 is the MSVC vector-constructor iterator: its body is
 * `for (i = 0; i < [ebp+0x10]; ++i) { ecx = ptr + i*size; call [ebp+0x14]; }`.
 * 0x1005B050 is slice3_39.h's BrTextBoxInit and 0x1005B0C0 its destructor
 * (slice6_72.h already identified the latter as BrTextBoxDtor). And
 * 3 * 0x438 == 0xCA8, so the array ends at 0x2B5C + 0xCA8 == 0x3804, which is
 * the very next offset the constructor writes.
 *
 * WRONG IF: 0x1007F680 is not the iterator, in which case those five pushes
 * are some other function's arguments and the exact landing on 0x3804 is
 * coincidence. Its prologue was read; it is the iterator.
 *
 * ---------------------------------------------------------------------------
 * ADJ-2. +0x2F66..+0x2F8C are fields of aText[0], not fields of the control.
 *
 * slice6_71.h and slice6_72.h nest them in an item; slice6_73.h makes them
 * flat control fields (f2F78, f2F80, f2F84, f2F88, f2F8C). Nesting wins, and
 * not by vote -- by what the vtable's "set text" slot does.
 *
 * 0x10047EB0 (vtable +0x34) writes, in this order: a strcpy into +0x2B65; an
 * OR into +0x2B60; a byte into +0x2B64; zero into +0x2F78, +0x2F68 and +0x2F66;
 * then +0x2F80, +0x2F88, +0x2F6C, +0x2F70. Subtract the array base 0x2B5C and
 * every one of those is a NAMED field of slice3_39.h's BrTextBox:
 *
 *     +0x2B5C -> +0x000 pVtbl      +0x2F66 -> +0x40A width
 *     +0x2B60 -> +0x004 f04        +0x2F68 -> +0x40C height
 *     +0x2B64 -> +0x008 f08        +0x2F6C -> +0x410 x
 *     +0x2B65 -> +0x009 sz[]       +0x2F70 -> +0x414 y
 *     +0x2F78 -> +0x41C f41C       +0x2F80 -> +0x424 left
 *     +0x2F84 -> +0x428 f428       +0x2F88 -> +0x42C right
 *     +0x2F8C -> +0x430 f430
 *
 * slice3_39.h derived those offsets from BrTextBox's OWN constructor and its
 * two measuring methods, which never mention the control. Two independent
 * derivations landing on the same twelve offsets is not a coincidence.
 *
 * WRONG IF: BrTextBox's own field offsets were mis-derived. They come from
 * 0x1005B050 and 0x1005B0D0/0x1005B160 directly.
 *
 * ---------------------------------------------------------------------------
 * ADJ-3. The step arrays hold FIFTY entries, not 24.
 *
 * slice6_71.h says BR71_STEP_COUNT is 24, from the highest index the one
 * function it read (0x10051D30) happened to write. That is a floor, not a
 * bound. The constructor gives the bound twice over:
 *
 *     mov ecx,0x32 / lea edi,[esi+0x2978] / rep stosd   ; 50 int32
 *     mov ecx,0x19 / or eax,-1 / lea edi,[esi+0x2A40] / rep stosd  ; 25 dwords
 *
 * 0x2978 + 50*4 == 0x2A40 exactly, and 0x2A40 + 25*4 == 0x2AA4, which is the
 * next field the constructor writes. 25 dwords over a stride-2 array is 50
 * uint16 -- the same 50. slice6_71.h's own evidence agrees: it found
 * +0x29B4 == +0x2978 + 15*4 paired with +0x2A5E == +0x2A40 + 15*2, i.e. the
 * two arrays are parallel with strides 4 and 2. Fifty of each.
 *
 * This is the br_phase.h 20-vs-22 shape inverted: there the larger count was
 * an over-read; here the smaller was an under-read. In both cases the count
 * that makes the neighbouring field land where the code puts it is the count.
 *
 * WRONG IF: the two arrays are not parallel, or the constructor deliberately
 * clears past the end of a shorter array into unrelated fields. The second
 * would have to be true at BOTH ends -- +0x2978's fill stopping exactly on
 * +0x2A40 and +0x2A40's stopping exactly on +0x2AA4.
 *
 * ---------------------------------------------------------------------------
 * ADJ-4. +0x2A42 is aStepId[1]. It is not a field.
 *
 * slice3_33.h, slice6_72.h and slice6_73.h all carry a scalar `f2A42`, and
 * slice3_32.h a BR_SCR_UI_W2A42 "alternate code". Under ADJ-3 that address is
 * element 1 of the 50-entry uint16 array based at +0x2A40. Element 0 is what
 * the vtable's "place" slot writes: 0x10047FB0 stores its last argument as a
 * WORD into +0x2A40 (and the same word into +0x1E20C).
 *
 * WRONG IF: the array's stride is 4 rather than 2, which would make +0x2A42
 * the high half of element 0. Ruled out by slice6_71.h's index-15 pairing and
 * by two independent headers reading +0x2A40 and +0x2A42 as separate int16.
 *
 * ---------------------------------------------------------------------------
 * ADJ-5. +0x2AB6 is int16 aChild[25], not a scalar. +0x2AB4 is its count.
 *
 * slice3_33.h, slice6_72.h and slice6_73.h all say `uint16 f2AB6 -- receives
 * cCtl + 1`. slice3_32.h says `int16[] child indices into the page`. The
 * consumer decides, and it is unambiguous. 0x10048530, the page's per-frame
 * driver, does:
 *
 *     mov  ax, word ptr [esi+0x2AB4]     ; the control's child count
 *     test ax,ax / jle  ...              ; SIGNED, skip when <= 0
 *     lea  ebp,[esi+0x2AB6]
 *     movsx eax, word ptr [ebp]          ; a SIGNED index
 *     mov  ecx, dword ptr [ebx+eax*4+0x18]   ; ... into the PAGE's apCtl
 *
 * A count at +0x2AB4 guarding a sign-extended index at +0x2AB6 that scales by
 * 4 into apCtl is a count/array pair, not two scalars. The constructor agrees:
 * `mov ecx,0xC / lea edi,[esi+0x2AB6] / rep stosd / stosw` is 12 dwords plus
 * one word == 25 uint16, ending exactly on +0x2AE8.
 *
 * The three headers are not wrong about what they saw -- a builder storing
 * `cCtl + 1` is storing aChild[0]. They are wrong that the storage stops there.
 *
 * WRONG IF: the constructor's fill runs past the array into +0x2AE8's
 * neighbourhood. It does not: +0x2AE8 is written explicitly two instructions
 * later, so the fill stops precisely where the next field begins.
 *
 * ---------------------------------------------------------------------------
 * ADJ-6. The block at +0x3838 is slice3_39.h's BrTextList, all 0x1A9D4 bytes.
 *
 * slice3_33.h models a lone vtable slot there; slice6_71.h adds +0x383C;
 * slice6_72.h and slice6_73.h add +0x383C and +0x384C. All four then place
 * further fields at +0x1E1C8..+0x1E204 as if those belonged to the CONTROL.
 * They do not -- they are inside this sub-object.
 *
 *     lea ecx,[esi+0x3838] ; call 0x1005B7F0
 *
 * and 0x1005B7F0 is slice3_39.h's BrTextListInit: a 0x1A9D4-byte container of
 * BrTextBox aItems[100] at +0x2C, BrTextBlob aBlobs[100] at +0x1A60C, header
 * words at +0x1A92C..+0x1A938 and a zeroed tail at +0x1A99C..+0x1A9D0. And
 * 0x3838 + 0x1A9D4 == 0x1E20C, the next offset the control's constructor
 * writes. The control's tail is therefore exactly two fields wide.
 *
 * The mapping the four headers will need at migration:
 *
 *     ctl +0x383C  = list.f04          ctl +0x384C  = list.f14
 *     ctl +0x3850  = list.f18          (slice3_32.h BR_SCR_UI_F3850)
 *     ctl +0x3C98  = list.aItems[0].f434, stride 0x438
 *     ctl +0x1E1E8 = list.f1A99C[5]    ctl +0x1E1F4 = list.f1A99C[8]
 *     ctl +0x1E200 = list.f1A99C[11]   ctl +0x1E204 = list.f1A99C[12]
 *
 * UNRESOLVED, and deliberately not papered over: slice6_73.h's f1E1C8 and
 * f1E1D0 land on list +0x1A990 and +0x1A998, which fall in the gap between
 * BrTextList's header words and its f1A99C[14] tail -- a region slice3_39.h
 * does not model at all. Naming them means editing slice3_39.h, which is a
 * different pass. Until then the canonical control cannot express those two.
 *
 * WRONG IF: BrTextList's extent were mis-derived. It comes from that object's
 * own constructor, and the 0x1E20C landing is an independent check on it.
 *
 * ---------------------------------------------------------------------------
 * ADJ-7. slice2_23.h's "the offsets overlap under any single layout" is wrong,
 *        and that is why the control was a byte array for four passes.
 *
 * Its exact objection: "0x1003EE50 indexes an array based at +0x2B5C with
 * stride 0x438; 0x1003EBE0 indexes a second array based at +0x3C98 with the
 * SAME stride 0x438, and 0x3C98 - 0x2B5C == 0x113C is not a multiple of 0x438".
 * Both observations are correct. The inference is not, because the two arrays
 * are not the same array:
 *
 *     +0x2B5C  is  aText[i]                     (3 entries, ADJ-1)
 *     +0x3C98  is  list.aItems[i].f434          (100 entries, ADJ-6)
 *
 * because the list begins at +0x3838, its own item array at list +0x2C, i.e.
 * control +0x3864, and 0x3864 + 0x434 == 0x3C98 -- f434 being BrTextBox's last
 * field. Two stride-0x438 grids at different bases, exactly as expected of an
 * object that embeds three text boxes and a list of a hundred more. The
 * remaining objection, that "+0x3838 and +0x1E20C do not sit on either grid",
 * is answered the same way: they are ordinary fields.
 *
 * The whole 0x1E214 reconciles under ONE layout, with no overlap anywhere.
 * That removes the stated reason for the byte-addressed BrUiObj and its
 * port-only pointer-slot array.
 *
 * WRONG IF: BrTextBox's last field is not f434, or the list's item array does
 * not start at list +0x2C. Both come straight from the two constructors.
 *
 * ---------------------------------------------------------------------------
 * ADJ-8. The control's +0x04..+0x18 hooks are `int32_t (*)(BrUiCtl_ *)`.
 *
 * slice3_33.h, slice6_71.h, slice6_72.h and slice6_73.h all type them
 * `void (*)(void *pArg)`, and all four say why: they only ever STORE them.
 * slice3_32.h's BrScrUiHookFn is `int32_t (*)(BrUiObj *)`, and it is the one
 * that watched a call. 0x10048530 does
 *
 *     mov eax,[esi+0x14] / test eax,eax / je / push esi / call eax
 *     add esp,4 / test eax,eax / je                 ; the RESULT is used
 *
 * and the same push/call/add-4 shape on +0x04. __cdecl, one argument, and that
 * argument is the control itself -- not an opaque void *. The return value of
 * +0x14 selects whether the control is drawn at all.
 *
 * +0x10 is called too, by slice2_23.c (BR_UI_OFF_ONAPPLY), which types it
 * `void (*)(BrUiObj *)` and DISCARDS the result. That is not a contradiction --
 * a caller may ignore a return -- so the int32 return is the safe superset of
 * the two observed sites.
 *
 * WRONG IF: the six slots do not share a shape. +0x04, +0x10 and +0x14 have
 * observed call sites and slice3_32.h reports +0x08 and +0x18's results
 * inspected as well. +0x0C alone is assumed by symmetry and is the weakest
 * claim in this header; one call site would settle it.
 *
 * ---------------------------------------------------------------------------
 * ADJ-9. The page has a hook at +0x0C, and the three page hooks take NO
 *        arguments -- they are not the control's hook type.
 *
 * slice6_73.h's BrUiPage_ has pfn04 and pfn08 only, both typed
 * `void (*)(void *)`. The constructor zeroes +0x04, +0x08 AND +0x0C, and
 * 0x10048530 calls two of them:
 *
 *     mov eax,[ebx+4]   / cmp eax,ebp / je / call eax     ; no push, no add
 *     mov eax,[ebx+0xC] / cmp eax,ebp / je / call eax     ; no push, no add
 *
 * No argument is pushed and no stack is cleaned, so they are not __cdecl with
 * an argument. Nor are they __thiscall: ecx still holds `this` at the FIRST
 * call only because nothing has clobbered it since entry, and it is NOT
 * re-established before the second. That the compiler knows how is settled
 * three instructions later -- `mov ecx,ebx` immediately precedes the genuine
 * __thiscall to 0x100484F0. It sets ecx when a callee wants it and does not
 * here. `void (*)(void)`, which is what slice3_32.h already says.
 *
 * WRONG IF: the callees are __fastcall/__thiscall and the original is relying
 * on a clobbered ecx, i.e. is already broken. The missing reload is the
 * evidence; a disassembly of any installed hook would confirm it.
 *
 * ---------------------------------------------------------------------------
 * ADJ-10. The page is 0x348 and really does end with TWO words.
 *
 * slice3_33.h, slice6_72.h and slice6_73.h stop at cSel (+0x344). The
 * constructor zeroes +0x344 AND +0x346, and 0x100484F0 reads +0x344 as a
 * modulus and stores the clamped result in +0x346 -- a count and a current
 * selection, which is slice3_32.h's reading. 0x346 + 2 == 0x348, so the two
 * words are what makes the allocation size come out exact.
 *
 * ---------------------------------------------------------------------------
 * ADJ-11. The control's allocation is 0x1E214. 0x1E210 is a field, not the end.
 *
 * slice2_23.h defines BR_UI_OBJ_MIN_SIZE as 0x1E210. That is a MINIMUM -- the
 * highest offset that header saw -- and reading it as the size is what the
 * task brief inherited. The builders push the literal:
 *
 *     1004D6C3  push 0x1E214 / call 0x1007DFE0        ; operator new
 *
 * and slice3_32.h (BR_SCR_UI_PW1E210) and slice6_71.h (p1E210) both establish
 * +0x1E210 as a pointer field, whose end is 0x1E214. Not a conflict once the
 * two claims are read as what they are; recorded because it looks like one.
 *
 * ---------------------------------------------------------------------------
 * NOT A CONFLICT, recorded so it is not re-litigated: slice6_72.h says the
 * page's fX is "190.0 or 195.0" and slice3_33.h / slice6_73.h say 195.0. Those
 * are values different builders store, not competing claims about the layout.
 * 0x1004D640 stores 0x43430000 (195.0f) and 0x43020000 (130.0f).
 *
 * NOT SETTLED HERE: slice3_33.h's BrUiPhase still declares the phase's page
 * array as 22 entries. br_phase.h adjudicated that to 20 and is canonical;
 * this header does not restate the argument.
 * ===========================================================================
 */

/* --------------------------------------------------------------------------
 * Original-layout constants. These describe the 32-BIT object and are used
 * only for the allocation floor and for the arithmetic assertions below.
 * -------------------------------------------------------------------------- */
#define BR_UI_PAGE_ORIG_SIZE   0x348u     /* the literal the builders push  */
#define BR_UI_CTL_ORIG_SIZE    0x1E214u   /* likewise -- see ADJ-11         */
#define BR_UI_TEXTBOX_ORIG_SIZE  0x438u   /* slice3_39.h's BrTextBox        */
#define BR_UI_TEXTLIST_ORIG_SIZE 0x1A9D4u /* slice3_39.h's BrTextList       */

/* Element counts, each pinned by a constructor fill whose end abuts the next
 * field. The assertions at the bottom re-derive every one of them. */
#define BR_UI_PAGE_CTL_MAX   200   /* apCtl, +0x018 .. +0x338; the page
                                    * ctor's `rep stosd` count is 0xC8      */
#define BR_UI_CTL_TEXTS        3   /* aText, +0x2B5C .. +0x3804  (ADJ-1)    */
#define BR_UI_CTL_STEPS       50   /* aStepMs/aStepId            (ADJ-3)    */
#define BR_UI_CTL_CHILDREN    25   /* aChild, +0x2AB6 .. +0x2AE8 (ADJ-5)    */
#define BR_UI_CTL_A0060       50   /* +0x0060 .. +0x0128                    */
#define BR_UI_CTL_A012A     2500   /* +0x012A .. +0x283A                    */
#define BR_UI_CTL_A283C       50   /* +0x283C .. +0x2904                    */
#define BR_UI_CTL_A2904       25   /* +0x2904 .. +0x2968                    */
#define BR_UI_CTL_A2AF0       25   /* +0x2AF0 .. +0x2B54                    */

typedef struct BrUiCtl_      BrUiCtl_;
typedef struct BrUiCtlVtbl_  BrUiCtlVtbl_;
typedef struct BrUiPageVtbl_ BrUiPageVtbl_;
/* NOTE: `BrUiPage_` is NOT typedef'd here -- br_phase.h already does it, and
 * repeating a typedef is a C11 extension this tree does not use. */

/* ===========================================================================
 * Hook types
 * ========================================================================== */

/* The control's +0x04/+0x08/+0x0C/+0x10/+0x14/+0x18 slots -- see ADJ-8.
 * slice3_32.h's BrScrUiHookFn is the same type over its own BrUiObj. */
typedef int32_t (*BrUiCtlHookFn_)(BrUiCtl_ *pCtl);

/* The page's +0x04/+0x08/+0x0C slots -- see ADJ-9. No arguments at all. */
typedef void (*BrUiPageHookFn_)(void);

/* ===========================================================================
 * Vtables
 * ========================================================================== */

/* 0x1008F6F8. TWO slots only: 0x1008F700, the phase's vtable, begins eight
 * bytes later and the shipped .rdata makes it the tail of this one. slice3_32.h
 * verified that overlap and called it a linker artefact rather than a
 * hierarchy; the classes differ (page +0x14 is a count word, phase +0x14 is a
 * pointer array), so nothing may be derived from it. */
struct BrUiPageVtbl_ {
    void    *(*f00)(BrUiPage_ *pThis, int32_t nFlags);  /* 0x100484C0 */
    int32_t  (*f04)(BrUiPage_ *pThis);                  /* 0x10048530 */
};

/* 0x1008F6B8, sixteen slots (+0x00..+0x3C, read out of the image). This merges
 * slice3_32.h's BrScrUiVtbl (which typed the low slots and +0x3C),
 * slice2_23.h's BrUiObjVtbl (which typed +0x14) and slice3_33.h /
 * slice6_72.h / slice6_73.h (which typed +0x34 and +0x38). Slots nothing has
 * been seen to call stay `void *`: do not read a signature into them. */
struct BrUiCtlVtbl_ {
    void    *f00;                                     /* 0x100478A0 */
    void   (*f04)(BrUiCtl_ *pThis);                   /* 0x100480A0 */
    void   (*f08)(BrUiCtl_ *pThis);                   /* 0x10048010 */
    int32_t (*f0C)(BrUiCtl_ *pThis);                  /* 0x10048180 */
    int32_t (*f10)(BrUiCtl_ *pThis);                  /* 0x10047A10 */
    void   (*f14)(BrUiCtl_ *pThis, int32_t msg,
                  int32_t a, int32_t b);              /* 0x100479D0 */
    void   (*f18)(BrUiCtl_ *pThis, void *p);          /* 0x10047980 */
    void   (*f1C)(BrUiCtl_ *pThis);                   /* 0x10047930 */
    int32_t (*f20)(BrUiCtl_ *pThis);                  /* 0x10047A60 */
    void    *f24;                                     /* 0x10047CB0 */
    float  (*f28)(BrUiCtl_ *pThis, int32_t ms);       /* 0x10047CE0 */
    void    *f2C;                                     /* 0x10047D10 */
    void   (*f30)(BrUiCtl_ *pThis);                   /* 0x10047D30 */

    /* +0x34 = 0x10047EB0. Set the control's text: strcpy into aText[0].sz and
     * fill in that box's geometry (ADJ-2). Where the text comes from the
     * string table the call site is BrStrGet(id), and only ONE of the four
     * pushes is cleaned by the caller -- how the shape was pinned. */
    void   (*f34)(BrUiCtl_ *pThis, const void *pText,
                  int32_t a2, int32_t a3, const void *pStyle);

    /* +0x38 = 0x10047FB0. Place the control. Every header records that a4 is 2
     * and a5 is 5 at every call site without saying where they land; the body
     * is 25 instructions long and says exactly:
     *     pOwner  <- +0x2AE8      flags |= +0x1C       x -> +0x3C
     *     a4     |= +0x24         a5    |= +0x28       y -> +0x40
     *     a6      -> +0x2968      a7 (WORD) -> aStepId[0] AND -> +0x1E20C  */
    void   (*f38)(BrUiCtl_ *pThis, BrPhase_ *pOwner, float x, float y,
                  int32_t flags, int32_t a4, int32_t a5,
                  int32_t a6, int32_t a7);

    int32_t (*f3C)(BrUiCtl_ *pThis);   /* 0x10048060 -- nonzero = skip */
};

/* ===========================================================================
 * The page (0x348, ctor 0x10048470)
 *
 * The constructor writes, in address order: +0x10, +0x14, +0x338, +0x33C, the
 * vtable, +0x04, +0x08, +0x0C, then `rep stosd` 200 dwords from +0x18, then
 * +0x340, +0x344, +0x346. Every field below is one of those. +0x16 is the one
 * it skips.
 * ========================================================================== */
struct BrUiPage_ {
    const BrUiPageVtbl_ *pVtbl;  /* +0x000  = 0x1008F6F8   slice3_32/73      */
    BrUiPageHookFn_      pfn04;  /* +0x004                 ADJ-9             */
    BrUiPageHookFn_      pfn08;  /* +0x008                                   */
    BrUiPageHookFn_      pfn0C;  /* +0x00C  slice3_32 only ADJ-9             */
    int32_t              f10;    /* +0x010  zeroed at build                  */
    uint16_t             cCtl;   /* +0x014  slice3_33 cCtl / slice3_32 nItems*/
    uint16_t             w16;    /* +0x016  the ctor SKIPS it; never seen
                                  *         read. Named, not explained.      */
    BrUiCtl_ *apCtl[BR_UI_PAGE_CTL_MAX];  /* +0x018 .. +0x338                */
    float                fX;     /* +0x338  190.0 or 195.0, per builder      */
    float                fY;     /* +0x33C  111.0 or 130.0, per builder      */
    BrPhase_            *pOwner; /* +0x340  the PHASE, never another page    */
    uint16_t             cSel;   /* +0x344  selectable count / modulus       */
    uint16_t             iSel;   /* +0x346  current selection      ADJ-10    */
};

/* ===========================================================================
 * The control (0x1E214, ctor 0x100476C0)
 *
 * Named fields are ones an existing header named; the aNNNN arrays are regions
 * the constructor establishes whose purpose is not established. Nothing here is
 * invented: an unnamed region is spelled as an array of the width the
 * constructor filled it with, never as a plausible field.
 * ========================================================================== */
/* PACKED, and only this struct.
 *
 * a012A is an array of dwords that starts at +0x12A -- a 2-mod-4 address.
 * Natural alignment cannot place it there: the compiler bumps it to +0x12C,
 * and because the array is 10000 bytes long every single field after it
 * inherits the drift, which is most of the struct.  Packing pins a012A where
 * the original put it and the rest of the layout falls into place behind it.
 *
 * The pack MUST NOT reach BrTextBox (slice3_39.h, included above and already
 * a complete type here).  BrTextBox has two internal 2-byte alignment gaps
 * that are genuinely present in the original; packing it too would shorten it
 * from 0x438 to 0x434, and aText[3] would then pull every field after the
 * array 12 bytes short.  Keep this pragma around this struct alone. */
#pragma pack(push, 1)
struct BrUiCtl_ {
    const BrUiCtlVtbl_ *pVtbl;   /* +0x00000  = 0x1008F6B8                   */
    BrUiCtlHookFn_ pfn04;        /* +0x00004  }                              */
    BrUiCtlHookFn_ pfn08;        /* +0x00008  } the six hook slots -- ADJ-8. */
    BrUiCtlHookFn_ pfn0C;        /* +0x0000C  } the ctor zeroes 04/08/0C/14/ */
    BrUiCtlHookFn_ pfn10;        /* +0x00010  } 18 together and comes back   */
    BrUiCtlHookFn_ pfn14;        /* +0x00014  } for +0x10 as its very last   */
    BrUiCtlHookFn_ pfn18;        /* +0x00018  } instruction. Order preserved.*/

    int32_t  flags1C;            /* +0x0001C  ctor = 1; f38 ORs its `flags`  */
    int32_t  f20;                /* +0x00020                                 */
    int32_t  flags24;            /* +0x00024  f38 ORs a4 (always 2)          */
    int32_t  flags28;            /* +0x00028  f38 ORs a5 (always 5)          */
    uint8_t  b2C;                /* +0x0002C  ctor = 0xFF                    */
    uint8_t  pad2D[3];           /* +0x0002D  not written by anything seen   */
    float    fTweenX;            /* +0x00030  slice3_32 BR_SCR_UI_F30        */
    float    fTweenY;            /* +0x00034  BR_SCR_UI_F34                  */
    float    fTweenZ;            /* +0x00038  BR_SCR_UI_F38                  */
    float    x;                  /* +0x0003C  f38's x   (slice2_23 F3C)      */
    float    y;                  /* +0x00040  f38's y   (slice2_23 F40)      */
    float    f44;                /* +0x00044  ctor = 0x3F7D70A4 == 0.99f     */
    int16_t  w48;                /* +0x00048                                 */
    int16_t  w4A;                /* +0x0004A                                 */
    int32_t  f4C;                /* +0x0004C                                 */

    /* The rectangle the builders compute with __ftol and store here. The ctor
     * does NOT initialise these four, and operator new does not zero, so they
     * are garbage until a builder writes them -- preserved, not "fixed". */
    int32_t  rcLeft;             /* +0x00050  slice3_33/72/73 f50            */
    int32_t  rcTop;              /* +0x00054  f54 -- also = (int)y elsewhere  */
    int32_t  rcRight;            /* +0x00058  f58 == f50 + 0x7F              */
    int32_t  rcBottom;           /* +0x0005C  f5C == f54 + 0x21              */

    int32_t  a0060[BR_UI_CTL_A0060];  /* +0x00060 .. +0x00128                */
    int16_t  wStep;              /* +0x00128  BR_SCR_UI_W128: index into
                                  *           aStepMs/aStepId               */
    /* +0x0012A: 2500 dwords the ctor fills with -1. It starts at a 2-mod-4
     * address in the original, which is why it cannot be folded into any
     * neighbouring array. Purpose not established. */
    int32_t  a012A[BR_UI_CTL_A012A];  /* +0x0012A .. +0x0283A                */
    uint8_t  pad283A[2];         /* +0x0283A  the ctor skips these two bytes */
    int32_t  a283C[BR_UI_CTL_A283C];  /* +0x0283C .. +0x02904                */
    int32_t  a2904[BR_UI_CTL_A2904];  /* +0x02904 .. +0x02968                */

    int32_t  f2968;              /* +0x02968  f38's a6; gates 0x100480A0     */
    int32_t  f296C;              /* +0x0296C  selects 0x100480A0's two arms  */
    int32_t  f2970;              /* +0x02970  last tick                      */
    int32_t  f2974;              /* +0x02974  accumulated ms                 */

    /* The two parallel step tables. FIFTY entries each -- see ADJ-3. */
    int32_t  aStepMs[BR_UI_CTL_STEPS];   /* +0x02978 .. +0x02A40             */
    uint16_t aStepId[BR_UI_CTL_STEPS];   /* +0x02A40 .. +0x02AA4, ctor = -1.
                                          * [0] is f38's a7; [1] is what four
                                          * headers call `f2A42` (ADJ-4).    */

    int32_t  f2AA4;              /* +0x02AA4                                 */
    int32_t  f2AA8;              /* +0x02AA8                                 */
    int16_t  w2AAC;              /* +0x02AAC                                 */
    uint8_t  pad2AAE[6];         /* +0x02AAE  untouched by anything seen     */

    uint16_t cChild;             /* +0x02AB4  BR_SCR_UI_W2AB4                */
    int16_t  aChild[BR_UI_CTL_CHILDREN]; /* +0x02AB6 .. +0x02AE8 -- SIGNED
                                          * indices into the owning page's
                                          * apCtl. See ADJ-5.                */

    BrPhase_ *pOwner;            /* +0x02AE8  f38's first argument. This is
                                  *           the field slice3_32.h had to
                                  *           put in a port-only slot array
                                  *           (BR_SCR_SLOT_PHASE) because it
                                  *           had no phase type; br_phase.h
                                  *           supplies one.                  */
    int32_t  f2AEC;              /* +0x02AEC  ctor = 1                       */
    int32_t  a2AF0[BR_UI_CTL_A2AF0];  /* +0x02AF0 .. +0x02B54, ctor = -1     */
    int32_t  f2B54;              /* +0x02B54  ctor = 1                       */
    int32_t  f2B58;              /* +0x02B58  ctor = 0                       */

    /* THREE text boxes, not one -- ADJ-1. aText[0] is the one the vtable's
     * +0x34 slot writes, and is where every f2B60 / b2B64 / f2F66 / f2F68 /
     * f2F6C / f2F70 / f2F78 / f2F80 / f2F84 / f2F88 / f2F8C in the four
     * partial models actually lives (ADJ-2). */
    BrTextBox aText[BR_UI_CTL_TEXTS]; /* +0x02B5C .. +0x03804                */

    /* The tween block. Names are slice3_32.h's BR_SCR_UI_TW*. */
    int32_t  twXOn;              /* +0x03804                                 */
    int32_t  twYOn;              /* +0x03808                                 */
    uint8_t  twXDir;             /* +0x0380C  1 up, 0xFF down, 0 = done      */
    uint8_t  twYDir;             /* +0x0380D                                 */
    uint8_t  pad380E[2];         /* +0x0380E                                 */
    float    twXEnd;             /* +0x03810                                 */
    float    twYEnd;             /* +0x03814                                 */
    int32_t  twActive;           /* +0x03818                                 */
    float    twLo;               /* +0x0381C                                 */
    float    twHi;               /* +0x03820                                 */
    float    twRate;             /* +0x03824  == (twHi - twLo) / n           */
    int32_t  twTick;             /* +0x03828                                 */
    int32_t  twMs;               /* +0x0382C                                 */
    int32_t  f3830;              /* +0x03830                                 */
    int16_t  w3834;              /* +0x03834                                 */
    int16_t  w3836;              /* +0x03836                                 */

    /* The embedded list -- ADJ-6. This one member absorbs +0x03838 all the way
     * to +0x1E20C, including every f383C / f384C / f3850 / f3C98 / f1E1E8 /
     * f1E1F4 / f1E200 / f1E204 the partial models carry. */
    BrTextList list;             /* +0x03838 .. +0x1E20C                     */

    uint16_t w1E20C;             /* +0x1E20C  f38's a7 again; 2/3/5/0x34     */
    uint8_t  pad1E20E[2];        /* +0x1E20E                                 */
    void    *p1E210;             /* +0x1E210  base of a stride-0x10 array
                                  *           (slice3_32 BR_SCR_UI_PW1E210,
                                  *           slice6_71 p1E210). Its end is
                                  *           what makes the object 0x1E214. */
};
#pragma pack(pop)

/* ===========================================================================
 * Allocation
 *
 * Never `operator new(0x348)` or `operator new(0x1E214)` in the port: on LP64
 * both structs are larger. Both macros are no-ops on a 32-bit host.
 * ========================================================================== */
#define BR_UI_PAGE_ALLOC_SIZE                                    \
    ((uint32_t)(sizeof(struct BrUiPage_) > (size_t)BR_UI_PAGE_ORIG_SIZE \
                ? sizeof(struct BrUiPage_) : (size_t)BR_UI_PAGE_ORIG_SIZE))

#define BR_UI_CTL_ALLOC_SIZE                                     \
    ((uint32_t)(sizeof(BrUiCtl_) > (size_t)BR_UI_CTL_ORIG_SIZE   \
                ? sizeof(BrUiCtl_) : (size_t)BR_UI_CTL_ORIG_SIZE))

/* ===========================================================================
 * Compile-time assertions -- C99 negative-array-size, the style of
 * port/tests/test_layout.c.
 *
 * NOT asserted: absolute byte offsets. Those are 32-bit-only and do not
 * survive LP64, exactly as br_phase.h warns.
 *
 * Asserted: (a) field ORDER, which holds on any host, and (b) the integer
 * arithmetic that PINS each element count -- every one of these is a
 * constructor fill whose end lands on the next field's offset, and is the
 * actual evidence for the count. If someone later "corrects" a count, one of
 * these fails and points at the reasoning above.
 * ========================================================================== */
#if defined(_MSC_VER) && _MSC_VER < 1200
#define BR_UI_ASSERT(name, cond)
#else
#define BR_UI_ASSERT(name, cond) typedef char BR_UI_##name[(cond) ? 1 : -1]
#endif

/* --- the arithmetic that pins the counts (host-independent) -------------- */
BR_UI_ASSERT(page_apctl_fills_to_338,
             0x018u + (unsigned)BR_UI_PAGE_CTL_MAX * 4u == 0x338u);
BR_UI_ASSERT(page_ends_at_348,
             0x346u + 2u == BR_UI_PAGE_ORIG_SIZE);

BR_UI_ASSERT(ctl_a0060_fills_to_128,
             0x0060u + (unsigned)BR_UI_CTL_A0060 * 4u == 0x0128u);
BR_UI_ASSERT(ctl_a012A_fills_to_283A,
             0x012Au + (unsigned)BR_UI_CTL_A012A * 4u == 0x283Au);
BR_UI_ASSERT(ctl_a283C_fills_to_2904,
             0x283Cu + (unsigned)BR_UI_CTL_A283C * 4u == 0x2904u);
BR_UI_ASSERT(ctl_a2904_fills_to_2968,
             0x2904u + (unsigned)BR_UI_CTL_A2904 * 4u == 0x2968u);
/* ADJ-3: 50, not 24. Both arrays, both ends. */
BR_UI_ASSERT(ctl_stepms_fills_to_2A40,
             0x2978u + (unsigned)BR_UI_CTL_STEPS * 4u == 0x2A40u);
BR_UI_ASSERT(ctl_stepid_fills_to_2AA4,
             0x2A40u + (unsigned)BR_UI_CTL_STEPS * 2u == 0x2AA4u);
/* ADJ-5: an array of 25, not a scalar. */
BR_UI_ASSERT(ctl_achild_fills_to_2AE8,
             0x2AB6u + (unsigned)BR_UI_CTL_CHILDREN * 2u == 0x2AE8u);
BR_UI_ASSERT(ctl_a2AF0_fills_to_2B54,
             0x2AF0u + (unsigned)BR_UI_CTL_A2AF0 * 4u == 0x2B54u);
/* ADJ-1: three text boxes, ending exactly on the tween block. */
BR_UI_ASSERT(ctl_atext_fills_to_3804,
             0x2B5Cu + (unsigned)BR_UI_CTL_TEXTS * BR_UI_TEXTBOX_ORIG_SIZE
             == 0x3804u);
/* ADJ-6: the list ends exactly on +0x1E20C. */
BR_UI_ASSERT(ctl_list_fills_to_1E20C,
             0x3838u + BR_UI_TEXTLIST_ORIG_SIZE == 0x1E20Cu);
/* ADJ-7: slice2_23.h's second stride-0x438 grid is inside the list. */
BR_UI_ASSERT(ctl_3C98_is_list_item0_f434,
             0x3838u + 0x2Cu + 0x434u == 0x3C98u);
/* ADJ-11: +0x1E210 is a field; its end is the allocation. */
BR_UI_ASSERT(ctl_ends_at_1E214,
             0x1E210u + 4u == BR_UI_CTL_ORIG_SIZE);
/* slice3_39.h's own two counts, restated as the control depends on them. */
BR_UI_ASSERT(textlist_items_fill_to_1A60C,
             0x2Cu + (unsigned)BR_TEXTLIST_ITEMS * BR_UI_TEXTBOX_ORIG_SIZE
             == 0x1A60Cu);

/* --- field ORDER: holds on 32- and 64-bit alike -------------------------- */
BR_UI_ASSERT(page_order_hooks_after_vtbl,
             offsetof(struct BrUiPage_, pfn0C) > offsetof(struct BrUiPage_, pVtbl));
BR_UI_ASSERT(page_order_cctl_after_f10,
             offsetof(struct BrUiPage_, cCtl) > offsetof(struct BrUiPage_, f10));
BR_UI_ASSERT(page_order_apctl_after_cctl,
             offsetof(struct BrUiPage_, apCtl) > offsetof(struct BrUiPage_, cCtl));
BR_UI_ASSERT(page_order_fx_after_apctl,
             offsetof(struct BrUiPage_, fX) > offsetof(struct BrUiPage_, apCtl));
BR_UI_ASSERT(page_order_owner_after_fy,
             offsetof(struct BrUiPage_, pOwner) > offsetof(struct BrUiPage_, fY));
BR_UI_ASSERT(page_order_isel_after_csel,
             offsetof(struct BrUiPage_, iSel) > offsetof(struct BrUiPage_, cSel));

BR_UI_ASSERT(ctl_order_hooks_after_vtbl,
             offsetof(BrUiCtl_, pfn18) > offsetof(BrUiCtl_, pVtbl));
BR_UI_ASSERT(ctl_order_flags_after_hooks,
             offsetof(BrUiCtl_, flags1C) > offsetof(BrUiCtl_, pfn18));
BR_UI_ASSERT(ctl_order_rect_after_f4C,
             offsetof(BrUiCtl_, rcLeft) > offsetof(BrUiCtl_, f4C));
BR_UI_ASSERT(ctl_order_rect_is_ltrb,
             offsetof(BrUiCtl_, rcBottom) > offsetof(BrUiCtl_, rcRight) &&
             offsetof(BrUiCtl_, rcRight)  > offsetof(BrUiCtl_, rcTop)   &&
             offsetof(BrUiCtl_, rcTop)    > offsetof(BrUiCtl_, rcLeft));
BR_UI_ASSERT(ctl_order_wstep_after_a0060,
             offsetof(BrUiCtl_, wStep) > offsetof(BrUiCtl_, a0060));
BR_UI_ASSERT(ctl_order_stepid_after_stepms,
             offsetof(BrUiCtl_, aStepId) > offsetof(BrUiCtl_, aStepMs));
BR_UI_ASSERT(ctl_order_achild_after_cchild,
             offsetof(BrUiCtl_, aChild) > offsetof(BrUiCtl_, cChild));
BR_UI_ASSERT(ctl_order_owner_after_achild,
             offsetof(BrUiCtl_, pOwner) > offsetof(BrUiCtl_, aChild));
BR_UI_ASSERT(ctl_order_atext_after_owner,
             offsetof(BrUiCtl_, aText) > offsetof(BrUiCtl_, pOwner));
BR_UI_ASSERT(ctl_order_tween_after_atext,
             offsetof(BrUiCtl_, twXOn) > offsetof(BrUiCtl_, aText));
BR_UI_ASSERT(ctl_order_list_after_tween,
             offsetof(BrUiCtl_, list) > offsetof(BrUiCtl_, twMs));
BR_UI_ASSERT(ctl_order_w1E20C_after_list,
             offsetof(BrUiCtl_, w1E20C) > offsetof(BrUiCtl_, list));
BR_UI_ASSERT(ctl_order_p1E210_last,
             offsetof(BrUiCtl_, p1E210) > offsetof(BrUiCtl_, w1E20C));

/* --- the port must never allocate less than the original did ------------- */
BR_UI_ASSERT(page_covers_orig,
             sizeof(struct BrUiPage_) >= (size_t)BR_UI_PAGE_ORIG_SIZE);
BR_UI_ASSERT(ctl_covers_orig,
             sizeof(BrUiCtl_) >= (size_t)BR_UI_CTL_ORIG_SIZE);
/* The embedded objects are the bulk of the control; if either ever shrank
 * below its original the control could still pass the test above by accident. */
BR_UI_ASSERT(textbox_covers_orig,
             sizeof(BrTextBox) >= (size_t)BR_UI_TEXTBOX_ORIG_SIZE);
BR_UI_ASSERT(textlist_covers_orig,
             sizeof(BrTextList) >= (size_t)BR_UI_TEXTLIST_ORIG_SIZE);

/* --- the counts themselves, so a silent edit cannot pass ----------------- */
BR_UI_ASSERT(page_ctl_max_is_200,  BR_UI_PAGE_CTL_MAX == 200);
BR_UI_ASSERT(ctl_texts_is_3,       BR_UI_CTL_TEXTS    == 3);
BR_UI_ASSERT(ctl_steps_is_50,      BR_UI_CTL_STEPS    == 50);
BR_UI_ASSERT(ctl_children_is_25,   BR_UI_CTL_CHILDREN == 25);

#endif /* BR_UI_H */
