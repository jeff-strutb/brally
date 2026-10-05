"""A minimal stand-in for gdb's Python API, over the GDB remote protocol to
winedbg's --gdb server.  Only what c2prio's tracer uses: software breakpoints
(int3 written into the image, since winedbg answers Z0 with "unsupported"),
continue, register and memory reads, memory writes, detach, the exited event.
"""
import re
import socket
import struct
import time
import os, sys
DEBUG = os.environ.get('SHIM_DEBUG')
def _log(*a):
    if DEBUG: print('[shim]', *a, file=sys.stderr, flush=True)

REGS = ['eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi', 'eip', 'eflags']


class error(Exception):
    pass


class MemoryError(error):
    pass


class _RSP:
    def __init__(self, port):
        last = None
        for _ in range(400):
            try:
                self.s = socket.create_connection(('127.0.0.1', port))
                break
            except OSError as e:
                last = e
                time.sleep(0.05)
        else:
            raise error(f'cannot connect: {last}')
        self.s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.buf = b''

    def _read(self):
        while True:
            i = self.buf.find(b'$')
            j = self.buf.find(b'#', i) if i >= 0 else -1
            if i >= 0 and j >= 0 and len(self.buf) >= j + 3:
                pkt = self.buf[i + 1:j]
                self.buf = self.buf[j + 3:]
                self.s.sendall(b'+')
                return pkt.decode('latin1')
            d = self.s.recv(1 << 16)
            if not d:
                raise error('connection closed')
            self.buf += d

    def cmd(self, p):
        cs = sum(p.encode('latin1')) & 0xff
        self.s.sendall(b'$' + p.encode('latin1') + b'#%02x' % cs)
        return self._read()


_r = None
_tid = None            # thread of the last stop
_bps = {}              # addr -> [Breakpoint, ...] (several may share an address)
_inserted = {}         # addr -> original byte (hex)
_exited_cbs = []
_exited = False
_regcache = None
_exe = ''


class _Events:
    class _Exited:
        def connect(self, fn):
            _exited_cbs.append(fn)
    exited = _Exited()


events = _Events()


def connect(port, exe_name):
    global _r, _exe
    _r = _RSP(port)
    _exe = exe_name
    _r.cmd('?')


def _mem_read(addr, n):
    out = b''
    while n > 0:
        k = min(n, 0x800)
        rep = _r.cmd('m%x,%x' % (addr, k))
        if not rep or rep.startswith('E'):
            raise MemoryError(f'cannot read {addr:#x}')
        b = bytes.fromhex(rep)
        if not b:
            raise MemoryError(f'cannot read {addr:#x}')
        out += b
        addr += len(b)
        n -= len(b)
    return out


def _mem_write(addr, data):
    rep = _r.cmd('M%x,%x:%s' % (addr, len(data), bytes(data).hex()))
    if rep != 'OK':
        raise MemoryError(f'cannot write {addr:#x}: {rep}')


def _regs(tid):
    global _regcache
    if _regcache and _regcache[0] == tid:
        return _regcache[1]
    _r.cmd('Hg' + tid)
    g = _r.cmd('g')
    vals = struct.unpack('<10I', bytes.fromhex(g[:80]))
    _regcache = (tid, dict(zip(REGS, vals)))
    return _regcache[1]


def _set_eip(tid, v):
    global _regcache
    _r.cmd('Hg' + tid)
    before = _r.cmd('g')[:80]
    rep = _r.cmd('P8=%s' % struct.pack('<I', v).hex())
    after = _r.cmd('g')[:80]
    if DEBUG:
        b = struct.unpack('<10I', bytes.fromhex(before)); a2 = struct.unpack('<10I', bytes.fromhex(after))
        diff = [(REGS[i], hex(b[i]), hex(a2[i])) for i in range(10) if b[i] != a2[i]]
        _log('P8', diff)
    if rep != 'OK':
        raise error(f'cannot set eip: {rep}')
    _regcache = None


def _sync():
    """Make the int3s in memory match the enabled breakpoints."""
    want = {a for a, bl in _bps.items() if any(b.enabled for b in bl)}
    for a in list(_inserted):
        if a not in want:
            _mem_write(a, bytes.fromhex(_inserted.pop(a)))
    for a in want:
        if a not in _inserted:
            orig = _mem_read(a, 1)
            _inserted[a] = orig.hex()
            _mem_write(a, b'\xcc')


def _step_over(tid, addr):
    """The thread sits on a breakpoint address: run its real instruction once.
    winedbg can answer the step with another thread's pending event, so step
    until this thread's eip has left the address."""
    global _regcache
    if DEBUG:
        rg = _regs(tid)
        st = struct.unpack('<3I', _mem_read(rg['esp'], 12))
        _log('before step %#x: eip %#x esp %#x edi %#x stack %s code %s' % (addr, rg['eip'], rg['esp'], rg['edi'], [hex(x) for x in st], _mem_read(addr, 6).hex()))
    orig = bytes.fromhex(_inserted[addr]) if addr in _inserted else _mem_read(addr, 1)
    if orig == b'\xe8':
        # call rel32: winedbg's single step over a call pushes 8 bytes, so do
        # the call ourselves: push the return address and jump to the target.
        rel = struct.unpack('<i', _mem_read(addr + 1, 4))[0]
        rg = _regs(tid)
        esp = rg['esp'] - 4
        _mem_write(esp, struct.pack('<I', addr + 5))
        _r.cmd('Hg' + tid)
        if _r.cmd('P4=%s' % struct.pack('<I', esp).hex()) != 'OK':
            raise error('cannot set esp')
        _set_eip(tid, (addr + 5 + rel) & 0xffffffff)
        _regcache = None
        return 'call emulated'
    if addr in _inserted:
        _mem_write(addr, bytes.fromhex(_inserted[addr]))
    for _ in range(100):
        rep = _r.cmd('vCont;s:' + tid)
        _regcache = None
        if rep.startswith('W') or rep.startswith('X'):
            break
        if _regs(tid)['eip'] != addr:
            if DEBUG:
                rg = _regs(tid)
                st = struct.unpack('<3I', _mem_read(rg['esp'], 12))
                _log('stepped %#x -> eip %#x esp %#x edi %#x stack %s rep %s' % (addr, rg['eip'], rg['esp'], rg['edi'], [hex(x) for x in st], rep[:20]))
            break
        _log('step did not move', rep[:30])
    else:
        raise error(f'cannot step off {addr:#x}')
    if addr in _inserted:
        _mem_write(addr, b'\xcc')
    return rep


_pending_step = None   # (tid, addr) when the last real stop is on a breakpoint
_pass_sig = None       # (tid, sig) to hand back to the program on the next resume


def _continue():
    global _tid, _regcache, _exited, _pending_step
    while True:
        _sync()
        if _pending_step:
            t, a = _pending_step
            _pending_step = None
            _step_over(t, a)
        global _pass_sig
        if _pass_sig:
            t, sg = _pass_sig
            _pass_sig = None
            rep = _r.cmd('vCont;C%s:%s;c' % (sg, t))
        else:
            rep = _r.cmd('vCont;c')
        _regcache = None
        _log('stop', rep[:40])
        if rep.startswith('W') or rep.startswith('X'):
            _exited = True
            for fn in _exited_cbs:
                fn(rep)
            return
        m = re.search(r'thread:([0-9a-fA-F]+)', rep)
        if not m:
            continue
        tid = m.group(1)
        eip = _regs(tid)['eip']
        a = eip - 1
        _log('eip %#x' % eip, 'bp' if a in _inserted else '')
        if a in _inserted:
            _set_eip(tid, a)
            _tid = tid
            stop = False
            for bp in _bps.get(a, []):
                if not bp.enabled:
                    continue
                if type(bp).stop is Breakpoint.stop:
                    stop = True
                elif bp.stop():
                    stop = True
            if stop:
                _pending_step = (tid, a)
                return
            _step_over(tid, a)
            continue
        # anything else: a first-chance exception in the program (pass it on,
        # as gdb does for SIGSEGV) or winedbg's attach-thread break (just go on)
        sig = rep[1:3]
        if sig != '05' and eip != 0xfff40d6c:
            _pass_sig = (tid, sig)


def execute(cmd, to_string=False):
    global _pending_step
    cmd = cmd.strip()
    if cmd == 'continue':
        if _exited:
            raise error('The program is not being run.')
        _continue()
        return ''
    if cmd == 'detach':
        for a in list(_inserted):
            _mem_write(a, bytes.fromhex(_inserted.pop(a)))
        _pending_step = None
        _r.cmd('D')
        return ''
    if cmd.startswith('set ') or cmd.startswith('target '):
        return ''
    raise error(f'unsupported command {cmd!r}')


class Breakpoint:
    def __init__(self, spec, internal=False):
        self.addr = int(spec.lstrip('*'), 0)
        self.enabled = True
        _bps.setdefault(self.addr, []).append(self)

    def stop(self):
        return True


class _Frame:
    def read_register(self, name):
        return _regs(_tid)[name]


def selected_frame():
    return _Frame()


class _Inferior:
    def read_memory(self, addr, n):
        return memoryview(_mem_read(addr, n))

    def write_memory(self, addr, data):
        _mem_write(addr, bytes(data))

    def threads(self):
        return [] if _exited else [_tid]


def selected_inferior():
    return _Inferior()


class _Progspace:
    @property
    def filename(self):
        return _exe


def current_progspace():
    return _Progspace()
