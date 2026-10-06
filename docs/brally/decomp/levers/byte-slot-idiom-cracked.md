# Byte slot idiom cracked

*Recorded 2026-09-01.*

> byte-slot idiom CRACKED via fresh BrGlRectFill transcription (0x1001E380 byte-exact) - plain scalar uint8_t locals; residue class is register-death timing, not spelling; also the mispaired-report-row trap

2026-08-31: The `mov byte [esp+S],r ... mov R,dword [esp+S]; and R,0xff`
pattern (drawcar's pack wall, 6+ functions) is PLAIN SEPARATE uint8_t
LOCALS - no array, no volatile, no source `& 0xFF`. The dword+mask is
VC5's widening when the stored register DIED before the read (calls or
scheduler kills between); if it survives, VC5 store-forwards and deletes
the store. Proven by transcribing 0x1001E380 BrGlRectFill fresh to
byte-exact (914 B, filed in src/brally/core/drawing/br_dlglide.c, sweep 2/2,
commit 8afdafa). Full mechanics in docs/brally/VC5-IDIOMS.md (three 2026-08-31
entries).

**Why:** BrCarDrawVehicle (0x1000A110) sits at REGNORM 46+56 with this
as its main wall - the spelling is now right in-tree; only the
register-death timing differs. The five other unmatched carriers
(0x10039620, 0x10028BB0, 0x1006AB80, 0x10036220, 0x10058AF0) got
cheaper.

**Phantom-frontier census (2026-08-31, ran after the crack):** 338 of 432
diff rows score a Glide VA their file never claims. Breakdown: 239 rows /
91 KB are `shared/body` twins (byte-identical in both binaries - scores
HONEST, ordinary transcription frontier); 22 rows / 13.9 KB `shared/prefix`
(tails diverge - half-phantom, includes BrCarStateEncode,
BrScenePropsDraw); ~58 rows / 13 KB callsite/ptrsite-paired (bodies never
verified - spot-check); 3 rows `renderer/slot` DIFFERENT CODE - ALL THREE
now harvested: 0x1001E380 BrGlRectFill (byte-exact), 0x10021270
BrGlGbiCall (byte-exact, FIRST COMPILE), 0x1001E080 BrGlInstall (1 region
/ 3 bytes: inline jne+ret vs near-je to shared tail ret; 6 probes dead,
tag withheld, permuter bait). All three filed in
src/brally/core/drawing/br_dlglide.c. Census script inline in the session; redo
via report.csv diff rows vs @implements claims + shared.csv class.

**DIFF(14) callconv cluster = the EAX-pattern thiscall wall (verified on
0x1003F860):** the ~12 UiHook/PhaseLeave functions the refine batch left
at DIFF(14) "callconv" all contain a virtual thiscall with a STACK-pushed
immediate arg + callee cleanup (`mov eax,[ecx]; push 1; call [eax]`)  - 
unreachable from C (int args ride edx under the fastcall trick, struct
coercion is C2115, float puns push from memory). Same class as
BrCarStateEncode's Glide writer and BrPhaseLeave. Park ALL of these for
the C++ workstream; zero-arg vcalls (EDX pattern) remain reachable.
Refine batch 2026-08-31 went 0/260 - the automated frontier is dry;
what remains is hand idioms, the C++ class, and the coloring tail.

**Stub-vs-body harvest state (end of 2026-08-31 session):** 63 rows where
recomp < 50% of orig. LANDED: 0x100634B0 BrGlCfgSave (930 B byte-exact,
first compile). NEAR: 0x10031B80 BrGlTrackHdrRead (12 scheduling regions,
generated unrolled body in slice2_20.c), 0x10059410 BrGlNavPoll (6
regions, DirectInput poller in br_uinav.c, tag pending), 0x1001E080 (1
region). RECLASSIFIED OFF THE C FRONTIER: the slice8_86 five
(0x100439B0/0x10044860/0x100451F0/0x100458D0/0x10045EF0, 11.3 KB) plus
0x1004A840 and 0x10048160 are C++ EH-frame functions -> cpp_work lane;
0x10062640 is a quat-to-matrix x87-DAG (float wall class). Remaining
unaudited: ~40 smaller stub rows -- rerun the stub census (recomp/orig <
0.5 over diff rows) to enumerate.

**How to apply:**
- Never grind a "near-miss" whose report row pairs a D3D body against
  Glide bytes - 0x1001E380 showed 824 phantom diffs that way; a FRESH
  914 B transcription reached byte-exact in ~10 probes. Check the row's
  source file actually claims the Glide VA before triaging.
- br_dlglide.c is an /O2 /Op TU (fn.py hardcodes /O2 - use the sweep or
  manual cl for it). br_drawcar.c is proven plain /O2 (BrCarDrawBody
  BREAKS under /Op - do not switch it).
- Related walls mapped in VC5-IDIOMS: /Op fild serialization,
  vertex-major inline-cast fan-out, temp-read clamp = memory-homed arg.
- See [vc42-not-brglide](../toolchain/vc42-not-brglide.md) (compiler stays MSVC 5.0 for the DLL) and
  [register-rotation-is-a-symptom](../triage/register-rotation-is-a-symptom.md) (drawcar's rotation snapped when
  its caches were removed - recompute-per-site, confirmed by the TGR
  N64 twin at 0x80232ed4).
