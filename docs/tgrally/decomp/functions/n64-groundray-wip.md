# N64 groundray wip

*Recorded 2026-10-07.*

> BrGroundRay 0x8021F380 (cartick.c) T4 2026-10-06, 5be66869 (session cd04f8), gate 708/0; levers: locals for pEye x/y on one line, continue-guard blocks lengthen loop constants' live ranges (uopt nocs divisor)

**DONE 2026-10-06, commit 5be66869, image gate 708/0.** d7b110 took it 387 -> 64; cd04f8 finished 64 -> 0 by hand.

Levers (each one reasoned edit, confirmed by diff and the instrD7 allocator log):
- ROM head loads pEye->y into f20 and pEye->x into f18 (uopt range) = locals: `x = pEye->x; y = pEye->y;` then `origin.y = y; origin.x = x;`. 64 -> 14. Two webs tied on save are taken in ascending web number (def order), so x is defined first; writing both defs on ONE line lets as1 issue y's load first (line tie-break, L59). 14 -> 10.
- Last residue was FP colour ORDER (bestFar f24, bestNear f26, 5.0 f28, 1.5 f30). save = net / nocs, nocs = 2 + ((occ + live-in blocks - 2) >> 2) (L7/L29). 5.0 needed one more divisor step = exactly +2 blocks inside the loop. `if (!(dist <= 0.0f && dist < bestFarDist)) continue;` (fjp + ujp per test = empty blocks, no code change) instead of a wrapping if: EXACT. The natural `dist > 0.0f` spelling flips the compares (c.lt + bc1t), so the negated form is the source.
- Tooling: build/tgrally/n64/search/8021F380/cd/ has fp.sh (FP web save/nocs/X table from the CDX log), uc.py + cap.py (cfe ucode control flow per source line from my own capture; the shared capture dir is used by other sessions, so match cwd.txt by realpath).

Related: [n64-spill-slot-order-lever](../levers/n64-spill-slot-order-lever.md), [n64-camchasestep-wip](n64-camchasestep-wip.md), [feedback-hand-transcription-only](../rules/hand-transcription-only.md).
