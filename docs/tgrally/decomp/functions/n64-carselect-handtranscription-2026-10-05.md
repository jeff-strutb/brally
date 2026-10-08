# N64 carselect handtranscription

*Recorded 2026-10-05.*

> BrCarSelect 0x8020D004 (16 KB) T4 2026-10-05 (ec9f6e1c, image gate 662/0) by hand transcription; last residue was a WRONG void PROTOTYPE of BrRomUnpack; same fix applies to BrMenu 0x8020AD5C and BrIntroScreen 0x8020686C

BrCarSelect (0x8020D004, src/tgrally/menus/carselect.c) went T3 -> T4 on 2026-10-05: commit ec9f6e1c + README regen 1b5b808c, image gate 662 placed / 0 bytes differ. Session 422ec5, by hand transcription (3872 -> 14 diffs by reading the ROM; I then wasted a long stretch on variant sweeps for the last 14, which led to [feedback-hand-transcription-only](../rules/hand-transcription-only.md)).

**The last 14 instructions = a wrong prototype.** The sound-bank clearing loop after `BrRomUnpack(...)` had ROM regs const 0x100 = v0, bank1 ptr = v1, bank2 ptr = a0 (ours: a0 / v0 / v1). carselect.c declared `void BrRomUnpack(...)`. The definition (romread.c) returns `unsigned int` (the unpacked length). With an int return, v0 is live at the start of the block after the call, so every web defined in the loop preheader (both SR pointer temps, and the counter in BrIntroScreen) avoids v0, while the bound constant (loop block only) gets it. Fixed by declaring `unsigned int BrRomUnpack(void *dst, int rom, void *s);`.

**How it was found (the method to reuse):** (1) siblings BrMenu and BrIntroScreen have the identical ROM register pattern in totally different contexts, so the cause was the idiom, not BrCarSelect's context; (2) a 25-line repro (statics + ReadSize/Alloc/Unpack + loop) reproduced our wrong pattern; (3) the trace showed the pointer webs live in the preheader block and the constant only in the loop block, so "something live in v0 at the preheader" -> the call's return value -> prototype. **Lesson: when a whole idiom colours wrong in every sibling, check the declarations of the calls next to it (return type, params) against their definitions BEFORE touching the loop.**

**Pending:** BrMenu (frontbuttons.c 0x8020AD5C) and BrIntroScreen (credits.c 0x8020686C) declare `void BrRomUnpack(unsigned char *dst, int rom, int s);` too (also racetick.c, romfile.c declare it void). Their T3 residue very likely includes the same clearing loop; fix the prototype there first.

Other levers that moved BrCarSelect:
- ROM stores a global back unchanged (`lw v0,G ... sw v0,G`) = FUNCTION-STATIC locals promoted by uopt with live-range-split store-backs (precedent BrMenu). Declare them in address order inside the function, keeping unused slots as fillers.
- Struct fields over byte offsets: `season->place[round][i]` / `times[round][i]` (2D) stopped a cross-call CSE; `D_8031B2C8[1].x10` (struct array) instead of a scalar.
- Chained assignment `A = B = C = D = 0;` sets which flag addresses get saved registers (right-to-left store order).
- Locals are coloured as one variable where live ranges connect: the ROM's s5 is `p` in every results loop; the ready loop uses n; the sound-channel loop uses back. The unused `int i` stays declared (its home keeps the frame).
- ROM rodata base 0x802A7A6C (4 mod 8): our doubles land +4, harmless (content check).
- uopt `cupcosts` (instrumented uopt.c ~156875): occurrence byte +19 = preferred arg register (colour numbering 1=v0, 3=a0), set by passedinreg; calls at block boundaries cost caller-saved regs, and for callees defined earlier in the same file only the registers that callee uses.
