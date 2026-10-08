# N64 coverage scripts

*Recorded 2026-09-28.*

> N64 n64box game-through suite via goal-driven driver (n64drive.py) - landed f4167140; facts learned, what's still unreached

the project lead asked (2026-09-28) for N64 unicorn game-through scripts as thorough as the PC's. LANDED 2026-09-29 as f4167140 (tooling 7678198e): 49 generated scripts in tools/tgrally/n64box_scripts (60 total; broken arcade_2p.txt removed). Suite T4 reach 302/403 → 343/403 fns (n64probe.py --cover). All 49 replay IDENTICAL (n64drive.py verify --all). Peer N64 session told to re-run A5/A7.

**Tools:** n64probe.py (menu row 0x80316244, BrModeSet, setup vars, `--cover --dir`), n64drive.py (plans → scripts; drafts to build/tgrally/n64/drive, `gen --install` into suite; show/verify), n64drive_plans.py. n64box: `p2` pad lines, `frames N`, `rumble` (osMotorInit ok, PfsInitPak ID_FATAL).

**Facts:** default unlocks Coastline + cars 1,8; cheats typed during a race (title/menus eat A/B/Z/dpad), cheat table 0x8028DF48. Racing line = segment chain from car+0xF5C. Mirror tracks: same coords, steering inverted. Arcade has TIME UP; Practice no AI. Instant replay (in race tick): A restarts, START exits. Credits (Options row 6) = demo B; demo C only after a season end (mode 5). Paint cursor 0x8028D12C/130 in 640x480; paint exit → title. Controller type = pad record +0x25 (C: d-pad steer; D: stick-up throttle, Z brake).

**Still unreached / weak:** autopilot stalls off Coastline (0.1-0.3 lap per 2400 frames; several TIME UP); demo C / season end; BrRumbleInsertPrompt (prompt path); BrRumbleInitAll is uncalled in ROM (EXCLUDED candidate); remaining paint tools (TextDraw, DashLine, MirrorSide, FlipApply); BrFatal/halt.
