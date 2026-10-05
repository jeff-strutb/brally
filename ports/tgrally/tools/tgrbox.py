"""tgrbox.py -- tools/tgrally/n64box.py as the port's comparison tools use it.

Two differences from the stock box, both where the box's model of the
machine is not the machine:

  * __d_to_ull / __f_to_ull (0x80266A08, 0x80266AA8).  The box runs them as
    Python (int(x) masked to 64 bits, which wraps a negative), but the ROM's
    code converts with the FPU's exception flags checked and returns all
    ones for anything outside [0, 2^64), a negative included: here they
    return what the ROM's instructions return.  (The game feeds a negative
    sound rate through __d_to_ull; the port does it the ROM's way, tgr_f2ull.)
  * the display-list digest leaves out a non-CI SETTILE's palette field,
    which the RDP never reads and BrTexLoad fills from stack residue.

The decomp's own tools keep using n64box.py unchanged.
"""
import hashlib
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.join(ROOT, 'tools/tgrally'))
import n64box  # noqa: E402


class Box(n64box.Box):
    def install_hle(self):
        super().install_hle()
        self.hle[0x80266A08] = ('__d_to_ull', lambda: self.to_ull(self.fdouble(12)))
        self.hle[0x80266AA8] = ('__f_to_ull', lambda: self.to_ull(self.fsingle(12)))

    def to_ull(self, x):
        """the ROM's __d_to_ull: truncation toward zero inside [0, 2^64),
        all ones outside it (and for NaN)"""
        v = int(x) if x == x and -1.0 < x < 18446744073709551616.0 else 0xFFFFFFFFFFFFFFFF
        self.ret(v >> 32, v & 0xffffffff)

    def dl_digest(self, dl):
        h = hashlib.sha1()
        seg = [0] * 16
        stack = [dl]
        seen = 0

        def r32(va):
            return struct.unpack('>I', self.uc.mem_read(self.phys(va), 4))[0]

        def addr(x):
            return (seg[(x >> 24) & 0xF] + (x & 0xFFFFFF)) | 0x80000000
        while stack and seen < 200000:
            pc = stack.pop()
            while seen < 200000:
                seen += 1
                w0, w1 = r32(pc), r32(pc + 4)
                op = w0 >> 24
                if op == 0xF5 and (w0 >> 21) & 7 != 2:
                    w1 &= ~0x00F00000
                h.update(struct.pack('>II', w0, w1))
                pc += 8
                if op == 0xDB and (w0 >> 16) & 0xFF == 0x06:
                    seg[((w0 & 0xFFFF) >> 2) & 0xF] = w1 & 0x1FFFFFFF
                elif op == 0x01:
                    h.update(self.read(addr(w1), 64))
                elif op == 0x04:
                    h.update(self.read(addr(w1), 16 * (((w0 >> 10) & 0x3F) or 1)))
                elif op == 0x03:
                    h.update(self.read(addr(w1), 16))
                elif op == 0x06:
                    if (w0 >> 16) & 0xFF != 1:
                        stack.append(pc)
                    pc = addr(w1)
                elif op == 0xB8:
                    break
        return h.hexdigest()[:16]


STACKS = n64box.Box.STACKS
M = n64box.M
GPR = n64box.GPR
sx = n64box.sx
ROM_PATH = n64box.ROM_PATH
