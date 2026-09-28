# Layout: one folder per responsibility

Folders are named for **what a subsystem is responsible for**, not for the
technique it uses or the layer it sits in.

    startup/    bring the game up and take it down: entry point, message
                loop, window, the DirectX requirement check, the single-
                instance guard, which step runs each frame
    settings/   what the player chose and what the machine is: install
                directory, saved games, persistent state
    gamedata/   locate, read and decode the game's own files: POD archives,
                car definitions, track files, object tables, allocation pools
    geometry/   positions, orientations and the arithmetic that moves them
    drawing/    turn geometry and images into pixels: display lists,
                textures, surfaces, fonts
    scene/      what is in the world and where: track scene, racing line,
                collision grid
    driving/    how a car behaves: integrator, tyre and contact response
    racing/     the rules of a race: laps, gates, standings, opponents
    menus/      the front end: pages, controls, navigation
    controls/   reading what the player is doing
    audio/      sound and music
    net/        multiplayer: sessions, peers, and the wire format a car's
                state is squeezed into. The pack/unpack helpers live here
                and not in geometry/ even though they read like arithmetic --
                their responsibility is the PROTOCOL, and filing them by
                their maths would scatter one wire format across three
                folders.
    gfx/        the HOST Metal backend. Not game code, and named for what it
                is rather than filed with drawing/

`net/` was added 2026-09-03. Multiplayer was the one subsystem with no
responsibility folder: its ~84 functions sat in address batches with nowhere
to go, so "file everything" was impossible until it existed.

## Why not `math`, `physics`, `platform`, `render`?

Those name a *technique*, a *discipline*, or a *layer*: how something is done,
or where it sits. They are the names a programmer reaches for and they group
unrelated things: `platform/` had the entry point next to the archive reader
next to the allocator, which share nothing except being unglamorous.

`geometry/` is the one that stays close to a technique name, and that is honest
rather than lazy: it owns spatial representation, which genuinely is its
responsibility. It is not a folder of "maths we happened to need"; bit twiddling
lives there only because `br_bits` is span and interval arithmetic, and if it
ever grows unrelated helpers they belong with their consumer, not here.

## The address batches are gone

The `sliceN_MM.c` files were a **batch of whatever occupied one address range**
in the original -- a decompilation-process artifact, not an architecture. Every
matched function was filed out of them into its module. What remained was the
port's BRD3D-era transcription plus dead copies of bodies already matched
elsewhere, so on 2026-09-28 all 56 moved to `ports/macos/legacy/`. Their
headers (`include/sliceN_MM.h`) stay: matched modules include them, and the
declarations they carry are part of those modules' translation units.

## src/ is the decomp and nothing else

Port code -- a body that exists only so the Mac build runs -- lives under
`ports/macos/`, never here. See `ports/README.md` for how the port compiles a
module whose original body only builds for the 32-bit target.

## Rules

- A new module goes in a responsibility folder. Never add a `sliceN_MM.c`.
- Every banner states the responsibility it serves and what the module does;
  every function carries its original address and a description of behaviour.
- `build.sh` discovers `src/core/**/*.c` recursively and names objects by
  path; `build.d/*.deps` name modules by basename, so a module changes folder
  without touching them.
