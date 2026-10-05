# Reference glide sdk and c2read

*Recorded 2026-10-05.*

> Glide 2.x SDK headers (tools/glide2x-sdk via setup.sh) and tools/c2read (VC5 C2 allocator/frame/symbol-id reader) - use for order-only and register walls on the PC decomp

Committed 16228b5d (2026-10-05):

- **Glide 2.x SDK headers**: `setup.sh` fetches glide.h, glideutl.h, glidesys.h, sst1vid.h, 3dfx.h, fx*.h from the Homeworld source release (github aheadley/homeworld, pinned commit c1e7f492, src/rgl/3dfx) into `tools/glide2x-sdk/` (gitignored). Compile with `FN_OPTS='/O2 /I tools/glide2x-sdk'`, prefix `#define _CRTIMP __declspec(dllimport)` + `<windows.h>` + CRT + `<glide.h>`. Windows+CRT+Glide put a file's globals at ~22.5k symbol ids (bit 14 set).
- **tools/c2read/c2read.py SOURCE.c FUNC [--frame] [--symbols NAMES] [--trace] [--json]**: Byte Tactics' c2prio tracer (MIT) run through gdbshim.py (GDB remote protocol to `winedbg --gdb`, no gdb; int3 breakpoints, call step emulated). Uses `tools/msvc5/bin-sp3/C2.EXE` (SHA matches BT's). Object byte-identical to a normal compile. Occasionally stops before the sorted list: rerun.
- **Byte Tactics docs** (github HectorBailey/byte-tactics docs/c2-regalloc.md, worker-guide.md): MSVC 5 register allocator read from C2.EXE; order-only ties follow symbol ids (bit 14 / mod 65536); frame slots by reference count.

Measured on BrSceneDlBuild ([scenedl-natural-retranscription-2026-10-05](../functions/scenedl-natural-retranscription-2026-10-05.md)): a declaration count putting the function's late locals past the 65536 wrap closed three order-only walls (wheel address order, arena guard, tail fixup) but flipped the B7 OR order.

Related: [hand-transcription-only](../rules/hand-transcription-only.md), [x87-wall-mechanism-2026-09-13](../levers/x87-wall-mechanism-2026-09-13.md).
