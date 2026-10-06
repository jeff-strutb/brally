# Split screen

The N64 game *Top Gear Rally* has a two-player split-screen mode. The PC version
appears to ship with **no way to select it**, yet the decompilation shows the
split-screen renderer is fully present, complete, and wired up. It was built, and
then left one step short of playable.

What's actually in the binary:

- **A complete two-way split renderer.** The frame setup switches on the view
  count with exactly two arms: one full-screen view, or **two stacked
  half-height views** (top and bottom, each clipped to its own half). There is no
  three- or four-way path; it's strictly a two-player top/bottom split.
  (`BrFrameBeginDl`, `src/brally/core/drawing/br_framebegin.c`)
- **A per-view frame loop.** The frame drawer iterates `for (i = 0; i < views; i++)`,
  building each view's own camera and scene, and the camera code halves its height
  "which is what a split screen needs." Every downstream system (HUD, lap-time
  layout, on-screen captions, the "wait for player" prompts) already carries live
  `views == 2` branches. (`BrFrameDraw`, `src/brally/core/drawing/br_framedrive.c`;
  `src/brally/core/scene/br_camera.c`)
- **Generic multi-car control.** Cars are driven by a per-car function pointer;
  AI cars point at the AI controller, and the engine already runs any number of
  independently-controlled cars per frame. The physics don't care whether a given
  car is steered by a human or the AI. (`BrRaceDriverStep`,
  `src/brally/core/racing/br_racestep.c`)

So the screen, the cameras, the HUD, the second car, and the physics are all ready.
The one missing piece is **input for a second local player**:

- Input is a singleton. There is one keyboard buffer and one mouse buffer (the
  `[2]` you see is current/previous-frame double-buffering, not two players), and
  one control layout. (`src/brally/core/controls/br_inputpoll.c`, `br_ctrlquery.c`)
- The routine that applies a player's controls to a car, `BrCtlInputApply`, takes
  a car pointer but reads its input from a **single global** with no device or
  player index: hand it any car and it feeds that car the same one human's input.
  (`src/brally/core/driving/br_ctlinput.c`)

To turn this into a working mode you would need to add a second device binding /
control layout, give the input applier a per-player selector so entrant 0 and
entrant 1 read different devices, assign the second entrant a human controller
instead of the AI one, and expose the mode in a menu. Everything below that (the
hard part, the rendering) is already done.
