# Jump table override crash class

*Recorded 2026-09-22.*

> In-game crash class - lockstep override rows pinned switch jump-table dispatches to the ORIGINAL table addresses, dispatching byte-different T3 bodies mid-instruction

**2026-09-21: the "gets into menus, crashes getting in-game" class.** A T3 body
is byte-different from the original, so its inline switch jump tables live at
different offsets. The exact per-body target is `va + label_offset` from the
object symbol table (`reloc_pair.jump_table_slots`, a same-section `$L` DIR32
slot). But `tools/brally/lockstep_rows.py` emitted a `config/brally/reloc_overrides.csv` row
for those slots copying the ORIGINAL operand (the original table VA), and the
override channel in `tools/brally/image_build_t3.py` applied LAST and UNGUARDED - so the
placed body's `jmp [table]` pointed into the ORIGINAL layout, landing
mid-instruction in the byte-different body → invalid opcode / page fault. Menus
never hit the switch; each race mode hits a different case, so the crash EIP
differs per mode but always lands 1 - 3 bytes off a real boundary.

Found via BrRaceStep 0x10019A70 (3 of its 4 switches wrong; the 4th had no
override, hence the "3/4" score). Same latent bug in FUN_100038f0, FUN_1002f790,
BrTextEmitString 0x10015B10.

**Fix (three parts):** (1) the override loop in both lanes of image_build_t3 now
skips slots that are in `jump_table_slots` - those are exact from the symbol
table and no hand row may clobber them; (2) lockstep_rows.py no longer emits
same-section `$L` jump-table rows; (3) removed the 13 dead jump-table rows from
the CSV.

**2026-09-21 SECOND VARIANT - the addend-drop (Quick Race null-deref at
0x1001a45e).** Same root (lockstep override copies the ORIGINAL operand,
codegen-blind) but via a global reference, not a jump table. BrRaceStep's tyre
loop: our object built `mov eax, <?g_AF2094>+8` (DIR32 reloc, ADDEND 0x8) then
read the driver pointer at `[eax-8] = g_AF2094`. The original used base+0, so
its operand was 0x10af2094; the lockstep row copied that (comment even said
"addend 0x8") and DROPPED the +8, shipping `mov eax,0x10af2094` → `[eax-8]` =
0x10af208c = null → page fault, but ONLY in the Quick Race case the A5 oracle
never seeded. The OBJECT and the cert were CORRECT - this was pure image
placement. Fix: `lockstep_rows.py` now also skips any site whose symbol is
address-bearing (`address_in_name(sym) is not None`) - placement resolves those
EXACTLY as addr+our_addend, addend-aware, so a hand copy is at best redundant
and drops the addend when the two compilers pick different base offsets.
Removed the one bad row (0x10019A70 off 0x9E7). A repo-wide scan found this was
the ONLY addend-dropped address-bearing override in the whole file. Verify the
placed base: `mov eax, 0x10af209c` (not 0x10af2094). LESSON for the project lead's trust
question: a T3 cert verifies the compiled OBJECT; a wrong shipped byte can still
come from the hand-override/placement layer, which is a BUILD bug, not a cert
failure - the decomp source stands.

** The guard MUST be jump-table-only, NOT all identity slots.** First attempt
guarded `const_slot_values` too and REGRESSED BrFadeTick / BrCtlAiBody /
BrCarDrawVehicle: const_slot_values is CONTENT-matched and mislocates a constant
on a shared leading dword (BrFadeTick's float pool $T1315 = `00000000 ...`
matched a zero dword at .text 0x10072a7c instead of the real .rdata pool
0x10077370). For const slots the hand override is the sanctioned correction and
MUST still win - only jump-table dispatches are exact-and-unoverridable. The
contract gate does NOT catch a wrong const value (it is data, not
contract-validity); verify placed bytes directly. See [placed-image-verification](placed-image-verification.md).

Verify a fix by disassembling the placed image: BrRaceStep's 4 dispatches must
read `0x1001c4d8/c4f4/c508/c518` (into the body), not `0x1001c648/c664/c678`.

**The BrRaceStep placed-image A5 DIFF at 0x106B8080 is a FALSE POSITIVE, run to
ground 2026-09-21.** Placed bytes == cert-clean resolve_bytes (0 diffs, so NOT a
placement bug); orig-vs-orig control is EQUIVALENT (harness sound). Traced it:
BrRaceStep uses ebp AS A ZERO REGISTER (`xor ebp,ebp` at 0x10019a8e + 12 more
sites); the original spells `push ebp` (1-byte-cheaper `push 0`) at 0x1001b17e,
the transcription spells literal `push 0` - identical. The oracle's profiled
seed reached that call on a path that never re-zeroed ebp, so it fed the ORIGINAL
side stale ebp=0xfffffb9d → callee 0x10008d60 stored 0x83 vs the transcription's
0x4d. On hardware ebp=0 and both store 0x4d. Lesson: a single-global A5 DIFF on a
giant switch-orchestrator, where the differing operand is a `push ebp`/`cmp
,ebp` zero-idiom, is a partial-seed artifact ([oracle-runs-orchestrators](oracle-runs-orchestrators.md)), not
an equivalence gap. BrRaceStep is legitimately T3 bracestep-wall.
