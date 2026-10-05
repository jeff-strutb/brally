# Native 64bit portable core 2026 10 03 addendum

*Recorded 2026-10-03.*

> Verify display work with real screen captures (screencapture), fullscreen included; BR_SHOTS reads the offscreen target and hides present bugs

BR_SHOTS/brr_shot read the renderer's offscreen target, not what reaches the screen. A Metal present that still fitted to 4:3 squeezed every frame on screen while every target shot looked right; the project lead caught it (2026-10-03, fixed f620d507).

**Why:** the present (target -> drawable) is a separate step the shots never exercise.
**How to apply:** for any window/aspect/scaling change, check with `screencapture -x` of the screen (or `-l <winid>`), in fullscreen (AXFullScreen via System Events) as well as a resized window, in both profiles. Related: [native-64bit-portable-core-2026-09-30](native-64bit-portable-core-2026-09-30.md).
