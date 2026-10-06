# BrCarDrawVehicle coupled-cluster attempt (one shot)

2026-08-31. Coordinated edit of colour-pack overlapping loads, cull XOR
+ reflect-flag ternaries, delayed lodOff store, scaled-index lights
bytes, and flag290C=1 via field store. Scored once.

Result: **59 regions** (floor is 41). Reverted. File clean at bcd873b.

What DID land in that one compile (not kept):
- all 10 orig `neg`/`sbb` insns
- 6 `mov byte [esp]` (pack slots)
- 3 `mov byte [r*8+base]` lights stores
- pCar in **ebp**, zero-reg **esi** (not ebx/ebp)
- frame `sub esp, 0x44` (orig 0x4c)

REGNORM 53 extra / 74 miss looked structurally closer than 75/102, but
the frame + zero-reg rotation exploded the region count. 41 is the
coupled-region floor for a single coordinated attempt.
