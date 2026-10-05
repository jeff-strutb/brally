# Rec2 corpus blocked on exe

*Recorded 2026-09-14.*

> External ext (ext) idiom corpus - blocked at step 2; disc EXE is a SafeDisc stub, ext targets a different (decrypted/patched) CARMA2_HW.EXE

**State 2026-09-13:** Plan = build an external proven-idiom corpus from
madebr/ext (C2, MSVC 5 SP3, reccmp byte-matching), wire into tools/corpus.py
as `--corpus ext`, query our x87/register-order walls (0x1006DD20,
0x10029D70, 0x1006D530, 0x1002A050; byte-widen; xor-beside-zero;
and-eax-0xff-before-mov-dh-al). Full step list is in the 2026-09-13 session
transcript.

**Done:** `tools/cdrip.py` (committed 83c4350) converts the raw bin track to
ISO; disc is all Mode 1. EXEs extracted to `build/external/c2/` (git-ignored,
never commit). ext cloned at `build/external/ext` (no license - read/index
only, never copy into src/).

**Blocker:** the retail US disc's `CARMA2_HW.EXE` (159,744 B,
sha256 7b4d356f…) is a C-Dilla/SafeDisc v1 loader stub (`BoG_` signature);
the real game EXE is the encrypted `CARMA2_HW.icd` (2,676,736 B). ext
targets a plain `CARMA2_HW.EXE` sha256 9b896c2c… . The disc's launcher
`carma2.exe` and `d3d.bdd` ALSO mismatch ext's other two target hashes, so
ext likely targets the v1.02-patched installed game, not retail-1.0 disc
files. SafeDisc decryption = DRM circumvention - not done.

**2026-09-13 update:** project lead supplied the official 1.02 patch at
`reference/c2/c2ptch12/` - it is ALSO SafeDisc-wrapped (stub EXE 205,312 B
sha256 1c612b6b… + encrypted .icd 2,679,808 B). The patch does NOT unblock.
ext's target must be a DRM-free re-release binary - GOG ships C2 at 1.02
with no SafeDisc; that is the likely source of sha256 9b896c2c… .

**Resume:** project lead supplies a `CARMA2_HW.EXE` with sha256
9b896c2cbb170c01b3e9f904ce5e1808db29fe5b51184a5d55a6d19b1799b58d (GOG or
other DRM-free re-release) into
`build/external/c2/`, then continue at step 3: ext matching build with our
`tools/msvc5` cl.exe + period DirectX SDK (archive.org, stage under tools/),
reccmp report, keep 100% rows, `build/match/corpus_rec2.csv`, index as
opt-in corpus (rule 0: refcheck must stay Glide-keyed).
