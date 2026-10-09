# Top Gear Rally on the PlayStation

The N64 game on Sony's console: the native port's core (`ports/tgrally/src`,
the decomp with the port's address and byte-order conversions) compiled for
the R3000A, over a platform layer for the PlayStation. The target is the
whole game on a retail console (2 MB of RAM, 1 MB of VRAM, 512 KB of sound
RAM, the CD).

## Building

```
ports/tgrally-ps1/tools/toolchain.sh     # binutils, GCC 14 and gdb for mipsel-none-elf, into build/toolchains/ps1
ports/tgrally-ps1/tools/duckstation.sh   # DuckStation (portable, in build/tools/ps1) and the BIOS from reference/ps1/bios
ports/tgrally-ps1/link.sh                # build/tgrally/ps1/tgrally.exe
```

`link.sh` runs `build.sh` (the core, every TU of `ports/tgrally/src` but those
in `skip.txt`), generates the data symbols with `ports/tgrally/tools/globals.py`
for the R3000 (`TGR_TARGET=mipsel-unknown-none-elf`), and links the platform:
the native port's where it carries over (the scheduler, the controllers and
Controller Pak, libultra's printf, the game's software mixer) and this
port's own (`platform/`). `RAM=8` (for now the default) links for a
dev-kit's 8 MB; the retail console's 2 MB is the target.

## The platform

| | |
|---|---|
| `hw/crt0.s`, `hw/ps1.ld` | the PS-EXE's start, its memory |
| `hw/exc.s`, `hw/irq.c` | the exception vector: interrupts (the VBlank) and a fault report (registers on the TTY, `fatal.txt`) |
| `hw/ctx.s`, `os/host_ps1.c` | the game's threads as coroutines under the native port's scheduler (`ports/tgrally/platform/os/thread.c`): one runs at a time and they switch only in OS calls, as on the N64 |
| `hw/ps1hw.c` | the clock (root counter 1 counting lines), the TTY |
| `os/main_ps1.c` | power-on: `run.cfg`, the data lifted, `BrBoot` |
| `os/io_ps1.c`, `os/lift_ps1.c` | the native port's io.c and lift.c with the cartridge's data read from a file |
| `gfx/` | each graphics task's digest for the trace; the GTE and GPU renderer (to come) |
| `libc/` | the C library: strings, a heap, files over PCDRV, `sqrtf` correctly rounded (every float checked against the host's) |

## Checking it

`tools/dsrun.py RUNDIR IMAGE` runs the game in DuckStation with RUNDIR as the
PCDRV root: `run.cfg` there sets the run (`script=`, `frames=`, `trace=`,
`headless=1`, `TGR_*` switches), `romdata.bin` is the cartridge's data
(`ports/tgrally/tools/assets.py`), and the game writes its trace and `done`
there. The trace is the native port's (`--trace`): every display list's
digest, every swap, every audio buffer, so a PlayStation run is compared
with the native port's line for line. `--gdb-at S` attaches gdb to
DuckStation's GDB server. `--cpu CachedInterpreter`: DuckStation's
recompiler stops with SIGILL on this game for now.

As of the first run: the Options script (`tools/tgrally/n64box_scripts/options.txt`,
5,062 retraces) is identical to the native port's, line for line.
