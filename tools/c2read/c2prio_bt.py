# Byte Tactics tools/c2prio.py (github.com/HectorBailey/byte-tactics, MIT, see
# LICENSE.byte-tactics); only comments changed (their references to the
# Byte Tactics documents); tools/c2read/c2read.py drives it.
"""Show C2's own register candidates for one function: priority, order, register.

    uv run tools/c2prio.py 0x4cf570
    uv run tools/c2prio.py 0x4cf570 build/scratch/0x4cf570/try.cpp
    uv run tools/c2prio.py 0x4cf570 --trace      # also every colouring step
    uv run tools/c2prio.py 0x47d2e0 --blocks bit,los   # where each priority comes from
    uv run tools/c2prio.py 0x424c00 --inline           # also the /Ob2 inline decisions
    uv run tools/c2prio.py 0x424c00 --symbols g_game   # also symbol ids and the file's symbol count
    uv run tools/c2prio.py 0x4c8bb0 --frame            # also the frame layout: counts, slots, offsets
    uv run tools/c2prio.py 0x4c8bb0 --rotation         # also the expression temporaries' rotation
    uv run tools/c2prio.py 0x4c0820 --ids              # also the freed ids each split piece reuses

This compiles the function's file with the real back end (C2.EXE) running under
a debugger and reads the global register allocator's own data
(docs/c2-regalloc.md) for that one function. For each register candidate (a
web of a local, parameter, global, compiler temporary or constant) it prints,
in the order FUN_0041bdd7 sorted them:

  - its priority (candidate +0x0c, from FUN_0040ee1d) and the tie key at +0x40
    (the number of the last tuple that writes it); the list is sorted by
    priority, then +0x40, both larger first;
  - its spill cost (+0x3c) and reference count (+0x24);
  - the registers it is still allowed when the list is built;
  - the register it gets, or `split` with the registers of its pieces, or
    `memory` (`immediate` for a constant);
  - a name: the local, parameter or global, `temp` for a compiler temporary,
    `local temp` for an unnamed front-end local, a constant's value, and the
    source lines that read or write it.

--trace adds every colouring step: each register choice with the registers
still allowed and FUN_0041b785's costs, splits, re-sorts and skipped
candidates.

--blocks [NAMES] adds, for every candidate in the list (or for the ones named,
comma-separated: a name as the table prints it, or #id), each basic block's
share of its priority in FUN_0040ee1d's first pass: the block's weight w and
count K, then w * K * cost for a block that references the candidate, or -w * K
for one it is live through without a reference. The shares add up to the
priority in the table.

--inline adds, before the table, C2's /Ob2 inline decisions for the function
(Byte Tactics guide, "The /Ob2 inline budget, read out of C2.EXE"): its IL
size and budget, then every call site to an inline candidate in the order C2
visits them (the depth-1 sites in source order, each inlined callee's own
sites right after it), with the depth, R (this level's sites still to come,
this one included), the budget left at this level, the callee's IL size and
whether it was inlined. A callee is inlined when its IL size is at most the
budget left, or under 41 whatever the budget. Its cost is its IL size if that
is 41 or more, else 0; its own sites start from (budget left - cost) / R, and
every level above loses what is inlined below it as well.

--symbols NAMES adds, before the table, the symbol id of each name
(comma-separated: a global or function by its plain or mangled name, a local
or parameter that is a register candidate, or a type, which never reaches C2
itself, by the first of its members C2 has), in full and in 16 bits, plus the
file's symbol count and C2's own counter. The front end numbers every
declaration in the file with one counter (an unused `extern int` takes 1, a
prototype 2, a one-member struct 7; there is no separate count of types), and
some register and operand-order ties follow bits of these ids
(docs/c2-regalloc.md, "Symbol ids").

--frame adds C2's frame layout, which runs after the allocator: the locals left
in memory in C2's list order with each one's size and reference count and the
slot it opens or joins, then the slots in their final order with their offsets
from the bottom of the locals (Byte Tactics guide, "Get the frame layout from
the reference counts"). A compiler temporary's spill home is named after its
candidate.

--rotation adds the expression temporaries in code-generation order, each with
the rotating pointer before it, the register it got and the rule that chose it
(docs/c2-regalloc.md, "Temporaries").

--ids adds C2's freed candidate ids. FUN_0040ecd2 frees a candidate by pushing
it onto a list (head at 0x493230), and FUN_0040ebb6 gives a new candidate, such
as a split piece, the id on top of that list, so the id freed last is reused
first; a piece gets a new id only when the list is empty. It prints each free
during allocation, with the pass that made it, and for each batch of splits the
candidates split, the freed list as it then stands, the id each piece takes
from it and the split candidates' ids freed afterwards. A step's splits run in
candidate id order, and after a region the one split later is reloaded first,
so this shows why two reloads come out in a given order and which drop or
piece to add or remove to change it.

How: a copy of C2.EXE under build/c2prio/<run>/ has `jmp $` at its entry
point. CL runs that copy (/B2), winedbg attaches to it with its gdb server, and
gdb runs this same file as a Python script that puts the entry bytes back and
records what the allocator does at a dozen addresses. The compile then finishes
normally. Needs gdb with Python (the distribution's gdb package); several runs
can go at once. To debug the tool itself, --keep keeps the run directory with
gdb.log, and C2PRIO_DEBUG=1 logs gdb's remote protocol into it.
"""

import json
import os
import struct
import sys

try:
    import gdb  # this file is also the script gdb runs inside the debugger
except ImportError:
    gdb = None


# --- C2.EXE (VC++ 5.0 SP3) addresses -------------------------------------------

C2_SHA256 = "e75aecaf4073b0817ffb638cae5fff636b2e2d1a090daabf8f68dbc954515fae"
ENTRY = 0x452797          # PE entry point; the copy spins here (EB FE) until gdb attaches
ENTRY_BYTES = b"\x55\x8b"  # its first two bytes, push ebp; mov ebp, esp

REFS = 0x416A8D           # FUN_00416a8d entry (from FUN_00414a64), once per function; ecx = function
NEWCAND = 0x40EC45        # FUN_0040ebb6's ret: a new candidate in eax (ids of freed ones are reused)
DEFREF = 0x416AD3         # FUN_00416a8d: a tuple writes a candidate; edi = candidate, eax = tuple number
USEREF = 0x416C94         # FUN_00416a8d: a tuple reads a candidate; eax = candidate
DRIVER = 0x416E6A         # FUN_00416e6a entry, the global allocator
SORTED = 0x4172F4         # FUN_0041bdd7 has built the sorted list; ebx = class
LOWSPILL = 0x417013       # spill cost <= 0, not worth a register as it stands; esi = candidate
DEFER = 0x417246          # FUN_0045aaf9 put it back into the list; ecx = candidate
CHOOSE = 0x4171CF         # call FUN_0041b785; esi = candidate, edi = its interference set
CHOSEN = 0x4171D4         # back from FUN_0041b785; register at candidate +0x10
SPLIT = 0x439385          # FUN_00439385 entry, live-range split; ecx = candidate
RESORT = 0x416FE2         # priorities recomputed (FUN_0040ee1d again) and the list re-sorted
END = 0x417317            # every register class done

# Freed candidates, for --ids. FUN_0040ecd2 frees a candidate: at FREECAND it
# has pushed it onto the freed list (head at FREE_HEAD, chained at +0x2c, the
# id kept at +0x1c); edx is the id and [esp+8] the return address, which says
# which pass freed it: FUN_0041c72e and FUN_0041ca9b (called from the
# allocator's entry) drop candidates before the sort, the first by forwarding
# them into their uses, FUN_004375fe takes one out of
# allocation (FUN_0041a6f8 calls it for a spill cost that is not positive),
# and FUN_00437e67 frees the candidates a split has replaced with pieces.
# FUN_0040ebb6 pops this list for every new candidate.
FREECAND = 0x40ED2B
FREE_HEAD = 0x493230
FREED_BY = {0x41C9B9: "dropped before the sort, forwarded into its uses (FUN_0041c72e)",
            0x41CCBC: "dropped before the sort (FUN_0041ca9b)",
            0x437747: "taken out of allocation (FUN_004375fe)",
            0x437945: "taken out of allocation (FUN_004375fe)",
            0x4380C1: "freed after a split (FUN_00437e67)"}
SPLIT_FREE = 0x4380C1

# FUN_0040ee1d (priorities), for --blocks. At the end of each block's tuple walk
# (BLOCK_END) the block is at [esp+0x20] (first tuple +0x1c, end +0x20, loop
# depth +0x86), ebx lists the candidates the block references that are not in
# the live list, [esp+0x10] is the live list (flag 0x10 at +6: referenced here)
# and ebp a set C2 also counts into K; +0x18 of each candidate is its cost in
# this block. At BLOCK_WK, edi = w and esi = K. C2 then adds w * K * cost to
# each referenced candidate (0x40f70d for the ebx list, 0x40f742 for the live
# list) and subtracts w * K from each unreferenced live one (0x40f72c); the
# tool repeats that arithmetic from what it read at BLOCK_END.
BLOCK_END = 0x40F5C7
BLOCK_WK = 0x40F5F5

# The /Ob2 inliner, for --inline. INL_FUNC: a function's inline pass starts;
# [ecx] is its symbol (name at +0x18, IL size in the low 16 bits of +0x64).
# INL_SITE: a call site; ebx is the callee's symbol (same fields), and C2's
# locals hold the budget left at this level [esp+0x48], the depth [esp+0x30]
# and R, this level's sites still to come [esp+0x2c]. INL_DONE: the site just
# seen is being inlined.
INL_FUNC = 0x42491E
INL_SITE = 0x424EEF
INL_DONE = 0x424F95

# The frame layout (FUN_0043f93b), for --frame. It runs after the allocator,
# during code generation. FRAME_PACK is FUN_00440cbd, called once per local in
# the list order (size, then reference count): ecx is the symbol (+0 storage
# record, name at +0x18; +4 kind; +0x20 size; +0x34 reference count; +0x38 its
# number in the slots' member sets) and edx the frame size so far. It joins the
# newest earlier slot that is at least half its size and does not interfere
# with it, or opens a new slot. At FRAME_PACKED (0x43f9dd) every local is
# packed and [esp+0x10] is the total; above 0x80 bytes C2 then re-sorts the
# slots with FUN_00459eb7 (by refs * 1000 / size, larger first, unstable), and
# FRAME_LAID (0x43f9f1) follows either way. The slots are 20-byte entries at
# [SLOTS], from index [SLOT_LO] to [SLOT_COUNT] - 1: +0 the member set (a bit
# set over the symbols' numbers), +4 the interference set, +8 size, +0xc
# reference count. Slot SLOT_LO is nearest esp.
FRAME_PACK = 0x440CBD
FRAME_PACKED = 0x43F9DD
FRAME_LAID = 0x43F9F1
SLOTS = 0x4910B8
SLOT_LO = 0x4910B0
SLOT_COUNT = 0x4910C0
FRAME_SYMS = 0x4910B4     # the symbols by their number in the member sets

# Expression temporaries (regasg.c's FUN_00435c37, docs/c2-regalloc.md
# "Temporaries"), for --rotation. TEMP is its entry: edx is the tuple (its line
# at +0x10), ROT_PTR the rotating pointer into the register table at 0x491100
# (eax, ecx, edx, then the others). TEMP_REG is FUN_00435f38, which takes the
# register chosen in ecx; its return address says which of FUN_00435c37's
# rules chose it.
TEMP = 0x435C37
TEMP_REG = 0x435F38
ROT_PTR = 0x491120
TEMP_ROUTES = {0x435CF9: "rotation", 0x435DB5: "hint", 0x435E74: "first free",
               0x435E87: "spill", 0x435E22: "spill", 0x435EEB: "spill"}

# Symbol ids, for --symbols. The front end (C1XX) numbers every symbol it
# declares, in one counter for the whole file, and writes the number into the
# IL; FUN_00420250 decodes it (15 bits in two bytes, else 31 in four) and the
# IL symbol reader FUN_004206b7 stores it at symbol +0x28 (kind at +4, name at
# +0x18). FUN_0040d5a8 hashes the symbols C2 reads into SYM_HASH by id & 0x3ff
# (chained at +0; FUN_0041f453 looks them up). The IL's header (read at
# 0x452f17, once per file) sets FIRST_ID and NEXT_ID to the front end's count
# at the end of the file, so it covers every declaration in the file, before
# or after any function. FUN_0040d5d8 then hands out NEXT_ID to the symbols C2
# creates itself (temporaries, inlined locals, COMDAT sections), for one
# function after another, never resetting it.
SYM_HASH = 0x48FB6C
FIRST_ID = 0x497DF8
NEXT_ID = 0x491050
SYM_KINDS = {1: "data", 2: "data", 3: "label", 4: "function", 9: "section", 14: "function", 16: "function"}

# Return addresses of the FUN_0040ebb6 calls that make the pieces of a split
# candidate, and the register holding the candidate being split at each.
PIECE_SITES = {0x4386B9: "ebx", 0x438C93: "ebp", 0x438E38: "ebp"}

LINE = 0x48E004           # the current line while FUN_00416a8d walks the tuples
LIST_HEAD = 0x4910D4
HASH = 0x493238           # 1024 buckets of candidates by id & 0x3ff, chained at +0x2c
COSTS = 0x4931D8          # FUN_0041b785's cost of each register (9 ints, by register number)
REG_TABLE = 0x494758      # register descriptors, 0x50 bytes each, by register number

REG_NAMES = {0: "noreg", 1: "eax", 2: "ecx", 3: "edx", 4: "ebx", 5: "esp", 6: "ebp", 7: "esi", 8: "edi"}
ORDER = [1, 2, 3, 7, 8, 4, 6]   # the allocator's register order (0x49b4a8)
LETTERS = {1: "a", 2: "c", 3: "d", 7: "s", 8: "i", 4: "b", 6: "p"}
KINDS = {3: "temp", 4: "local", 5: "param", 7: "global", 13: "const"}


def tracer() -> None:
    """Runs inside gdb: attach, let C2 go, record the target function's allocation."""
    import time
    import traceback

    t0 = time.time()
    cfg = json.load(open(os.environ["C2PRIO_CONFIG"]))
    out = {"functions": [], "inline": [], "error": None}

    def finish():
        out["seconds"] = round(time.time() - t0, 2)
        with open(cfg["out"], "w") as fh:
            json.dump(out, fh)

    debug = ["set debug timestamp on", "set debug remote 1"] if os.environ.get("C2PRIO_DEBUG") else []
    for cmd in ("set pagination off", "set confirm off", "set auto-solib-add off",
                "set breakpoint always-inserted on", "set debuginfod enabled off", *debug):
        try:
            gdb.execute(cmd, to_string=True)
        except gdb.error:
            pass
    try:
        gdb.execute(f"target remote | {cfg['relay']}", to_string=True)
    except gdb.error as e:
        out["error"] = f"could not connect to winedbg: {e}"
        return finish()
    inf = gdb.selected_inferior()
    exe = gdb.current_progspace().filename or ""
    if cfg["exe"] not in exe.lower():
        # Another run's winedbg got the port first: leave its C2 alone.
        gdb.execute("detach", to_string=True)
        out["error"] = f"connected to {exe!r}, not this run's copy of C2"
        return finish()
    inf.write_memory(ENTRY, ENTRY_BYTES)

    def rd(addr, n):
        return bytes(inf.read_memory(addr, n))

    def u32(addr):
        return struct.unpack("<I", rd(addr, 4))[0]

    def cstr(addr):
        if not addr:
            return None
        for size in (256, 32, 4):
            try:
                return rd(addr, size).split(b"\0")[0].decode("latin1")
            except gdb.MemoryError:
                continue
        return None

    def reg(name):
        return int(gdb.selected_frame().read_register(name)) & 0xFFFFFFFF

    def bitset(addr):
        """C2's sparse bit set: a header pointing at chunks {base, next, 32 bits}."""
        if not addr:
            return None
        bits, chunk, n = [], u32(addr), 0
        while chunk and n < 4096:
            base, nxt, word = struct.unpack("<III", rd(chunk, 12))
            bits += [base + i for i in range(32) if word >> i & 1]
            chunk, n = nxt, n + 1
        return bits

    leaves = {}

    def leaf(sym):
        """A candidate's leaf: +4 kind, +0x10 type, +0 storage record (name at
        +0x18, frame offset at +0xc); a constant's value node is at +0x28."""
        if sym in leaves:
            return leaves[sym]
        info = {}
        if sym:
            raw = rd(sym, 0x2C)
            st = struct.unpack_from("<I", raw, 0)[0]
            info = {"kind": raw[4], "type": struct.unpack_from("<H", raw, 0x10)[0]}
            if st:
                sraw = rd(st, 0x2C)
                info["name"] = cstr(struct.unpack_from("<I", sraw, 0x18)[0])
                info["offset"] = struct.unpack_from("<i", sraw, 0xC)[0]
                info["sid"] = struct.unpack_from("<I", sraw, 0x28)[0]
            if raw[4] == 13:
                node = struct.unpack_from("<I", raw, 0x28)[0]
                if node:
                    vraw = rd(node, 0x10)
                    if struct.unpack_from("<I", vraw, 4)[0] == 0x11D:
                        info["value"] = struct.unpack_from("<i", vraw, 0xC)[0]
        leaves[sym] = info
        return info

    def regnum(p):
        if not p:
            return 0
        if REG_TABLE <= p < REG_TABLE + 0x50 * 64 and (p - REG_TABLE) % 0x50 == 0:
            return (p - REG_TABLE) // 0x50
        return -1

    def walk(head, full=True):
        """The candidate list from its head (+0x14 is the next one)."""
        res, seen = [], set()
        while head and head not in seen and len(res) < 100000:
            seen.add(head)
            raw = rd(head, 0x44)
            f = lambda o: struct.unpack_from("<I", raw, o)[0]
            s = lambda o: struct.unpack_from("<i", raw, o)[0]
            d = {"id": f(0x1C), "prio": s(0xC), "k40": f(0x40), "spill": s(0x3C)}
            if full:
                d.update(refs=f(0x24), allowed=bitset(f(0x20)))
            res.append(d)
            head = f(0x14)
        return res

    def hashed():
        res, table = [], rd(HASH, 4096)
        for b in range(1024):
            c, seen = struct.unpack_from("<I", table, 4 * b)[0], set()
            while c and c not in seen:
                seen.add(c)
                raw = rd(c, 0x30)
                res.append((struct.unpack_from("<I", raw, 0x1C)[0], struct.unpack_from("<I", raw, 0)[0]))
                c = struct.unpack_from("<I", raw, 0x2C)[0]
        return res

    kind, names = cfg["match"]   # exact, prefix or substr

    def wanted(name):
        if name is None:
            return False
        if kind == "exact":
            return name in names
        if kind == "substr":
            return any(n in name for n in names)
        return any(name.startswith(n) for n in names)

    cur = None
    fn24 = 0

    def free_list(limit=256):
        """The freed ids, the next one reused first."""
        ids, p = [], u32(FREE_HEAD)
        while p and len(ids) < limit:
            ids.append(u32(p + 0x1C))
            p = u32(p + 0x2C)
        return ids

    def ev(what, c, **extra):
        raw = rd(c, 0x20)
        cur["events"].append(dict(e=what, id=struct.unpack_from("<I", raw, 0x1C)[0],
                                  prio=struct.unpack_from("<i", raw, 0xC)[0], **extra))

    def symbol_ids(own):
        """--symbols: every symbol in SYM_HASH as [id, kind, name], and the counters."""
        syms, table = [], rd(SYM_HASH, 4096)
        for b in range(1024):
            s, seen = struct.unpack_from("<I", table, 4 * b)[0], set()
            while s and s not in seen:
                seen.add(s)
                raw = rd(s, 0x2C)
                syms.append([struct.unpack_from("<I", raw, 0x28)[0], raw[4],
                             cstr(struct.unpack_from("<I", raw, 0x18)[0])])
                s = struct.unpack_from("<I", raw, 0)[0]
        return {"own": own, "first": u32(FIRST_ID), "next": u32(NEXT_ID), "symbols": syms}

    def on_refs():
        """FUN_00416a8d(function, candidates by web): every candidate exists by now."""
        nonlocal cur, fn24
        fn = reg("ecx")
        name = cstr(u32(u32(fn) + 0x18))
        cur = None
        leaves.clear()
        if wanted(name):
            fn24 = u32(fn + 0x24)
            cur = {"name": name, "events": []}
            if cfg.get("symbols"):
                cur["ids"] = symbol_ids(u32(u32(fn) + 0x28))
            out["functions"].append(cur)
            for cid, sym in hashed():
                cur["events"].append({"e": "new", "id": cid, "parent": None, "leaf": leaf(sym),
                                      **({"sym": sym} if cfg.get("frame") else {})})

    def on_new():
        c = reg("eax")
        site = u32(reg("esp"))
        parent = u32(reg(PIECE_SITES[site]) + 0x1C) if site in PIECE_SITES else None
        raw = rd(c, 0x20)
        sym = struct.unpack_from("<I", raw, 0)[0]
        cur["events"].append({"e": "new", "id": struct.unpack_from("<I", raw, 0x1C)[0], "parent": parent,
                              "leaf": leaf(sym), **({"sym": sym} if cfg.get("frame") else {})})

    def on_ref(c, rw):
        # FUN_00416a8d's locals: the tuple at [esp+0x14], its number at [esp+0x20].
        # The line is the tuple's own (relative to the line before the body's
        # `{`), or failing that the last one FUN_00416a8d saw.
        frame = rd(reg("esp") + 0x14, 0x10)
        own = struct.unpack("<H", rd(struct.unpack_from("<I", frame, 0)[0] + 0x10, 2))[0]
        cur["events"].append({"e": "ref", "id": u32(c + 0x1C), "rw": rw,
                              "n": struct.unpack_from("<I", frame, 0xC)[0],
                              "line": own or (u32(LINE) - fn24) & 0xFFFF})

    def on_sorted():
        cur["events"].append({"e": "sorted", "class": reg("ebx"), "list": walk(u32(LIST_HEAD))})

    def on_choose():
        c = reg("esi")
        ev("choose", c, allowed=bitset(u32(c + 0x20)))

    def on_chosen():
        c = reg("esi")
        ev("chosen", c, reg=regnum(u32(c + 0x10)), costs=list(struct.unpack("<9i", rd(COSTS, 36))))

    def cand_list(head):
        """[id, flags at +6, cost at +0x18, register at +0x10] along the +0x14 chain."""
        res, seen = [], set()
        while head and head not in seen and len(res) < 100000:
            seen.add(head)
            raw = rd(head, 0x20)
            res.append([struct.unpack_from("<I", raw, 0x1C)[0], raw[6],
                        struct.unpack_from("<i", raw, 0x18)[0], struct.unpack_from("<I", raw, 0x10)[0]])
            head = struct.unpack_from("<I", raw, 0x14)[0]
        return res

    last_block = {}

    def on_block_end():
        esp = reg("esp")
        blk = u32(esp + 0x20)
        lines, t, end, n = [], u32(blk + 0x1C), u32(blk + 0x20), 0
        while t and t != end and n < 5000:
            line = struct.unpack("<H", rd(t + 0x10, 2))[0]
            if line:
                lines.append(line)
            t, n = u32(t), n + 1
        e = {"e": "block", "set": len(bitset(reg("ebp")) or []), "refl": cand_list(reg("ebx")),
             "live": cand_list(u32(esp + 0x10)), "lines": lines}
        cur["events"].append(e)
        last_block["e"] = e

    def on_block_wk():
        if "e" in last_block:
            last_block["e"].update(w=reg("edi"), K=reg("esi"))

    block_handlers = {BLOCK_END: on_block_end, BLOCK_WK: on_block_wk} if cfg.get("blocks") else {}

    handlers = {
        NEWCAND: on_new,
        DEFREF: lambda: on_ref(reg("edi"), "w"),
        USEREF: lambda: on_ref(reg("eax"), "r"),
        DRIVER: lambda: cur["events"].append({"e": "driver"}),
        SORTED: on_sorted,
        RESORT: lambda: cur["events"].append({"e": "resort", "list": walk(u32(LIST_HEAD), False)}),
        LOWSPILL: lambda: ev("lowspill", reg("esi")),
        DEFER: lambda: ev("defer", reg("ecx")),
        CHOOSE: on_choose,
        CHOSEN: on_chosen,
        SPLIT: lambda: ev("split", reg("ecx"), **({"free": free_list()} if cfg.get("ids") else {})),
    }
    if cfg.get("ids"):
        handlers[FREECAND] = lambda: cur["events"].append({"e": "free", "id": reg("edx"),
                                                           "by": u32(reg("esp") + 8)})
    failed = []

    class Hook(gdb.Breakpoint):
        """Records and lets C2 run on, without a full stop in gdb."""

        def __init__(self, addr, fn):
            super().__init__(f"*{addr:#x}", internal=True)
            self.fn = fn
            self.enabled = False

        def stop(self):
            if cur is None:
                return False
            try:
                self.fn()
                return False
            except Exception:
                failed.append(traceback.format_exc())
                return True

    inl = {"fn": None}

    def on_inl_func():
        sym = u32(reg("ecx"))
        name = cstr(u32(sym + 0x18))
        inl["fn"] = None
        if wanted(name):
            inl["fn"] = {"name": name, "size": u32(sym + 0x64) & 0xFFFF, "sites": []}
            out["inline"].append(inl["fn"])

    def on_inl_site():
        if inl["fn"] is not None:
            callee, esp = reg("ebx"), reg("esp")
            inl["fn"]["sites"].append({"callee": cstr(u32(callee + 0x18)), "size": u32(callee + 0x64) & 0xFFFF,
                                       "budget": struct.unpack("<i", rd(esp + 0x48, 4))[0],
                                       "depth": u32(esp + 0x30), "R": u32(esp + 0x2C), "inlined": False})

    def on_inl_done():
        if inl["fn"] is not None and inl["fn"]["sites"]:
            inl["fn"]["sites"][-1]["inlined"] = True

    class InlineHook(gdb.Breakpoint):
        """Like Hook, but for the inliner, which runs before `cur` is set."""

        def __init__(self, addr, fn):
            super().__init__(f"*{addr:#x}", internal=True)
            self.fn = fn

        def stop(self):
            try:
                self.fn()
                return False
            except Exception:
                failed.append(traceback.format_exc())
                return True

    inline_hooks = []
    if cfg.get("inline"):
        inline_hooks = [InlineHook(addr, fn) for addr, fn in
                        ((INL_FUNC, on_inl_func), (INL_SITE, on_inl_site), (INL_DONE, on_inl_done))]

    # --frame and --rotation: the frame layout and the expression temporaries
    # come after the allocator, in code generation, so these hooks are on from
    # the wanted function's END until the next function's REFS.
    post = {"fn": None, "temps": []}

    def frame_slots():
        base, lo, count = u32(SLOTS), u32(SLOT_LO), u32(SLOT_COUNT)
        slots = []
        table = u32(FRAME_SYMS)
        for i in range(count if 0 <= count < 4096 else 0):
            members, _, size, refs, _ = struct.unpack("<5I", rd(base + 20 * i, 20))
            nums = bitset(members) or []
            names = []
            for m in nums:
                st = u32(u32(table + 4 * m)) if table else 0
                names.append(cstr(u32(st + 0x18)) if st else None)
            slots.append({"size": size, "refs": refs, "members": nums, "names": names, "own": i >= lo})
        return slots

    def on_frame_pack():
        s = reg("ecx")
        raw = rd(s, 0x3C)
        st = struct.unpack_from("<I", raw, 0)[0]
        post["fn"]["events"].append({"e": "fpack", "sym": s, "kind": raw[4],
                                     "name": cstr(u32(st + 0x18)) if st else None,
                                     "size": struct.unpack_from("<I", raw, 0x20)[0],
                                     "refs": struct.unpack_from("<i", raw, 0x34)[0],
                                     "num": struct.unpack_from("<I", raw, 0x38)[0], "total": reg("edx")})

    def on_frame_packed():
        post["fn"]["events"].append({"e": "frame", "phase": "packed", "total": u32(reg("esp") + 0x10),
                                     "slots": frame_slots()})

    def on_frame_laid():
        post["fn"]["events"].append({"e": "frame", "phase": "laid", "slots": frame_slots()})

    def on_temp():
        t = reg("edx")
        ptr = u32(ROT_PTR)
        post["temps"].append(len(post["fn"]["events"]))
        post["fn"]["events"].append({"e": "temp", "line": struct.unpack("<H", rd(t + 0x10, 2))[0],
                                     "ptr": u32(ptr) if ptr else 0})

    def on_temp_reg():
        route = TEMP_ROUTES.get(u32(reg("esp")))
        if route is None or not post["temps"]:
            return
        e = post["fn"]["events"][post["temps"].pop()]
        ptr = u32(ROT_PTR)
        e.update(reg=reg("ecx"), route=route, after=u32(ptr) if ptr else 0)

    class PostHook(gdb.Breakpoint):
        """Like Hook, for code generation after the wanted function's allocation."""

        def __init__(self, addr, fn):
            super().__init__(f"*{addr:#x}", internal=True)
            self.fn = fn
            self.enabled = False

        def stop(self):
            if post["fn"] is None:
                return False
            try:
                self.fn()
                return False
            except Exception:
                failed.append(traceback.format_exc())
                return True

    post_hooks = []
    if cfg.get("frame"):
        post_hooks += [PostHook(FRAME_PACK, on_frame_pack), PostHook(FRAME_PACKED, on_frame_packed),
                       PostHook(FRAME_LAID, on_frame_laid)]
    if cfg.get("rotation"):
        post_hooks += [PostHook(TEMP, on_temp), PostHook(TEMP_REG, on_temp_reg)]

    hooks = [Hook(addr, fn) for addr, fn in handlers.items()]
    block_hooks = [Hook(addr, fn) for addr, fn in block_handlers.items()]
    # Real stops, where the hooks are switched on and off: each function's
    # candidates are ready at REFS, and its allocation is over at END. The
    # block hooks only run in FUN_0040ee1d's first pass, from the allocator's
    # entry to the sorted list (each re-sort runs it again).
    gdb.Breakpoint(f"*{REFS:#x}", internal=True)
    gdb.Breakpoint(f"*{END:#x}", internal=True)
    if block_hooks:
        gdb.Breakpoint(f"*{DRIVER:#x}", internal=True)
        gdb.Breakpoint(f"*{SORTED:#x}", internal=True)
    first_pass = False

    exited = []
    gdb.events.exited.connect(exited.append)
    try:
        while not exited and not failed:
            try:
                gdb.execute("continue", to_string=not debug)
            except gdb.error:
                if exited or not inf.threads():
                    break
                raise
            if exited or failed:
                break
            pc = reg("eip")
            if pc == REFS:
                post["fn"] = None
                on_refs()
                first_pass = False
            elif pc == END:
                post["fn"], post["temps"] = cur, []
                cur = None
            elif pc == DRIVER:
                first_pass = cur is not None and not any(e["e"] == "sorted" for e in cur["events"])
            elif pc == SORTED:
                first_pass = False
            for h in hooks:
                h.enabled = cur is not None
            for h in post_hooks:
                h.enabled = post["fn"] is not None
            for h in block_hooks:
                h.enabled = cur is not None and first_pass
    except Exception:
        failed.append(traceback.format_exc())
    if failed:
        out["error"] = failed[0]
        try:
            for h in hooks + block_hooks + inline_hooks + post_hooks:
                h.enabled = False
            gdb.execute("detach", to_string=True)  # let C2 finish without us
        except Exception:
            pass
    finish()


# --- the host side ---------------------------------------------------------------

def host_main() -> None:
    import argparse
    import hashlib
    import re
    import secrets
    import shlex
    import shutil
    import socket
    import subprocess
    import time
    from pathlib import Path

    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from check import (DEFAULT_FLAGS, FILE_FLAGS, FILE_FLAGS_LINE, FORBIDDEN, GAP_FILE_FLAGS, GAP_FORBIDDEN,
                       annotations, find_source, is_gap_source,
                       mangled_prefixes, select_function, winpath)
    from coff import parse_object

    root = Path(__file__).resolve().parent.parent
    tc = root / "toolchain" / "msvc5-sp3"

    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("address", type=lambda s: int(s, 16))
    ap.add_argument("source", type=Path, nargs="?")
    ap.add_argument("--sym", help="substring of the mangled name, if the annotation can't be used")
    ap.add_argument("--flags", default=DEFAULT_FLAGS)
    ap.add_argument("--trace", action="store_true", help="also print every colouring step")
    ap.add_argument("--blocks", nargs="?", const="", metavar="NAMES",
                    help="also print each block's share of each candidate's priority (or only of NAMES: "
                         "comma-separated names as the table prints them, or #id)")
    ap.add_argument("--inline", action="store_true",
                    help="also print the /Ob2 inliner's decision at each call site (budget, depth, R, IL sizes)")
    ap.add_argument("--symbols", metavar="NAMES",
                    help="also print the symbol id of each of NAMES (comma-separated globals, functions, "
                         "candidate locals or types) and the file's symbol count")
    ap.add_argument("--frame", action="store_true",
                    help="also print C2's frame layout: each local's size and reference count in packing "
                         "order, the slot it opens or joins, and the slots' final order and offsets")
    ap.add_argument("--rotation", action="store_true",
                    help="also print the expression temporaries in code-generation order, with the rotating "
                         "pointer before each one and the register it gets")
    ap.add_argument("--ids", action="store_true",
                    help="also print the freed candidate ids: each free during allocation and, for each "
                         "batch of splits, the freed list and the id each new piece takes from it")
    ap.add_argument("--json", type=Path, help="write the raw trace to this file")
    ap.add_argument("--keep", action="store_true", help="keep the run directory under build/c2prio/")
    args = ap.parse_args()

    if shutil.which("gdb") is None or shutil.which("winedbg") is None:
        sys.exit("c2prio needs gdb and winedbg; ask the human to install gdb (it comes with Python)")
    c2 = tc / "BIN" / "C2.EXE"
    if not c2.exists():
        sys.exit("Toolchain missing, run tools/setup_toolchain.sh")
    if hashlib.sha256(c2.read_bytes()).hexdigest() != C2_SHA256:
        sys.exit(f"{c2} is not the VC++ 5.0 SP3 C2.EXE whose addresses this tool uses")

    src = args.source or find_source(args.address)
    if src is None:
        sys.exit(f"no file under src/ has '// FUNCTION: {args.address:#x}'")
    qualname = next((q for a, q in annotations(src) if a == args.address), None)
    text = src.read_text(errors="replace")
    gap = is_gap_source(src)      # gap code allows inline asm and its own flags
    bad = (GAP_FORBIDDEN if gap else FORBIDDEN).search(text)
    if bad:
        sys.exit(f"{src}: '{bad.group(0)}' is not allowed; write the function in plain C++")
    flags = args.flags.split()
    m = FILE_FLAGS_LINE.search(text)
    if m:
        allowed = GAP_FILE_FLAGS if gap else FILE_FLAGS
        flags += [f for f in m.group(1).split() if f in allowed and f not in flags]

    if args.sym:
        match = ["substr", [args.sym]]
    elif qualname and qualname.startswith("="):
        match = ["exact", [qualname[1:]]]
    elif qualname:
        match = ["prefix", mangled_prefixes(qualname)]
    else:
        match = ["substr", [""]]

    env = dict(os.environ, WINEPREFIX=str(root / "toolchain" / "wineprefix"), WINEDEBUG="-all")
    env.pop("BT_TOOLCHAIN", None)
    if not env.get("WINEARCH"):
        sysreg = root / "toolchain" / "wineprefix" / "system.reg"
        arch = re.search(r"^#arch=(\S+)", sysreg.read_text(errors="replace"), re.M) if sysreg.exists() else None
        env["WINEARCH"] = arch.group(1) if arch else "win64"

    runs = root / "build" / "c2prio"
    runs.mkdir(parents=True, exist_ok=True)
    for old in runs.iterdir():  # left behind by a run that was killed
        try:
            if time.time() - old.stat().st_mtime > 86400:
                shutil.rmtree(old, ignore_errors=True)
        except OSError:
            pass
    token = secrets.token_hex(4)
    run = runs / token
    run.mkdir()
    # A unique file name, so this run finds its own C2 among other processes.
    exe = run / f"c2p{token}.exe"
    data = bytearray(c2.read_bytes())
    off = file_offset(data, ENTRY)
    assert data[off:off + 2] == ENTRY_BYTES
    data[off:off + 2] = b"\xeb\xfe"  # jmp $
    exe.write_bytes(data)
    (run / "MSPDB50.DLL").symlink_to(tc / "BIN" / "MSPDB50.DLL")
    obj = run / "out.obj"
    out_json = run / "trace.json"

    procs = []
    t0 = time.time()
    try:
        fd = [f"/Fd{winpath(run / 'out.pdb')}"] if "/Gi" in flags else []
        cl = subprocess.Popen([str(root / "tools" / "wcl"), "/c", *flags, *fd, f"/I{winpath(root / 'include')}",
                               f"/B2{winpath(exe)}", f"/Fo{winpath(obj)}", winpath(src.resolve())],
                              cwd=root, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        procs.append(cl)
        pid = None
        while pid is None:
            info = subprocess.run(["winedbg"], input="info process\nquit\n", env=env, capture_output=True,
                                  text=True, timeout=60).stdout
            for line in info.splitlines():
                mm = re.match(r"\s*=?\s*([0-9a-fA-F]{8})\s+\d+\s+(?:\\_ )?'(.*)'", line)
                if mm and mm.group(2).lower() == exe.name:
                    pid = int(mm.group(1), 16)
            if pid is None:
                if cl.poll() is not None:
                    sys.exit(f"compile failed before C2 ran:\n{cl.stdout.read()}")
                if time.time() - t0 > 600:
                    sys.exit("C2 did not start within 600 s")
                time.sleep(0.05)
        with socket.socket() as sk:
            sk.bind(("", 0))
            port = sk.getsockname()[1]
        srv = subprocess.Popen(["winedbg", "--gdb", "--no-start", "--port", str(port), str(pid)], env=env,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        procs.append(srv)
        cfg = run / "config.json"
        me = str(Path(__file__).resolve())
        relay_cmd = " ".join(shlex.quote(a) for a in (sys.executable, me, "--relay", str(port)))
        cfg.write_text(json.dumps({"relay": relay_cmd, "exe": exe.name, "match": match, "out": str(out_json),
                                   "blocks": args.blocks is not None, "inline": args.inline,
                                   "symbols": args.symbols is not None, "frame": args.frame,
                                   "rotation": args.rotation, "ids": args.ids}))
        g = subprocess.run(["gdb", "-nx", "-q", "-batch", "-x", me],
                           env=dict(env, C2PRIO_CONFIG=str(cfg)), capture_output=True, text=True, timeout=3600)
        (run / "gdb.log").write_text(g.stdout + g.stderr)
        trace = json.loads(out_json.read_text()) if out_json.exists() else None
        if trace is None or trace.get("error"):
            kill_matching(exe.name)
            why = trace["error"] if trace else f"{g.stdout}{g.stderr}"
            sys.exit(f"the trace failed:\n{why}")
        log = cl.communicate(timeout=600)[0].replace("\r", "")
        if cl.returncode != 0 or not obj.exists():
            sys.exit(f"compile failed:\n{log}")
        elapsed = time.time() - t0
        picked, err = select_function(parse_object(obj.read_bytes(), obj.name), args.sym, qualname)
        if picked is None:
            sys.exit(err)
        fns = [f for f in trace["functions"] if f["name"] == picked[0]]
        if args.json:
            args.json.write_text(json.dumps(fns, indent=1))
        if not fns:
            sys.exit(f"C2 never ran the global allocator for {picked[0]}")
        brace = body_line(text, args.address)
        try:
            shown = src.resolve().relative_to(root)
        except ValueError:
            shown = src
        print(f"{args.address:#x}  {picked[0]}  ({shown}, {elapsed:.1f} s)")
        if args.inline:
            print_inline([f for f in trace.get("inline", []) if f["name"] == picked[0]])
        if args.symbols is not None:
            print_symbols(fns[0], [n.strip() for n in args.symbols.split(",") if n.strip()])
        for n, fn in enumerate(fns):
            if len(fns) > 1:
                print(f"\nC2 allocated this function {len(fns)} times; run {n + 1}:")
            report(fn, brace, args.trace, args.blocks)
            if args.frame:
                print_frame(fn, brace)
            if args.rotation:
                print_rotation(fn, brace)
            if args.ids:
                print_ids(fn)
    finally:
        for p in procs:
            if p.poll() is None:
                p.kill()
        kill_matching(exe.name)
        if not args.keep:
            shutil.rmtree(run, ignore_errors=True)


def file_offset(pe: bytes, va: int) -> int:
    """File offset of a virtual address in a PE image."""
    peoff = struct.unpack_from("<I", pe, 0x3C)[0]
    nsec, optsize = struct.unpack_from("<H", pe, peoff + 6)[0], struct.unpack_from("<H", pe, peoff + 20)[0]
    base = struct.unpack_from("<I", pe, peoff + 24 + 28)[0]
    sec = peoff + 24 + optsize
    for i in range(nsec):
        vsize, vaddr, rsize, raddr = struct.unpack_from("<IIII", pe, sec + 40 * i + 8)
        if vaddr <= va - base < vaddr + max(vsize, rsize):
            return va - base - vaddr + raddr
    raise ValueError(f"{va:#x} is not in the image")


def kill_matching(name: str) -> None:
    """Kill this run's Wine processes (CL and the C2 copy), found by the copy's unique name."""
    import signal
    for pid in os.listdir("/proc"):
        if not pid.isdigit():
            continue
        try:
            cmd = open(f"/proc/{pid}/cmdline", "rb").read().decode("latin1").lower()
        except OSError:
            continue
        if name in cmd:
            try:
                os.kill(int(pid), signal.SIGKILL)
            except OSError:
                pass


def body_line(text: str, address: int):
    """Line of the `{` that opens the annotated function's body. C1 numbers each
    tuple's line from the line before it."""
    from check import ANNOTATION
    lines = text.splitlines(keepends=True)
    start = None
    for i, l in enumerate(lines):
        m = ANNOTATION.match(l)
        if m and int(m.group(1), 16) == address:
            start = i
    if start is None:
        return None
    body = "".join(lines[start + 1:])
    depth, i, line = 0, 0, start + 2
    while i < len(body):
        ch = body[i]
        if ch == "\n":
            line += 1
        elif body.startswith("//", i):
            i = body.find("\n", i) - 1 if "\n" in body[i:] else len(body)
        elif body.startswith("/*", i):
            end = body.find("*/", i + 2)
            end = len(body) if end < 0 else end
            line += body.count("\n", i, end)
            i = end + 1
        elif ch in "\"'":
            j = i + 1
            while j < len(body) and body[j] != ch and body[j] != "\n":
                j += 2 if body[j] == "\\" else 1
            i = j
        elif ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        elif ch == "{" and depth == 0:
            return line
        elif ch == ";" and depth == 0:
            return None  # a declaration, not a definition
        i += 1
    return None


# --- the report ------------------------------------------------------------------

class Cand:
    """One candidate, from its creation until C2 frees it (C2 reuses the ids of freed ones)."""

    def __init__(self, cid, leaf, root=None):
        self.id, self.leaf = cid, leaf
        self.root = root or self
        self.pieces = []        # on a root: every piece split off it or off its pieces
        self.lines = []
        self.reg = None
        self.split = False
        self.listed = False
        self.late = False       # made during allocation by a call this tool does not know


def model(fn: dict):
    """Replay the events, binding each to the candidate its id meant at that moment."""
    cur, roots = {}, []
    phase = "build"
    for e in fn["events"]:
        k = e["e"]
        if k == "driver":
            phase = "alloc"
        elif k == "new":
            parent = cur.get(e["parent"]) if e["parent"] is not None else None
            c = Cand(e["id"], e["leaf"], parent.root if parent else None)
            c.sym = e.get("sym")   # the symbol, recorded only with --frame
            if parent:
                parent.split = True
                c.root.pieces.append(c)
            else:
                c.late = phase != "build"
                roots.append(c)
            cur[e["id"]] = c
            e["cand"] = c
        elif k == "ref":
            if e["id"] in cur:
                cur[e["id"]].lines.append(e["line"])
        elif k in ("sorted", "resort"):
            e["cands"] = [cur.get(s["id"]) for s in e["list"]]
            if k == "sorted":
                for c in e["cands"]:
                    if c:
                        c.listed = True
        elif "id" in e:
            c = e["cand"] = cur.get(e["id"])
            if c is not None and k == "chosen":
                c.reg = e["reg"]
            elif c is not None and k == "split":
                c.split = True
    return roots


def compress(nums) -> str:
    nums = sorted(set(nums))
    parts, i = [], 0
    while i < len(nums):
        j = i
        while j + 1 < len(nums) and nums[j + 1] == nums[j] + 1:
            j += 1
        parts.append(str(nums[i]) if i == j else f"{nums[i]}-{nums[j]}")
        i = j + 1
    s = ",".join(parts)
    return s if len(s) <= 16 else f"{nums[0]}..{nums[-1]}"


def describe(c) -> str:
    """A readable name for a candidate."""
    if c is None:
        return "?"
    leaf = c.leaf
    kind, name = leaf.get("kind"), leaf.get("name")
    width = {1: " (8-bit)", 2: " (16-bit)"}.get(leaf.get("type", 0) & 0xFF, "")
    if kind == 13:
        v = leaf.get("value")
        s = "const ?" if v is None else f"const {v if -10 < v < 10 else hex(v & 0xFFFFFFFF)}"
    elif kind == 7 and name:
        from check import base_name  # host_main put tools/ on the path
        s = base_name(name)
    elif name:
        s = (name[1:] if name.startswith("_") else name) + width
    elif kind == 4:
        off = leaf.get("offset", 0)
        s = "local temp" + (f" [{'-' if off < 0 else '+'}{abs(off):#x}]" if off else "") + width
    else:
        s = KINDS.get(kind, f"kind {kind}") + width
    return s if c.root is c else f"{s}, piece of #{c.root.id}"


def allowed_str(bits) -> str:
    if bits is None:
        return "-"
    return "".join(LETTERS[r] if r in bits else "." for r in ORDER)


def regname(r) -> str:
    return REG_NAMES.get(r, f"reg{r}")


def result(c) -> str:
    """The register a candidate ended with, or what happened to it instead."""
    if c is None:
        return "?"
    none = "immediate" if c.leaf.get("kind") == 13 else "memory"
    if not c.split:
        return regname(c.reg) if c.reg else none
    rest = [p for p in c.root.pieces if not p.split]
    got = [f"{regname(p.reg)} #{p.id}" for p in rest if p.reg]
    left = len(rest) - len(got)
    if not got:
        return f"split, {none}"
    return "split: " + ", ".join(got) + (f", {left} piece{'s' * (left > 1)} {none}" if left else "")


def print_inline(runs) -> None:
    """--inline: the /Ob2 inliner's decision at each call site, as C2 visited them."""
    if not runs:
        print("\nC2's inliner did not run for this function.")
        return
    for n, fn in enumerate(runs):
        if len(runs) > 1:
            print(f"\nC2 ran the inliner on this function {len(runs)} times; run {n + 1}:")
        print(f"\n/Ob2 inline decisions: the function's IL size is {fn['size']}, its budget "
              f"{min(max(1000, 2 * fn['size']), 35000)} (max(1000, 2 x IL size), at most 35000).")
        if not fn["sites"]:
            print("No call sites to inline candidates.")
            continue
        print("Call sites in the order C2 visits them: depth-1 sites in source order, each inlined callee's own")
        print("sites right after it. budget = what is left at the site's level; R = this level's sites still to")
        print("come, this one included. An inlined callee costs its IL size if 41 or more (else 0), and its")
        print("own sites start from (budget - cost) / R.")
        print(f"{'depth':>5} {'R':>4} {'budget':>7} {'IL':>5}  {'inlined':<7}  callee")
        for s in fn["sites"]:
            indent = "  " * max(0, s["depth"] - 1)
            print(f"{s['depth']:>5} {s['R']:>4} {s['budget']:>7} {s['size']:>5}  {'yes' if s['inlined'] else 'no':<7}  "
                  f"{indent}{s['callee']}")


def print_symbols(fn: dict, names) -> None:
    """--symbols: the ids of the symbols named (globals from C2's symbol table,
    locals and parameters from the function's candidates), the file's
    symbol count and C2's own counter, read when the function's allocation started."""
    import re
    from check import base_name  # host_main put tools/ on the path
    ids = fn.get("ids")
    if not ids:
        print("\nno symbol ids were read for this function")
        return
    cands = []
    for c in model(fn):
        cands += [c] + c.pieces
    syms = sorted(ids["symbols"])
    first, nxt = ids["first"], ids["next"]
    print("\nsymbol ids (docs/c2-regalloc.md, \"Symbol ids\"): one front-end counter numbers every declaration "
          "in the\nfile, types included (C2 keeps no count of types); the effects measured so far follow the ids "
          "modulo 65536.")
    print(f"  file total {first} ({first & 0xFFFF:#06x} in 16 bits, bit 14 {first >> 14 & 1}): the front end's "
          f"count at the end of the\n  file, from the IL's header, so every declaration in the file moves it. "
          f"C2 numbers the symbols\n  it makes itself (temporaries, inlined locals, sections) on from there, "
          f"function after function,\n  and had reached {nxt} ({nxt & 0xFFFF:#06x}) when this function's "
          f"allocation started.")

    def row(label, kind, sid):
        print(f"  {label:<34.34} {kind:<9} {sid:>7}  {sid & 0xFFFF:#06x} {sid >> 14 & 1:>7} {sid >> 15 & 1:>7}")

    print(f"  {'name':<34} {'kind':<9} {'id':>7}  {'low 16':<6} {'bit 14':>7} {'bit 15':>7}")
    own = [s for s in syms if s[0] == ids["own"] and s[1] in (4, 14, 16)]
    row(f"{base_name(own[0][2]) if own else '?'} (this function)", "function", ids["own"])
    for name in names:
        hits = [s for s in syms if s[2] and (s[2] == name or base_name(s[2]) == name)]
        for sid, kind, sname in hits:
            row(sname if len(hits) > 1 else name, SYM_KINDS.get(kind, f"kind {kind}"), sid)
        if hits:
            continue
        local = [c for c in cands if c.leaf.get("sid") is not None and c.leaf.get("kind") != 7
                 and (name in (f"#{c.id}", describe(c)) or describe(c).startswith(name + " ("))]
        for c in local:
            kind = KINDS.get(c.leaf.get("kind"), "?")
            row(f"{describe(c)} #{c.id}" if len(local) > 1 or name.startswith("#") else name, kind, c.leaf["sid"])
        if local:
            continue
        scope = re.compile(r"^\?(?:\?_?[0-9A-Z])?(?:[\w$]+@)?%s@(?:[\w$]+@)*@" % re.escape(name))
        members = [s for s in syms if s[2] and scope.match(s[2])]
        if members:
            print(f"  {name}: a type, which never reaches C2. It was numbered before its members; the first "
                  f"one C2 has:")
            row(f"  {base_name(members[0][2])}", SYM_KINDS.get(members[0][1], f"kind {members[0][1]}"), members[0][0])
        else:
            print(f"  {name}: neither in C2's symbol table (the global symbols the IL uses) nor a register "
                  f"candidate\n    here; types and unused declarations never reach C2")


def report(fn: dict, brace, trace: bool, blocks=None) -> None:
    roots = model(fn)
    events = fn["events"]

    def lines(c):
        if not c or not c.lines:
            return ""
        if brace is None:
            return "+" + compress(c.lines)
        return compress([brace - 1 + d for d in c.lines])

    sorts = [e for e in events if e["e"] == "sorted"]
    for s in sorts:
        cname = "integer" if s["class"] == 0 else "x87"
        print(f"\n{cname} register candidates in C2's order (FUN_0041bdd7): priority, then +0x40, larger first.")
        print("FUN_0041b785 colours them from the top. allowed: a=eax c=ecx d=edx s=esi i=edi b=ebx p=ebp")
        print(f"{'#':>3} {'id':>4}  {'candidate':<22} {'lines':<16} {'prio':>6} {'+0x40':>5} {'spill':>5} "
              f"{'refs':>4}  {'allowed':<7}  register")
        for i, (snap, c) in enumerate(zip(s["list"], s["cands"])):
            print(f"{i:>3} {snap['id']:>4}  {describe(c):<22.22} {lines(c):<16} {snap['prio']:>6} "
                  f"{snap['k40']:>5} {snap['spill']:>5} {snap['refs']:>4}  {allowed_str(snap['allowed']):<7}  "
                  f"{result(c)}")
    if not sorts:
        print("\nC2's global allocator had no integer candidates in this function.")
    unlisted = [c for c in roots if not c.listed and not c.late and (c.lines or c.leaf.get("name"))]
    x87 = [c for c in unlisted if c.leaf.get("type", 0) >> 12 == 4]
    dropped = [c for c in unlisted if c not in x87]
    if dropped:
        print("\nnot in the list: dropped before the sort (FUN_0041a6f8 keeps a candidate with one reference "
              "in memory\nand forwards a copy into its use):")
        for c in dropped:
            print(f"    {c.id:>4}  {describe(c):<22.22} {lines(c):<16} {result(c) if c.split else ''}".rstrip())
    if x87 and not any(s["class"] != 0 for s in sorts):
        print("\nfloating-point candidates (the x87 stack, FUN_0045f4b7, is not traced): "
              + ", ".join(f"{describe(c)} ({lines(c)})" for c in x87))
    if trace:
        print_trace(events)
    if blocks is not None:
        print_blocks(events, blocks, lambda ls: compress(ls if brace is None else [brace - 1 + d for d in ls]))


def print_blocks(events, names: str, show_lines) -> None:
    """Each block's share of each candidate's priority in FUN_0040ee1d's first
    pass, from the lists read at BLOCK_END and w and K read at BLOCK_WK."""
    sort = next((e for e in events if e["e"] == "sorted"), None)
    if sort is None:
        return
    first = events.index(sort)
    start = max((i for i, e in enumerate(events[:first]) if e["e"] == "driver"), default=0)
    blocks = [e for e in events[start:first] if e["e"] == "block" and "K" in e]
    snap = {s["id"]: (s, c) for s, c in zip(sort["list"], sort["cands"])}
    label = {cid: describe(c) for cid, (s, c) in snap.items()}

    shares = {}   # candidate id -> [(block number, w, K, cost or None, share)]
    for n, b in enumerate(blocks):
        wk = b["w"] * b["K"]
        for cid, flags, cost, regp in b["refl"]:
            if regp == 0:                      # 0x40f70d: only a candidate with no register yet
                shares.setdefault(cid, []).append((n, b["w"], b["K"], cost, wk * cost))
        for cid, flags, cost, regp in b["live"]:
            if flags & 0x10:                   # 0x40f742: referenced in this block
                shares.setdefault(cid, []).append((n, b["w"], b["K"], cost, wk * cost))
            elif wk:                           # 0x40f72c: live through it unreferenced
                shares.setdefault(cid, []).append((n, b["w"], b["K"], None, -wk))

    want = None
    if names:
        want = set()
        for tok in (t.strip() for t in names.split(",") if t.strip()):
            for cid, text in label.items():
                if tok in (f"#{cid}", str(cid), text) or text.startswith(tok + " ("):
                    want.add(cid)
        if not want:
            print(f"\n--blocks: no candidate in the list is called {names!r}")
            return
    print("\nwhere each priority comes from (FUN_0040ee1d's first pass, before the sort): a block that "
          "references\na candidate adds w * K * cost (cost 2 a reference, 0 or 1 for a constant), one "
          "it is live\nthrough without a reference takes w * K. w: loop weight; K: candidates "
          "referenced in the block.")
    used = set()
    for s in sort["list"]:
        cid = s["id"]
        if want is not None and cid not in want:
            continue
        rows = shares.get(cid, [])
        total = sum(r[4] for r in rows)
        check = "" if total == s["prio"] else f"   (the shares add up to {total})"
        print(f"\n#{cid} {label.get(cid, '?')}: priority {s['prio']}{check}")
        print(f"    {'block':<6} {'lines':<16} {'w':>3} {'K':>3} {'cost':>5} {'share':>6}")
        for n, w, k, cost, share in rows:
            used.add(n)
            print(f"    B{n:<5} {show_lines(blocks[n]['lines']):<16} {w:>3} {k:>3} "
                  f"{'-' if cost is None else cost:>5} {share:>+6}")
    print("\nblocks above and the candidates each one references (K counts these"
          + (", plus C2's own set" if any(b["set"] for b in blocks) else "") + "):")
    for n in sorted(used):
        b = blocks[n]
        refs = [r[0] for r in b["refl"]] + [r[0] for r in b["live"] if r[1] & 0x10]
        extra = f" + {b['set']} in C2's set" if b["set"] else ""
        print(f"    B{n:<5} {show_lines(b['lines']):<16} w {b['w']} K {b['K']}{extra}: "
              + ", ".join(f"{label.get(r, '?')} #{r}" for r in refs))


def print_frame(fn: dict, brace) -> None:
    """--frame: C2's frame layout (FUN_0043f93b), read at FRAME_PACK, FRAME_PACKED and FRAME_LAID."""
    events = fn["events"]
    packs = [e for e in events if e["e"] == "fpack"]
    packed = next((e for e in events if e["e"] == "frame" and e["phase"] == "packed"), None)
    laid = next((e for e in events if e["e"] == "frame" and e["phase"] == "laid"), None)
    if not packs or packed is None or laid is None:
        print("\nno frame layout was read for this function (C2 lays out only the locals left in memory)")
        return
    by_sym = {}
    for c in model(fn):
        for x in [c] + c.pieces:
            if x.sym is not None:
                by_sym.setdefault(x.sym, x)

    def label(p, lines=True):
        if p["name"]:
            return p["name"][1:] if p["name"].startswith("_") else p["name"]
        c = by_sym.get(p["sym"])
        if c is None:
            return KINDS.get(p["kind"], f"kind {p['kind']}")
        shown = compress(c.lines if brace is None else [brace - 1 + d for d in c.lines]) if c.lines else ""
        return f"{describe(c)} #{c.id}" + (f" {shown}" if shown and lines else "")

    names = {p["num"]: label(p) for p in packs}
    short = {p["num"]: label(p, False) for p in packs}
    own = [s for s in packed["slots"] if s["own"]]
    hidden = len(packed["slots"]) - len(own)
    slot_of = {m: k for k, s in enumerate(own) for m in s["members"]}
    print("\nframe layout (FUN_0043f93b, in code generation): the locals left in memory in C2's list order "
          "(size,\nthen reference count, larger first; ties in the order they reached their count). Each "
          "joins the\nnewest earlier slot that is at least half its size and does not interfere with it, or "
          "opens a\nnew one (FUN_00440cbd). A temporary is named after the candidate it spills.")
    print(f"  {'#':>3}  {'local':<36} {'size':>6} {'refs':>4}  slot")
    opened = set()
    for n, p in enumerate(packs):
        k = slot_of.get(p["num"])
        if k is None:
            where = "?"
        elif k not in opened:
            opened.add(k)
            where = f"opens S{k}"
        else:
            mates = [short.get(m, "?") for m in own[k]["members"] if m != p["num"]]
            where = f"joins S{k} ({', '.join(mates)})"
        print(f"  {n:>3}  {label(p):<36.36} {p['size']:>#6x} {p['refs']:>4}  {where}")
    total = packed["total"]
    ids = {(s["size"], tuple(s["members"])): k for k, s in enumerate(own)}
    if total > 0x80:
        how = (f"the locals total {total:#x}, over 0x80, so C2 re-sorted the slots by\nrefs * 1000 / size, "
               f"larger first, with an unstable quicksort (FUN_00459eb7)")
    else:
        how = f"the locals total {total:#x}, not over 0x80, so the slots keep their packing order"
    print(f"\nthe slots, nearest esp first: {how}.\nThe offset is from the bottom of the locals; in the body, "
          f"[esp+N] adds 4 for each register\nthe prologue pushes after reserving them.")
    print(f"  {'offset':>7} {'slot':>4} {'size':>6} {'refs':>4}  locals")
    off = 0
    for s in laid["slots"][hidden:]:
        k = ids.get((s["size"], tuple(s["members"])))
        print(f"  {off:>+#7x} {'?' if k is None else f'S{k}':>4} {s['size']:>#6x} {s['refs']:>4}  "
              + ", ".join(names.get(m, f"#{m}") for m in s["members"]))
        off += s["size"]
    if hidden:
        params = [n[1:] if n and n.startswith("_") else (n or "temp")
                  for s in packed["slots"][:hidden] for n in s["names"]]
        print(f"  (C2's slot table starts with {hidden} entr{'ies' if hidden > 1 else 'y'} for the parameters, "
              f"above the return address: {', '.join(params)})")


def print_rotation(fn: dict, brace) -> None:
    """--rotation: the expression temporaries FUN_00435c37 placed, in order."""
    temps = [e for e in fn["events"] if e["e"] == "temp"]
    if not temps:
        print("\nno expression temporaries were read for this function")
        return
    print("\nexpression temporaries in code-generation order (FUN_00435c37; docs/c2-regalloc.md, "
          "\"Temporaries\"):\nthe rotating pointer (0x491120) before each one, the register it got and the "
          "rule that chose it.\nrotation: the first free one of eax, ecx, edx from the pointer, which then "
          "moves past it; hint: the\nregister of a variable it is copied to or from; first free: the first "
          "free register in table\norder, when the rotation finds none; spill: C2 freed one. Only rotation "
          "moves the pointer.")
    print(f"  {'#':>3}  {'line':>5}  {'pointer':<7}  {'register':<8}  {'rule':<10}  pointer after")
    for n, e in enumerate(temps):
        line = "?" if not e["line"] else str(e["line"] if brace is None else brace - 1 + e["line"])
        print(f"  {n:>3}  {line:>5}  {regname(e['ptr']):<7}  {regname(e['reg']) if 'reg' in e else '-':<8}  "
              f"{e.get('route', 'none'):<10}  {regname(e['after']) if 'after' in e else '-'}")


def print_ids(fn: dict) -> None:
    """--ids: the frees and the ids split pieces reuse (FUN_0040ecd2 pushes, FUN_0040ebb6 pops)."""
    model(fn)
    events = fn["events"]
    print("\nfreed candidate ids: FUN_0040ecd2 pushes a freed candidate's id onto a list (0x493230) and "
          "FUN_0040ebb6\ngives each new candidate, such as a split piece, the id on top, so the id freed "
          "last is reused\nfirst. A step's splits run in candidate id order; after a region the one split "
          "later is reloaded first.")
    step, ctx, batch = 0, "before colouring", None

    def name(e):
        s = describe(e.get("cand"))
        return f"#{e['id']} " + (s.replace(", piece of ", " (piece of ") + ")" if ", piece of " in s else s)

    def flush():
        nonlocal batch
        if batch is None:
            return
        print(f"  {batch['ctx']}: split, in this order: " + "; ".join(batch["split"]))
        free = batch["free"]
        print("    freed ids, next reused first: " + (" ".join(map(str, free[:24])) or "none")
              + (" ..." if len(free) > 24 else ""))
        if batch["pieces"]:
            print("    pieces made: " + "; ".join(batch["pieces"]))
        if batch["back"]:
            print("    then freed, the split candidates' own ids: " + " ".join(batch["back"]))
        batch = None

    # The freed list, top first: read from C2 at each split, and kept up to
    # date in between from the frees (pushes) and new candidates (pops).
    lifo = []
    for e in events:
        k = e["e"]
        if k == "free":
            lifo.insert(0, e["id"])
        elif k == "new":
            reused = bool(lifo) and lifo[0] == e["id"]
            if reused:
                lifo.pop(0)
        if k == "split":
            if batch is None or batch["pieces"] or batch["back"]:
                flush()
                batch = {"ctx": ctx, "split": [], "pieces": [], "back": []}
            batch["split"].append(name(e))
            batch["free"] = e.get("free", [])
            lifo = list(batch["free"])
            continue
        if k == "new" and e.get("parent") is not None and batch is not None and not batch["back"]:
            batch["pieces"].append(name(e) + ("" if reused else ", a new id"))
            continue
        if k == "free" and e["by"] == SPLIT_FREE and batch is not None:
            batch["back"].append(str(e["id"]))
            continue
        flush()
        if k == "sorted" and e["class"] == 0:
            ctx = "before colouring"
        elif k == "chosen":
            step += 1
            ctx = f"after step {step} ({name(e)} -> {regname(e['reg'])})"
        elif k == "lowspill":
            ctx = f"after step {step}, {name(e)} not coloured (spill cost not positive)"
        elif k == "free":
            print(f"  freed {name(e)}: {FREED_BY.get(e['by'], hex(e['by']))}")
        elif k == "new" and e.get("parent") is not None:
            print(f"  {ctx}: piece {name(e)} made, "
                  + ("reusing the id on top of the freed list" if reused else "a new id"))
    flush()


def print_trace(events) -> None:
    print("\ncolouring, step by step (C2 reuses the ids of freed candidates for new pieces):")
    current = {}
    step = 0
    for e in events:
        k = e["e"]
        label = f"#{e['id']} {describe(e.get('cand'))}" if "id" in e else ""
        if k == "sorted":
            print(f"  -- {len(e['list'])} {'integer' if e['class'] == 0 else 'x87'} candidates sorted")
        elif k == "resort":
            print("  -- priorities recomputed (FUN_0040ee1d), the rest re-sorted: "
                  + ", ".join(f"#{s['id']} {s['prio']}" for s in e["list"][:24])
                  + (" ..." if len(e["list"]) > 24 else ""))
        elif k == "choose":
            current = e
        elif k == "chosen":
            step += 1
            costs = ", ".join(f"{regname(r)} {c:+d}" for r, c in enumerate(e["costs"]) if c)
            print(f"  {step:>3}. {label:<38.38} prio {e['prio']:>6}  allowed {allowed_str(current.get('allowed'))}"
                  f" -> {regname(e['reg'])}" + (f"   costs {costs}" if costs else ""))
        elif k == "lowspill":
            print(f"       {label:<38.38} prio {e['prio']:>6}  spill cost not positive, not coloured now")
        elif k == "defer":
            print(f"       {label:<38.38} prio {e['prio']:>6}  put back into the list (FUN_0045aaf9)")
        elif k == "split":
            print(f"       {label:<38.38} prio {e['prio']:>6}  no register left, split (FUN_00439385)")


def relay(port: int) -> None:
    """gdb's link to winedbg's gdb server (gdb runs it as `target remote | ...`).

    winedbg reads gdb's first packet but answers it only when more bytes
    arrive, and gdb resends only after its 2 s timeout. So after the first
    packet this sends one `+` (an ack, which the server ignores) to wake it,
    and otherwise passes the bytes through unchanged."""
    import selectors
    import socket
    import time
    for _ in range(600):
        try:
            sock = socket.create_connection(("localhost", port))
            break
        except ConnectionRefusedError:
            time.sleep(0.05)
    else:
        sys.exit("winedbg's gdb server did not open")
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    sel = selectors.DefaultSelector()
    sel.register(sock, selectors.EVENT_READ)
    sel.register(0, selectors.EVENT_READ)
    nudge = None   # when to wake the server, while its first answer is pending
    while True:
        timeout = None if nudge is None else max(0.0, nudge - time.time())
        ready = sel.select(timeout)
        if not ready and nudge is not None:
            sock.sendall(b"+")
            nudge = time.time() + 0.05
            continue
        for key, _ in ready:
            if key.fileobj is sock:
                data = sock.recv(65536)
                if not data:
                    return
                if b"$" in data:
                    nudge = None
                while data:
                    data = data[os.write(1, data):]
            else:
                data = os.read(0, 65536)
                if not data:
                    return
                sock.sendall(data)
                if nudge is None and data.lstrip(b"+").startswith(b"$qSupported"):
                    nudge = time.time() + 0.02


if gdb is not None:
    tracer()
elif __name__ == "__main__" and sys.argv[1:2] == ["--relay"]:
    relay(int(sys.argv[2]))
elif __name__ == "__main__":
    host_main()
