"""Check every @implements claim for the shape of an address attached to the
wrong symbol, by comparing the CALLS the original makes against the calls the
C body makes.

WHY

A random-sample audit found two claims in port/src/audio/ of exactly one shape:

    /* @implements 0x1006B880 glide br_ftol64 */

0x1006B880 is a 201-byte sound-channel start routine that calls two functions
and an MSVCRT `_ftol` thunk; br_ftol64 is a six-line static helper that models
`_ftol` and calls nothing. The address's real transcription sat a file away,
unclaimed. Nothing in the toolchain could see it: manifest.py only checks that
a line parses, the compiler does not know what an address means, and the suite
tested the helper -- which worked fine, being a correct helper wrongly labelled.

WHAT WAS TRIED FIRST, AND WHY IT WAS THROWN AWAY

The obvious detector is a size ratio: 201 bytes of x86 cannot be six lines of C.
It was written, and its selftest -- does it rank the two PROVEN-wrong claims? --
failed on both counts:

  - 0x1006B880 scored 28.7 bytes/line but sat below forty other claims, because
    this tree is full of legitimate one-line FORWARDERS. BrGbiClipCodes is four
    lines forwarding to the shared implementation and scores 40.5, higher than
    the real defect.
  - 0x1006B6C0 scored 8.0, dead in the middle of the healthy range. Undetectable.

So the ratio ranked a correct forwarder above a proven defect and missed the
other defect entirely. It was deleted rather than tuned. CONVENTIONS.md: a
detector you have not validated is not evidence -- and a detector that fails its
own calibration set is worse than none, because its output looks like a worklist.

WHAT THIS MEASURES INSTEAD

Calls, which survive the forwarder problem. A forwarder makes a call; a helper
that models a CRT routine makes none. For each claim:

  original : distinct CALL targets in the disassembly, import thunks included
  port     : call expressions in the C body

A claim where the original calls two or more functions and the C body calls
NOTHING is the flag. It survives the forwarder problem that sank the ratio, and
it flags 12 of 759 claims -- short enough to read rather than skim.

WHAT IT STILL DOES NOT CATCH, STATED PLAINLY

One of the two proven-wrong claims. 0x1006B880 is caught: the original makes two
calls, br_ftol64 makes none. 0x1006B6C0 is NOT: the original makes two calls and
br_ftol32 makes one, and "two versus one" is ordinary variance, not a signal.
Nor would a claimed-callee check catch it -- none of 0x1006B6C0's callees are
themselves claimed, so there is nothing to compare against.

So this detector's measured recall on its own calibration set is ONE OUT OF TWO,
and the miss is not a threshold that can be tuned; it is a case with no
mechanical signal, found by reading the bytes. Anyone using this file to decide
that the manifest is clean is using it wrong. It narrows where to look. Random
sampling remains the only thing that estimates how much is wrong.

The flag is a smell, not a verdict, in the other direction too: an original
whose callees were all inlined into the port trips it honestly, and several of
the current twelve are exactly that.

Usage:
    claimcheck.py                the worklist
    claimcheck.py --selftest     the claim it CAN catch must stay caught
"""
import sys, os, csv, re, bisect

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
os.chdir(ROOT)

import pe as pelib
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

# The two claims the audit PROVED wrong, and which of them this detector can
# see. Both are listed because the KNOWN-MISS is the more important entry: it
# is the standing record that this tool's recall is one in two, kept here so
# the number cannot quietly become "it flags twelve things" in the retelling.
#
# (addr, build, symbol, file) -- the file is here because the selftest reads
# the symbol's BODY, and it must not need the claim to still exist to do that.
KNOWN_BAD = {
    ('0x1006B880', 'glide', 'br_ftol64', 'br_sfx.c'),
    ('0x1006B6C0', 'glide', 'br_ftol32', 'br_sfx.c'),
}
KNOWN_CAUGHT = {('0x1006B880', 'br_ftol64')}

# WHY THE SELFTEST NO LONGER READS config/ported.csv
#
# It used to: it ran the worklist and asserted 0x1006B880/br_ftol64 was still
# in it. That contract is self-destroying, and it destroyed itself -- both
# calibration claims have since been REPAIRED in the tree (0x1006B880 now
# names BrSfxChanStart in br_sfxsrc.c, which is the real 201-byte
# transcription, and 0x1006B6C0 carries no claim at all). The moment the
# defect was fixed the selftest failed, reporting a REGRESSION IN THE
# DETECTOR when what had actually happened was a repair in the tree. A
# calibration that cannot survive its own calibration case being fixed is
# measuring the manifest, not the detector.
#
# So the selftest now reconstructs each historical pair from the two things
# that do not change -- the bytes at the address, and the helper's body, which
# is still in br_sfx.c under a banner explaining why it claims nothing -- and
# applies the rule to it directly. It asserts exactly what it always meant to:
# the detector FIRES on the pair it can see and DOES NOT fire on the pair it
# cannot. If either helper is ever deleted the selftest says so and fails,
# rather than passing on an empty set.

# Not calls, though they parse like one.
NOT_CALLS = {
    'if', 'for', 'while', 'switch', 'return', 'sizeof', 'do', 'else',
    'int', 'char', 'float', 'double', 'void', 'unsigned', 'signed', 'long',
    'short', 'const', 'static', 'struct', 'union', 'enum', 'typedef',
    'int8_t', 'int16_t', 'int32_t', 'int64_t',
    'uint8_t', 'uint16_t', 'uint32_t', 'uint64_t', 'size_t', 'intptr_t',
    'CHECK', 'assert',
}


class Image:
    """One PE plus its function map. The two are paired deliberately: reading
    an address out of one image with the other's map disassembles the wrong
    bytes at a right-looking address, which reads as a real function."""

    def __init__(self, dll, mapfile):
        self.p = pelib.load(dll)
        self.text, self.text_va = self.p.text()
        self.funcs = {}
        with open(mapfile) as fh:
            for r in csv.DictReader(fh):
                self.funcs[int(r['va'], 16)] = int(r['size'])
        self.starts = sorted(self.funcs)
        self.md = Cs(CS_ARCH_X86, CS_MODE_32)

    def calls(self, va):
        """Distinct direct call targets in the function at `va`, or None if the
        address is not a known function start."""
        size = self.funcs.get(va)
        if not size:
            return None
        off = va - self.text_va
        if off < 0 or off + size > len(self.text):
            return None
        out = set()
        for ins in self.md.disasm(self.text[off:off + size], va):
            if ins.mnemonic == 'call':
                op = ins.op_str
                if op.startswith('0x'):
                    out.add(int(op, 16))
                else:
                    out.add(op)             # indirect: through a register or slot
        return out


def body(path, symbol):
    """The text of `symbol`'s definition, or None if there is no definition.

    A claim on a DECLARATION implements nothing, so None is itself a result
    worth reporting rather than an error to swallow.
    """
    try:
        with open(path, errors='ignore') as fh:
            src = fh.readlines()
    except OSError:
        return None
    pat = re.compile(r'\b%s\s*\(' % re.escape(symbol))
    for i, ln in enumerate(src):
        if ln.startswith((' ', '\t', '*', '/')) or not pat.search(ln):
            continue
        for j in range(i, min(i + 20, len(src))):  # signatures here run long
            s = src[j].rstrip()
            # A one-line body: either `{ return f(a, b); }` under the
            # signature, or `int F(void) { return 1; }` all on one line. Both
            # are common here -- br_dl.c writes its opcode handlers the first
            # way, slice2_19.c and slice8_85.c the second -- and an earlier
            # version of this parser reported TWENTY of them as HAVING NO
            # DEFINITION. That false finding looked exactly like the one real
            # one buried in it, which is the failure mode this whole tool
            # exists to catch, reproduced by the tool itself.
            if '{' in s and s.endswith('}'):
                return ''.join(src[i:j + 1])
            if s.endswith(';'):
                break                       # a declaration
            if s.endswith('{'):
                for k in range(j + 1, len(src)):
                    if src[k].startswith('}'):
                        return ''.join(src[i:k + 1])
                return None
    return None


def c_calls(text, symbol):
    """Call expressions in a C body.

    Two things must be excluded or the count is never zero, which is the whole
    signal. Comments: an @implements banner names functions, and counting those
    would hide exactly the defect this tool looks for. And the function's own
    name, which appears in its signature -- that alone made the first version
    of this detector report a call count of 1 for a body that calls nothing,
    and it failed its calibration set because of it.
    """
    text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.S)
    text = re.sub(r'//[^\n]*', ' ', text)
    out = set()
    for m in re.finditer(r'\b([A-Za-z_][A-Za-z0-9_]*)\s*\(', text):
        if m.group(1) not in NOT_CALLS and m.group(1) != symbol:
            out.add(m.group(1))
    return out


def find(name):
    for base in ('port/src', 'port/include', 'port/host'):
        for dirpath, _, files in os.walk(base):
            if name in files:
                return os.path.join(dirpath, name)
    return None


def main():
    img = {
        'glide': Image('orig/BRGlide.dll', 'config/functions_glide.csv'),
        'd3d':   Image('orig/BRD3D.dll',   'config/functions.csv'),
    }

    flagged, nobody, unknown, ok = [], [], [], 0
    with open('config/ported.csv') as fh:
        for r in csv.DictReader(fh):
            addr = '0x' + r['address'].upper()[2:]
            build, sym = r['build'], r['symbol']
            orig = img[build].calls(int(addr, 16))
            if orig is None:
                unknown.append((addr, build, sym, r['file']))
                continue
            path = find(r['file'])
            txt = body(path, sym) if path else None
            if txt is None:
                nobody.append((addr, build, sym, r['file']))
                continue
            mine = c_calls(txt, sym)
            # THE FLAG: the original delegates, the port does not.
            if len(orig) >= 2 and len(mine) == 0:
                flagged.append((len(orig), addr, build, sym, r['file']))
            else:
                ok += 1

    flagged.sort(reverse=True)

    if '--selftest' in sys.argv:
        # Apply the RULE to each historical pair directly. Nothing here reads
        # config/ported.csv: the pairs are defects that have since been fixed,
        # and the detector must still be shown to see the one it can see.
        claimed = {('0x' + r['address'].upper()[2:], r['symbol'])
                   for r in csv.DictReader(open('config/ported.csv'))}
        bad = 0
        for addr, build, sym, fname in sorted(KNOWN_BAD):
            orig = img[build].calls(int(addr, 16))
            path = find(fname)
            txt = body(path, sym) if path else None
            if orig is None or txt is None:
                print("SELFTEST FAILED: cannot reconstruct %s / %s -- %s is "
                      "gone from %s, so the calibration case no longer exists "
                      "and this tool's recall is no longer measured."
                      % (addr, sym, sym, fname))
                bad = 1
                continue
            mine = c_calls(txt, sym)
            fires = len(orig) >= 2 and len(mine) == 0
            want  = (addr, sym) in KNOWN_CAUGHT
            state = ("STILL CLAIMED -- unrepaired"
                     if (addr, sym) in claimed else "repaired in the tree")
            if fires != want:
                print("SELFTEST FAILED: the rule %s on %s %s (original makes "
                      "%d calls, %s makes %d) but the calibration says it %s."
                      % ("fires" if fires else "does not fire", addr, sym,
                         len(orig), sym, len(mine),
                         "should" if want else "should not"))
                bad = 1
            elif want:
                print("  caught: %s %-12s original makes %d calls, port makes "
                      "%d   (%s)" % (addr, sym, len(orig), len(mine), state))
            else:
                print("  KNOWN MISS: %s %s -- %d calls vs %d is not a signal. "
                      "No threshold fixes this.   (%s)"
                      % (addr, sym, len(orig), len(mine), state))
        if bad:
            return 1
        print("selftest ok: %d/%d proven-wrong claims caught by the rule, "
              "%d flagged in the tree as it stands"
              % (len(KNOWN_CAUGHT), len(KNOWN_BAD), len(flagged)))
        return 0

    print("claims checked: %d clean, %d FLAGGED, %d no definition found, "
          "%d address not a function start" % (ok, len(flagged), len(nobody),
                                               len(unknown)))
    print()
    print("FLAGGED -- the original delegates, the port calls nothing. This is a")
    print("smell, not a verdict: an original whose callees were all inlined into")
    print("the port trips it honestly. Read the bytes before concluding.")
    print()
    for n, addr, build, sym, f in flagged:
        mark = '  <- PROVEN WRONG' if (addr, sym) in KNOWN_BAD else ''
        print("  %-12s %-5s %-34s orig calls %2d, port 0   %s%s"
              % (addr, build, sym[:34], n, f, mark))

    if nobody:
        print()
        print("NO DEFINITION FOUND (%d). A claim on a declaration implements" % len(nobody))
        print("nothing; a symbol this parser cannot find may be a parser miss.")
        for d in nobody[:20]:
            print("  %-12s %-5s %-34s %s" % d)
        if len(nobody) > 20:
            print("  ... and %d more" % (len(nobody) - 20))

    if unknown:
        print()
        print("ADDRESS IS NOT A FUNCTION START in its claimed build (%d)." % len(unknown))
        print("Either the claim points into the middle of a function, or the")
        print("build tag is wrong. Both are defects.")
        for d in unknown[:20]:
            print("  %-12s %-5s %-34s %s" % d)
        if len(unknown) > 20:
            print("  ... and %d more" % (len(unknown) - 20))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
