# Layout: by architectural concern

    math/      vectors, matrices, fixed point, bit twiddling
    platform/  entry point, main loop, window, config, files, pools
    input/     the game's own input handling
    render/    display lists, textures, surfaces, fonts
    world/     tracks, scenes, paths, car data
    physics/   integrator, collision
    race/      race step, laps, AI
    ui/        pages, controls, navigation
    audio/     bank, mixer, output, music
    gfx/       the HOST backend (Metal). Not game code.

## Why 63 files are still loose at the top level

They are named `sliceN_MM.c` -- a **batch of whatever happened to occupy one
address range** in the original. That is a decompilation-process artifact, not
an architecture, and those batches mix concerns freely: `slice3_44.c` holds a
3x3 matrix solve, a 4x4 transform builder *and* the rigid-body integrator;
`slice2_16.c` holds screen fade and GBI texture scanning.

Filing them requires splitting them at FUNCTION granularity, which is real work
and is not done. Leaving them visibly out of place is the point: the top-level
listing is the remaining backlog, and it shrinks only when functions actually
move.

## Rules

- A new module goes in a concern directory. Never add a `sliceN_MM.c`.
- Every file's banner states its concern and what it does; every function
  carries its original address and a plain description of its behaviour.
- `build.sh` discovers `port/src/**/*.c` recursively and names objects by
  BASENAME, so a module can change directory without touching any
  `build.d/*.deps` file.
