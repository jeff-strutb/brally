# Boss Rally: earlier long-form notes

Finished-lane notes, session dumps and retired starting documents, restored
from the archive that was removed from the tree on 2026-09-29. They are
history, not procedure: [MATCHING.md](../MATCHING.md) is the current
procedure, [VC5-IDIOMS.md](../VC5-IDIOMS.md) the idiom book, and
`tools/corpus.py` the query. Where a later note in [decomp/](../decomp/)
contradicts one of these, the later note wins.

| what | why it is here |
|---|---|
| `CONVENTIONS.md`, `DECOMP_NOTES.md`, `DECOMP_VIABILITY.md` | pre-matching / D3D-era starting docs; some claims are wrong |
| `STRUCTURAL-PLAYBOOK.md`, `T1-INTAKE-LANE.md` | replaced by MATCHING.md |
| `VC5-IDIOMS-*.md`, `idioms-A/B/C.md` | session dumps; the merge target was the main idiom book |
| `cpp-*`, `eh-workstream-notes.md` | C++ EH lane notes |
| `*-exe-notes.md`, `exe-integration-notes.md` | in-scope EXE game code is complete |
| `gen-*-notes.md` | generator fold recipes; the generators themselves live under `tools/` |
| `incidents.md` | why the standing orders exist (D3D twice, autofile, claim N) |

## Index

- [CONVENTIONS](CONVENTIONS.md): Rules this port follows. They exist because breaking each one has already cost
- [DECOMP_NOTES](DECOMP_NOTES.md): This is the previous README, kept verbatim for the hard-won detail in it:
- [DECOMP_VIABILITY](DECOMP_VIABILITY.md): `BossRally.BIN/.cue` is a MODE1/2352 data track (58,143 sectors, about 130 MB)
- [STRUCTURAL-PLAYBOOK](STRUCTURAL-PLAYBOOK.md): This file is the task spec for structural matching work. It
- [T1-INTAKE-LANE](T1-INTAKE-LANE.md): Feed this file to a fresh session. It is the procedure that produced nine
- [VC5-IDIOMS-batch1](VC5-IDIOMS-batch1.md): Proven against BRGlide.dll this session. Merge into `docs/VC5-IDIOMS.md`.
- [VC5-IDIOMS-big](VC5-IDIOMS-big.md): Proven against BRGlide.dll orig bins. Infer source from the bytes; never
- [VC5-IDIOMS-dll](VC5-IDIOMS-dll.md): Proven against BRGlide.dll orig bins. Infer source from the bytes; never
- [VC5-IDIOMS-dll2](VC5-IDIOMS-dll2.md): Proven against BRGlide.dll orig bins. Infer source from the bytes; never
- [VC5-IDIOMS-fresh1](VC5-IDIOMS-fresh1.md): Proven against BRGlide.dll orig bins. Infer source from the bytes; never
- [VC5-IDIOMS-fresh2](VC5-IDIOMS-fresh2.md): Proven against BRGlide.dll orig bins. Infer source from the bytes; never
- [VC5-IDIOMS-fresh3](VC5-IDIOMS-fresh3.md): Proven against BRGlide.dll orig bins. Infer source from the bytes; never
- [VC5-IDIOMS-midA](VC5-IDIOMS-midA.md): Proven against BRGlide.dll orig bins. Infer source from the bytes; never
- [VC5-IDIOMS-midB](VC5-IDIOMS-midB.md): Proven against BRGlide.dll orig bins. Infer source from the bytes; never
- [VC5-IDIOMS-structural](VC5-IDIOMS-structural.md): Proven against BRGlide.dll this session. Merge into `docs/VC5-IDIOMS.md`.
- [VC5-IDIOMS-t2](VC5-IDIOMS-t2.md): Proven against BRGlide.dll orig bins, 2026-08-27. None of the four
- [VC5-IDIOMS-t3](VC5-IDIOMS-t3.md): Proven against BRGlide.dll this session. Merge into `docs/VC5-IDIOMS.md`.
- [audio-xm-notes](audio-xm-notes.md): The port plays the Top Gear Rally soundtrack as lossless audio. The N64 build
- [bossrally-exe-notes](bossrally-exe-notes.md): BossRally.exe is the ~40 KB intro stub (`orig/BossRally.exe`). `.text` is
- [brally-exe-notes](brally-exe-notes.md): BRally.exe is the 8 KB game launcher (`orig/BRally.exe`). `.text` is 3,584
- [cpp-family1-notes](cpp-family1-notes.md): Harness: `build/cpp_work/<VA>.cpp` + `python3 tools/cpp_score.py --va <VA>`.
- [cpp-family2-notes](cpp-family2-notes.md): Harness: same as Task 1 (`build/cpp_work/<VA>.cpp`, `cl /O2 /GX /MD`,
- [cpp-family3-notes](cpp-family3-notes.md): Harness: `build/cpp_work/<VA>.cpp` + `python3 tools/cpp_score.py --va <VA>`.
- [cpp-family4-notes](cpp-family4-notes.md): Harness: `build/cpp_work/<VA>.cpp` + `python3 tools/cpp_score.py --va <VA>`.
- [cpp-family5-notes](cpp-family5-notes.md): Harness: `build/cpp_work/<VA>.cpp` + `python3 tools/cpp_score.py --va <VA>`.
- [cpp-family6-notes](cpp-family6-notes.md): Harness: `build/cpp_work/<VA>.cpp` + `python3 tools/cpp_score.py --va <VA>`.
- [cpp-family7-notes](cpp-family7-notes.md): Harness: `build/cpp_work/<VA>.cpp` + `python3 tools/cpp_score.py --va <VA>`.
- [cpp-harness-notes](cpp-harness-notes.md): 80 functions (97,204 B = 20.2% of BRGlide.dll `.text`) thunk to
- [cpp-integration-notes](cpp-integration-notes.md): The C pipeline (`tools/match_sweep.py` → `build/match/report.csv`) is
- [cpp-nearmiss-notes](cpp-nearmiss-notes.md): Harness: `build/cpp_work/<VA>.cpp` + `python3 tools/cpp_score.py --va <VA>`.
- [eh-workstream-notes](eh-workstream-notes.md): The `cxx-eh-frame-wall` note treated `push -1` / `fs:[0]` as unreachable
- [exe-integration-notes](exe-integration-notes.md): The three in-scope EXEs (BRally.exe, SetVideo.exe, BossRally.exe) were
- [gen-callconv-notes](gen-callconv-notes.md): Standalone transform (`tools/gen_callconv.py`). Do **not** copy this file
- [gen-fresh-notes](gen-fresh-notes.md): Standalone transforms (`tools/gen_fresh.py`). stringops + charret were
- [gen-structural-notes](gen-structural-notes.md): Standalone transforms (`tools/gen_structural.py`). Do **not** copy this file
- [gen-structural2-notes](gen-structural2-notes.md): Standalone transforms (`tools/gen_structural2.py`). Do **not** copy this file
- [idioms-A](idioms-A.md): `src/core/drawing/br_tex3d_expand.c`, 8480 B, 2407 insns. Second/third
- [idioms-B](idioms-B.md): Call-shape class: unmatched orig with FF15 stdcall, Glide E8 thunk,
- [idioms-C](idioms-C.md): Proven against `build/match/orig_setvideo/<VA>.bin` with `exe_sweep.py`
- [incidents](incidents.md): These are not current procedure. They are why `the project rules` is short and strict.
- [setvideo-exe-notes](setvideo-exe-notes.md): **STATUS: the game code in this binary is DONE.** All 42 user functions in
