# C++ match integration — src/core/cpp + cpp_sweep

The C pipeline (`tools/match_sweep.py` → `build/match/report.csv`) is
`.text`-only and C-only. It cannot emit or score `__CxxFrameHandler`
unwind. Verified C++ EH matches live in a sibling tree and a sibling
report. This file is the layout; `docs/cpp-harness-notes.md` is the
harness and idiom.

## Layout

```
src/core/cpp/<VA>.cpp          one TU per matched function
tools/cpp_score.py             4-piece scorer (body, FuncInfo, unwind, handler)
tools/cpp_sweep.py             walks src/core/cpp, writes report_cpp.csv
build/match/report_cpp.csv     C++ rows (report.csv columns + `pieces`)
tools/total.py                 C from report.csv; C++ from report_cpp.csv
```

`build/cpp_work/` is the scratch pad. Wall attempts stay there
(frame-lands, body-coloring: 0x100439B0 / 44860 / 45EF0 / 4F8C0 /
485B0 / 4DA00, and the family-6 page-builder / stack-dtor bodies).
They are not `@implements` in `src/` and they do not count.

Each filed TU is tagged:

```
/* @implements 0x<VA> glide <name>
 * @cpp_kind dtor|ctor|method|scalar_deleting
 * @cpp_symbol <mangled>
 */
```

`@implements` means all four pieces are 0, not "the body looks right".
`cl` is invoked as `/O2 /GX /MD` (VC5 C++ EH, `/MD` CRT). See
`docs/cpp-harness-notes.md`.

## Sweep

```
python3 tools/cpp_sweep.py                       # every src/core/cpp/*.cpp
python3 tools/cpp_sweep.py src/core/cpp/0x....cpp
python3 tools/cpp_sweep.py --summary
```

For each `@implements` it compiles the TU via `cpp_score.compile_cpp`
(own `build/match/obj_cpp/`, never the C `obj_*` dirs) and scores:

| piece | where | how |
|---|---|---|
| function body | orig `.text` at the VA | reloc-masked `match_sweep.score` |
| FuncInfo | `.xdata$x` / `.rdata`, magic `0x19930520` | structural (maxState / nTry / nIP / toState) |
| unwind action(s) | sibling `.text` VAs | reloc-masked against each orig bin |
| handler thunk | 10 B `mov eax, FuncInfo; jmp __CxxFrameHandler` | reloc-masked |

`status=match` only when `pieces=4/4`. `diffs` is the body `.text` count
(0 on a match). `opt` is `O2` / `Od` / `O2y` — `/GX /MD` are implied.

The C sweep is untouched: `match_sweep.sources()` walks `*.c` only, and
`report.csv` is a different file. Do not fold `/GX` into the C sweep.

`tools/total.py` counts C++ from `report_cpp.csv` (and runs `cpp_sweep`
once if that file is missing). It no longer walks `build/cpp_work`.

## Filing a new match

1. Land a 4-piece 0 in `build/cpp_work/<VA>.cpp`
   (`python3 tools/cpp_score.py --va <VA>` → exit 0 and `all four`).
2. Copy it to `src/core/cpp/<VA>.cpp` with the tags above.
3. `python3 tools/cpp_sweep.py src/core/cpp/<VA>.cpp` — merges the row.
4. Do not file a wall. Sidecar-only (FuncInfo/unwind/handler MATCH,
   body diffs > 0) stays in `cpp_work`.
