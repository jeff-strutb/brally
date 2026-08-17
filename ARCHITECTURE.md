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

- ~~39,973 bytes classed `unknown`~~ — **characterised, see below.**
- **13,343 bytes** `d3d_only`, reachable only from renderer entry points.
- ~~The 256-entry table at 0x100A9A58~~ — **decoded, see below.**
- `Boot.exe` (1,453 functions) and `BossRally.exe` (215) are identified from
  strings and imports, **not from reading their code**. The judgement that
  neither contains game logic is an inference.
- Which function installs each of the 1,148 singleton hooks. That mapping is
  the missing half of the architecture, and it is what would let a subsystem be
  described by what runs it rather than by what it calls.

## The installer map — the missing half, now built

`tools/hookmap.py` reads the relocation table the other way round: a relocation
inside `.text` whose stored dword is itself a function is an **address-taking
instruction**, so the function containing it is an installer. That yields the
`F installs G` edge the call graph cannot produce.

    functions installed as hooks : 419   of which ported: 229 (55%)
    distinct installers          : 140   of which ported:  75 (54%)
    installed from exactly one function : 357
    installed from several              :  62

The heaviest installers are the engine's wiring points — each is where one
subsystem is assembled, and each is worth more than its byte count suggests
because nothing below it runs until it does:

    0x10051600  25 hooks   4109 B      0x100439B0  21 hooks   3746 B
    0x100498A0  20 hooks   3993 B      0x1004CBA0  20 hooks   3671 B
    0x1004BE00  19 hooks   3475 B      0x1004AEE0  17 hooks   3862 B
    0x1001FD70  15 hooks    371 B  <-- 15 hooks in 371 bytes: a pure table
    0x10029B50  13 hooks    285 B  <-- likewise

This is the correct work order. A hook target ported without its installer does
nothing; an installer ported without its targets installs NULLs. The pairs are
the unit of useful progress, which is not something the byte-count coverage
figure can express.

**Limits, stated because the survey's first two attempts both produced
confident numbers that described the tool rather than the game:** this finds
where a constant address is materialised, so it misses a pointer copied from
one slot to another, and it misses a pointer *computed* rather than taken —
which is exactly what the undecoded 256-entry table at 0x100A9A58 is likely to
be. `installs` is therefore a superset of "could install" and a subset of "does
install at run time".

## The 256-entry table at 0x100A9A58 — decoded

It is the **display-list opcode dispatch**, indexed by the command byte. 256
slots, **28 live handlers**, and the remaining 228 all point at one 8-byte stub
at `0x10021240` — the default/no-op for an unimplemented opcode. That is a
closed command set, established by the data rather than inferred from a name.

    0x01 matrix      0x03 movemem     0x04 (584 B)     0x06 dlist
    0xB1 (696 B)     0xB6 cleargeom   0xB7 setgeom     0xB8 enddlist
    0xB9 ...         0xBC moveword    0xBD popmatrix   0xBF (378 B)
    0xDC 0xDD 0xDE 0xDF 0xE1 0xE2 0xE3 tilerects 0xE4
    0xED 0xF2 0xF6 0xF7 0xF8 0xFA 0xFB 0xFC

**13 of 28 are ported** (all but one under their D3D addresses in
`slice2_16.c`); 15 remain, totalling 2,592 bytes. That is a small, bounded and
fully enumerated piece of work, which is what decoding the table bought.

The high opcodes (`0xB1`–`0xFC`) are the N64 F3DEX/RDP command set; the low
ones (`0x01`–`0x06`) are the F3DEX matrix and display-list commands. A sibling
analysis of the N64 ROM confirms the PC build handles a subset of stock F3DEX
and repurposes `0xE1`, which the N64 never emits.

## The `unknown` bucket — 138 functions, 39,973 bytes

First, a naming correction that matters: `unknown` in `config/shared.csv` means
**crossdiff could not pair it with a Glide counterpart**. It does not mean
"unidentified", and it does not mean "unported" — six of the 138 are already
transcribed, including `0x1004D640`, one of the sixteen screen builders. The
class name invited exactly the wrong reading and this document previously took
it.

They are also **D3D addresses**, so `config/survey.csv` (built from the Glide
image) cannot cross-reference them at all. 132 of 138 simply do not appear in
it. That is a property of the two artifacts, not a finding about the code.

What they actually are, from the D3D image:

    138 functions        39,973 bytes    11 B .. 3,111 B, median 121 B
      already ported        6             2,149 bytes
      not ported          132            37,824 bytes

      no direct caller anywhere in BRD3D    51    23,187 bytes
        ...but the address IS taken         44
        ...neither called nor taken          7

The 51 with no caller are the interesting group and they are more than half the
bytes. 44 of them have their address stored somewhere, so they are dispatched
through — consistent with the engine-wide pattern. The largest are 1,000–1,200
byte functions that reference **no strings at all** and open by reading globals
in the `0x104Bxxxx`/`0x104Cxxxx` range, several in exact-size twin pairs
(`0x100228F0` and `0x100231D0` are both 1,223 bytes). That shape — long, string-
free, globals-driven, duplicated per variant — is what a specialised inner loop
looks like, and it is why they have no Glide twin: the Glide build reaches the
hardware through `glide2x.dll` instead.

**7 of the 138 are neither called nor pointer-taken** — genuinely unreferenced
in the D3D image, or reached only by a computed target.

Note that this "7" is about the D3D `unknown` bucket specifically. The Glide
survey's `dead` tier is a different and larger set — see below.

## The `dead` tier — 47 functions, 3,120 bytes, and mostly not interesting

`config/survey.csv` marks 47 functions in the Glide image as reached by neither
a call nor a stored pointer. Read literally that is "0.7% of the program is
dead code", which would be a finding. It mostly is not, and the bytes say why:

    x3   55 8B EC 5D C3                    push ebp; mov ebp,esp; pop ebp; ret
    x2   55 8B EC B8 01000000 5D C3        ... mov eax,1; pop ebp; ret
    x2   C3                                a bare `ret`
    x2   CB                                `retf`
    x2   CF                                `iret`

So the set is three things mixed together:

- **Genuinely empty functions.** `push ebp / mov ebp,esp / pop ebp / ret` and
  `return 1` stubs, kept by the linker and called by nobody. Real, and
  uninteresting.
- **Map artifacts.** The single-byte `C3`/`CB`/`CF` entries are not functions
  at all — those are the terminator bytes `funcmap2` scans for, promoted to
  function starts by its weak gap-and-prologue heuristic. Six of the 47.
- **A handful of real unreferenced functions**, the largest 579 bytes.

41 of the 47 are not 16-byte aligned, which is consistent rather than
suspicious: this build has zero inter-function padding, so a genuine function
often begins on an odd boundary immediately after its predecessor.

None of it is worth porting, and the reason to write it down is that "47 dead
functions" and "0.7% dead code" are both technically true and both misleading.

The next analysis is not "port more functions". It is: for each stored function
pointer, find the instruction that stores it and the instruction that calls
through it. That produces the real module boundaries, and until it exists,
subsystem claims in this repository are inferences from naming.
