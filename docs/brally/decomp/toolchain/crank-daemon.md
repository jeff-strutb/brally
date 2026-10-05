# Crank daemon

*Recorded 2026-09-08.*

> tools/crank.py is the unattended byte-exact cranker (2026-09-07) -- how to run it, what it did on day one, and the loop it needs a person for (miss -> hand-solve one -> new lever)

**tools/crank.py (2026-09-07)** is the zero-token churner the project lead asked for
("a script that sits and churns away and byte-exacts functions using my
computer power"). Deterministic lever sweep in the function's OWN TU,
scored register-blind; match -> sweep -> commit -> filing.csv (with
dropped-row restore); miss -> corpus census -> `@t4-pass` line ->
`t3.py --qualify` -> tag if it passes. Learns: per-lever accept stats
order the levers, samebase record bases remembered
(build/match/crank_records.csv), tried candidates per file hash never
recompiled (crank_state.json). Never commits into a sliceN file: writes
build/match/crank_refile.txt for the HAND move (rule 6).

**Run it:** `nohup .venv/bin/python tools/crank.py --all --max-bytes 1000 --workers 10 --loop > build/match/crank_daemon.log 2>&1 &`
(14 cores; ~6 s/compile/worker). Progress: `tail -f build/match/crank.log`.
Stop: `pkill -f tools/crank.py`. Never two daemons at once.

**Day one (2026-09-07):** 32 register-only rows, 6.5 h single worker:
4 byte-exact (0x100283C0 samebase; 0x100345F0 filepos:front;
0x10031030 split_add; 0x10064120 filepos:end + stmt order, hand-refiled
into driving/br_rbaccum.c), 24 ledgered misses, 5 too small to count.
Launched 19:1x as a 10-worker loop over 191 C diff rows <= 1000 B.

**Why:** the machine batch (ghidra_to_match --refine) returned 0 of 296 on
2026-09-07; the permuter scored raw bytes in a wrapped TU and had none of
the 09-03..06 levers. What is left is one SOURCE FACT per function.

**How to apply:** when the daemon misses, read the best-variant residue in
crank.log, hand-solve ONE (the corpus census names solved twins), add it
as a lever in crank.py (samebase is the template), let the loop re-run.
Every crank miss with >= 10 compiles is a counted Gate-B pass, so three
rounds at identical numbers certify T3 automatically -- that is per the
rule as written; if the project lead wants machine passes to count differently,
change t3.py, not the daemon.
