# Feedback labelled toggle diagnosis

*Recorded 2026-10-01.*

> For visual artifacts the project lead can see, run a parked, labelled on/off cycle of effects and let the project lead pick -- don't theorise from screenshots

2026-10-01: I spent hours chasing night "ghosting" (copies of the car around it) as a TAA/motion-blur problem. The project lead said: stop guessing, load the game, sit still at the start line, toggle one effect at a time with its name on screen. That found it in two rounds. It was not temporal at all: my own asphalt-tone 8-tap wide average (ytone in host_fx.m compfs) let the car's zero surface colour in at night through an absolute 0.01 margin, giving 8 static dark copies of the car (fixed 03554b6d).

**Why:** the project lead can see the artifact live; screenshots and my headless captures (4:3, bumper cam, stalls from shots) kept missing it, and a theory is not evidence.
**How to apply:** for any visual artifact, build a private binary that cycles effects every 4-5 s with a CATextLayer label in the window (pattern: badge in host_fx.m), launch it windowed via BR_SCRIPT parked at the start line (no autopilot, chase cam = PGDN, night = 6 WEATHER clicks at 252,157 after a 420-frame boot wait), at the project lead's window size (`window W H`). If an effect "fixes" it only by darkening, suspect anything that effect multiplies. Also: when a new screen-space gather is added, check it at night -- absolute thresholds break when values get tiny. Related: [remastered-look](../remaster/remastered-look.md), [diagnose-dont-hypothesize](../decomp/triage/diagnose-dont-hypothesize.md).
