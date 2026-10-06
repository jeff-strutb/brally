# Gap sessions audit

*Recorded 2026-09-28.*

> 2026-09-28 gap-session audit -- the 26 brbox scripts missed most real content; A7 on 154 new sessions broke 2 certified T3s (FIXED same day, 10 scripts added); ref-fill masked 64 T4 relocation defects (FIXED, see refslot-gate); EXCLUDED hold, proofs corrected

Measured 2026-09-28 (harness + results in build/brally/win32/brbox/gap/: probe_excl.py, a7.py, refslot_audit.py).

**Coverage gaps in tools/brally/brbox_scripts** (all real retail sessions, none scripted):
attract demo (90 s idle on menu, mode 4 cine 0), weathers 1 fog / 2 storm / 4 rain (only 0,3 raced),
13 of 14 tracks (cheats hazel+brielle unlock all; TRACK row cycles 2,1,0,14,13,11..3), 30 of 32 cars
(all six cheats; car = (1-clicks) mod 32), camera keys END/KP1 and PGDN/KP3 (F1-F4 are TAUNT sounds,
not views; bindings table .data 0x100B38A0, 28 actions x3). INI keys chosenTrack etc. are overridden by
the menu; gameMode=5 (gamewin.trk) is reset by the menu -- unreachable. Autopilot faults on Mine
variant (track 9) -- harness, not game (hold UP runs clean).

**M1 was broken (A7 on gap sessions, 46/154 differed, all localized to 2 certified T3s) -- both FIXED 2026-09-28 (9481a4c5):**
- BrEnvEmit 0x10017110: texrect x off by one quarter-pixel (x87 rounding) -- 12 cars, snow/storm tracks.
- BrCarDrawVehicle 0x1000A110: colour bytes swapped DDEEFF00 vs EEDDFF00 in RAIN on every track --
  its "classified byte-compose group" residue is a real bug, invisible when the two bytes are equal.

**M2 caveat:** image_build ref-fills 3,984 T4 reloc slots from the original; refslot_audit found 64 T4
functions that match only through that mask: halt_baddata stubs 0x10073974/9 are data not code;
0x100623E0 float const 1 ULP off; 0x10056260 145 "images\\\\x.bmp" double-backslash literals; one C
symbol standing for several original variables (g_s17 x15, g_br73 x5, g_menu$S445 x3, g_weather,
g_fPfxDt, g_BrSndNearest, g_brFfb, g_BrCharMap, g_BrLoad, $T1852); C++ placeholder callees (EnterFn x30,
PrepFn, Enter2Fn, SendPkt) leave call targets unpinned.

**EXCLUDED still unreachable, proofs wrong:** DECAL combine IS emitted every sky frame by
BrSceneSetupFrame via BrRdpSetCombineLERP (runtime-packed, so the constant search missed it); lit-no-Z IS
entered (snow; END view; cars 9/10/25/26 whose dial sprite mode!=0 early-returns after CLR Z) but no
G_VTX ever lands in it (post-dial call tree has no vertex emits). LitDecal hinges on every sky DL issuing
a combine before B7 0x20205 -- held on all 14 tracks.

**Why:** the project lead asked to verify T3/T4/M1/M2 against real sessions. **How to apply:** A5/A7 certify only
what scripts play; before trusting a T3, check its inputs vary (equal bytes hide swaps, boundary floats
hide x87 rounding). Rule 11/12: do not open these T3s unless the project lead names them.

Related: [a5-a7-coverage-gap](a5-a7-coverage-gap.md), [t3-image-and-arith-traps-2026-09-24](../traps/t3-image-and-arith-traps-2026-09-24.md), [t3-certified-standard](../rules/t3-certified-standard.md).
