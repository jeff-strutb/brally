# Shared tree partial commit

*Recorded 2026-09-16.*

> In a shared working tree, `git commit -- <file>` commits the WORKING-TREE version (sweeping in other sessions' uncommitted hunks); pathspecs do NOT protect you - partial-stage your hunks and commit the INDEX.

**When several parallel sessions share ONE working tree, a shared file (e.g.
tools/t3b_verify.py) shows EVERYONE's uncommitted edits together in
`git status` / `git diff`. Committing your change then has a trap that the
usual "use pathspecs" rule does NOT cover.**

**Why:** `git commit -- tools/foo.py` commits the WORKING-TREE content of that
path, IGNORING the index. So even after you carefully `git apply --cached` only
your hunks, a pathspec commit re-includes the other session's uncommitted hunks
in the same file. I did exactly this once - swept another session's
`_EXACT_REGIONS` into my commit - and had to recover.

**How to commit only YOUR hunks in a file that also holds another session's
uncommitted work:**
1. `git diff HEAD -- <file> > full.patch`; split into hunks (`@@` boundaries)
   and keep only yours (classify by a marker string unique to each change).
2. `git apply --cached mine.patch` - stages only your hunks.
3. Verify: `git diff --cached --name-only` (only <file>), and
   `git diff --cached | grep -c <their-marker>` == 0.
4. `git commit` with NO pathspec and NO `-a` - commits the INDEX (just your
   staged hunks). Other files stay unstaged and out of the commit.
5. Sanity: `git show HEAD:<file> | python -m py_compile -` (a partial file must
   still be valid without the other session's hunks - fine when hunks are in
   disjoint regions).

**Recovery if you already pathspec-committed their work:** `cp <file> backup`,
`git reset --mixed HEAD~1` (leaves the working tree untouched - both sets of
edits stay on disk), then do the partial-stage-and-commit-index dance above.

**Better still:** if the other session's hunks are a coherent unit (their
machinery coupled to their own profile), coordinate - let them commit their
file, or split by FILE so each session owns whole files. See
[parallel-session-clobber](parallel-session-clobber.md), the commit-every-match rule (which say "commit
with explicit pathspecs" - TRUE for isolating unrelated files, but insufficient
here because pathspec commits the working tree, not the index).
