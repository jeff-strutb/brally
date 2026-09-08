# Incidents (why the standing orders exist)

These are not current procedure. They are why `CLAUDE.md` is short and strict.

## Reference binary is Glide. D3D was matched twice.

`d98f480` (2026-08-15) and `a7eb7cd` (2026-08-19) both pointed the matcher at
`BRD3D.dll`. Pairing one binary's bytes with the other's function map
disassembles the wrong bytes at a right-looking address. `tools/refcheck.py`
fails if the corpus is not Glide-keyed. `BRD3D.dll` statically links ~100 KB
of CRT; matching it means matching Microsoft's CRT.

## `autofile.py` filed matches into `sliceN_MM.c` by address

That is where 570 stranded functions came from. The mover was "deleted"
2026-09-03 and the file was left in the tree, still willing to insert into
slices. It now refuses to run. A match is born in its module; the pre-commit
hook rejects a new VA in an existing address batch.

## `claim_lane.py claim N` handed out the giants

Its rank file (`build/match/triage_rank.csv`) is a 2026-08-28 snapshot. Once
SHAPE rows were gone it scored FrameDraw, CtlAiBody, ObjDlBuild, Tex3dExpand
as a "20 small functions" lane (2026-09-07). Lock only through
`tools/t4lane.py --claim` (`claim --va`). Bare `claim N` is a hard error.

## Colouring walls are not source-permutable

Permuter 0/95, refine batch 0/258 (commit `5a4a338`), crank 4 exact / 536
miss. `crank.py --all --max-bytes 1000000 --loop` then ran 13 hours, including
`divergence.py --key 6` on Tex3dExpand (the playbook already said key 10).
Crank is a Pool A lottery. `--all --loop` and `--all --max-bytes > 400` refuse.

## A rule nothing checks is a preference

`fileaudit.py` used to fail unconditionally while any backlog remained, so
nobody ran it. Ratchets (undescribed 0, batches 58, stranded 11) exist so the
check can be green and still bite when a number goes up.
