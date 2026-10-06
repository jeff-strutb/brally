# Render frontier draining

*Recorded 2026-08-22.*

> Method and running state for porting the per-frame race render (0x10011FA0) TODO callees.

Milestone 6's render half = port the per-frame race render `0x10011FA0`. It
cannot be transcribed until its direct callees are ported. The enumerated queue
is **`config/brally/render_frontier.csv`** (va, glide_size, aligned, status, name,
note).

**Why B (drain frontier) before A (scene-entity model):** each boundary-clean
callee is a confirmed, auditable win, AND porting them surfaces the real
entity-struct field accesses (e.g. 0x10011EA0 loops over g_B73538), which is
exactly the evidence [scene-entity-model](scene-entity-model.md) needs. So B de-risks A rather than
competing with it. A stays the structural unlock; do it once B has exposed the
field usage.

**The loop that worked (validated on 0x10017F60):**
1. Read the Glide body from `asm/<page>.asm` (Glide is the reference target per
   README - NOT D3D; `work/slice*/*.asm` is the D3D dump). Confirm size/pairing
   with `tools/brally/manifest.py <addr>` and `tools/brally/whereis.py <addr>`.
2. Trace every global/callee the body touches to understand role before naming.
   Name globals `g_<hexaddr>` (matches asm annotation + repo convention). Do NOT
   overclaim a role you cannot prove - address names beat guessed names.
3. Faithful linkage: a global written/read from other objects is `extern` in the
   slice header, not `static`.
4. Tag `@implements 0x<glideaddr> glide <Sym>` - tag the build you actually read.
   Many neighbours are tagged d3d only because they were transcribed from the
   D3D dump; glide tags are fine (102 exist) and count directly against the
   Glide coverage target.
5. Behavioural test in `port/tests/test_<slice>.c`, wired into its `main`.
6. `./build.sh && ./tools/regress.sh` (green = 135/135), then
   `.venv/bin/python tools/brally/claimcheck.py` (see [capstone-venv-auditors](../decomp/toolchain/capstone-venv-auditors.md)).
7. Mark the row `done` in render_frontier.csv; commit (no credit trailers).

**State (2026-08-21, main `c78f090`): 8 TODO left** (38 done, 4 n/a, 50 rows  - 
counted straight out of `config/brally/render_frontier.csv`, which is the authority;
the narrative below it lags).

**Prior state (2026-08-18, main `2838aad`): 10 TODO left.**
LANDED `BrModelLightsDraw` (glide 0x10011D20 / d3d twin 0x100147B0, slice2_14.c,
`a02b064`) - the FIRST real frontier draw fn. Builds a Z-up frame at the AI path
root's first point, emits 2 G_MTX cmds (identity BrG_0AA730 + proj slot
g_BrMtxSlot), scales 1/1024, draws the modelLights prop list via BrScenePropsDraw.
Method notes that paid off: (1) the D3D twin is byte-identical - read it to
disambiguate stack aliasing (I miscounted glide pushes; a 3rd `push edx` at
0x10011DF1 puts z-translation at M.m[3][2], giving a clean 0x40-byte BrMat4 M).
(2) slice2_14.c uses a COMMENT-BANNER convention (`/* 0xADDR -- ... */`), NOT
@implements - so it's outside claimcheck/ported.csv; don't try to `manifest
--emit` it. (3) LINK CLOSURE TRAP: slice2_21 (BrVec3Direction) and slice1_05
(BrMat4Mul) are grab-bag objects that drag trig/span/pool/dplay into a
standalone test link - unbounded. Fix per this project's stub philosophy: link
only self-contained cheap modules (br_vec, br_mat) and STUB the heavy-object
helpers faithfully in the test (tests/brally/deps/test_slice2_14.deps = just br_vec br_mat).
g_077284 = -2.0 (z lift +2). g_5BCAEC/g_680944 modelled as `BrPropList
*g_BrModelLights` (slice2_14.c, NULL until the blob loader runs).

### 0x1000BEB0 = BrCarDrawBody - LANDED (2026-08-17, `d297e7e`)
render_frontier 11→10 TODO; 135/135 green; claimcheck 809 clean. In
br_drawcar.c, `@implements 0x1000BEB0 glide BrCarDrawBody`. **It is BOTH the
body DL emitter AND the accumulator writer** - the earlier "note was wrong,
it's NOT the accumulator writer" note was itself wrong. Its glow blocks
`fadd/fstp` g_4B16AC (front glare) and g_4B16A0 (back glare) = THE
BrSceneAccumReset pair. Writer/reset/consumer loop now closed (writer here,
reset slice2_15 0x10017F60, consumers 0x10011FA0).
METHOD THAT WORKED (reusable for the car-draw siblings):
1. Recover structure from the byte-identical Glide body; the D3D twin
   disambiguates the stack-aliasing region (same as BrModelLightsDraw).
2. Combiner words: hand-derive from 0x1002F900's ones-fill shift chain +
   the mux table (CC:0→31 1→6 TEXEL0→1 PRIM→4; AC:0→7 1→6 …). Front combiner
   uses token 1 = G_?CMUX_1 in the d slot. Words: #1 w0=0xFCFFFFFF
   w1=0xFFFF73B9, #2 w0=0xFC121824 w1=0xFF33FFFF (literals in the test).
3. GLOW: transcribe the stack-aliasing arithmetic (0x1000C1C2..0x1000C38A)
   against tools/brally/x87emu.py, NOT by hand. Harness recipe (scratch
   glow_golden.py): dump the fn + the 7 BrVec3 callees + the 7-byte fsqrt
   (0x10002570); drop the fn tail (0x1000C38A..end) and append a sentinel
   (0x1000C38A,'ret',''); set esi=pCar, esp=scratch, edi=0; run from
   0x1000C1C2; read g_4B16A0/AC. **GOTCHA: BrVec3DivBy reads g_0775F0=1.0 as
   its 1.0/s numerator - populate it or every normalize yields 0.** Golden:
   dist 9, threshold cleared 0.05 → 0.05*750/81 = 0.462963074. x87emu only
   models C0 (never C3), fine for non-boundary compares. Consts: g_0771A8=0.0
   (div-guard), g_0771CC=0.95 (threshold), g_0771D0=750.0 (gain).
4. Glare formula: unit dir = camPos-(pos+row0), align vs camera-basis0 and
   car-basis0; front if dir·camBasis0<0 → g_4B16AC += (val-0.95)*750/len²
   with val=-(dot2·dot1); back if >0.95 → g_4B16A0 += (val-0.95)*750/len²
   with val=dot2·dot1. Vb=1.0*row0+0.0*row2 (shipped consts) = row0, emit the
   two calls faithfully anyway.
NEW STATE (br_drawcar.c/.h): g_BrCarMtxSlot[16] (0x102735B0), g_BrCarLightSlot
[16] (0x10273600) - per-car pooled-matrix DL addrs 0x1000A110 fills, 0 until
then; BrG_6C3308 (model scratch, slice2_18), BrG_0AA838/60/68 (canned-DL
proxies, slice2_18). Raw byte-offset record+model access (BR_CAR_OFF_ROW2/
CAMSLOT/MODEL, BR_MODEL_OFF_TEXRECS/BODYDL). TK_ONE=1 token added. Test stubs
BrGfxEmitTexCmd, supplies the slice2_18/19/15 globals locally (deps unchanged:
slice1_05 br_mat br_vec …).
**NEXT (both remaining car-draw siblings, both big): 0x1000A110 (draw ×2,
7577B - WRITES g_BrCarMtxSlot/g_BrCarLightSlot via the pool + the light/lookat
matrices; already MAPPED in br_drawcar.c under the BrCarDrawPlan comment) and
0x1000EAF0 (per-frame car setup, 9354B). After those two the frontier is
essentially the frame driver 0x10011FA0 itself.**

### 0x1000A110 = BrCarDrawVehicle -- LANDED THEN REVERTED (2026-08-18)
`66dbe21` landed it, `2838aad` reverted it. Frontier stays at 10 TODO.
**The transcription is intact in history -- `git show 66dbe21` to recover it;
do NOT re-decode from asm.** Pulled because it was ~75% transcribed with 0%
executed under an `@implements` asserting 100% (no test ever called it; the
+14 test lines were link-satisfying globals only). Full post-mortem and the
re-land checklist: [implements-requires-execution](../decomp/rules/implements-requires-execution.md). What the reverted commit
contained, all still valid and reusable: 7577B, the car-draw main entry: LOD
selection, fog, matrix setup, light colours, culling, render mode, then 4
draw passes (body/underside/glass/detail) + wheel dispatch + model cost.
11 AUDIT FIXES applied: wrong post-lights opcode (BA001402->BA001001),
phantom puts removed (3 between branch/culling, 3 E8 sync pipes that only
belong with B6 clear-geom sequences), ternary guards on unconditional
MOVEMEMs removed (lights 2/3/4 + specular), 3 over-gated draw passes
restructured (underside/glass/detail DL null checks now gate only the final
G_DL emission, not the setup puts), inverted refDL condition (emits when
suppress OR i29B4, not when both clear), incorrect specMem section replaced
with TODO. 3 internal TODO blocks remain: light-dir (0xA5A1-0xA6F3),
specular setup (0xA6F6-0xA81B), post-detail block (0xB925-0xBC7B) -- all
need x87emu arg-capture, NOT eyeballing (a prior attempt guessed wrong and
was reverted). Reflection pass (0xB685-0xB925) also TODO (dead path,
g_BrDrawReflectEnable BSS 0). NEW STATE: 15 globals in br_drawcar.c/h
(g_BrDrawClass, g_BrDrawLights, g_BrDrawModeBase, g_BrDrawSuppress, etc.),
wheel_call bridge fn, s_refIndex/s_refColor static tables. Test deps
unchanged (slice1_05 br_mat br_vec br_seg slice1_03 br_gamestep).

### 0x1000A110 was LEAF-COMPLETE + READY (2026-08-17, `264133f`)
Checked every TODO's callees for leaf-completeness: **0x1000A110 is the ONLY
leaf-complete frontier fn** - all others still have 1-2 unported callees
(0x10011EA0→0x10016830; 0x10011650+0x100119C0→0x10010fb0/0x10011300;
0x10014E00→0x1006a440/80; 0x1006EC30→0x10034fc0/0x100686d0;
0x10017110→0x10008d60/0x100597f0; 0x1000EAF0→7 unported incl. big 0x1000cba0/
0x1000e320). So 0x1000A110 next, unambiguously.
- Its 2 "unported leaves" in the old map (guLookAtReflectF 0x1002A4D0 /
  guLookAtHiliteF 0x1002A200) were STALE - both ported as BrLightDirsFromLookAt
  / BrLightDirsAndAngles (slice2_17.c). Last real leaf 0x100625A0 = BrPool32Alloc
  now tagged (`264133f`). _ftol (0x10074560) = truncate cast.
- SCALE: 7577B, 141 put sites, 48 calls, 70 x87 ops, 91 branches. ~4x
  BrCarDrawBody. NOT a rush job - the map comment itself says the claim was
  withheld precisely because a partial pass is the documented failure.
- The full section map is ALREADY in br_drawcar.c (the "BrCarDrawPlan" comment,
  verified by a prior careful read): signature f(pCar, lod) cdecl; 6 guards;
  DETAIL LEVEL (0x1000A174, dist compares, write !(d>=k) never (d<k));
  FOG (class-2 only, __ftol(alpha*255), G_SETFOGCOLOR rgb 0x106E72F0/86A4/7290);
  MATRICES (Scale(1/255)*car→g_273570 into g_BrCarMtxSlot[iCar]=0x102735B0[i],
  then *g_6E9A38→g_6E78F0 into g_BrCarLightSlot[iCar]=0x10273600[i]); TWO LIGHT
  COLOURS (3 arms, 0x1000A393); LIGHTS (static Lights1 0x100A9FF0/F8 or per-car
  copy w/ __ftol(player m00/01/02*-120)); CULLING (swap cull bits if 0x106EA3F4
  != 0x106E8204); RENDER MODE; 5 DRAW PASSES (body/underside/glass/detail/
  reflection, each gated); SPECULAR (guLookAt + 3 BrPool32Alloc); 1 indirect
  call thru 0x118ED1BC. STACK TRAP: [esp+0x64] aliases lod / cmd-ptr / arg1.
- PLAN for the transcription pass: raw-record access (BR_CAR_OFF_* + new
  offsets); reuse put/put_slot/TK_* + combiner hand-derivation; command-
  stream test over the main path + each gated pass; x87emu golden vectors
  (extend scratch glow_golden.py: load callee listings, set the record/cam/
  consts, run isolated numeric blocks) for the 5 numeric blocks (detail level,
  fog alpha, 2 light colours, light dir, specular). New per-car light-copy arena
  0x102733A0 (24*iCar stride = Ambient8+Light16).

### 0x1000A110 FULL DECODE COMPLETE + all consts pinned (2026-08-17)
Read all 1847 asm lines (scratch glide_0x1000A110.asm); map in br_drawcar.c
confirmed accurate. CONSTS (from BRGlide.dll .rdata, verified): g_077190=255
(fog*255), g_077194=40 / g_077198=80 (detail-level dist thresholds, `!(d>=k)`),
g_07719C=100 (dist<100 flag → esp+0x24), g_0771A0=0.1 (light div dist*0.1),
g_0771A4=1.0 (max clamp), g_0771A8=0.0, g_0771B4=10.0 (hilite threshold, `test
ah,0x41`), g_0771B8=-120.0 (light-dir scale), g_0771C0=0.0 DOUBLE (glass
dot>0 test, `dc1d ...771c0`), g_0771C8=-20.3718 (hilite tile-size from
player+0x2718). Signature f(pCar,lod) - but 0x10011FA0 always passes lod=0, and
lod is IN/OUT at [esp+0x64]. 8 BrRdpSetCombineLERP calls (0xABF8 opaque-setup,
0xAF7E+0xAFD8 body/underside pair, 0xB382 glass, 0xB555 detail, 0xB7C4+0xBA13
reflection pair, 0xBE64 final). 5 gated passes emit model->pModel(g_6EA398)
LOD-record fields at +0x8038(underside DL, at +0x8000+40*lod... actually +eax
where eax=lod*40), +0x8030, +0x8024, +0x803c(reflection), +0x8028; texrecs
+0x8014 (BrGfxEmitTexCmd 5/6/3). Reflection pass (0xB685) heavily gated on
g_B7153C set + g_6ED6B0/B4 clear + g_273304 clear + car+0x29B4 clear + a 2-arm
g_6ED6AC-vs-player test; uses indirect call [0x118ED1BC] (model DL hook, model
+0x80 always + one of +0x84/88/8C/90) and guLookAt hilite→G_SETTILESIZE. Tail:
2nd body emit (+0x8028), pop, wheels 0x10009C10 (LATE for non-class2, EARLY at
0xACCA for class2), final combiner, model cost +0x8000 added to g_6E86AC.
Nothing left to DECODE - it is a pure transcription+verify job now.

### 0x1000A110 SESSION HANDOFF - everything the next session needs (2026-08-17, pushed `264133f`)
DID NOT LAND 0x1000A110: I have the whole thing decoded + all artifacts below,
but the light-dir/specular/reflection blocks are interleaved x87+stack that I
started to GUESS (and got the light-dir branch WRONG) - reverted rather than
ship guesses. The reusable artifacts (regenerate the scratch tools from
these; scratch does NOT persist):

**8 COMBINER WORDS** (extracted via combiners.py: grab 17 pushes before each
`call 0x1001cf90`, feed slice1_05.c's mux algo). Two cross-validate (proof the
method is right): @ABF8 == BrCarDrawWheels plain, @B382/@BE64 == BrCarDrawBody #2.
- @ABF8 opaque-setup : w0=FC127FFF w1=FFFFF238  {TEXEL0,0,PRIM,0/0,0,0,TEXEL0/0,0,0,COMBINED/0,0,0,COMBINED}
- @AF7E body/under #1: FC127FFF / FFFFF238  (same tokens)
- @AFD8 body/under #2: FC1219FF / FFFFFE38  {TEXEL0,0,PRIM,0/TEXEL0,0,PRIM,0/0,0,0,COMBINED/0,0,0,COMBINED}
- @B382 glass        : FC121824 / FF33FFFF  {TEXEL0,0,PRIM,0}x4
- @B555 detail       : FC127FFF / FFFFF838  {TEXEL0,0,PRIM,0/0,0,0,PRIM/0,0,0,COMBINED/0,0,0,COMBINED}
- @B7C4 reflection#1 : FCFFFFFF / FFFDF2F9  {0,0,0,TEXEL1_A/0,0,0,TEXEL0/0,0,0,TEXEL1_A/0,0,0,TEXEL0}
- @BA13 reflection#2 : FC167E2C / 55FEF379  {TEXEL0,SHADE,0x3F4,SHADE/0,0,0,TEXEL0/(same)/0,0,0,TEXEL0}
- @BE64 final body   : FC121824 / FF33FFFF  {TEXEL0,0,PRIM,0}x4

**GLOBAL RESOLUTION (caught 11 aliasing hazards - MUST reuse these, not coin):**
glide→existing d3d model: 0x106E72F0=BrG_6C0260(fogR), 0x106E86A4=BrG_6C1614(fogG),
0x106E7290=BrG_6C0200(fogB), 0x106E8610=BrG_6C1580, 0x106E79F8=BrG_6C0968,
0x106E79F0=BrG_6C0960, 0x106ED64C=BrG_6C65BC, 0x106EA3EC=BrG_6C335C (light bytes),
0x106E8204=BrG_6C1174 (cull ref), 0x106E7700/04/08=BrG_6C0670 (BrVec3), 0x106ED6A8=
BrG_6C6618, 0x106ED6A4=BrG_6C6614, 0x106E86AC=BrG_6C161C(model-cost, ADD to slice2_18),
0x100A9F00=BrG_0AA770 (reflect alt DL), 0x100A9FC8=BrG_0AA838, 0x100A9FF8/F0=BrG_0AA868/60.
Race globals: 0x106EA3F4=g_brRaceBeginDifficulty, 0x100AA044=g_brRaceBeginNTexSet,
0x100A9360=g_brCfgGameMode, 0x100ABAA0=BrBootGlobal_ABAA0.
NEW to coin (glide-addr, br_drawcar; not modelled anywhere): 0x10273640 modeBase,
0x10273304 suppress, 0x106ED6C0 lodFloor, 0x10273390 dir0(BrVec3), 0x10273560
dir1(BrVec3), 0x10273688 refIndex, 0x10273520 class[16], 0x102733A0 lights[24*16],
0x10B7153C reflectEnable(BSS 0), 0x100A5B40 reflectFlag(=1), 0x1184C474/480
reflectColorA/B(BSS), 0x118ED1BC modelDlHook(fn ptr,BSS), 0x106B7C80/78 two colour
bytes (no twin found), 0x106EED38 trackFlags(ptr to 84-byte gate recs), 0x100A5C88
texBlob. STATIC tables (read from image): s_refIndex[16]={3,3,0,1,2,2,0,0,12,0..},
s_refColor[8]={0x118EDA10,0x118ED210,0x118ED210,0x100B98C0,0x118ED1F0,0x118EE210,
0x118EE210,0x100BA0C8}.

**VERIFIED LOGIC (transcribe directly, ~75%):** guards (5 ptrs +0xF08/168/170/16C/
174, then g_BrCarVisAny[iCar]); detail lod (NTexSet==2 ? (!(d>=40)?0:!(d>=80)?1:2)
raised to lodFloor : lodFloor; +=bias; clamp<=2); distNear=!(d>=100); fog(class2:
FogAlpha=ftol(f29B0*255), F8000000 payload=(6C0260<<24|6C1614<<16|6C0200<<8|
alpha)); +0x290C gate (car+0x294C!=0 && trackFlags[k*84+0x4C]&0x10 -> flag290C,
car+0x2714); matrices (Scale(1/255)*car->World into pModelMtx/CarMtxSlot; World*
View->Combined into pCombMtx/CarLightSlot; keep REAL ptr for BrGuMtxStore, emit
truncated); player guard (==6C2CF8 && (ac==+0x273C||+0x2890) && 6C6614==0 ->ret);
class[iCar]=lod; 3-arm light colours (arm1 6C661C!=0: colourA=(6C1580,6C335C,
6C0968)/max(d*0.1,1) packed <<24/16/8, colourB=(byte80,6C0960,6C65BC) RAW; arm2
flag290C: A=0,B=(those*4/5 via 0x66666667); arm3: A=(6C1580,6C335C,6C0968) raw,
B=(byte80,6C0960,6C65BC) raw); header G_MTX x2; lights (both 6C661C&6C6624 clear:
static Lights1 BrG_0AA868/60; else copy 24B into lights[24*iCar], dir=ftol(player
m00/01/02*-120)); cull (swap 0x1000/0x2000 if difficulty!=6C1174); render mode.
Then 4 non-reflection passes + tail (all put-idiom, LOD fields model+lod*40+
0x8024/28/30/38/3c, texcmd 5/6/3). VERIFY: command-stream test + x87emu golden
for the numeric BYTES (fog/light-colour/light-dir).

**REMAINING (the hard 25%, do with x87emu - do NOT eyeball):**
1. light-dir 0xA5A1-0xA6F3: BrVec3 ops (Negate 0x10034340, Sub, Length, DivBy,
   Midpoint 0x100346d0, br_dl_normalise 0x100344d0) building g_273390/560. My
   guess was WRONG (branches negate cam/player not car). x87emu it (glow harness).
2. specular 0xA6F6-0xA81B: capture the exact 11+20 args to guLookAtReflectF
   (0x1002a4d0=BrLightDirsFromLookAt) / guLookAtHiliteF (0x1002a200=
   BrLightDirsAndAngles) by stubbing the calls in x87emu and reading the pushed
   stack. 4 pool allocs first (1 discarded, pool16 hilite, 2x pool32).
3. reflection pass 0xB685-0xB925: gated (6C6618... g_B7153C set + 6ED6B0/B4 clear
   + suppress clear + car+0x29B4 clear + 2-arm 6C661C-vs-player). Integer + the
   palette tables + indirect call [modelDlHook](model+0x80, one of +0x84/88/8C/90)
   + hilite->G_SETTILESIZE from [esp+0x2c]. Cold path (reflectEnable BSS 0).
Naming used: BrCarDrawVehicle(void*, int32_t lodBias); refl_color helper.

### 0x1000BEB0 original scope (kept for reference) - FULLY SCOPED (2026-08-17)
The per-car body DL emitter (D3D twin 0x1000E950, 1576B). Next on the critical
path to a visible car; gates on g_BrCarVisOpaque[iCar] (the flag 0x10009FC0
sets). ALL 9 callees ported: BrRdpSetCombineLERP(0x1001cf90), BrGfxEmitTexCmd
(0x1002ab32), BrVec3 Dot/Scale/DivBy/Sub/Add/MulAddTo/Len (0x10034310/360/3f0/
560/5c0/6a0/7f0). Home: br_drawcar.c, @implements glide 0x1000BEB0, reuse the
file's put/put_slot/TK_* combiner idioms (BrCarDrawWheels is the template).

STRUCTURE (verified by reading the full D3D twin):
- Guards: return unless (g_6C661C||g_6C6624); pCar=[esp+0x28]; return if
  g_BrCarVisOpaque[pCar->iCar]==0; if pCar==g_6C2CF8(player) && g_6C6490==
  pCar+0x27c4 return; if pCar->bKind(+0x29af)==2 return.
- g_6C3308 = pCar->pModel(+0x29c4). Then ~30 put header cmds: G_MTX
  {0x01060040, g_BrCarMtxSlot[iCar]} + {0x01030040, g_BrMtxSlot}; 4×G_MOVEMEM
  {0x039e/98/9a/9c 0010, g_BrCarLightRec[iCar](+0,+0x10,+0x20,+0x30)}; G_DL
  {0x06000000, BrG_0AA838}; BrGfxEmitTexCmd(5, pModel+0x8014); E7 sync;
  BA001402; moveword BC00000A/040A/200A(0xffffff00)/240A(0xffffff00);
  BrRdpSetCombineLERP #1; B900031D {0x4049d8}; BA000602 {0x80}; then IF
  pModel[+0x802c]!=0 emit G_DL {0x06000000, pModel[+0x802c]} (the BODY geom);
  E7 sync; BA000602 {BrG_6C0688}.
- GLOW MATH (0x1000EC55-0x1000EE2A) - HIGHEST RISK, VERIFY WITH x87emu: two
  mirror blocks, gated pCar!=player, accumulate headlight glow into g_575504
  (0x10575504) and g_5754F8 (0x105754F8). Uses BrVec3 Add/Sub/Len/DivBy/Dot/
  Scale/MulAddTo on pos(+0x30), basis(+0x00), headlight(+0x20), camera
  g_6C6490+0x30. Consts: g_08F1F8=0.0, g_08F21C=0.95, g_08F220=750.0. HAS THE
  STACK-ALIASING TRAP the file warns of ([esp+N] names different locals as esp
  shifts) - do NOT hand-derive; drive it through tools/brally/x87emu.py.
- Tail (~15 put): E7; BA001402; BD000000(popmtx); B6000000{0x40000};
  BC000002{0x80000040}; G_MOVEMEM{0x03860010,BrG_0AA868}; {0x03880010,
  BrG_0AA860}; BA000c02{BrG_6C0258}; BA000e02{0}; BrRdpSetCombineLERP #2.
- Combiner #1 tokens (extracted, arg order pOut,a0,b0,c0,d0,Aa0..Ad0,a1..Ad1):
  {_, 0,0,0,1, 0,0,0,TK_TEXEL0(0x3e9), 0,0,0,1, 0,0,0,0x3e9}. Combiner #2 still
  to extract from the pushes before 0x1000EF6A.

NEW STATE to declare (bounds recoverable like the vis arrays): car offsets
+0x20 HEAD, +0x29c4 MODEL; model +0x8014 TEXRECS, +0x802c BODYDL; g_6C3308
(model scratch ptr), g_BrCarMtxSlot[16]/g_BrCarLightRec[16] (uint32, per-car DL
addrs, 0-until-setup), BrG_0AA838/60/68 (canned-DL proxies like BrG_0AA730),
g_575504/g_5754F8 (float glow accumulators). Header decls were drafted then
reverted (kept tree clean at `896ea41`) - this is a big atomic function, land
it with the command-stream test (à la test_plain_pass's 68-cmd check) AND
x87emu on the glow blocks, in fresh context. Did NOT rush it (no-shortcuts).

### 0x10009FC0 LANDED - car-draw subsystem OPENED (2026-08-17, `896ea41`)
BrCarVisibilityUpdate is in br_drawcar.c; render_frontier 12→11 TODO; 135/135
green; claimcheck 808 clean. ARCHITECTURE DECISION MADE + recorded in
br_drawcar.h: RAW 0x2B68-record access (BR_CAR_OFF_* offsets) for write-back
functions, keep BrCarView repack for read-only draw. Stood up the recovered
shared state: g_BrCarVisOpaque/g_BrCarVisAny (int32[16]), g_BrFrameHull (global
BrSpanVolume, empty until the unported frame-setup builds it - faithful/inert),
BrG_6C6614 added to slice2_18.h. Verified CFG + all 3 span-arg mappings against
the byte-identical D3D twin 0x1000CA90 (the technique that de-risks these).
Test-link lesson (again): stub the grab-bag callees (BrFogFactorAtPoint,
BrSpanTestPoint), keep br_vec real to check the probe math; define slice2_18
globals test-local. **The subsystem is now OPEN - the siblings 0x1000BEB0
(emit), 0x1000A110 (draw ×2 7577B), 0x1000EAF0 (setup) reuse this same
state/convention.** Original decode retained below.

### 0x10009FC0 FULLY DECODED - it's the car VISIBILITY/CULL pass; gateway to the deferred car-draw subsystem (2026-08-17)
Investigated as the next frontier target (322B, smallest real one left). Decode:
per-car pass, arg esi=pCar (the 0x2B68 car record). Guard pCar+0xF08. Computes
pCar+0x2730 = BrFogFactorAtPoint(&pCar+0x30) [the car world pos = mtx.m[3]].
Zeros two per-car visibility flag arrays g_arrA[0x10273648] and g_arrB[0x10273350]
at [pCar->iCar(+0x140)]. Then: if pCar==g_6E9D88 (player car) OR on-screen
(span tests) → sets the flags per bKind(+0x29AF): bKind!=2 sets BOTH arrA+arrB=1,
bKind==2 sets only arrB=1 (arrA=opaque pass, arrB=translucent pass - feeds the
0x1000A110 draw fn called twice). Off-screen → both stay 0 (culled). Player-car
branch checks the f2734 self-link (+0x2734 == pCar+0x273c or +0x2890) and g_6ED6A4.
The g_6ED6AC/g_6ED6B4 mode flags force the span-test path (a fwd-projected point
pos+6.0*mtx.m[0] via BrVec3MulAdd). There's a DEAD BrVec3Dist(pCar+0x30,
g_6ED520+0x30) whose result is discarded.

**ALL callees ARE ported**: BrVec3Dist, BrFogFactorAtPoint (=glide 0x1002B3F0 /
d3d 0x10031D3F, already impl in slice2_18.c - NOT a gfx-emit, the earlier note was
wrong), BrSpanTestPoint, BrVec3MulAdd. Array bounds RECOVERED (not guessed): both
are 16-entry int32 arrays - the next global sits exactly +0x40 after each base,
matching BR_CARDATA_CARS=16.

**WHY IT'S NOT A CLEAN WIN - it opens the deferred car-draw subsystem** (the one
br_drawcar.h + slice2_14.c explicitly declined to "invent a state object" for). It
needs 4 pieces of NEW shared state, none in the port yet:
1. A car-record byte-offset access convention (BrCarView in br_drawcar.h is a
   DOC view, not a memory-accurate overlay - original reaches fields by raw
   offset; no ported car fn exists to copy an idiom from).
2. The two 16-entry visibility flag arrays (g_arrA 0x10273648, g_arrB 0x10273350)
   - need C storage + a home.
3. Mode/ref globals: g_6ED6A4/AC/B4 (int flags), g_6E9D88 (player car ptr),
   g_6ED520 (ref/camera ptr, its +0x30 = a pos).
4. A GLOBAL "current-frame span volume" - the port's span API (BrSpanTestPoint/
   BrSpanBuildHull) is ALL EXPLICIT-POINTER (transcribed from D3D), but glide
   0x10033FD0 is a 2-arg (x,y) variant using an IMPLICIT global volume (via glide
   0x10033F90 / d3d 0x1003A910). A real glide-vs-d3d API split. Porting the glide
   fn needs that global volume identified + wired.
So the honest state: the frontier's clean wins are drained. The next real unlock
is STANDING UP the car-draw subsystem state (items 1-4), which then unblocks
0x10009FC0 AND its big siblings 0x1000A110 (7577B) / 0x10009C10 / 0x1000EAF0.
This IS "the render-state model, car half."

### DEFINITIVE: the entire remaining frontier IS the car-draw subsystem (2026-08-17)
Probed the other "non-car" candidates; they're car functions too:
- 0x1000BEB0 is NOT the "accumulator writer" (the earlier note was wrong) - it's a per-car
  DL EMITTER: esi=pCar, reads +0x29af/+0x29c4, uses g_6ED6AC/B4, g_6E9D88,
  g_6ED520, g_6EA360(proj slot), heavy g_6E7710 cursor, 17 calls (0x10034xxx gfx).
- 0x10011EA0 is the FPS readout (not an entity loop); drags the text subsystem.
So 0x10009FC0 (cull) + 0x1000BEB0 (emit) + 0x1000A110 (draw ×2) + 0x10009C10 +
0x1000EAF0 (setup) are ALL the car-draw subsystem. THERE IS NO CLEAN NON-CAR
FRONTIER FUNCTION LEFT. **WARNING: the frontier annotations in this file's older
"Remaining TODO" list are UNRELIABLE - 3 confirmed misreads (0x10011EA0,
0x1002B3F0, 0x1000BEB0). Re-decode before trusting any of them.**

### THE architecture decision the subsystem hinges on (get this right FIRST)
br_drawcar.c operates on a REPACKED compact BrCarView (uses pCar->bKind etc. as
struct members, but BrCarView is byte-accurate ONLY through +0x140 - it's a
translation target, and the raw-record->BrCarView repack shim is NOT ported).
BUT 0x10009FC0 WRITES car state (fog -> car+0x2730) and shared flag arrays, and
0x1000BEB0 writes gfx state - write-back that a read-only repack CANNOT model. So
the subsystem forces a choice: (a) RAW 0x2B68-record access via an offset header
(faithful, handles write-back, but a second pattern alongside BrCarView), or
(b) make BrCarView a LIVE byte-accurate overlay of the raw record (one pattern,
but a big retype of the existing repack code). Recommend (a): raw-record offset
access for the cull/emit/setup functions; keep BrCarView for the read-only
wheel/model draw. This decision, made once, unblocks the whole cluster.

### Why 0x10009FC0 wasn't rush-ported this session
Its 6-exit CFG + the span-test arg mapping depend on exact x87 stack offsets that
need x87emu-level verification (the glide 2-arg BrSpanTestPoint reads through
internal push-shifted [esp+N] - hand-derivation is error-prone). Shipping it
unverified would violate recover-don't-guess. NEXT SESSION: (1) make the arch
decision (a), (2) create the car-record offset header + declare the recovered
state (2×int32[16] flag arrays g_arrA 0x10273648/g_arrB 0x10273350, g_6ED6A4/AC/B4,
g_6E9D88, g_6ED520, and g_BrFrameHull = a global BrSpanVolume at glide 0x10AC2C54
/ d3d 0x10A9BBC4 whose grid the glide 2-arg span test reads), (3) port 0x10009FC0
verifying against x87emu, then the siblings.

**Prior state (main `bfc472e`): 13 TODO left.**
DONE this session: `BrSceneAccumReset` (0x10017F60, slice2_15.c) - zeroes the
two per-frame accumulators g_4B16A0/g_4B16AC at frame top; called between
BrSceneSetupFrame and BrSpanBuildHull. Its WRITER is 0x1000BEB0 (still TODO) and
its consumers live inside 0x10011FA0; the pair is read back as
(g_4B16AC - g_4B16A0*k) and (g_4B16A0 + g_4B16AC), each an alpha to
BrFadeDrawSprite (0x10017F80). Geometric meaning not yet pinned.

Remaining TODO, easiest-first: 0x10011EA0 (253B, loops g_B73538 - entity array,
feeds A), 0x10009FC0 (322B, passed struct ptr), 0x10011D20 (383B, early-out on
g_5CCB78), 0x100119C0 (837B), 0x10011650 (847B), 0x10014E00 (1007B), 0x1006EC30
(1331B), 0x1000BEB0 (1576B, accumulator writer), 0x10017110 (2039B), 0x1000A110
(7577B), 0x1000EAF0 (9354B). Two UNALIGNED starts need boundary verification
first: 0x1002C50E (1585B), 0x1002CB49 (744B).

## FRONTIER IS NOW GATED ON THE RENDER-STATE MODEL (found 2026-08-17)

After draining the pure-math deps, EVERY remaining first-level frontier function
needs either unmodelled shared scene/render state or its unported frontier
siblings. Verified:
- 0x10011D20 (FULLY DECODED, see below) needs g_0A9EC0 (shared DL blob, in
  config/brally/globals_shared.csv, referenced as a cmd payload by ~7 render fns),
  g_6EED48 (scene-object ptr), g_5BCAEC (props ctx), g_6EA360, g_5BC764
  (RUNTIME global, set elsewhere - not a const). Only g_6E78F0 already exists
  (br_mat.c scratch matrix).
- 0x100119C0 is 50x gfx-emit (g_6E7710 cursor, which the port abstracts via
  BrGfxAlloc/BrGfxEmit) AND calls 0x10010FB0 + 0x10011300, themselves frontier
  TODOs. So it's mid-tree, not a leaf.
So the NEXT REAL UNLOCK is direction A: model the shared render/scene state
(recurring globals g_6E7710 [gfx cursor, already abstracted], g_0A9EC0 [shared
DL], g_6EED48/g_5BCAEC [scene objects/props ctx], plus the entity struct from
[scene-entity-model](scene-entity-model.md)). B has proven A is the gate. Do NOT force low-fidelity
placeholder-global ports of the draw functions.

### 0x10011D20 decoded (port once render-state exists)
Guard: return unless g_5CCB78 (enable) AND g_6EED48 (object ptr) non-null.
Builds a Z-up orientation matrix M (3x4 in a 0x40 stack frame) for the object:
  row1(fwd) = BrVec3Direction(obj+0x4c, obj+0x58)   [two vec3s in the object]
  row2(up)  = (0,0,1)
  row0(right)= row1 x row2   (BrVec3Cross 0x100342B0)
  row1(fwd) = row2 x row0    (BrVec3Cross again - re-orthogonalise)
  row3(translation) = (obj->f4c, obj->f50, obj->f54 + g_5BC764 - g_077284),
     then -= 0.3*right (BrVec3MulAdd 0x10034660, s=-0.3=0xbe99999a),
     then -= 0.6*fwd   (BrVec3MulAdd, s=-0.6=0xbf19999a).
Emits 2 gfx cmds via g_6E7710 cursor: {0x1060040, g_0A9EC0}, {0x1030040,
g_6EA360} (use BrGfxAlloc in the port). Then BrMat4Scale(g_6E78F0, s,s,s) with
s=0x3a800000=1/1024; BrMat4Mul(g_6E78F0, M, M) → M = scale*M (NOTE BrMat4Mul's
OUTPUT is arg3); BrScenePropsDraw(g_5BCAEC, &M) [0x1001D1B0]. Net: draws a
1/1024-scaled oriented prop/marker at the object, offset back-and-left. g_077284
value still TBD (a .rdata const, fsub'd once).

### Render-state model - RESOLVED: the 0x106EED00 block IS the BrTrack header (2026-08-17, `cb27800`)
The "dense scene-state block ~0x106EED00" was a MISREAD. There is no absolute
store to 0x106EED48 (or any 0x106EEDxx) because the whole run g_6EECD8..g_6EEE38
is the resident .TRK HEADER BUFFER at glide 0x106EECD8 (d3d 0x106C7C48),
bulk-loaded/swapped/relocated in one pass by BrTrackLoad 0x100311C0 (pushes the
base 0x106EECD8 to swapper 0x10031B80 at 0x10031275; 4,000,000-byte size guard
at 0x10031282). A read of absolute [0x106EEDxx] = abHdr[0x106EEDxx-0x106EECD8].
So NO new model to build - it's already in br_track.h (0x230-byte BrTrack), and
the AI/race half is named:
- g_6EED48 = header +0x70 = BR_TRK_H_AIPATH (br_ai.h) - the AI path ring root
  (the "scene-object ptr" 0x10011D20 reads). Its target +0x64 = lap length
  (br_race.h pfLapLength). +0x74 = BR_TRK_H_AIPATH2.
- g_6EED70 = +0x98 = BR_TRK_H_GATES (stride 0x14); g_6EEE38 = +0x160 =
  BR_TRK_H_CGATES. BrRaceRules (br_race.h) gathers these; BrRaceGateStep
  (0x1005FF00) already ported. Ring buffer usage: g_B1CA20 mod g_6EEE38.
Recorded in br_track.h's "WHERE THIS BUFFER LIVES AT RUNTIME" note (`cb27800`).

### THE REAL render-state gaps for 0x10011D20 (not the BrTrack header)
Having resolved g_6EED48, 0x10011D20's still-unmodelled deps are GFX/DL globals:
- g_0A9EC0 (0x100A9EC0) - a STATIC display-list/material payload blob; its
  ADDRESS is written into gfx cmd payload slots (`mov [eax+4],0x100a9ec0`) at
  ~7 emit sites (1000ECCD/1000F133/1000F869/100103D1/10010F1B/1002BDE2/10011E32).
  Model as a .data DL-payload const the port emits via BrGfxAlloc.
- g_6EA360 (0x106EA360) - a MUTABLE gfx-context ptr (writer at 0x1002D615/
  0x1002D6E7 in 0x1002Dxxx; read by many draw fns). The 2nd cmd payload
  {0x1030040, g_6EA360}. Model as a render-state variable.
- g_5BC764 = g_brRaceFade (br_racestep.h) - DONE.
- g_5BCAEC - props draw ctx, seeded at 0x1001AA66 with 0x105BCAF8 (dest);
  partly in br_racebegin.c. Feeds BrScenePropsDraw (0x1001D1B0).
NEXT: model g_0A9EC0 + g_6EA360 (the gfx cmd-payload pair) to unblock 0x10011D20.

### PAIR RESOLVED - both were already modelled; 0x10011D20 is now unblocked (2026-08-17, `6036545`)
The cmd-payload pair was NOT unmodelled - the port has them under their D3D
twins (I'd grepped the glide hex and missed them):
- g_0A9EC0 (glide) = d3d 0x100AA730 = **BrG_0AA730** (slice2_18.h) - a `const
  BrMat4` FLOAT IDENTITY (verified the .data bytes: 1.0 diagonal). Emitted as
  G_MTX 0x01060040 (PUSH|LOAD modelview), the base modelview. NB the PC
  renderer keeps float BrMat4 in the DL; no N64 s15.16 packing (RSP never sees).
- g_6EA360 (glide) = d3d 0x106C32D0 = **g_BrMtxSlot** (slice2_19.h) - a
  `BrMat4*` into the frame matrix pool = the current PROJECTION matrix. Set by
  BrCamMatrixSetup/Fixed (d3d 0x10033E83/0x10033F7E = glide 0x1002D534/
  0x1002D62F, still shared-unported) via BrPoolAlloc (glide 0x10062500, the
  64-byte-slot allocator: pool g_B1CF20, 256/bank, bank g_6ED67C, count
  g_B24FA0). Emitted as G_MTX 0x01030040 (LOAD projection).
Added glide xrefs to both decls (`6036545`).

**BIGGER FINDING: 0x10011D20 (glide) / 0x100147B0 (d3d) is now PORTABLE.** The
slice2_14.c note that called it blocked on 0x1003ADA0's "unknown contract" is
STALE - 0x1003ADA0 is ported as BrVec3Direction (slice2_21.c:39). Every callee
now maps to a ported d3d twin: BrVec3Direction, BrVec3Cross, BrVec3MulAdd,
BrMat4Scale, BrMat4Mul (0x100306C0), BrScenePropsDraw (0x1002FB20). Payloads
BrG_0AA730 + g_BrMtxSlot ✓. Guards: g_6EED48=BR_TRK_H_AIPATH ✓, g_5CCB78
(enable). Remaining small unknowns only: g_5BCAEC = the BrPropList arg to
BrScenePropsDraw (seeded 0x1001AA66 with 0x105BCAF8, in br_racebegin.c - needs
its instance/type wired), g_5BC764=g_brRaceFade ✓, and g_077284 (.rdata const,
fsub'd once, TBD). **NEXT: transcribe 0x10011D20 into a draw slice** (decode in
the "0x10011D20 decoded" section above) - it draws a 1/1024-scaled oriented
prop/marker at the path object. First real frontier draw fn to land.
