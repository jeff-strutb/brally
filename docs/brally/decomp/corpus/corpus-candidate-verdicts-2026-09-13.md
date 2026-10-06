# Corpus candidate verdicts

*Recorded 2026-09-13.*

> SECOND external corpus (--corpus ext2, ext2 leak, VC5) LIVE: 88/1515 byte-exact incl. x87 span generators; rebuild + query commands inside; staging under build/brally/analysis/corpus/ext2. Plus 2026-09-13 candidate REJECTIONS: devilution (VC6), frogger-psx (GCC/MIPS), Frogger 2 (VC6-era), Army Men (VC4.2, anomaly-TU niche). Filter: same compiler as reference + per-function byte-certifiable.

**The filter (proved by [ext-corpus-2026-09-13](ext-corpus-2026-09-13.md)):** a corpus pays only when it is
(a) the SAME compiler as the reference binary (VC5 for BRGlide, IDO for `n64/`)
and (b) per-function byte-exact certifiable under OUR toolchain. Vendor-source
leaks beat reconstructed decomps on (b) but still die on (a).

Checked 2026-09-13, all REJECTED for the main lane:

- **diasurgical/devilution** - Diablo 1.09b is VC6 SP5+PP; functionally
  accurate, comparer tolerates divergence. Both properties fail.
- **HighwayFrogs/frogger-psx** - byte-exact but PSY-Q GCC 2.6/MIPS. Not VC5,
  and not IDO either, so useless for `n64/` too.
- **Frogger 2 dev archive (hiddenpalace)** - PC final Sept 2000 = VC6-era.
  N64 material is IDO/KMC but `n64/`'s blocker is string pairing, not idioms.
- **Army Men 1998 leak (archive.org `armymen_202209`, 338 MB RAR)**  - 
  **VC 4.2**: both shipped EXEs in `src/brally/RunTime/` are PE linker 4.20
  (built 1998-04-18), makefile is DevStudio 4.20 with `/Gr /MD /W3 /GX /Zi /O2`.
  Rejected for the main lane. **Niche option:** vendor source + shipped /O2
  EXE + debug EXE with .pdb = a ready same-compiler corpus for our single
  VC4.2 anomaly TU ([vc42-is-the-real-compiler](../toolchain/vc42-is-the-real-compiler.md)) IF that TU still has open
  rows. Archive was downloaded, verified, and deleted; bsdtar reads the RAR
  (v4) directly, no unrar needed.

** BUILT - the ext2 corpus is LIVE as `--corpus ext2`** (corpus.py c926c18;
builder build/brally/analysis/corpus/ext2corpus.py, git-ignored). **RULE: the game's
name NEVER goes in the repo - in-tree it is only "ext2"**, same secrecy as
C2/ext. Staging: build/brally/analysis/corpus/ext2/{src, ext2.exe, tomb21->src symlink};
retail ext2.exe extracted from the project lead's PC disc rip at reference/ext2
(MODE1 bin/cue -> ISO -> InstallShield5 data1.cab; IS5 cab = file table at
cab_descriptor+0xC, data_offset absolute, ONE raw-deflate stream per file,
NO chunk framing). EXE: 870,400 B, linker 5.2, built 1997-10-31, sha256
aba30d2f… **Self-locating scorer** (no VA annotations): longest fixed run
≥8 B anchors a search of .text, all non-reloc fragments verify, unique hit =
address+bytes proof; min 24 proof bytes, tail guard. **First sweep: 28/630
byte-exact** (8 from the 3D span-generator TUs incl. a 644 B x87 perspective
generator - our wall's domain; 20 game logic), 334 nolocate = SOURCE DRIFT
(tree is 1998-2000 vintage vs 1997-10-31 EXE; BACKUP/ = May-1998, game/ =
TR3-era). **DX5-UNLOCKED FINAL: 88/1515 byte-exact, 4070 instructions
indexed** - DX5 SDK headers fetched by HTTP-range ISO9660 walk of archive.org
`ms-dx5-sdk` (no 278 MB download) into build/brally/analysis/corpus/ext2/dx5inc, passed as
the FIRST -I so they shadow the compiler's DX3-era copies. Opened the whole
specific/ renderer lane (35 matches incl. 730 B do_detail_option) and 13 in
3dsystem (all four xgen_* span generators, VAs 0x00402b80-0x00402fd0).
Remaining: 99 compile_fail (BACKUP-era header clash), 988 nolocate (drift).
`show --corpus ext2` resolves mangled C++ members fine.  Do NOT add -I
dirs onto the tree itself: the project's own time.h shadows CRT <time.h>
(quoted includes resolve includer-relative, exactly like the original
build). All four parked x87 rows MISS in ext2 at every window probed.

**Use / rebuild (everything .venv/bin/python, from repo root):**
```
tools/brally/corpus.py find --corpus ext2 --from <VA> --at <off> --len 12 --source
tools/brally/corpus.py show --corpus ext2 --va 0x004xxxxx --at 0xOFF
build/brally/analysis/corpus/ext2corpus.py --jobs 8      # re-score (also --only <tu.c>)
tools/brally/corpus.py build --corpus ext2        # re-index after a re-score
```
Same gotcha as ext: a 2-token pattern needs `--exact --min-len 2`. If the
git-ignored staging is ever lost: source 7z re-extracts with bsdtar (item in
the find note below), ext2.exe re-extracts from the disc rip at
reference/ext2 by the cab recipe above, DX5 headers re-fetch by the
HTTP-range walk of `ms-dx5-sdk` SDK/INC.

Original find note (2026-09-13): ext2 source leak (archive.org
`tomb-raider-ii-core-design-eidos-1997-source-code.-7z`, 15 MB 7z, bsdtar
reads it): VC5.** TOMBRAID.DSP is DevStudio Format Version 5.00, vc50.pdb
debris in tree, release flags `/G5 /Zp1 /W3 /Zi /O2` (one config `/Gr /O2
/Op` - our re-triage variant). In-tree gameflow.exe Release = PE linker 5.2,
built 1997-11-04 (near-final; retail 1997-11-21). 411 C/CPP files incl.
`3DSYSTEM/` sw+hw renderers = x87-heavy 3D math with KNOWN SOURCE - candidate
second laboratory for the x87 operand-role wall. **Missing: retail PC
ext2.exe** (archive has only gameflow + PSX exe) - project lead must supply, e.g.
GOG TR 1+2+3 installer via the C2-round Inno extractor. Verify retail EXE
linker version + source-vs-retail drift before building the recipe.

If hunting again: want VC5-built byte-matched Win32 decomps (main lane) or
IDO-built N64 matched decomps (n64 tree, low value while pairing is the wall).
