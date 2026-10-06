# Generated dir retired

*Recorded 2026-09-28.*

> 2026-09-28 src/brally/core/generated/ emptied and deleted -- every body refiled into its module, byte-identical; refile helpers, traps, and the "project lead wants architectural correctness, not port" rule

2026-09-28: every function in `src/brally/core/generated/` (and tiny_stubs.c, ghidra_batch.c) was refiled into
its module and the directory deleted. No slice*.c holds a tag (only prose mentions). fileaudit OK, image
gate 0 bytes, portcheck 0 new findings vs 1e7307a3~1. Counts after: T4 1311, T3 188.

**Why:** project lead: "I want our completed work to be architecturally and technically correct. The port is
still a side note." Refiling = right module, accurate WHAT IT DOES, types reconciled to the file's
existing declarations, no new warnings, byte-identical.

**How to apply / traps found:**
- Helpers live in `build/brally/win32/match/t3d/` (verify.py with RENAME=old=new for renamed callees, genmove.py,
  genmerge.py, setfile.py, gate.sh, round.sh). genmove strips includes AND `_CRTIMP` -- a target file
  without `#define _CRTIMP __declspec(dllimport)` turns FF15 CRT calls into E8 (br_sprfont.c, br_save.c).
- A T3 body's colouring moves with ANY preceding code: BrUiSprBlit changed at every br_surf.c position;
  BrRaceCarPickIndex changes if its unused <windows.h>/<mmsystem.h> are dropped. Give such a group its
  own TU (br_sprblit.c, br_racecar.c, br_collrespreset.c) and say so in the header.
- Read the callers before writing the comment: several old comments were wrong (0x100099D0 is
  IDirectPlay::DestroyPlayer, 0x10031140 loads the track's .hnt texture hints, 0x10054390 is BrTextBox
  slot +0x14 key handler). Names shared with the port were kept; comments fixed.
- Callee xrefs: scan reference/brally/orig/BRGlide.dll .text for E8 rel32 (no pefile in venv; parse PE by hand).
- zsh does not word-split `$VAR` -- use `${=VAR}` in git pathspecs.

Related: [refile-byte-identity-2026-09-25](refile-byte-identity-2026-09-25.md), [filing-and-description-gate](filing-and-description-gate.md), [thiscall-via-fastcall](../cpp-lane/thiscall-via-fastcall.md).
