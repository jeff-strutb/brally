# Readme status was stale

*Recorded 2026-08-17.*

> The README's "Status"/"entry point not ported" claims were stale and wrong; the entry-point spine is done. Query the tree, not the doc.

# README Status section was stale (corrected 2026-08-17)

The README claimed `RallyMain` and its callees were unported and "the next work."
FALSE - verified by `@implements` bodies + passing tests:
- `0x10019730` main loop → `startup/br_mainloop.c` (test green)
- `0x10019670` window → `startup/br_window.c`
- `0x100194C0` wndproc/input → `controls/br_input.c`
- `0x1001D8A0` = **`BrDxDetect`** (DirectX version), NOT "the argument parse" →
  `startup/br_dxver.c`. It never touches the command line.
- `0x10007E80/0x10063860/0x1006D1A0/0x10063060/0x10056260` also ported.
- Only 3 init callees remain on the counted frontier: `0x10007F10`, `0x10007F40`, `0x10009C00`.

Lesson: **`tools/isported.py` is comment-shape heuristics and NOT authoritative**
(it reported all eleven absent). Use `tools/whereis.py` and grep for `@implements`
(case-insensitive - source uses UPPERCASE hex, e.g. `0x1006DE70`). The manifest also
undercounts functions that are implemented but lack an `@implements` tag (BrMat3Solve
was one). Always query the tree before trusting any coverage/status prose in the README.

The real frontier is milestone 6: the OBB collision RESPONSE - see [collision-response-decomp-state](../../port/collision-response-decomp-state.md).
