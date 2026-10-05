# Mac app and music

*Recorded 2026-09-29.*

> Boss Rally.app packaging (package_app.sh) and the native music decision/implementation in the 32-bit lane; what traps were hit

2026-09-29: the music decision is SETTLED (project lead chose it; ports/MUSIC-DECISION-PENDING.md now says DECIDED). Do not re-ask.
- `ports/macos/wasm/package_app.sh` builds a self-contained `build/app/Boss Rally.app` from the BIN/CUE and the TGR ROM (extract cached in build/app/extract, keyed on MD5s + a layout number).
- Music: `ports/macos/wasm/native/music.m` replaces the CD-music leaves (MCI + EAR CD channel) with AVAudioEngine; PC = CD FLACs, N64 = the ROM's .xm modules played live by libopenmpt (static link of Homebrew .a files, build_wasm.sh MPT_LIBS). N64 cues come from the N64 game itself: title module ROM 0x0EBC00, race table D_8026FF24 at ROM 0x70F24 (extract_modules.py). N64 gain -4.0 dB; a peak limiter at the device rate catches CD resampling overs.
- The project lead wants native, modern replacements of the Win9x layer (same philosophy as the renderer), not emulation of MCI/EAR.

Traps found (worth knowing for any port work):
- `br_musiccmd.c` defines empty local stand-ins for real functions, so the translated dispatcher never reached them.
- Bare `return;` after a call in an int function relies on MSVC eax; the wasm lane returns nothing (BrCdTrackGet). -O2 inlines same-file callees, so overriding the callee alone does not reach them.
- Verifying audio: BR_MUSICWAV records the output; compare at the source rate (44.1 kHz was bit-exact), because 8 kHz downsampled correlation understates the match.
- An intermittent race-sky corruption exists in the lane independent of music (1 in 6 runs); a task chip was raised.
- Windowed test runs steal focus, and the project lead's keystrokes landed in the game. Prefer headless.

2026-09-30: the original makes NO music call at race end (whole-DLL caller scan of the CD module). The project lead chose: the menus after a race play the title cue, in every version. music.m race_left calls BrCdTrackPlay(2) when g_pfnStep leaves BrRaceStep 0x10019A70. Headless repro: host_script.c now supports `autopilot on|off`; use a fresh BR_SAVEDIR (build/wasm/save keeps an 11-lap setting). Always cap BR_LOG output (`| head -c`): one uncapped run filled the disk.
Texture-cache trap: fx_texture's tm can be the same object as t (the entries are __unsafe_unretained), so releasing both crashed on the second race load. tex_drop in host_glide.m handles this.

2026-09-30 (e8231291): PROJECT DECISION: N64 + remastered pieces follow the COURSE, not the random CD track (CD stays random). music.m cue: in BrRaceStep and mode != 4, g_brCfgChosenTrack 0x100B3014 index t<12, t%6 in 0-4 -> race piece t%6 (desert/mountain/coast/mine/amazon = TGR Desert/Mountain/Coastline/Strip Mine/Jungle); race.trk (5,11), gamewin (12), bonus (13,14), ending, menus -> title (project lead chose title for circuit + bonus). Pieces restart only when the cue changes. Headless check: BR_LOG `music: play N, piece C` lines; scripts 35/36/37 cover mine/amazon/bonus; host_script lacks `waittext`, so use `sleep 11000` + ESC for race end.

Related: [native-renderer-not-hacks](native-renderer-not-hacks.md), [port-never-touches-decomp](../decomp/rules/port-never-touches-decomp.md), [macos-port-32bit-wasm-lane](macos-port-32bit-wasm-lane.md).

2026-09-30 replay fix (c8c374a2, native/varblock.c): same empty-stand-in class -- br_objlife.c's BrExt_10067880/10067900 (BrVarSave/Load) were inlined no-ops, so the last-lap snapshot never saved/restored and the replay quit after 2 s. An @replaces on a wrapper does NOT reach callers in the same TU (inlined): override the caller too. Ground-truth method that found it in minutes: a Python script on tools/brbox_drive (make_box/Driver/attach) with unicorn UC_HOOK_MEM_WRITE / UC_HOOK_CODE on the suspect globals -- the original runs the 21_quickrace_finish script in ~1 min.
