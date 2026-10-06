# Branch vs branchless select for adjacent-constant → call argument

Proven on BrHudDraw 0x10015300 (2026-08-31), REGNORM 20+23 → 19+21.

`BrHandleLookup(p, (flag != 0) ? 0xEBu : 0xECu)` - a ternary between two
constants that differ by 1, used directly as a call argument - makes VC5 /O2
emit the BRANCHLESS carry trick:

    neg eax ; sbb eax, eax ; add eax, 0xec      ; = flag ? 0xEB : 0xEC

The original BRANCHED. Forcing it: assign an intermediate in an if/else,
default-then-override form:

    unsigned uStr = 0xECu;
    if (flag != 0) uStr = 0xEBu;
    BrHandleLookup(p, uStr);

→ `mov eax,0xec ; test ecx,ecx ; je L ; mov eax,0xeb ; L:` (a real branch).

RESIDUAL (not yet solved): the original PUSHes the immediate directly inside
each branch arm (`push 0xeb` / `push 0xec`, one shared call after a jmp) and
keeps the flag in EAX; the if/else form routes the value through a register
(`mov eax,0xeb`) and puts the flag in ECX. That last step is push-scheduling
+ register choice - likely needs the ternary to remain the direct nested
call argument while still branching (open).
