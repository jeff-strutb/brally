# Parallel session clobber

> A parallel session silently destroys work four ways - a wiped src edit, a deleted committed docs section, a revert that does NOT move HEAD, and its dirty shared file riding along in YOUR commit.

Parallel sessions in this repo do not merely move HEAD under you. Twice in one
session (2026-09-03) one destroyed work outright:

1. **An uncommitted edit to `src/brally/core/slice6_76.c` vanished** while I was
   probing it. A `tools/brally/refile_group.py` run in another session rewrote the
   file wholesale; my new function was simply gone, with no conflict and no
   warning. HEAD had moved 7746938 → 8607a8f in the meantime.
2. **A COMMITTED section of `docs/brally/VC5-IDIOMS.md` was deleted.** Commit
   `4f477a9` appended a 63-line idiom entry; commit `59ac8ed` (another
   session, ~10 minutes later) rewrote the file from a copy it had read
   BEFORE mine landed - 48 insertions, 88 deletions, my whole entry among
   them. `git log` shows both commits; nothing flags the loss.

**Why:** these tools and sessions write whole files from an in-memory copy
rather than editing in place, so anything that landed between their read and
their write is discarded. Being committed protects history, not the file.

**How to apply:**
- **Commit a new function the moment it sweeps clean** - do not hold it
  through another probe cycle. This is what the commit-every-match rule
  already says; the new fact is that the loss mode is silent deletion, not a
  merge conflict.
- **After appending to a shared doc (`docs/brally/VC5-IDIOMS.md`, `README.md`,
  `docs/the notes index`), re-grep for your text before ending the session.** If it
  is gone, recover it with `git show <your-sha>:<path>` and re-append; do not
  retype it.
- **Never rewrite a shared CSV or doc from a full read** - my own
  `config/brally/filing.csv` rewrite flipped 872 lines from CRLF to LF and had to be
  fixed in a follow-up commit. Splice the one line in, preserving the file's
  line endings (`b"\r\n"`), and check `git show --stat`.
- Always `git commit -m "..." -- <paths>` (never a bare commit), and
  `git status --short src/brally/` before taking a target.

## Two more modes, 2026-09-03 session 2 - and one of them corrupts a COMMIT MESSAGE

3. **A REVERT THAT DOES NOT MOVE HEAD.** Twice in one session my edits to
   `src/brally/core/drawing/br_dlcmd.c` vanished and `git status` went **clean** with
   HEAD **unchanged** - the file was simply back at its HEAD content. Both of
   the modes above involved HEAD moving, so "did HEAD move?" is NOT a
   sufficient check. The only reliable test is to `grep the file for your own
   text`.
4. **The second wipe landed BETWEEN my edit and my commit**, so commit
   `ff0873f` recorded a residue note describing two code changes **that were
   not in the tree**. The message was wrong, the measurements in it referred
   to code nobody could see, and nothing failed. `ad8e03d` re-applied them.
   A silent revert does not just lose work - it can make you publish a false
   record of what the tree contains.

** A PATHSPEC COMMIT IS NOT THE PROTECTION THE INDEX IMPLIES.**
`git commit -m "..." -- <path>` still takes that path's **whole working-tree
state**, not your hunk. `config/brally/filing.csv` was already dirty with another
session's module reassignments; my one-line splice was correct, but commit
`caefe5d` swept **40 of their lines** in under my message. The pathspec stops
*other files* riding along; it does nothing about *other people's changes in
the file you name*.

**How to apply, sharpened:**
- On a shared file, **commit immediately after every edit that survives a
  compile** - do NOT hold it until it is byte-exact. A probe cycle is long
  enough to lose the work, and this lane's rule of "commit when it sweeps
  clean" is too late.
- **After committing, grep the file for your own text.** Not `git status`,
  not `git log` - both looked healthy while the work was gone.
- **Before committing any shared CSV or doc, run `git diff <that file>` and
  read it.** If it carries changes that are not yours, leave it out of your
  commit entirely rather than explaining it afterwards. Checking
  `git show --stat` after the fact is too late to fix attribution.
