# Goal coverage not playability

*Recorded 2026-08-18.*

> THE governing objective - a complete MAME-standard bit-exact decomp via the most EFFICIENT path. Playability is only a side effect, NEVER a driver.

**THE NORTH STAR (project lead stated this repeatedly and forcefully; stop making the project lead
repeat it):** the goal is a **true MAME-standard decompilation** - every
function transcribed and verified bit-exact against the original - reached by
the **MOST EFFICIENT PATH POSSIBLE**. That is the ONLY objective function.

**Playability (a car drives, menus navigate, a race renders) is a SIDE EFFECT,
never a reason to prioritize anything.** Do NOT order work by demo value, by
"which milestone looks impressive," by "what makes a playable race," or by
"what gives a usable result." Those are all wrong criteria for this project.

**Correct ordering = pure coverage efficiency:**
- Order functions by **cheapest-to-verify given current dependencies** (leaves
  up). Never attempt a big function before its sub-leaves are done - that forces
  stubs/guesses and rework (that's what wrecked the 0x1000A110 attempt).
- Batch similar functions; build/reuse verification harnesses once.
- Drive ONE metric up monotonically: **verified-function coverage** (see
  README's function-count + byte measures). Zero rework.
- NO demo detours. NO "path to playable" framing. NO presentation overhead
  beyond a coverage log. Report progress as verified count, not milestones.

**Why this bit:** across sessions the work drifted toward playability (the
physics/collision push, "path to a playable race" tables, "finish the menus so
it's usable" recommendations). Every one of those was the wrong optimization
target and the project lead had to correct it over and over. It kept recurring because
it was not recorded as the governing rule. It is now. Obey it by default.

Related: no-token-thrashing (efficiency of execution). This file is about
WHAT to optimize; that one is about not wasting tokens doing it.
