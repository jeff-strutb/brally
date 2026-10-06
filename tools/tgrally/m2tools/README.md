# N64 M2 lab tools

The probes, screens and searches used while taking N64 functions from T3 to
T4 (byte-exact) in October 2026: ROM-listing dumps (`dv.py`, `dvx.py`,
`getfn.py`), residue screens, allocator traces and the search drivers
(statement order, declaration order, operand flips, line joins, ring
injection). Since 2026-10-05 the N64 rule is hand transcription from the ROM
listing, so the search drivers are kept as the record of what was tried, not
as the working method; the dumps and the read-only screens are still the
first tools to reach for. Run from the repository root; scratch output goes
under `build/tgrally/n64/`.
