# Mac remastered lighting

*Recorded 2026-09-29.*

> Experimental "Remastered" modern-lighting layer in the Mac 32-bit lane (host_fx.m), the ~ toggle, and the screenshot capture race it exposed

2026-09-29: the project lead asked for an experimental PoC of UE5-style lighting on the Mac port (project lead said it would be rolled back later). Built uncommitted in ports/macos/wasm: new host/host_fx.m; edits in host/host_glide.m (G-buffer MRT: att1 normal+coverage RGBA16F, att2 world pos+class RGBA32F; class 0 2D, 1..1.9 lit geometry (+0.9*fog), 3 sky), native/render.m (world pos = clip * inverse of the DL projection slot 0x105CCD00, which holds view x projection; world is Z-up, ~1 unit = 1 m), host/host_app.m (one line: hfx_key). Tilde (~, keyCode 0x32) toggles Remastered/Original live, NSUserDefaults "Renderer"; BR_FX=0/1 env; headless default off.

Weather index g_brCarPhysWeather 0x104B15E8: 0 sunny, 1 fog, 2 storm, 3 snow, 4 RAIN AT NIGHT (dark sky, lit windows). Menu cycle from sunny: 4,3,2,1,0. Parallel scripted runs share saved config unless each gets its own BR_SAVEDIR.

**Capture race (real bug, fixed in host_glide.m flush_wait):** shots taken when no command buffer was open read the target while the GPU was still writing it -> black dotted 32-px tile streaks. This is what earlier notes called the intermittent "race-sky corruption" in port screenshots; it was never on screen.

**Why:** later sessions may be asked to keep, commit or roll this back; another session (high-quality car model) built on hfx_on/compfs class 3.5-4.5 and hglide_native_pass.
**How to apply:** check `git status ports/macos/wasm` before rolling back -- peers' edits live in the same files. GPU cost measured at 2560x1920: whole frame 10-14 ms with FX (shafts ~2 ms, shadows ~1.5 ms). Related: [mac-native-renderer-2026-09-29](../port/mac-native-renderer-2026-09-29.md), [native-renderer-not-hacks](../port/native-renderer-not-hacks.md).

## 2026-09-30 (a603935e)
- Wheel surface byte (+0x1A0): per the game's grip tables at 0x100b4c30/0x100b5050, **3 and 11 are sealed (tarmac)**, everything else loose. Old code wrongly treated 3 as snow.
- Smoke/dust are now the port's own (fx_sim: slip + surface), game lists B0/AC ignored; game dust sprite tex draws with dmask ON, kind 0 -> suppression is `(!S.dmask || !kind)`.
- Plume look: summed kernels + 3D RG8 noise (mk_plume_noise), SOFT noise modulation. The Schneider coverage remap turns thin mist into isolated blobs -- don't use it for low-density media.
- Halo around car = TAA/MB car box swallowing road; box floor now = wheel contact plane in mv.cur[i][3].w.
- Capture hygiene: BR_SHOT_EVERY=0 means EVERY frame (filled the disk once); run from a private copy of the binary (other sessions `pkill brally`); BR_VCLOCK=4 capture ~6 s.
- GPU cost per frame: measure with BR_GPUFRAME=1 (frame-to-frame GPU end spacing). The old BR_FX_STAT "whole frame" span (21-24 ms) OVERSTATES it: two frames in flight overlap. True cost 2026-10-01 at 2560x1920 night: 13-15 ms = the BR_FX_PROF pass sum; nothing hidden.

## 2026-09-30 later (dd654ce1, 6d469ad1)
- Collision mesh for a road map: tri idx u16x4 at *0x106EECE4, verts f32x3 at *0x106EECEC, surface byte (&7, 3 = sealed) at *0x106EED6C; tri count = (flags-faces)/8, index 0 unused (br_collgrid.c). host_fx roadmap rasterises it once per track.
- The game's colours are ALREADY night-darkened (~0.15 linear): anything that lights "albedo" from game colour (headlights) must divide by g_wdark.
- Pale rim round every object = screen-space ring average in the material pass crossing onto foreground; fixed with same-surface ring (half-res ringfs). Suspect screen-space neighbourhood gathers first for halos.
- TAA/MB halos: class-aware clamp/blur (car class 4 vs world).
- Scanned Snow015 is snow-over-grass; rock/snow now take game colour + scan relief only.

## 2026-09-30 night: box under the car
- Since 024c78e4 compfs relights `ga` (albedo x baked light), but `col` still holds the game's own projected car shadow + fog. Anything in compfs that reads `col` for surface tone (ring pass -> asphalt tone, bump) brings the old shadow back as a flat dark box with ghost "dots" (ring taps). Fixed: ring + bump read `ga` (rgb*a). Remaining `col` reads are hue-only classification.
- Debug method that found it: private relink (host/*.m recompiled into scratch, linked with build/wasm/nat generated objects) with extra BR_FX_DEBUG early-returns per compfs stage. BR_PICK is useless for 3D draws (clip-space verts).
