"""ABI screens for the contract-valid (T3+) image.

A T3 body is certified against its OWN compile, so a call-site convention
error survives every byte metric: the callee's argument register is dead, or
the stack is popped twice, and nothing notices until real hardware faults
(0x100597C0, 2026-09-21).  These screens compare the PLACED bytes against the
reference at the ABI level and run as part of the image gate:

  A  register-argument liveness -- a call inside a T3 body to a callee whose
     ECX/EDX is live-in (read before written on some path) must have that
     register defined since the last clobbering call, unless the reference
     caller of the same target inside the same function also leaves it
     untouched (a thiscall pass-through of the caller's own incoming ECX).
  B  double pop -- a call to a callee whose reference body `ret K`s (K>0)
     must not be followed by `add esp, ...` unless some reference caller of
     that target does the same (batched cleanup for other cdecl args).
  C  callee cleanup -- the placed body's own `ret` immediates must equal the
     reference function's at the same VA; byte-exact callers depend on them.
  D  span termination -- the last real instruction of a placed under-slot
     body must be control-terminal (ret/jmp) when the reference's is: a body
     truncated mid-tail falls through its padding into the NEXT function
     (BrGlNavPoll -> 0x100597C0, the 2026-09-21 credits-screen page fault).

The acceptance rule is always the reference's own behaviour, never a list of
known sites.  Screens run over the exact function bounds; an annexed
(over-slot) body is screened from its .t3x bytes, not the thunk span.
"""
import struct

import capstone

_MD = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
_MD.detail = True

_ALIAS = {
    'ecx': ('ecx', 'cx', 'cl', 'ch'),
    'edx': ('edx', 'dx', 'dl', 'dh'),
}


def _touches(insn, want, mode):
    r, w = insn.regs_access()
    names = [_MD.reg_name(x) for x in (r if mode == 'r' else w)]
    return any(n in _ALIAS[want] for n in names)


def _disasm(blob, va0, va, size):
    off = va - va0
    return list(_MD.disasm(blob[off:off + size], va))


def callee_abi(ref, ref_va0, target, max_bytes=4096, max_steps=4000):
    """(live_in set over {'ecx','edx'}, ret-K set) for the REFERENCE function
    at `target`.  CFG walk with a per-path written set; `push ecx/edx` is a
    frame slot (the value is irrelevant, not an argument read) and
    `xor r,r` / `sub r,r` are definitions."""
    insns = {i.address: i
             for i in _disasm(ref, ref_va0, target, max_bytes)}
    live, rets = set(), set()
    seen = {}
    stack = [(target, frozenset())]
    steps = 0
    while stack and steps < max_steps:
        addr, written = stack.pop()
        while addr in insns and steps < max_steps:
            steps += 1
            if addr in seen and written >= seen[addr]:
                break
            seen[addr] = written if addr not in seen else (seen[addr] & written)
            insn = insns[addr]
            m = insn.mnemonic
            wset = set(written)
            if m == 'ret':
                rets.add(int(insn.op_str, 0) if insn.op_str else 0)
                break
            if m == 'call':
                # caller-saved: whatever follows reads the callee's garbage,
                # not an argument of THIS function
                wset |= {'ecx', 'edx'}
            elif m in ('xor', 'sub') and insn.op_str.count(',') == 1:
                a, b = [s.strip() for s in insn.op_str.split(',')]
                if a == b and a in ('ecx', 'edx'):
                    wset.add(a)
            elif m == 'push' and insn.op_str in ('ecx', 'edx'):
                pass
            else:
                for want in ('ecx', 'edx'):
                    if want not in written and _touches(insn, want, 'r'):
                        live.add(want)
                for want in ('ecx', 'edx'):
                    if _touches(insn, want, 'w'):
                        wset.add(want)
            written = frozenset(wset)
            if m == 'jmp':
                if insn.op_str.startswith('0x'):
                    t = int(insn.op_str, 16)
                    if target <= t < target + max_bytes:
                        addr = t
                        continue
                break
            if m.startswith('j') and insn.op_str.startswith('0x'):
                stack.append((int(insn.op_str, 16), written))
            addr = insn.address + insn.size
    return live, rets


def _call_states(insns, want_by_target):
    """[(site_addr, target, reg, defined?)] from a linear walk of one body:
    `defined?` is whether `reg` was written since the last clobbering call."""
    out = []
    defined = set()
    for insn in insns:
        if insn.mnemonic == 'call':
            if insn.op_str.startswith('0x'):
                t = int(insn.op_str, 16)
                for want in want_by_target.get(t, ()):
                    out.append((insn.address, t, want, want in defined))
            defined = set()
            continue
        for want in ('ecx', 'edx'):
            if _touches(insn, want, 'w'):
                defined.add(want)
    return out


def _ret_set(insns):
    return {int(i.op_str, 0) if i.op_str else 0
            for i in insns if i.mnemonic == 'ret'}


def _ref_callers_add_esp(ref, ref_va0):
    """{target: True} for targets where SOME reference call site is followed
    immediately by `add esp, ...` -- computed lazily by the caller."""
    sites = {}
    for j in range(len(ref) - 5):
        if ref[j] != 0xE8:
            continue
        rel = struct.unpack_from('<i', ref, j + 1)[0]
        t = ref_va0 + j + 5 + rel
        if not (ref_va0 <= t < ref_va0 + len(ref)):
            continue
        sites.setdefault(t, []).append(j)
    return sites


def _fall_off(blob, va0, va, size, max_steps=20000):
    """None when every reachable path in [va, va+size) ends in ret or a jmp
    out of the span; otherwise a description of the first path that runs
    past the span end or into undecodable bytes."""
    end = va + size
    insn_cache = {}

    def _at(addr):
        if addr not in insn_cache:
            got = list(_MD.disasm(blob[addr - va0:addr - va0 + 16], addr, 1))
            insn_cache[addr] = got[0] if got else None
        return insn_cache[addr]

    seen = set()
    work = [va]
    steps = 0
    while work and steps < max_steps:
        addr = work.pop()
        while va <= addr < end and steps < max_steps:
            steps += 1
            if addr in seen:
                break
            seen.add(addr)
            insn = _at(addr)
            if insn is None:
                return 'reaches undecodable bytes @0x%08x' % addr
            if insn.address + insn.size > end:
                return ('decodes an instruction crossing the span end '
                        '@0x%08x' % addr)
            m = insn.mnemonic
            if m == 'ret':
                break
            if m == 'jmp':
                if insn.op_str.startswith('0x'):
                    t = int(insn.op_str, 16)
                    if va <= t < end:
                        addr = t
                        continue
                    break               # tail jump out of the span
                break                   # indirect: jump-table dispatch
            if m.startswith('j') and insn.op_str.startswith('0x'):
                t = int(insn.op_str, 16)
                if va <= t < end:
                    work.append(t)
                else:
                    return ('branches out of the span to 0x%08x @0x%08x'
                            % (t, addr))
            addr = insn.address + insn.size
        else:
            if va <= addr and addr >= end:
                return 'falls past the span end after 0x%08x' % (end - 1)
    return None


def _reachable_rets(blob, va0, va, size, max_steps=20000):
    """The `ret` immediates on the reachable paths of [va, va+size) -- the
    same walk as _fall_off.  A body shorter than its slot leaves dead bytes
    after its last ret; a LINEAR decode of those can surface a `ret N` that
    no path executes (BrCrImpulseSolve 2026-09-23: a phantom `ret 2`)."""
    end = va + size
    rets = set()
    seen = set()
    work = [va]
    steps = 0
    while work and steps < max_steps:
        addr = work.pop()
        while va <= addr < end and steps < max_steps:
            steps += 1
            if addr in seen:
                break
            seen.add(addr)
            got = list(_MD.disasm(blob[addr - va0:addr - va0 + 16], addr, 1))
            if not got:
                break
            insn = got[0]
            m = insn.mnemonic
            if m == 'ret':
                rets.add(int(insn.op_str, 0) if insn.op_str else 0)
                break
            if m == 'jmp':
                if insn.op_str.startswith('0x'):
                    t = int(insn.op_str, 16)
                    if va <= t < end:
                        addr = t
                        continue
                break
            if m.startswith('j') and insn.op_str.startswith('0x'):
                t = int(insn.op_str, 16)
                if va <= t < end:
                    work.append(t)
            addr = insn.address + insn.size
    return rets


def abi_screen(ref, ref_va0, img, img_va0, t3_spans, annex_map=None):
    """Screen the placed T3 bodies; returns a list of human-readable flags
    (empty == clean).

    ref/img       -- the reference and placed .text bytes
    t3_spans      -- [(va, size, name)] for every T3-placed function
    annex_map     -- {va: (annex_va, body_bytes)} for over-slot bodies; those
                     are screened from the body, not the thunk span
    """
    annex_map = annex_map or {}
    flags = []

    # placed instruction streams, one per T3 body
    placed = {}
    for va, size, name in t3_spans:
        if va in annex_map:
            a_va, body = annex_map[va]
            insns = list(_MD.disasm(body, a_va))
        else:
            insns = _disasm(img, img_va0, va, size)
        placed[va] = insns

    # every direct call target reached from a placed T3 body
    targets = set()
    for insns in placed.values():
        for i in insns:
            if i.mnemonic == 'call' and i.op_str.startswith('0x'):
                t = int(i.op_str, 16)
                if ref_va0 <= t < ref_va0 + len(ref):
                    targets.add(t)

    abi = {t: callee_abi(ref, ref_va0, t) for t in targets}
    want_by_target = {t: v[0] for t, v in abi.items() if v[0]}

    ref_call_sites = None       # built lazily for screen B

    for va, size, name in t3_spans:
        insns = placed[va]
        ref_insns = _disasm(ref, ref_va0, va, size)

        # ---- A: register-argument liveness --------------------------
        mine = _call_states(insns, want_by_target)
        theirs = _call_states(ref_insns, want_by_target)
        for site, t, reg, ok in mine:
            if ok:
                continue
            # the reference passing its own incoming register through is
            # the proof this shape is legal for this target here
            if any(rt == t and rreg == reg and not rok
                   for _s, rt, rreg, rok in theirs):
                continue
            flags.append('A %s 0x%08x: call @0x%08x -> 0x%08x needs %s and '
                         'does not set it since the last clobber (the '
                         'reference caller does)' % (name, va, site, t, reg))

        # ---- B: double pop ------------------------------------------
        for k, insn in enumerate(insns):
            if insn.mnemonic != 'call' or not insn.op_str.startswith('0x'):
                continue
            t = int(insn.op_str, 16)
            rets = abi.get(t, (None, set()))[1]
            if not rets or rets == {0}:
                continue
            nxt = insns[k + 1] if k + 1 < len(insns) else None
            if nxt is None or nxt.mnemonic != 'add' \
                    or not nxt.op_str.startswith('esp'):
                continue
            if ref_call_sites is None:
                ref_call_sites = _ref_callers_add_esp(ref, ref_va0)
            legit = False
            for j in ref_call_sites.get(t, ()):
                after = list(_MD.disasm(ref[j + 5:j + 12], ref_va0 + j + 5))
                if after and after[0].mnemonic == 'add' \
                        and after[0].op_str.startswith('esp'):
                    legit = True
                    break
            if not legit:
                flags.append('B %s 0x%08x: call @0x%08x -> 0x%08x (ret %s) '
                             'is followed by `%s %s` and no reference '
                             'caller does that'
                             % (name, va, insn.address, t, sorted(rets),
                                nxt.mnemonic, nxt.op_str))

        # ---- C: callee cleanup --------------------------------------
        r_rets = _ret_set(ref_insns)
        p_rets = (_reachable_rets(img, img_va0, va, size)
                  if va not in (annex_map or {}) else _ret_set(insns))
        if r_rets and p_rets and r_rets != p_rets:
            flags.append('C %s 0x%08x: reference ret %s vs placed ret %s'
                         % (name, va, sorted(r_rets), sorted(p_rets)))

        # ---- D: span fall-off (reachability) ------------------------
        # Walk the placed body's control flow from its entry: a path that
        # decodes past the span end executes the NEXT function's bytes
        # (the truncated-tail class: BrGlNavPoll's third arm lost its
        # `ret 4` and slid into 0x100597C0).  Data tails after the final
        # ret -- jump tables, constant pools, padding -- are never reached
        # and never flag.  An indirect jmp (jump table dispatch) ends its
        # path unfollowed: table arms are certified by the byte pairing.
        if va not in annex_map:
            off = _fall_off(img, img_va0, va, size)
            if off is not None:
                flags.append('D %s 0x%08x: a reachable path %s -- '
                             'execution falls off the span into the next '
                             'function' % (name, va, off))

    return flags
