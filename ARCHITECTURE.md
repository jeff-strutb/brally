# What this codebase is

Written after a survey (`tools/survey.py`, `config/survey.csv`) that should
have been done before any porting. It was not, and the consequences are visible
throughout this project's history: work started in the middle, the entry point
went unread for most of a week, and five coverage figures were published
against a denominator whose exclusions were decided from filenames.

## The images on the disc, and which one is the game

| image | `.text` | functions | what it is | evidence |
|---|---:|---:|---|---|
| `BRGlide.dll` | 481,280 | ~2,140 | **the game**, Glide build | imports `glide2x.dll`, `ddraw`, `dinput`; exports `RallyMain` |
| `BRD3D.dll` | 581,632 | ~2,700 | the game, Direct3D build | no `glide2x`; +100 KB is a statically linked CRT |
| `BRally.exe` | 3,584 | 39 | launcher | reads `BossRally.ini`, `LoadLibrary`, `GetProcAddress("RallyMain")` |
| `Boot.exe` | 96,768 | 1,453 | CD autorun shell | spawns `SETUP.EXE`, `DXSETUP.EXE`, `SetVideo.exe` |
| `BossRally.exe` | 23,552 | 215 | intro player | plays `brally.avi`, launches `brally.exe`; **no** game-relevant imports |
| `SetVideo.exe` | 36,864 | — | **settings writer** | writes `[Video]`, `Driver=`, `D3DDrawCarShadow=` into `BossRally.ini` |
| `REMOVE.EXE` | 1,536 | — | uninstaller | |
| `SETUP.EXE`, `_SETUP.DLL`, `_ISDEL.EXE` | — | — | InstallShield chain | not PE32 |
| `BRString.dll` | — | 0 | resource-only satellite | `RT_STRING` in UTF-16LE, no code |

`SetVideo.exe` is worth calling out: it was excluded from the denominator as
"installer chrome" on the strength of its filename, and it in fact **produces
the configuration the game consumes** — the exact keys `br_appstart` parses. It
contains no game logic, but it defines a contract the game depends on.

## The finding that matters: this engine is dispatch-driven, not call-driven

Of `BRGlide.dll`'s 460 KB of mapped functions:

| reached how | bytes | share |
|---|---:|---:|
| **direct** — `call rel32` from a named root | 59,405 | **12.9%** |
| **indirect** — only via a stored function pointer | 397,640 | **86.4%** |
| **dead** — neither | 3,120 | 0.7% |

Only an eighth of the image can be reached by following calls from the entry
point. Everything else is reached by storing a function's address somewhere and
calling through it later.

And the pointers are overwhelmingly **not** in static tables. Of 1,501
function-pointer storage sites in 1,155 contiguous runs:

    256 entries  0x100A9A58  (.data)
     31 entries  0x100776F0  (.rdata)   } C++ vtables
     27 entries  0x10077680  (.rdata)   }
     17 entries  0x1007B004  (.rdata)
     15 entries  0x10077150  (.rdata)
      5 entries  0x100A9900  (.data)    the app state machine
      2 entries  0x1007944C  (.rdata)
    -------------------------------------------------------------
      7 tables of 2 or more entries
  1,148 SINGLETONS -- a lone pointer, stored into an object at run time

So the structure is a small number of vtables plus roughly a thousand
individual hooks installed into objects while the game runs. The UI is the
clearest case: its screen builders store 108 distinct hook addresses into
control slots, and a control with a NULL slot silently does nothing.

### What follows from that

**Static reachability cannot map this program.** A call-graph walk answers
"what does this function call", never "who will install this and when". The
first run of the survey put 87% of the image in `unreached`, and adding
pointer-taken roots then put 92% in `indirect` — both numbers described the
walker, not the game. The honest output is the three-tier table above.

**Subsystem ownership is mostly not derivable from the call graph.** Exclusive
ownership could be attributed for only ~33 KB, and over 26 KB of that is
reachable from several roots at once. Anything more precise has to come from
decoding *which function stores each pointer*, which is a different analysis.

**It explains why porting from the middle appeared to work.** Loose coupling
through hook tables means a subsystem can be transcribed and tested in
isolation and still do nothing, because nothing has installed it. Four
installers sat in this tree with no caller; that is not an oversight so much as
the natural failure mode of this architecture.

**It also explains the placeholders.** In a direct-call program an unported
callee is a link error. Here it is a NULL slot, which is indistinguishable at
run time from "installed and did nothing" — which is exactly why "the menu does
not navigate" was investigated for weeks as a bug in the menu.

## What is still not understood

- **39,973 bytes** inside `BRGlide.dll` classed `unknown` by `crossdiff.py` —
  neither paired with the D3D build nor identified as CRT. Unclassified, not
  merely unported.
- **13,343 bytes** `d3d_only`, reachable only from renderer entry points.
- The **256-entry table at 0x100A9A58** is the largest dispatch structure in
  the image and has not been decoded.
- `Boot.exe` (1,453 functions) and `BossRally.exe` (215) are identified from
  strings and imports, **not from reading their code**. The judgement that
  neither contains game logic is an inference.
- Which function installs each of the 1,148 singleton hooks. That mapping is
  the missing half of the architecture, and it is what would let a subsystem be
  described by what runs it rather than by what it calls.

## How this should change the work

The next analysis is not "port more functions". It is: for each stored function
pointer, find the instruction that stores it and the instruction that calls
through it. That produces the real module boundaries, and until it exists,
subsystem claims in this repository are inferences from naming.
