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
    gfx/        the HOST Metal backend. Not game code, and named for what it
                is rather than filed with drawing/

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

## Why 63 files are still loose at the top level

They are named `sliceN_MM.c`, a **batch of whatever occupied one address range**
in the original. That is a decompilation-process artifact, not an architecture,
and the batches mix responsibilities freely: `slice3_44.c` holds a 3x3 matrix
solve, a 4x4 transform builder *and* the rigid-body integrator; `slice2_16.c`
holds screen fade and GBI texture scanning.

Filing them means splitting at FUNCTION granularity. Leaving them visibly out
of place is the point: the top-level listing is the backlog, and it shrinks
only when functions actually move, a measurement that cannot be faked.

## Rules

- A new module goes in a responsibility folder. Never add a `sliceN_MM.c`.
- Every banner states the responsibility it serves and what the module does;
  every function carries its original address and a description of behaviour.
- `build.sh` discovers `port/src/**/*.c` recursively and names objects by
  BASENAME, so a module changes folder without touching any `build.d/*.deps`.
