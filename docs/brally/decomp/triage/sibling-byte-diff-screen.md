# Sibling byte diff screen

*Recorded 2026-09-04.*

> Byte-diff the extracted ORIGINALS against each other, grouped by identical size - it finds real cause-families (a 7-member clip-plane family, +5 byte-exact) that no residue-class screen sees.

Before writing a line of C for a suspected family, **diff the extracted
original bins against each other**, grouped by identical size:

```python
# for each size class in build/match/orig with no report.csv row,
# count differing bytes against the first member
```

On 2026-09-03 this found `0x1001F0D0 / F2B0 / F3F0 / F530 / F670 / F7B0 /
F8F0`: six functions of 311 bytes and one of 303, differing in **6 to 8 bytes
each**, every one of them either a field displacement in one `fld`/`fadd`
pair or a byte of a `call rel32` displacement. One macro body, seven
instantiations, five byte-exact the same afternoon. A full pass over the tree
found only one other family left (an 86-byte pair, 1 byte apart, which turned
out to be a merged map row and yielded 4 more).

**Why this works when residue-class grouping does not:** `triage.py --residue`
groups by how the DIFF looks, which is a symptom; members share nothing
causal, and transforms minted from one have historically swept ~0 siblings.
Diffing the originals groups by what the CODE is, which is the cause. It also
costs nothing - no compile, no sweep, just file reads - and it works on T1
functions that have no tree presence at all.

**How to apply:** run it at session start alongside `tiers.py --list T1`. A
size class where every member is within ~n/25 bytes of the first is one body
with parameters; work out what the differing bytes select (a field offset, an
immediate, a call target) and write the body once as a macro whose argument is
that thing. Do NOT factor it as a function taking a callback - that emits an
indirect call the original does not have. See [parked-is-not-walled](parked-is-not-walled.md) and
the clip-plane entry in the repo's `docs/VC5-IDIOMS.md`.
