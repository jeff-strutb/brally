# Feedback no periodic screenshots

*Recorded 2026-09-30.*

> RULE - never enable periodic frame capture (BR_SHOT_DIR / BR_SHOT_EVERY) in headless runs; named script shots only

Never set BR_SHOT_DIR (or BR_SHOT_EVERY) when running the Mac port headless. Capture only the named `shot` steps in the BR_SCRIPT (BR_SHOTS=dir).

**Why:** BR_SHOT_DIR saves a full frame every 30 swaps by default (every frame when BR_SHOT_EVERY=0), and each save stalls the GPU queue. That made every screenshot run slow, and on 2026-09-30 it filled the disk (four runs, 1280x960 PPMs every frame). the project lead: "Don't do that anymore."

**How to apply:** run scripts pass BR_SHOTS only; use BR_VCLOCK=4 for a ~6 s capture; run from a private copy of the binary (other sessions pkill brally). Related: [mac-remastered-lighting-2026-09-29](../remaster/mac-remastered-lighting-2026-09-29.md).
