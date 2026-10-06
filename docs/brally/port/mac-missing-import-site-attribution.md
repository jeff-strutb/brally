# Mac missing import site attribution

*Recorded 2026-09-30.*

> Mac wasm "host import not implemented: X::X" mid-game = t3manifest filed a certified file's call sites under a C draft of the same VA (fixed cb340c80, 2026-09-30)

2026-09-30: Championship died at car setup with "host import not implemented: Phase32F::Phase32F". The ctor body existed (0x100418C0); the CALLER 0x1004AEE0 is listed in report.csv under both br_seasonscreen.c (draft) and BrExt_10052030_1004AEE0.cpp (certified). t3manifest attributed sites via report.csv's first file, placement via the @t3 file, so the certified file had no sites and its call fell to a host stub. Fixed in cb340c80: sites use the placement's file.

Same day, 2fd0aa2c: host hfmt lacked MSVC `%C`, so the CD check (`"%C:\\"`) failed and every CD-gated page said "insert Boss Rally CD". The app bundles the disc as virtual D:, no real CD needed.

Known benign: "free of non-heap" x4 at quit after a championship = original BrMakeEnemyCarColorPanels (0x1005EDC0, T4) leaves 4 list slots uninitialised; host free rejects them.

**How to apply:** for a new "not implemented" stub, diff `w_missing(` lines in build/brally/wasm32/c/w2c_link.c and check sites.csv for the caller's file before touching w2c. Related: [obj-cpp-is-not-scratch](../decomp/traps/obj-cpp-is-not-scratch.md), [port-build-drift](port-build-drift.md).
