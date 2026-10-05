# Remaster skies

*Recorded 2026-09-29.*

> Remastered per-track, per-weather sky panoramas in the Mac port -- where sources, pack, loader and shader hook live; resolution and mapping

2026-09-29: 35 sky panoramas (7 envs x 5 weathers: desert mountain coast mine amazon race bonus x clear fog storm snow night), one gpt-image-2 text-to-image each (9 credits), 16:9 1536x864. Sources/prompts/ledgers in gitignored `ports/common/models/sky/` (tools/img.py, prompts/, source/<env>_<weather>.png); `ports/macos/tools/remaster_sky.py` packs them (min-error seam cut for a horizontal wrap, 2x Lanczos -> 2560x1728) to `sky/pack/`. Runtime: `host/host_sky.m` (hsky_tex: track g_brCfgChosenTrack 0x100B3014 -> env via the DLL's sky table; async decode; BR_SKY_DIR, BR_SKY_ENV), sampled in host_fx.m compfs `pano` (texture 6, u.tm.z gain), 60 deg per copy x6 around, horizon on the bottom row, 35 deg at top (chase cam measured ~39x30 deg centred on the horizon; the first 180x75 layout showed only bottom haze -- user saw 'no skymap'). Only drawn in the relight pass, so it follows the ~ Remastered toggle. package_app.sh copies pack to Resources/sky.

**Why:** the project lead asked for Meshy skymaps for every level and weather, toggled with Remastered.
**How to apply:** true detail is ~1280 px per 60 deg (upscale is interpolation only); more resolution needs more credits -- see [meshy-credit-budget](meshy-credit-budget.md). host_fx.m shared with the lighting session ([mac-remastered-lighting-2026-09-29](mac-remastered-lighting-2026-09-29.md)); split agreed: I owned the sky block. Committed: 237dfda9, 2b572e8e (peer, host_fx.m), 342c1c59 (mapping).
