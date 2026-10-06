# Placed image verification

> t3image_verify.py = the placed-bytes A5 sweep (cf1e9f7b); caught precedence split + 22-function silent truncation; t3_slot_ok.csv evidence allowlist; 108 place, 10 retracted shrink lanes

**2026-09-19c (cf1e9f7b), sequel to [lockstep-oracle-arbitration](lockstep-oracle-arbitration.md): the
self-verification layer.** `tools/brally/t3image_verify.py` reads each T3 span OUT
OF THE BUILT DLL and runs the A5 comparison -- no recompiling/resolving, so
no obj-variant mismatch; it tests what ships. First sweep: 70 EQUIVALENT,
3 identical, 2 DIFF.  ALWAYS run it after a gate before shipping.

**Two defect classes it caught (both latent for weeks):**
1. **Precedence split:** fill_function (oracle) read reloc_overrides.csv
   FIRST while the gate let pairing overwrite it -- obj-verify EQUIVALENT
   while the image carried role-swapped values (FadeTick). NOW: one order
   everywhere -- the CSV wins, disagreements print as NOTEs, hand rows beat
   machine rows on duplicate keys, lockstep_rows never overwrites existing.
    any new resolution layer must be threaded through BOTH consumers.
2. **Silent truncation:** compiled_functions slices orig-size bytes from a
   LONGER recompile -- 22 functions always shipped tail-cut
   (BrGbiSizeShift lost its ret; falls into the next fn on the >128 path).
   Gate now measures true code length (strip mixed 90/CC padding in a LOOP
   -- sequential rstrip under-strips) and blocks overhang unless (a) cut
   bytes byte-identical to image tail, or (b) config/brally/t3_slot_ok.csv records
   a placed-image A5 EQUIVALENT for that spliced form (11 admitted).

**State: 108/118 place. 10 retracted to ORIGINAL bytes (safe), each a
SHRINK-TO-FIT lane:** GbiSizeShift(+1B, DIFF-proven bad), HudDraw(+14B,
placed-DIFF at the DL cursor -- recolor-routing suspect), KeyTableFind(+4),
RcaFixup(+6), GlInstall(+3), CtlAiBody(+5), DPlaySysMsgLog(+1),
FUN_10036a30(+4), FUN_100583c0(+2), SprFontGlyphA(+1), WavReadData(+8).
NINE have a fitting variant already measured (O2y mostly, Od for
SaveBeginTimeAttack-class): CtlAiBody O2y=3768/3858, CrRespWalk O2y,
DPlaySysMsgLog O2y=443/469, MenuTime O2y=347... The lane = per-function
variant override in collect_t3 (compile that file at the fitting tag for
that fn) + regenerate its lockstep rows against THAT obj + placed-image
verify. No-fit ones (GbiSizeShift 97/96 etc.) = codegen-density respells.

**Sweep residue worth lanes:** 16 no-parsable-signature (extend
parse_signature / per-VA sig overrides), runaways (KeyTableFind@own-loop,
FrameDraw@10008FB0), `repe cmpsb` unimplemented in x87emu (DpAddressBuild),
BrEarLoad pairing-vs-lockstep 2-slot disagreement (0x104b164c/161c vs
165c/1658) unresolved -- oracle-arbitrate when its sig parses.
