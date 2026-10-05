# T3 is not a shortcut for fresh transcriptions

*Recorded 2026-09-16.*

> CORRECTED: a fresh transcription CAN reach T3 via the A5 oracle (it supersedes byte gates); the work is making the oracle RUN, not the byte grind. 0x1002F790 done.

**CORRECTED 2026-09-16 (the earlier claim in this note was WRONG).** A fresh
hand-transcription CAN reach T3 even with a huge byte gap. `t3.py --qualify`
makes A5 (the behavioural oracle) AUTHORITATIVE: a clean EQUIVALENT /
EQUIV-MODULO-FP verdict SUPERSEDES the byte-shape gates A1-A4 ([oracle-runs-orchestrators](oracle-runs-orchestrators.md),
[t3-certified-standard](../rules/t3-certified-standard.md)).  0x1002F790 (2517 B, the largest ex-T1)
certified T3 with insn-gap 186, rows 392, regions 11 -- byte gates all
"[superseded by A5 EQUIVALENT]".  Committed cf0122ec; T3 128.

So the real work for a T3 is NOT the byte grind -- it is making the A5 oracle
RUN and return EQUIVALENT on valid-state seeds.  For a packet/object-graph
orchestrator that means: extern "C" symbol (so obj-lookup + parse_signature
agree); m_<VA>/sub_<VA> callee names (so relocs resolve); a Profile in
oracle_profiles.py that seeds a VALID world (bounded, well-formed input +
gating globals); and whatever emulator/resolution capabilities the function's
instruction mix needs.  Gate B (2 zero-movement @t4-pass ledger lines at the
current numbers, one census yes) still requires a HONEST T4 grind first -- but
that grind is bookkeeping of a wall, not the certification itself.

** EMULATOR GOTCHA that masquerades as a transcription bug (cost most of a
session):** when the recompilation allocates a stdcall import into a register
and calls `call reg` (where the original used direct `call [mem]`), x87emu used
to mis-parse the register as a memory operand and model it as an unresolved
indirect call WITHOUT cleaning the callee's stdcall args -- leaking stack on
every such call.  Two leaked ReleaseMutex args drifted esp by 8 and moved a
stack object's pointer 8 bytes, so an end-of-input check read garbage and
LOOPED FOREVER (oracle: runaway / false record-path DIFF).  Only the recomp
tripped it (the original's direct call was handled).  FIXED (cf0122ec):
x87emu handles `call <reg>` (target = reg value, routed to the model if it is
an import); t3b_verify seeds each import IAT slot to its own address so a cached
`mov reg,[slot]; call reg` resolves.  **When the oracle DIFFs/runs-away only on
the recomp and the global writes otherwise match, suspect a `call reg`
cached-import esp leak before suspecting the transcription.**

Levers from the T4 grind (real, reusable): size a name scratch to its full
declared width to fix the frame; hoist index*stride into a held local; the
mutex-import register allocation is the irreducible byte residue.  The twin
0x100038F0 certified T3 the same way (base dispatcher, reuses this profile).
