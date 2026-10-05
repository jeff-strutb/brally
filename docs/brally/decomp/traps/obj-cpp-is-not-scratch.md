# Obj cpp is not scratch

*Recorded 2026-09-30.*

> build/match/obj_cpp is load-bearing -- deleting it silently unplaces every C++ body and breaks the Mac wasm boot

When cleaning build/match, keep obj_O2/Od/O2y/O2p/Odp, obj_img_dll_*, AND obj_cpp. The other obj_* folders (fn_*, sc_*, ghidra_ref_*, perm_*, ...) and t3d/ are throwaway.

**Why:** on 2026-09-30 a repo cleanup deleted obj_cpp. image_build.py `_cpp_rows` skips a C++ row whose `<base>_sweep_<VA>_<i>.obj` is missing instead of compiling it, so t3manifest placed no C++ bodies. The Mac wasm build then died at boot with "host import not implemented: Phase::Phase" (??0Phase@@QAE@XZ -> 0x10041B60 BrOptObjCtor) and a bus error. `tools/cpp_sweep.py` rebuilds it; t3.py and t3obj.py read it too.

**How to apply:** before any build/ cleanup, grep tools/ and ports/ for the dir name, including `OBJ_DIR` constants. After a cleanup, check the boot with `BR_HEADLESS=1 BR_SCRIPT=tools/brbox_scripts/10_boot.txt build/wasm/brally` (no "not implemented" lines). Related: [port-build-drift](../../port/port-build-drift.md).
