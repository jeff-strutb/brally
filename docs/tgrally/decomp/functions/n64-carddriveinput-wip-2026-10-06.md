# N64 carddriveinput wip

*Recorded 2026-10-06.*

> BrCarDriveInput 0x80222050 (carphys.c) T4 2026-10-06 4f6d755d (eba7ae), gate 679/0: levers - ixa array loads evaluate first in cfe (use plain fields), inline abs ternaries instead of a shared abs var, variable sharing read off the frame, PRE barrier via load placement

**DONE 2026-10-06, commit 4f6d755d, image gate 679/0** (blind 751 -> 0, all by reading the ROM). car.h BrCar.xe14[5] became five floats xe14..xe24 (entinit.c updated, still exact).

**Levers that generalise (N64 IDO 5.3):**
- **cfe evaluates an array-element load (ixa) BEFORE its sibling sub-expression**, regardless of source order (`S + c[3]` and `P * c[4]` both emit the array load first). A plain struct field keeps source order. If a commutative op has a lone memory operand first in ours but second in the ROM, the original used a named field, not an array.
- **Shared inline ternary temps:** writing `a = |s|; if (a < k)` puts every abs in one variable web that crosses regions and steals a colour; the ROM used `if ((s < 0.0f ? -s : s) < k)` (cfe pool temp). Fixed the sp colour, the dead-zone 0.0f piece and the 0.0f web in one go.
- **Read variable identity off the frame:** homes of spilled vars pin declaration order; 8-byte rounding hides one word, so check the cfe/uopt temp offsets (here SGN temp 0x34, sqrtf temp 0x38) to count locals exactly. ROM shared: upper bound/torque = t, arcade bonus = lo, slew flag ch = short k, left/right test = g, end delta = lim; ctlt separate (else flags' web steals a0 from the xe30 CSE).
- **Constant webs merge by value+dtype:** `flags & 0x100000` (unsigned) and `BrPadConsume(pad, 0x100000)` formed an a1 web placing li early; prototype `int bit` split them while `0x10000` (flags & pad->flags, both unsigned) stays one a0 web.
- **PRE hoist:** a load reachable from an upstream occurrence and anticipated downstream gets hoisted into an if-chain head; moving `flags = pad->flags` before the gear-up test (as the ROM's delay slot shows) removed the hoist.
- `-(SGN(s) * sqrtf(...))`: left call operand evaluated and spilled first; `x / 2` (int) divides; `car->xe3c = 0` int store keeps the zero off the coloured web; final rpm store before the brake clear.
- Tooling (scratch w/): symmap.sh maps uopt syms to homes via CDX_DETAIL_WEB raw10; pr/ probes for cfe/uopt behaviour; blind.py from search/80259D14.
