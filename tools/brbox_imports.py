"""brbox_imports.py -- the import edge of the boxed game (tools/brbox.py).

Every IAT slot of BRGlide.dll, and everything the game reaches dynamically
(LoadLibrary/GetProcAddress, COM vtables), resolves to one of:

  MODEL   behaviour the game's logic depends on, modelled faithfully:
          the heap, files served from the retail disc (brbox_fs), a virtual
          clock, the window/message system, scripted input, the CRT.
  STUB    pure output the game never reads back (Glide rendering, GDI, sound,
          CD audio): recorded, answered with the success a working machine
          gives.
  UNMODELLED  anything else raises GuestFault naming the import.  Nothing is
          ever answered by guess -- a new import surfaces as a stop.

A model is `fn(box, a)`: `a[i]` is the i-th dword argument.  It returns eax
(an int), (eax, edx), or None; or it is a generator that yields
('call', va, [args]) to run guest code (a WndProc, a qsort comparator) and
receives that call's eax.  See Box._drive.

Stub fidelity only ever cuts one way for the oracle: a stub that answers
differently from Windows can make the run reach FEWER real states, but both
sides of every A/B comparison see the same stub, so it can never manufacture
a false EQUIVALENT.
"""
import math
import struct

import brbox_fs
from brbox import GuestFault, Stop, EXE_BASE, CD_ROOT, HEAP_HI, BLOCK

MODELS = {}      # 'DLL!Name' -> (pop_bytes, fn, nargs)


def model(key, pop=None, nargs=None):
    """Register a model.  For a stdcall import `pop` is its argument bytes;
    cdecl imports (MSVCRT) pop 0.  `nargs` is what the call log records."""
    def deco(fn):
        MODELS[key] = (pop, fn, nargs)
        return fn
    return deco


def std(dll, name, nbytes):
    return model('%s!%s' % (dll, name), pop=nbytes, nargs=nbytes // 4)


def crt(name, nargs=0):
    return model('MSVCRT.dll!%s' % name, pop=0, nargs=nargs)


# ============================================================ heap =========

def heap_alloc(box, n, zero=False):
    hs = box.hs
    n = max((n + 15) & ~15, 16)
    best = None
    for i, (a, sz) in enumerate(hs.heap_free):
        if sz >= n:
            best = i
            break
    if best is not None:
        a, sz = hs.heap_free.pop(best)
        if sz - n >= 32:
            hs.heap_free.insert(best, (a + n, sz - n))
        else:
            n = sz
    else:
        a = hs.heap_next
        if a + n > HEAP_HI:
            return 0
        hs.heap_next = a + n
    hs.heap_blocks[a] = n
    if zero:
        box.wr(a, b'\0' * n)
    return a


def heap_free(box, a):
    hs = box.hs
    n = hs.heap_blocks.pop(a, None)
    if n is None:
        return False
    # address-ordered free list, coalescing neighbours
    fl = hs.heap_free
    i = 0
    while i < len(fl) and fl[i][0] < a:
        i += 1
    fl.insert(i, (a, n))
    if i + 1 < len(fl) and fl[i][0] + fl[i][1] == fl[i + 1][0]:
        fl[i] = (fl[i][0], fl[i][1] + fl[i + 1][1])
        fl.pop(i + 1)
    if i > 0 and fl[i - 1][0] + fl[i - 1][1] == fl[i][0]:
        fl[i - 1] = (fl[i - 1][0], fl[i - 1][1] + fl[i][1])
        fl.pop(i)
    return True


def new_handle(box):
    box.hs.next_handle += 4
    return box.hs.next_handle


# ======================================================== printf ===========

def c_format(box, fmt, argp, wide=False):
    """MSVCRT printf-family formatting.  Reads varargs from guest memory at
    `argp`; returns (text, bytes_of_args_consumed)."""
    out = []
    i = 0
    n = len(fmt)
    ap = argp
    while i < n:
        c = fmt[i]
        if c != '%':
            out.append(c)
            i += 1
            continue
        i += 1
        if i < n and fmt[i] == '%':
            out.append('%')
            i += 1
            continue
        flags = ''
        while i < n and fmt[i] in '-+ #0':
            flags += fmt[i]
            i += 1
        width = ''
        if i < n and fmt[i] == '*':
            width = str(struct.unpack('<i', box.rd(ap, 4))[0])
            ap += 4
            i += 1
        while i < n and fmt[i].isdigit():
            width += fmt[i]
            i += 1
        prec = None
        if i < n and fmt[i] == '.':
            i += 1
            prec = ''
            if i < n and fmt[i] == '*':
                prec = str(struct.unpack('<i', box.rd(ap, 4))[0])
                ap += 4
                i += 1
            while i < n and fmt[i].isdigit():
                prec += fmt[i]
                i += 1
        size = ''
        while i < n and fmt[i] in 'hlLIwF N':
            if fmt[i] == 'I' and fmt[i:i + 3] == 'I64':
                size = 'I64'
                i += 3
                continue
            size += fmt[i]
            i += 1
        if i >= n:
            break
        conv = fmt[i]
        i += 1
        spec = '%' + flags + width + ('.' + prec if prec is not None else '')
        if conv in 'di':
            if size == 'I64':
                v = struct.unpack('<q', box.rd(ap, 8))[0]
                ap += 8
            else:
                v = struct.unpack('<i', box.rd(ap, 4))[0]
                ap += 4
                if 'h' in size:
                    v = struct.unpack('<h', struct.pack('<H', v & 0xFFFF))[0]
            out.append((spec + 'd') % v)
        elif conv in 'uoxX':
            v = box.rd32(ap)
            ap += 4
            if 'h' in size:
                v &= 0xFFFF
            out.append((spec + conv) % v)
        elif conv in 'cC':
            # %C is the wide-char form: its int argument prints as one char
            v = box.rd32(ap) & (0xFF if conv == 'c' else 0xFFFF)
            ap += 4
            out.append((spec + 's') % chr(v & 0xFF))
        elif conv == 's':
            p = box.rd32(ap)
            ap += 4
            s = '(null)' if p == 0 else box.cstr(p, 1 << 16)
            out.append((spec + 's') % s)
        elif conv == 'S':
            p = box.rd32(ap)
            ap += 4
            ws = bytearray()
            while p and len(ws) < 1 << 16:
                ch = box.rd16(p + len(ws))
                if ch == 0:
                    break
                ws += struct.pack('<H', ch)
            out.append((spec + 's') % ('(null)' if not p else ws.decode('utf-16-le')))
        elif conv in 'eEfgG':
            v = struct.unpack('<d', box.rd(ap, 8))[0]
            ap += 8
            if v != v:
                out.append('-1.#IND00' if math.copysign(1, v) < 0 else '1.#QNAN0')
            elif v in (float('inf'), float('-inf')):
                out.append('1.#INF00' if v > 0 else '-1.#INF00')
            else:
                t = (spec + conv) % v
                if conv in 'eEgG':
                    # MSVCRT prints a three-digit exponent: 1.5e+003
                    import re
                    t = re.sub(r'([eE][+-])(\d\d)$', lambda m: m.group(1) + '0' + m.group(2), t)
                out.append(t)
        elif conv == 'p':
            v = box.rd32(ap)
            ap += 4
            out.append('%08X' % v)
        elif conv == 'n':
            box.wr32(box.rd32(ap), len(''.join(out)))
            ap += 4
        else:
            raise GuestFault('printf conversion %%%s unmodelled in %r' % (conv, fmt))
    return ''.join(out), ap - argp


def c_scan(box, text, fmt, argp):
    """MSVCRT sscanf subset: %d %i %u %x %f %e %g %s %c %[..] %n, widths, '*'
    suppression, 'l'/'h' sizes.  Returns the assignment count (-1 at EOF
    before the first conversion)."""
    import re
    si = 0
    fi = 0
    assigned = 0
    ap = argp
    tl = len(text)
    while fi < len(fmt):
        c = fmt[fi]
        if c.isspace():
            while si < tl and text[si].isspace():
                si += 1
            fi += 1
            continue
        if c != '%':
            if si < tl and text[si] == c:
                si += 1
                fi += 1
                continue
            break
        fi += 1
        if fmt[fi] == '%':
            if si < tl and text[si] == '%':
                si += 1
                fi += 1
                continue
            break
        suppress = False
        if fmt[fi] == '*':
            suppress = True
            fi += 1
        width = ''
        while fmt[fi].isdigit():
            width += fmt[fi]
            fi += 1
        width = int(width) if width else None
        size = ''
        while fmt[fi] in 'hlL':
            size += fmt[fi]
            fi += 1
        conv = fmt[fi]
        fi += 1
        if conv not in 'c[n':
            while si < tl and text[si].isspace():
                si += 1
        if si >= tl and conv != 'n':
            return assigned if assigned else -1
        lim = tl if width is None else min(tl, si + width)
        if conv in 'diux':
            pat = {'d': r'[+-]?\d+', 'u': r'[+-]?\d+', 'i': r'[+-]?(0[xX][0-9a-fA-F]+|0[0-7]*|\d+)',
                   'x': r'[+-]?(0[xX])?[0-9a-fA-F]+'}[conv]
            m = re.match(pat, text[si:lim])
            if not m:
                break
            tok = m.group(0)
            si += len(tok)
            v = int(tok, 16 if conv == 'x' else (0 if conv == 'i' else 10))
            if not suppress:
                p = box.rd32(ap)
                ap += 4
                if 'h' in size:
                    box.wr16(p, v)
                else:
                    box.wr32(p, v)
                assigned += 1
        elif conv in 'efgEG':
            m = re.match(r'[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?', text[si:lim])
            if not m:
                break
            tok = m.group(0)
            si += len(tok)
            v = float(tok)
            if not suppress:
                p = box.rd32(ap)
                ap += 4
                if 'l' in size:
                    box.wr(p, struct.pack('<d', v))
                else:
                    box.wr(p, struct.pack('<f', v))
                assigned += 1
        elif conv == 's':
            j = si
            while j < lim and not text[j].isspace():
                j += 1
            tok = text[si:j]
            si = j
            if not suppress:
                p = box.rd32(ap)
                ap += 4
                box.wr(p, tok.encode('latin1') + b'\0')
                assigned += 1
        elif conv == 'c':
            w = width or 1
            tok = text[si:si + w]
            if len(tok) < w:
                break
            si += w
            if not suppress:
                p = box.rd32(ap)
                ap += 4
                box.wr(p, tok.encode('latin1'))
                assigned += 1
        elif conv == '[':
            end = fmt.index(']', fi + (1 if fmt[fi] in '^]' else 0) + (1 if fmt[fi:fi + 2] == '^]' else 0))
            setspec = fmt[fi:end]
            fi = end + 1
            neg = setspec.startswith('^')
            chars = setspec[1:] if neg else setspec
            j = si
            while j < lim and ((text[j] in chars) != neg):
                j += 1
            if j == si:
                break
            tok = text[si:j]
            si = j
            if not suppress:
                p = box.rd32(ap)
                ap += 4
                box.wr(p, tok.encode('latin1') + b'\0')
                assigned += 1
        elif conv == 'n':
            if not suppress:
                p = box.rd32(ap)
                ap += 4
                box.wr32(p, si)
        else:
            raise GuestFault('sscanf conversion %%%s unmodelled' % conv)
    return assigned


# ======================================================= KERNEL32 ==========
K = 'KERNEL32.dll'


@std(K, 'DisableThreadLibraryCalls', 4)
def _dtlc(box, a):
    return 1


# ------------------------------------------------ waitable objects --------
# Events (manual/auto reset), mutexes and thread handles.  A wait that cannot
# be satisfied BLOCKs the calling thread (see Box._switch); the thread
# re-executes the same Wait* when it is rescheduled, so a satisfied retry
# consumes the object exactly as the first attempt would have.
WAIT_TIMEOUT = 0x102
INFINITE = 0xFFFFFFFF


def _signalled(box, h):
    ev = box.hs.events.get(h)
    if ev is not None:
        return ev['set']
    if h in box.hs.mutexes:
        return True
    for t in box.threads:
        if t.handle == h:
            return t.state == 'done'
    raise GuestFault('wait on unknown handle %08X' % h)


def _consume(box, h):
    ev = box.hs.events.get(h)
    if ev is not None and not ev['manual']:
        ev['set'] = False


def wait_ready(box, t):
    if t.wait is None:
        return True
    hs, wall, deadline = t.wait
    sig = [_signalled(box, h) for h in hs]
    if (all(sig) if wall else any(sig)):
        return True
    return deadline is not None and box.hs.ms >= deadline


def _wait(box, handles, wall, timeout):
    t = box.cur
    sig = [_signalled(box, h) for h in handles]
    if all(sig) if wall else any(sig):
        t.wait = None
        if wall:
            for h in handles:
                _consume(box, h)
            return 0
        i = sig.index(True)
        _consume(box, handles[i])
        return i
    if timeout == 0:
        t.wait = None
        return WAIT_TIMEOUT
    if t.wait is not None and t.wait[2] is not None and box.hs.ms >= t.wait[2]:
        t.wait = None
        return WAIT_TIMEOUT
    if box.subrun:
        raise GuestFault('blocking wait inside a live-oracle sub-run')
    if t.wait is None:
        t.wait = (list(handles), wall, None if timeout == INFINITE else box.hs.ms + timeout)
    return BLOCK


@std(K, 'WaitForSingleObject', 8)
def _wfso(box, a):
    return _wait(box, [a[0]], False, a[1])


@std(K, 'WaitForMultipleObjects', 16)
def _wfmo(box, a):
    hs = [box.rd32(a[1] + 4 * k) for k in range(a[0])]
    return _wait(box, hs, bool(a[2]), a[3])


@std(K, 'CreateMutexA', 12)
def _cmutex(box, a):
    h = new_handle(box)
    box.hs.mutexes[h] = 1
    return h


@std(K, 'ReleaseMutex', 4)
def _relmutex(box, a):
    return 1


@std(K, 'CreateEventA', 16)
def _cevent(box, a):
    h = new_handle(box)
    box.hs.events[h] = {'manual': bool(a[1]), 'set': bool(a[2])}
    return h


@std(K, 'SetEvent', 4)
def _setevent(box, a):
    ev = box.hs.events.get(a[0])
    if ev is None:
        return 0
    ev['set'] = True
    return 1


@std(K, 'CloseHandle', 4)
def _closeh(box, a):
    return 1


@std(K, 'InitializeCriticalSection', 4)
def _ics(box, a):
    box.wr(a[0], b'\0' * 24)
    return None


@std(K, 'DeleteCriticalSection', 4)
def _dcs(box, a):
    return None


@std(K, 'EnterCriticalSection', 4)
def _ecs(box, a):
    return None


@std(K, 'LeaveCriticalSection', 4)
def _lcs(box, a):
    return None


@std(K, 'CreateThread', 24)
def _cthread(box, a):
    if box.subrun:
        raise GuestFault('CreateThread inside a live-oracle sub-run')
    t = box.thread_create(a[2], a[3])
    if a[5]:
        box.wr32(a[5], t.tid)
    box.log('CreateThread(start=%08X, param=%08X) -> tid %d' % (a[2], a[3], t.tid))
    return t.handle


@std(K, 'ExitThread', 4)
def _exitthread(box, a):
    t = box.cur
    if t is box.threads[0]:
        raise Stop('main thread ExitThread(%d)' % a[0])
    t.state = 'done'
    t.code = a[0]
    return BLOCK


QPC_HZ = 1193182


@std(K, 'QueryPerformanceFrequency', 4)
def _qpf(box, a):
    box.wr(a[0], struct.pack('<q', QPC_HZ))
    return 1


@std(K, 'QueryPerformanceCounter', 4)
def _qpc(box, a):
    box.tick()
    box.wr(a[0], struct.pack('<q', int(box.hs.ms * QPC_HZ / 1000.0)))
    return 1


@std(K, 'Sleep', 4)
def _sleep(box, a):
    # Sleep blocks the CALLING thread; the others run on meanwhile.  Only
    # when no other thread could run does the sleep move the clock itself.
    # (Advancing the shared clock by every sleep let the network thread's
    # once-a-second Sleep(960) race virtual time 16x ahead of the frames,
    # and every WM_TIMER then fired each frame.)
    t = box.cur
    if t.wait is not None:                  # re-entered after the block
        if box.hs.ms >= t.wait[2]:
            t.wait = None
            return None
        return BLOCK
    if box.subrun or a[0] == 0 or not box.others_runnable():
        box.advance(a[0])
        if not box.subrun and a[0] == 0 and box.others_runnable():
            box.want_yield = True
        return None
    t.wait = ([], False, box.hs.ms + a[0])
    return BLOCK


@std(K, 'GetDriveTypeA', 4)
def _gdt(box, a):
    p = box.cstr(a[0]) if a[0] else box.hs.cwd[:3]
    return brbox_fs.DRIVES.get(p[:1].upper(), 1)       # 1 = DRIVE_NO_ROOT_DIR


@std(K, 'GetVolumeInformationA', 32)
def _gvi(box, a):
    p = box.cstr(a[0]) if a[0] else box.hs.cwd[:3]
    d = p[:1].upper()
    if d not in brbox_fs.DRIVES:
        return 0
    if a[1]:
        box.wcstr(a[1], brbox_fs.VOLUME[d], a[2])
    if a[3]:
        box.wr32(a[3], 0x12345678 if d == 'D' else 0x1C0FFEE)
    if a[4]:
        box.wr32(a[4], 255)
    if a[5]:
        box.wr32(a[5], 0)
    if a[6]:
        box.wcstr(a[6], 'CDFS' if d == 'D' else 'FAT32', a[7])
    return 1


@std(K, 'lstrcpyA', 8)
def _lstrcpy(box, a):
    box.wr(a[0], (box.cstr(a[1], 1 << 16) or '').encode('latin1') + b'\0')
    return a[0]


@std(K, 'lstrlenA', 4)
def _lstrlen(box, a):
    return len(box.cstr(a[0], 1 << 16) or '')


@std(K, 'GetVersionExA', 4)
def _gvex(box, a):
    # Windows 98 (4.10.1998), VER_PLATFORM_WIN32_WINDOWS
    box.wr(a[0] + 4, struct.pack('<IIII', 4, 10, 0x040A07CE, 1))
    box.wr(a[0] + 20, b' A \0')
    return 1


@std(K, 'GlobalMemoryStatus', 4)
def _gms(box, a):
    mb = 1 << 20
    box.wr(a[0], struct.pack('<IIIIIIII', 32, 30, 64 * mb, 40 * mb, 128 * mb, 100 * mb,
                             2048 * mb, 1900 * mb))
    return None


@std(K, 'GlobalAlloc', 8)
def _galloc(box, a):
    # One address serves as both HGLOBAL and pointer (GMEM_FIXED semantics);
    # GlobalLock/GlobalHandle map between them identically.
    return heap_alloc(box, a[1], zero=bool(a[0] & 0x40))


@std(K, 'GlobalLock', 4)
def _glock(box, a):
    return a[0]


@std(K, 'GlobalUnlock', 4)
def _gunlock(box, a):
    return 0


@std(K, 'GlobalHandle', 4)
def _ghandle(box, a):
    return a[0]


@std(K, 'GlobalFree', 4)
def _gfree(box, a):
    heap_free(box, a[0])
    return 0


@std(K, 'OutputDebugStringA', 4)
def _ods(box, a):
    box.log('ODS %s' % (box.cstr(a[0]) or '').rstrip())
    return None


# ------------------------------------------------- modules and GetProcAddress
MODULES = {}      # lowercase dll name -> handle (only the ones the box provides)


def _module_handle(name):
    n = name.lower()
    if not n.endswith('.dll'):
        n += '.dll'
    return n


@std(K, 'GetModuleHandleA', 4)
def _gmh(box, a):
    if a[0] == 0:
        return EXE_BASE
    n = _module_handle(box.cstr(a[0]))
    if n in ('brglide.dll',):
        return box.pe.image_base
    return box.hs.loaded.get(n, 0)


@std(K, 'LoadLibraryA', 4)
def _loadlib(box, a):
    n = _module_handle(box.cstr(a[0]).replace('/', '\\').split('\\')[-1])
    if n in box.hs.loaded:
        return box.hs.loaded[n]
    if n not in DYNAMIC:
        box.log('LoadLibraryA(%s) -> NULL (not provided)' % n)
        return 0
    box.hs.next_module = getattr(box.hs, 'next_module', 0) + 1
    h = 0x70000000 + 0x10000 * box.hs.next_module
    box.hs.loaded[n] = h
    box.log('LoadLibraryA(%s) -> %08X' % (n, h))
    return h


@std(K, 'FreeLibrary', 4)
def _freelib(box, a):
    for k, v in list(box.hs.loaded.items()):
        if v == a[0]:
            del box.hs.loaded[k]
    return 1


@std(K, 'GetProcAddress', 8)
def _gpa(box, a):
    mod = next((k for k, v in box.hs.loaded.items() if v == a[0]), None)
    if mod is None:
        return 0
    if a[1] < 0x10000:
        name = '#%d' % a[1]
    else:
        name = box.cstr(a[1])
    table = DYNAMIC[mod]
    ent = table.get(name)
    if ent is None:
        # decorated stdcall (_Name@N): the decoration IS the arg byte count
        if '@' in name and name.rsplit('@', 1)[1].isdigit() and '*' in table:
            pop = int(name.rsplit('@', 1)[1])
            ent = (pop, table['*'](name))
        else:
            box.log('GetProcAddress(%s, %s) -> NULL' % (mod, name))
            return 0
    pop, fn = ent
    return box.trap('%s!%s' % (mod, name), pop, fn)


# ========================================================= ADVAPI32 ========
A = 'ADVAPI32.dll'
REGISTRY = {
    'software\\southpeak interactive\\boss rally': {'Directory': brbox_fs.INSTALL_DIR},
}


@std(A, 'RegOpenKeyExA', 20)
def _regopen(box, a):
    k = (box.cstr(a[1]) or '').lower().strip('\\')
    if a[0] == 0x80000002 and k in REGISTRY:
        h = new_handle(box)
        box.hs.regkeys[h] = k
        box.wr32(a[4], h)
        return 0
    return 2          # ERROR_FILE_NOT_FOUND


@std(A, 'RegQueryValueExA', 24)
def _regquery(box, a):
    k = box.hs.regkeys.get(a[0])
    name = box.cstr(a[1]) if a[1] else ''
    vals = REGISTRY.get(k, {})
    v = next((vals[x] for x in vals if x.lower() == (name or '').lower()), None)
    if v is None:
        return 2
    data = v.encode('latin1') + b'\0' if isinstance(v, str) else struct.pack('<I', v)
    if a[3]:
        box.wr32(a[3], 1 if isinstance(v, str) else 4)       # REG_SZ / REG_DWORD
    cap = box.rd32(a[5]) if a[5] else 0
    if a[5]:
        box.wr32(a[5], len(data))
    if a[4]:
        if cap < len(data):
            return 234            # ERROR_MORE_DATA
        box.wr(a[4], data)
    return 0


@std(A, 'RegCloseKey', 4)
def _regclose(box, a):
    box.hs.regkeys.pop(a[0], None)
    return 0


@std(A, 'GetUserNameA', 8)
def _getuser(box, a):
    name = b'Player\0'
    cap = box.rd32(a[1])
    box.wr32(a[1], len(name))
    if cap < len(name):
        return 0
    box.wr(a[0], name)
    return 1


# ============================================================ USER32 =======
U = 'USER32.dll'
HWND_DESKTOP = 0x00010010
WM_CREATE, WM_DESTROY, WM_ACTIVATE, WM_SETFOCUS = 0x01, 0x02, 0x06, 0x07
WM_QUIT, WM_ACTIVATEAPP, WM_SHOWWINDOW, WM_SIZE = 0x12, 0x1C, 0x18, 0x05
WM_NCCREATE, WM_TIMER, WM_KEYDOWN, WM_KEYUP, WM_CHAR = 0x81, 0x113, 0x100, 0x101, 0x102
WM_CLOSE, WM_SETCURSOR, WM_ERASEBKGND, WM_PAINT = 0x10, 0x20, 0x14, 0x0F


def post(box, hwnd, msg, wp=0, lp=0):
    box.hs.msgq.append((hwnd, msg, wp & 0xFFFFFFFF, lp & 0xFFFFFFFF, int(box.hs.ms)))


def send(box, hwnd, msg, wp=0, lp=0):
    """Generator: deliver a message synchronously to the window's procedure."""
    w = box.hs.windows.get(hwnd)
    if w is None:
        return 0
    r = yield ('call', w['proc'], [hwnd, msg, wp & 0xFFFFFFFF, lp & 0xFFFFFFFF])
    return r


@std(U, 'RegisterClassA', 4)
def _regclass(box, a):
    proc = box.rd32(a[0] + 4)
    name = box.cstr(box.rd32(a[0] + 36))
    box.hs.classes[name.lower()] = proc
    return 0xC000 + len(box.hs.classes)


@std(U, 'CreateWindowExA', 48)
def _createwin(box, a):
    cls = a[1]
    cname = box.cstr(cls) if cls >= 0x10000 else '#%d' % cls
    proc = box.hs.classes.get(cname.lower())
    if proc is None:
        raise GuestFault('CreateWindowExA of unregistered class %r' % cname)
    hwnd = 0x00020000 + 0x10 * (len(box.hs.windows) + 1)
    box.hs.windows[hwnd] = {'proc': proc, 'cls': cname, 'w': a[6] & 0xFFFF, 'h': a[7] & 0xFFFF}
    # CREATESTRUCTA for WM_NCCREATE / WM_CREATE
    cs = box.host_alloc(48, 4)
    box.wr(cs, struct.pack('<12I', a[11], a[10], a[9], a[8], a[7], a[6], a[5], a[4],
                           a[3], a[2], a[1], a[0]))
    r = yield from send(box, hwnd, WM_NCCREATE, 0, cs)
    if r == 0:
        del box.hs.windows[hwnd]
        return 0
    r = yield from send(box, hwnd, WM_CREATE, 0, cs)
    if r & 0xFFFFFFFF == 0xFFFFFFFF:
        del box.hs.windows[hwnd]
        return 0
    return hwnd


@std(U, 'ShowWindow', 8)
def _showwin(box, a):
    hwnd = a[0]
    w = box.hs.windows.get(hwnd)
    if w is None:
        return 0
    was = w.get('visible', False)
    if a[1] != 0 and not was:
        w['visible'] = True
        # the activation sequence Windows sends a top-level window shown
        # for the first time with SW_SHOWNORMAL
        yield from send(box, hwnd, WM_SHOWWINDOW, 1, 0)
        yield from send(box, hwnd, WM_ACTIVATEAPP, 1, 0)
        yield from send(box, hwnd, 0x86, 1, 0)                 # WM_NCACTIVATE
        yield from send(box, hwnd, WM_ACTIVATE, 1, 0)          # WA_ACTIVE
        box.hs.focus = hwnd
        yield from send(box, hwnd, WM_SETFOCUS, 0, 0)
        yield from send(box, hwnd, WM_SIZE, 0, (w['h'] << 16) | w['w'])
    return 1 if was else 0


@std(U, 'UpdateWindow', 4)
def _updwin(box, a):
    return 1


@std(U, 'SetFocus', 4)
def _setfocus(box, a):
    old = box.hs.focus
    box.hs.focus = a[0]
    return old


@std(U, 'GetForegroundWindow', 0)
def _getfg(box, a):
    return box.hs.focus


@std(U, 'GetActiveWindow', 0)
def _getactive(box, a):
    return box.hs.focus


@std(U, 'GetDesktopWindow', 0)
def _getdesk(box, a):
    return HWND_DESKTOP


@std(U, 'IsWindow', 4)
def _iswin(box, a):
    return 1 if a[0] in box.hs.windows or a[0] == HWND_DESKTOP else 0


@std(U, 'FindWindowA', 8)
def _findwin(box, a):
    return 0            # no other copy of the game is running


@std(U, 'IsIconic', 4)
def _isiconic(box, a):
    return 0


@std(U, 'GetWindowThreadProcessId', 8)
def _gwtpid(box, a):
    if a[1]:
        box.wr32(a[1], 0xFFF00001)
    return 0xFFF00002


@std(U, 'BringWindowToTop', 4)
def _bwtt(box, a):
    return 1


@std(U, 'SetForegroundWindow', 4)
def _sfw(box, a):
    return 1


@std(U, 'GetLastActivePopup', 4)
def _glap(box, a):
    return a[0]


@std(U, 'GetWindowLongA', 8)
def _gwl(box, a):
    w = box.hs.windows.get(a[0])
    if w is None:
        return 0
    return w.get('long', {}).get(a[1] & 0xFFFFFFFF, 0)


@std(U, 'InvalidateRect', 12)
def _invrect(box, a):
    return 1


@std(U, 'SetCursor', 4)
def _setcursor(box, a):
    return 0


@std(U, 'LoadCursorA', 8)
def _loadcursor(box, a):
    return 0x00030001


@std(U, 'LoadIconA', 8)
def _loadicon(box, a):
    return 0x00030002


@std(U, 'RegisterWindowMessageA', 4)
def _regwinmsg(box, a):
    name = box.cstr(a[0])
    return box.hs.winmsgs.setdefault(name, 0xC100 + len(box.hs.winmsgs))


@std(U, 'PostMessageA', 16)
def _postmsg(box, a):
    post(box, a[0], a[1], a[2], a[3])
    return 1


@std(U, 'PostQuitMessage', 4)
def _pqm(box, a):
    post(box, 0, WM_QUIT, a[0], 0)
    return None


@std(U, 'SetTimer', 16)
def _settimer(box, a):
    box.hs.timers[(a[0], a[1])] = [a[2], box.hs.ms + a[2]]
    return a[1] if a[0] else 1


@std(U, 'KillTimer', 8)
def _killtimer(box, a):
    return 1 if box.hs.timers.pop((a[0], a[1]), None) is not None else 0


def _write_msg(box, p, m):
    hwnd, msg, wp, lp, t = m
    box.wr(p, struct.pack('<IIIIIii', hwnd, msg, wp, lp, t & 0xFFFFFFFF, 320, 240))


@std(U, 'PeekMessageA', 20)
def _peek(box, a):
    box.pump()
    q = box.hs.msgq
    for i, m in enumerate(q):
        if a[1] not in (0, m[0]):
            continue
        if (a[2] or a[3]) and not (a[2] <= m[1] <= a[3]):
            continue
        _write_msg(box, a[0], m)
        if a[4] & 1:             # PM_REMOVE
            del q[i]
        return 1
    box.idle_peek()
    if not box.subrun and box.others_runnable():
        box.want_yield = True
    return 0


@std(U, 'GetMessageA', 16)
def _getmsg(box, a):
    while True:
        box.pump()
        q = box.hs.msgq
        for i, m in enumerate(q):
            if a[1] not in (0, m[0]):
                continue
            if (a[2] or a[3]) and not (a[2] <= m[1] <= a[3]):
                continue
            del q[i]
            _write_msg(box, a[0], m)
            return 0 if m[1] == WM_QUIT else 1
        box.wait_for_event()


@std(U, 'WaitMessage', 0)
def _waitmsg(box, a):
    box.wait_for_event()
    return 1


@std(U, 'TranslateMessage', 4)
def _translate(box, a):
    hwnd, msg, wp = box.rd32(a[0]), box.rd32(a[0] + 4), box.rd32(a[0] + 8)
    if msg == WM_KEYDOWN:
        ch = VK_CHAR.get(wp)
        if ch is not None:
            post(box, hwnd, WM_CHAR, ch, box.rd32(a[0] + 12))
        return 1
    return 0


@std(U, 'DispatchMessageA', 4)
def _dispatch(box, a):
    hwnd, msg, wp, lp = (box.rd32(a[0] + 4 * k) for k in range(4))
    if msg == WM_TIMER and lp:
        r = yield ('call', lp, [hwnd, msg, wp, box.hs.ms and int(box.hs.ms)])
        return r
    r = yield from send(box, hwnd, msg, wp, lp)
    return r


@std(U, 'DefWindowProcA', 16)
def _defwndproc(box, a):
    hwnd, msg = a[0], a[1]
    if msg == WM_NCCREATE:
        return 1
    if msg == WM_CLOSE:
        yield from send(box, hwnd, WM_DESTROY, 0, 0)
        box.hs.windows.pop(hwnd, None)
        return 0
    return 0


@std(U, 'GetAsyncKeyState', 4)
def _gaks(box, a):
    vk = a[0] & 0xFF
    return 0x8000 if vk in box.hs.keys else 0


@std(U, 'MessageBoxA', 16)
def _msgbox(box, a):
    box.log('MessageBoxA: %r / %r' % (box.cstr(a[2]), box.cstr(a[1])))
    box.hs.messageboxes.append((box.cstr(a[2]), box.cstr(a[1])))
    return 1           # IDOK


@std(U, 'LoadStringA', 16)
def _loadstring(box, a):
    s = box.strings_for(a[0]).get(a[1] & 0xFFFF)
    if s is None or a[3] == 0:
        if a[3]:
            box.wr8(a[2], 0)
        return 0
    b = s.encode('latin1')[:a[3] - 1]
    box.wr(a[2], b + b'\0')
    return len(b)


@model('USER32.dll!wsprintfA', pop=0, nargs=2)
def _wsprintf(box, a):
    text, _ = c_format(box, box.cstr(a[1], 1 << 16), a.esp + 12)
    text = text[:1023]
    box.wr(a[0], text.encode('latin1') + b'\0')
    return len(text)


@std(U, 'LoadImageA', 24)
def _loadimage(box, a):
    # LoadImageA(hinst, name, type, cx, cy, fuLoad).  The game loads its
    # menu art as DIB sections straight from files (LR_LOADFROMFILE 0x10,
    # LR_CREATEDIBSECTION 0x2000) and then reads the bits via GetObjectA.
    if a[2] != 0:
        raise GuestFault('LoadImageA type=%d not modelled' % a[2])
    if not (a[5] & 0x10):
        # a bitmap RESOURCE: BrBmpLoadSurface tries the launcher's resources
        # first; BRally.exe carries no RT_BITMAP, so this is always NULL
        if box.exe_has_bitmaps():
            raise GuestFault('LoadImageA from resources: BRally.exe has bitmaps?')
        return 0
    name = box.cstr(a[1])
    data = box.vfs.read(name)
    if data is None or data[:2] != b'BM':
        box.log('LoadImageA(%s) -> NULL' % name)
        return 0
    off = struct.unpack_from('<I', data, 10)[0]
    hdr = struct.unpack_from('<IiiHHIIiiII', data, 14)
    _sz, w, h, _pl, bpp, comp, _isz, _xr, _yr, clrused, _ci = hdr
    if comp != 0:
        raise GuestFault('LoadImageA(%s): compressed BMP' % name)
    stride = ((w * bpp + 31) // 32) * 4
    bits_len = stride * abs(h)
    ncol = clrused or ((1 << bpp) if bpp <= 8 else 0)
    bits = heap_alloc(box, bits_len)
    box.wr(bits, data[off:off + bits_len])
    hbm = new_handle(box) | 0x00050000
    box.hs.gdi[hbm] = {'w': w, 'h': h, 'bpp': bpp, 'stride': stride, 'bits': bits,
                    'pal': data[14 + hdr[0]:14 + hdr[0] + 4 * ncol], 'hdr': data[14:14 + 40]}
    return hbm


@std(U, 'UnregisterClassA', 8)
def _unregclass(box, a):
    return 1


# ============================================================= GDI32 =======
G = 'GDI32.dll'


@std(G, 'GetStockObject', 4)
def _stock(box, a):
    return 0x00060000 | a[0]


@std(G, 'GetObjectA', 12)
def _getobject(box, a):
    o = box.hs.gdi.get(a[0])
    if o is None:
        return 0
    # BITMAP: bmType, bmWidth, bmHeight (always positive), bmWidthBytes,
    # bmPlanes, bmBitsPixel, bmBits
    bm = struct.pack('<iiiiHHI', 0, o['w'], abs(o['h']), o['stride'], 1, o['bpp'], o['bits'])
    # BITMAP is 24 bytes; DIBSECTION (84) appends the BITMAPINFOHEADER etc.
    if a[1] >= 84:
        ds = bm + o['hdr'] + b'\0' * 12 + struct.pack('<II', 0, 0)
        box.wr(a[2], ds[:84])
        return 84
    n = min(a[1], 24)
    if a[2]:
        box.wr(a[2], bm[:n])
    return n


@std(G, 'DeleteObject', 4)
def _delobj(box, a):
    o = box.hs.gdi.pop(a[0], None)
    if o is not None:
        heap_free(box, o['bits'])
    return 1


# ============================================================= ole32 =======
O = 'ole32.dll'


@std(O, 'CoInitialize', 4)
def _coinit(box, a):
    return 0


@std(O, 'CoUninitialize', 0)
def _couninit(box, a):
    return None


@std(O, 'CoCreateInstance', 20)
def _cocreate(box, a):
    import brbox_com
    r = brbox_com.cocreate(box, a[0], a[3], a[4])
    if r is not None:
        return r
    box.log('CoCreateInstance(clsid@%08X) -> REGDB_E_CLASSNOTREG' % a[0])
    if a[4]:
        box.wr32(a[4], 0)
    return 0x80040154


# =========================================================== MSACM32 =======
@std('MSACM32.dll', 'acmMetrics', 12)
def _acmmetrics(box, a):
    # ACM_METRIC_MAX_SIZE_FORMAT (50): the largest WAVEFORMATEX any codec uses
    if a[1] == 50:
        box.wr32(a[2], 50)
        return 0
    raise GuestFault('acmMetrics(%d) unmodelled' % a[1])


# ============================================================ DPLAYX =======
@std('DPLAYX.dll', '#4', 20)
def _dplobbycreate(box, a):
    # DirectPlayLobbyCreateA: report DirectPlay absent (DPERR_UNAVAILABLE).
    box.log('DirectPlayLobbyCreateA -> DPERR_UNAVAILABLE')
    if a[1]:
        box.wr32(a[1], 0)
    return 0x88770154


# ============================================================= WINMM =======
W = 'WINMM.dll'


@std(W, 'timeGetTime', 0)
def _tgt(box, a):
    box.tick()
    return int(box.hs.ms) & 0xFFFFFFFF


@std(W, 'timeBeginPeriod', 4)
def _tbp(box, a):
    return 0


@std(W, 'timeEndPeriod', 4)
def _tep(box, a):
    return 0


MCI_OPEN, MCI_CLOSE, MCI_PLAY, MCI_STOP, MCI_PAUSE, MCI_SET, MCI_STATUS = \
    0x803, 0x804, 0x806, 0x808, 0x809, 0x80D, 0x814
MCI_MODE_STOP, MCI_MODE_PLAY = 525, 526
CD_TRACKS = 9                      # a data track and eight audio tracks


@std(W, 'mciSendCommandA', 16)
def _mci(box, a):
    # (IDDevice, uMsg, fdwCommand, dwParam).  A CD-ROM drive with the game
    # disc: track 1 data, the rest Redbook audio.  Play/stop change the mode
    # the game polls; the music itself is output-only.
    dev, msg, flags, parms = a[0], a[1], a[2], a[3]
    st = box.hs.__dict__.setdefault('mci', {'open': False, 'mode': MCI_MODE_STOP, 'track': 0})
    if msg == MCI_OPEN:
        st['open'] = True
        if parms:
            box.wr32(parms + 4, 1)                  # wDeviceID
        return 0
    if not st['open']:
        return 0x101                                # MCIERR_INVALID_DEVICE_ID
    if msg == MCI_CLOSE:
        st.update(open=False, mode=MCI_MODE_STOP)
        return 0
    if msg == MCI_PLAY:
        st['mode'] = MCI_MODE_PLAY
        if parms and flags & 0x4:                   # MCI_FROM
            st['track'] = box.rd32(parms + 4) & 0xFF
        return 0
    if msg in (MCI_STOP, MCI_PAUSE):
        st['mode'] = MCI_MODE_STOP
        return 0
    if msg == MCI_SET:
        return 0
    if msg == MCI_STATUS and parms:
        # MCI_STATUS_PARMS {dwCallback, dwReturn, dwItem, dwTrack}
        item, track = box.rd32(parms + 8), box.rd32(parms + 12)
        val = {1: 3 * 60 * 1000,                   # MCI_STATUS_LENGTH
               2: 0,                               # MCI_STATUS_POSITION
               3: CD_TRACKS,                       # MCI_STATUS_NUMBER_OF_TRACKS
               4: st['mode'],                      # MCI_STATUS_MODE
               5: 1,                               # MCI_STATUS_MEDIA_PRESENT
               6: 10,                              # MCI_STATUS_TIME_FORMAT (TMSF)
               7: 1,                               # MCI_STATUS_READY
               8: st['track'] or 1,                # MCI_STATUS_CURRENT_TRACK
               0x4001: 1089 if track == 1 else 1088}.get(item)   # MCI_CDA_STATUS_TYPE_TRACK
        if val is None:
            return 0x112                            # MCIERR_UNSUPPORTED_FUNCTION
        box.wr32(parms + 4, val)
        return 0
    box.log('mciSendCommandA(dev=%d msg=%X flags=%X) unmodelled -> MCIERR_UNSUPPORTED_FUNCTION'
            % (dev, msg, flags))
    return 0x112


# mmio: WAV files are parsed for real (sounds are loaded and measured).
def _mmio(box, h):
    m = box.hs.mmio.get(h)
    if m is None:
        raise GuestFault('bad HMMIO %08X' % h)
    return m


def _fourcc(v):
    return struct.pack('<I', v)


@std(W, 'mmioOpenA', 12)
def _mmioopen(box, a):
    name = box.cstr(a[0])
    if a[2] & 0xFFFF0000 & ~0x10000:
        pass
    data = box.vfs.read(name)
    if data is None:
        if a[1]:
            box.wr32(a[1] + 12, 257)       # MMIOERR_FILENOTFOUND in wErrorRet
        return 0
    h = new_handle(box)
    box.hs.mmio[h] = {'data': data, 'pos': 0, 'buf': 0}
    return h


@std(W, 'mmioClose', 8)
def _mmioclose(box, a):
    m = box.hs.mmio.pop(a[0], None)
    if m and m['buf']:
        heap_free(box, m['buf'])
    return 0


@std(W, 'mmioSeek', 12)
def _mmioseek(box, a):
    m = _mmio(box, a[0])
    off = struct.unpack('<i', struct.pack('<I', a[1]))[0]
    base = {0: 0, 1: m['pos'], 2: len(m['data'])}[a[2]]
    m['pos'] = max(0, base + off)
    return m['pos']


@std(W, 'mmioRead', 12)
def _mmioread(box, a):
    m = _mmio(box, a[0])
    n = struct.unpack('<i', struct.pack('<I', a[2]))[0]
    chunk = m['data'][m['pos']:m['pos'] + max(n, 0)]
    box.wr(a[1], chunk)
    m['pos'] += len(chunk)
    return len(chunk)


@std(W, 'mmioDescend', 16)
def _mmiodescend(box, a):
    m = _mmio(box, a[0])
    d = m['data']
    flags = a[3]
    want = box.rd32(a[1] + 8) if flags & 0x60 else None      # FINDRIFF/FINDLIST: fccType
    wantid = box.rd32(a[1]) if flags & 0x10 else None         # FINDCHUNK: ckid
    end = len(d)
    if a[2]:
        pstart = box.rd32(a[2] + 12)
        end = pstart + box.rd32(a[2] + 4)
    pos = m['pos']
    while pos + 8 <= end:
        ckid, cksize = struct.unpack_from('<II', d, pos)
        fcc = struct.unpack_from('<I', d, pos + 8)[0] if ckid in (0x46464952, 0x5453494C) else 0
        ok = True
        if flags & 0x20 and not (ckid == 0x46464952 and fcc == want):
            ok = False
        if flags & 0x40 and not (ckid == 0x5453494C and fcc == want):
            ok = False
        if flags & 0x10 and ckid != wantid:
            ok = False
        if ok:
            dstart = pos + 8
            box.wr(a[1], struct.pack('<IIIII', ckid, cksize, fcc, dstart, 0))
            m['pos'] = dstart + (4 if ckid in (0x46464952, 0x5453494C) else 0)
            return 0
        if not (flags & 0x70):
            break
        pos += 8 + cksize + (cksize & 1)
    return 0x0104        # MMIOERR_CHUNKNOTFOUND


@std(W, 'mmioAscend', 12)
def _mmioascend(box, a):
    m = _mmio(box, a[0])
    dstart = box.rd32(a[1] + 12)
    size = box.rd32(a[1] + 4)
    m['pos'] = dstart + size + (size & 1)
    return 0


@std(W, 'mmioGetInfo', 12)
def _mmiogetinfo(box, a):
    # Direct buffer I/O: expose a window of the file as the MMIO buffer.
    m = _mmio(box, a[0])
    if not m['buf']:
        m['buf'] = heap_alloc(box, 0x10000)
    _mmio_fill(box, m, a[1])
    return 0


def _mmio_fill(box, m, info):
    chunk = m['data'][m['pos']:m['pos'] + 0x10000]
    box.wr(m['buf'], chunk)
    # MMIOINFO: dwFlags, fccIOProc, pIOProc, wErrorRet, htask, cchBuffer,
    # pchBuffer(24), pchNext(28), pchEndRead(32), pchEndWrite(36), lBufOffset(40), lDiskOffset(44)
    box.wr(info + 20, struct.pack('<IIIII', 0x10000, m['buf'], m['buf'], m['buf'] + len(chunk),
                                  m['buf'] + 0x10000))
    box.wr(info + 40, struct.pack('<II', m['pos'], m['pos'] + len(chunk)))
    m['bufpos'] = m['pos']


@std(W, 'mmioSetInfo', 12)
def _mmiosetinfo(box, a):
    m = _mmio(box, a[0])
    nxt = box.rd32(a[1] + 28)
    m['pos'] = m.get('bufpos', m['pos']) + (nxt - m['buf'])
    return 0


@std(W, 'mmioAdvance', 12)
def _mmioadvance(box, a):
    m = _mmio(box, a[0])
    nxt = box.rd32(a[1] + 28)
    m['pos'] = m.get('bufpos', m['pos']) + (nxt - m['buf'])
    _mmio_fill(box, m, a[1])
    return 0


# ============================================================ glide2x ======
GL = 'glide2x.dll'
GLIDE_POP = {}
GLIDE_RET = {}


def _glide(name, pop, ret=None):
    GLIDE_POP[name] = pop

    def fn(box, a, _name=name, _ret=ret):
        box.hs.glide[_name] += 1
        if box.on_glide is not None:
            box.on_glide(box, _name, a)
        return _ret(box, a) if callable(_ret) else _ret
    MODELS['%s!%s' % (GL, name)] = (pop, fn, pop // 4)


def _gr_query(box, a):
    # one Voodoo Graphics board: 4 MB frame buffer, one TMU with 4 MB
    hw = struct.pack('<i', 1) + struct.pack('<i', 0) + \
        struct.pack('<iiii', 4, 2, 1, 0) + struct.pack('<ii', 1, 4) * 3
    box.wr(a[0], hw)
    return 1


_LOD_W = [256, 128, 64, 32, 16, 8, 4, 2, 1]
_ASPECT = [(8, 1), (4, 1), (2, 1), (1, 1), (1, 2), (1, 4), (1, 8)]


def _tex_bytes(small, large, aspect, fmt):
    tot = 0
    ax, ay = _ASPECT[aspect] if 0 <= aspect < 7 else (1, 1)
    bpp = 2 if fmt >= 8 else 1
    for lod in range(large, small + 1):
        s = _LOD_W[lod] if 0 <= lod < 9 else 1
        w = s if ax >= ay else max(s // ay, 1)
        h = s if ay >= ax else max(s // ax, 1)
        tot += w * h * bpp
    return (tot + 7) & ~7


def _gr_calcmem(box, a):
    return _tex_bytes(a[0], a[1], a[2], a[3])


def _gr_texmemreq(box, a):
    info = a[1]
    small, large, aspect, fmt = (box.rd32(info + 4 * k) for k in range(4))
    return _tex_bytes(small, large, aspect, fmt)


def _gu_fog(box, a):
    near = struct.unpack('<f', struct.pack('<I', a[1]))[0]
    far = struct.unpack('<f', struct.pack('<I', a[2]))[0]
    out = bytearray(64)
    for i in range(64):
        w = (2.0 ** (3.0 + (i >> 2))) / (8 - (i & 3))
        f = 0.0 if far == near else (w - near) / (far - near)
        f = min(max(f, 0.0), 1.0)
        out[i] = int(f * 255.0)
    box.wr(a[0], out)
    return None


def _gr_lfb(box, a):
    if box.on_lfb is not None:
        box.on_lfb(box, a)
    return 1


for _n, _p, _r in [
        ('_grGlideInit@0', 0, None), ('_grGlideShutdown@0', 0, None),
        ('_grSstQueryHardware@4', 4, _gr_query), ('_grSstSelect@4', 4, None),
        ('_grSstWinOpen@28', 28, 1), ('_grSstWinClose@0', 0, None),
        ('_grTexMinAddress@4', 4, 0), ('_grTexMaxAddress@4', 4, 0x400000 - 0x20000),
        ('_grTexCalcMemRequired@16', 16, _gr_calcmem),
        ('_grTexTextureMemRequired@8', 8, _gr_texmemreq),
        ('_grTexSource@16', 16, None), ('_grTexClampMode@12', 12, None),
        ('_grTexLodBiasValue@8', 8, None), ('_grTexMipMapMode@12', 12, None),
        ('_grTexDownloadMipMap@16', 16, None), ('_grTexFilterMode@12', 12, None),
        ('_grTexCombine@28', 28, None), ('_grDepthMask@4', 4, None),
        ('_guFogGenerateLinear@12', 12, _gu_fog), ('_grFogTable@4', 4, None),
        ('_grFogMode@4', 4, None), ('_grFogColorValue@4', 4, None),
        ('_grDrawPolygonVertexList@8', 8, None), ('_grDrawTriangle@12', 12, None),
        ('_grAlphaBlendFunction@16', 16, None), ('_grAlphaTestFunction@4', 4, None),
        ('_grAlphaTestReferenceValue@4', 4, None), ('_grAlphaCombine@20', 20, None),
        ('_grColorCombine@20', 20, None), ('_grClipWindow@16', 16, None),
        ('_grDepthBufferMode@4', 4, None), ('_grDepthBufferFunction@4', 4, None),
        ('_grConstantColorValue@4', 4, None), ('_grBufferClear@12', 12, None),
        ('_grCullMode@4', 4, None), ('_grBufferNumPending@0', 0, 0),
        ('_grBufferSwap@4', 4, None), ('_grLfbWriteRegion@32', 32, _gr_lfb)]:
    _glide(_n, _p, _r)


# ============================================================= MSVCRT ======

def _fp_result(box, v, pops):
    """Complete an x87-returning CRT call: `pops` operands come off the FPU
    stack and the double `v` is pushed, by jumping to a tiny guest routine
    (the FPU register file is the guest's; the host never pokes it)."""
    box.wr(box.fpres, struct.pack('<d', v))
    return ('jump', box.fp_snippet[pops])


@crt('asin', 2)
def _asin(box, a):
    v = a.f64(0)
    try:
        r = math.asin(v)
    except ValueError:
        r = float('nan')
    yield _fp_result(box, r, 0)


@crt('_CIasin', 0)
def _ciasin(box, a):
    v = box.st(0)
    try:
        r = math.asin(v)
    except ValueError:
        r = float('nan')
    yield _fp_result(box, r, 1)


@crt('_CIpow', 0)
def _cipow(box, a):
    y = box.st(0)
    x = box.st(1)
    try:
        r = math.pow(x, y)
    except ValueError:
        r = float('nan')
    except OverflowError:
        r = float('inf')
    yield _fp_result(box, r, 2)


@crt('_finite', 2)
def _finite(box, a):
    v = a.f64(0)
    return 0 if (v != v or v in (float('inf'), float('-inf'))) else 1


@crt('malloc', 1)
def _malloc(box, a):
    return heap_alloc(box, a[0])


@crt('??2@YAPAXI@Z', 1)
def _new(box, a):
    return heap_alloc(box, a[0])


@crt('free', 1)
def _free(box, a):
    if a[0]:
        heap_free(box, a[0])
    return None


@crt('??3@YAXPAX@Z', 1)
def _delete(box, a):
    if a[0]:
        heap_free(box, a[0])
    return None


@crt('realloc', 2)
def _realloc(box, a):
    old, n = a[0], a[1]
    if old == 0:
        return heap_alloc(box, n)
    if n == 0:
        heap_free(box, old)
        return 0
    osz = box.hs.heap_blocks.get(old)
    if osz is None:
        raise GuestFault('realloc of unknown block %08X' % old)
    if n <= osz:
        return old
    new = heap_alloc(box, n)
    box.wr(new, box.rd(old, osz))
    heap_free(box, old)
    return new


@crt('memcpy', 3)
def _memcpy(box, a):
    if a[2]:
        box.wr(a[0], box.rd(a[1], a[2]))
    return a[0]


@crt('memmove', 3)
def _memmove(box, a):
    if a[2]:
        box.wr(a[0], box.rd(a[1], a[2]))
    return a[0]


@crt('strncpy', 3)
def _strncpy(box, a):
    s = (box.cstr(a[1], a[2] + 1) or '').encode('latin1')[:a[2]]
    box.wr(a[0], s + b'\0' * (a[2] - len(s)))
    return a[0]


@crt('strchr', 2)
def _strchr(box, a):
    s = box.cstr(a[0], 1 << 16).encode('latin1') + b'\0'
    i = s.find(bytes([a[1] & 0xFF]))
    return 0 if i < 0 else a[0] + i


@crt('strrchr', 2)
def _strrchr(box, a):
    s = box.cstr(a[0], 1 << 16).encode('latin1') + b'\0'
    i = s.rfind(bytes([a[1] & 0xFF]))
    return 0 if i < 0 else a[0] + i


@crt('strstr', 2)
def _strstr(box, a):
    s = box.cstr(a[0], 1 << 16)
    t = box.cstr(a[1], 1 << 16)
    i = s.find(t)
    return 0 if i < 0 else a[0] + i


@crt('strncmp', 3)
def _strncmp(box, a):
    s = box.cstr(a[0], a[2] + 1).encode('latin1')[:a[2]]
    t = box.cstr(a[1], a[2] + 1).encode('latin1')[:a[2]]
    return 0 if s == t else (-1 if s < t else 1)


@crt('_stricmp', 2)
def _stricmp(box, a):
    s = box.cstr(a[0], 1 << 16).lower().encode('latin1')
    t = box.cstr(a[1], 1 << 16).lower().encode('latin1')
    return 0 if s == t else (-1 if s < t else 1)


@crt('_strupr', 1)
def _strupr(box, a):
    s = box.cstr(a[0], 1 << 16)
    box.wr(a[0], s.upper().encode('latin1') + b'\0')
    return a[0]


@crt('toupper', 1)
def _toupper(box, a):
    c = a[0] & 0xFFFFFFFF
    return c - 32 if 0x61 <= c <= 0x7A else c


@crt('atoi', 1)
def _atoi(box, a):
    import re
    m = re.match(r'\s*([+-]?\d+)', box.cstr(a[0], 256) or '')
    return int(m.group(1)) & 0xFFFFFFFF if m else 0


@crt('_itoa', 3)
def _itoa(box, a):
    v, radix = a.s(0), a[2]
    digits = '0123456789abcdefghijklmnopqrstuvwxyz'
    neg = radix == 10 and v < 0
    u = -v if neg else (a[0] if radix != 10 else v)
    s = ''
    while True:
        s = digits[u % radix] + s
        u //= radix
        if u == 0:
            break
    box.wr(a[1], (('-' if neg else '') + s).encode('latin1') + b'\0')
    return a[1]


@crt('sprintf', 2)
def _sprintf(box, a):
    text, _ = c_format(box, box.cstr(a[1], 1 << 16), a.esp + 12)
    box.wr(a[0], text.encode('latin1') + b'\0')
    return len(text)


@crt('vsprintf', 3)
def _vsprintf(box, a):
    text, _ = c_format(box, box.cstr(a[1], 1 << 16), a[2])
    box.wr(a[0], text.encode('latin1') + b'\0')
    return len(text)


@crt('printf', 1)
def _printf(box, a):
    text, _ = c_format(box, box.cstr(a[0], 1 << 16), a.esp + 8)
    box.log('printf %s' % text.rstrip())
    return len(text)


@crt('sscanf', 2)
def _sscanf(box, a):
    return c_scan(box, box.cstr(a[0], 1 << 16), box.cstr(a[1], 1 << 16), a.esp + 12) & 0xFFFFFFFF


@crt('qsort', 4)
def _qsort(box, a):
    """MSVCRT's own qsort (VC5 qsort.c): median-less partition around the
    middle element with an explicit stack and an 8-element selection-sort
    cutoff.  The ALGORITHM is modelled, not just "a sort": with equal keys
    the order it leaves is part of the game's behaviour."""
    base, num, width, comp = a[0], a[1], a[2], a[3]
    if num < 2 or width == 0:
        return None

    def cmp(p, q):
        r = yield ('call', comp, [p, q])
        return r - (1 << 32) if r & 0x80000000 else r

    def swap(p, q):
        if p != q:
            x = box.rd(p, width)
            box.wr(p, box.rd(q, width))
            box.wr(q, x)

    lostk, histk = [], []
    lo, hi = base, base + width * (num - 1)
    while True:
        size = (hi - lo) // width + 1
        if size <= 8:
            h = hi
            while h > lo:
                mx = lo
                p = lo + width
                while p <= h:
                    c = yield from cmp(p, mx)
                    if c > 0:
                        mx = p
                    p += width
                swap(mx, h)
                h -= width
        else:
            mid = lo + (size // 2) * width
            swap(mid, lo)
            loguy, higuy = lo, hi + width
            while True:
                while True:
                    loguy += width
                    if loguy > hi:
                        break
                    c = yield from cmp(loguy, lo)
                    if c > 0:
                        break
                while True:
                    higuy -= width
                    if higuy <= lo:
                        break
                    c = yield from cmp(higuy, lo)
                    if c < 0:
                        break
                if higuy < loguy:
                    break
                swap(loguy, higuy)
            swap(lo, higuy)
            if higuy - 1 - lo >= hi - loguy:
                if lo + width < higuy:
                    lostk.append(lo)
                    histk.append(higuy - width)
                if loguy < hi:
                    lo = loguy
                    continue
            else:
                if loguy < hi:
                    lostk.append(loguy)
                    histk.append(hi)
                if lo + width < higuy:
                    hi = higuy - width
                    continue
        if not lostk:
            return None
        lo, hi = lostk.pop(), histk.pop()


@crt('srand', 1)
def _srand(box, a):
    box.hs.rand = a[0]
    return None


@crt('rand', 0)
def _rand(box, a):
    box.hs.rand = (box.hs.rand * 214013 + 2531011) & 0xFFFFFFFF
    return (box.hs.rand >> 16) & 0x7FFF


@crt('_errno', 0)
def _errno(box, a):
    return box.errno_va


@crt('strerror', 1)
def _strerror(box, a):
    return box.host_str('error %d' % a[0])


@crt('exit', 1)
def _exit(box, a):
    raise Stop('exit(%d)' % a.s(0))


@crt('?terminate@@YAXXZ', 0)
def _terminate(box, a):
    raise GuestFault('terminate()')


@crt('_except_handler3', 0)
def _eh3(box, a):
    raise GuestFault('_except_handler3 reached: a hardware exception was raised')


@crt('__CxxFrameHandler', 0)
def _cxxfh(box, a):
    raise GuestFault('__CxxFrameHandler reached: a C++ exception was thrown')


@crt('_initterm', 2)
def _initterm(box, a):
    p = a[0]
    while p < a[1]:
        fn = box.rd32(p)
        if fn:
            yield ('call', fn, [])
        p += 4
    return None


@crt('__dllonexit', 3)
def _dllonexit(box, a):
    box.hs.atexit.append(a[0])
    return a[0]


@crt('_onexit', 1)
def _onexit(box, a):
    box.hs.atexit.append(a[0])
    return a[0]


@crt('__p__iob', 0)
def _piob(box, a):
    return box.iob_va


# ---------------------------------------------------------------- stdio ----

def _file(box, fp):
    f = box.hs.files.get(fp)
    if f is None:
        raise GuestFault('FILE* %08X not open' % fp)
    return f


def _fdata(box, f):
    if f['wr'] is not None:
        return f['wr']
    return box.vfs.read(f['path']) or b''


@crt('fopen', 2)
def _fopen(box, a):
    path = box.cstr(a[0])
    mode = box.cstr(a[1])
    if 'r' in mode and '+' not in mode:
        data = box.vfs.read(path)
        if data is None:
            box.hs.errno = 2
            box.wr32(box.errno_va, 2)
            box.log('fopen(%s, %s) -> NULL' % (path, mode))
            return 0
        wr = None
    elif 'w' in mode:
        if not box.vfs.write(path, b''):
            return 0
        wr = bytearray()
    elif 'a' in mode or '+' in mode:
        old = box.vfs.read(path)
        if old is None and 'r' in mode:
            return 0
        wr = bytearray(old or b'')
    else:
        raise GuestFault('fopen mode %r' % mode)
    fp = box.host_alloc(32, 4)
    box.wr(fp, b'\0' * 32)
    box.hs.files[fp] = {'path': path, 'pos': len(wr) if 'a' in mode and wr is not None else 0,
                        'mode': mode, 'wr': wr, 'unget': [], 'eof': False}
    box.log('fopen(%s, %s) -> %08X' % (path, mode, fp))
    return fp


@crt('fclose', 1)
def _fclose(box, a):
    f = box.hs.files.pop(a[0], None)
    if f is None:
        return 0xFFFFFFFF
    if f['wr'] is not None:
        box.vfs.write(f['path'], bytes(f['wr']))
    return 0


@crt('fread', 4)
def _fread(box, a):
    buf, size, count, fp = a[0], a[1], a[2], a[3]
    if fp in box.std_files:
        return 0
    f = _file(box, fp)
    data = _fdata(box, f)
    want = size * count
    if want == 0:
        return 0
    got = bytearray()
    while f['unget'] and len(got) < want:
        got.append(f['unget'].pop())
    chunk = data[f['pos']:f['pos'] + (want - len(got))]
    f['pos'] += len(chunk)
    got += chunk
    if len(got) < want:
        f['eof'] = True
    box.wr(buf, bytes(got))
    return len(got) // size


@crt('fwrite', 4)
def _fwrite(box, a):
    buf, size, count, fp = a[0], a[1], a[2], a[3]
    data = box.rd(buf, size * count) if size * count else b''
    if fp in box.std_files:
        box.log('stdio %s' % data.decode('latin1').rstrip())
        return count
    f = _file(box, fp)
    if f['wr'] is None:
        return 0
    w = f['wr']
    p = f['pos']
    if p > len(w):
        w.extend(b'\0' * (p - len(w)))
    w[p:p + len(data)] = data
    f['pos'] = p + len(data)
    return count


@crt('fprintf', 2)
def _fprintf(box, a):
    text, _ = c_format(box, box.cstr(a[1], 1 << 16), a.esp + 12)
    fp = a[0]
    if fp in box.std_files:
        box.log('fprintf %s' % text.rstrip())
        return len(text)
    f = _file(box, fp)
    if f['wr'] is None:
        return 0xFFFFFFFF
    b = text.encode('latin1')
    w = f['wr']
    w[f['pos']:f['pos'] + len(b)] = b
    f['pos'] += len(b)
    return len(b)


@crt('fseek', 3)
def _fseek(box, a):
    f = _file(box, a[0])
    off = a.s(1)
    base = {0: 0, 1: f['pos'], 2: len(_fdata(box, f))}.get(a[2])
    if base is None or base + off < 0:
        return 0xFFFFFFFF
    f['pos'] = base + off
    f['unget'] = []
    f['eof'] = False
    return 0


@crt('ftell', 1)
def _ftell(box, a):
    f = _file(box, a[0])
    return (f['pos'] - len(f['unget'])) & 0xFFFFFFFF


@crt('getc', 1)
def _getc(box, a):
    f = _file(box, a[0])
    if f['unget']:
        return f['unget'].pop()
    data = _fdata(box, f)
    if f['pos'] >= len(data):
        f['eof'] = True
        return 0xFFFFFFFF
    c = data[f['pos']]
    f['pos'] += 1
    return c


@crt('ungetc', 2)
def _ungetc(box, a):
    if a[0] == 0xFFFFFFFF:
        return 0xFFFFFFFF
    f = _file(box, a[1])
    f['unget'].append(a[0] & 0xFF)
    f['eof'] = False
    return a[0] & 0xFF


# ---------------------------------------------------- directories / drives -
@crt('_getdrive', 0)
def _getdrive(box, a):
    return ord(box.hs.cwd[0].upper()) - 0x40


@crt('_chdrive', 1)
def _chdrive(box, a):
    d = chr(0x40 + a[0]) if 1 <= a[0] <= 26 else '?'
    if d not in brbox_fs.DRIVES:
        return 0xFFFFFFFF
    if box.hs.cwd[0].upper() != d:
        box.hs.cwd = d + ':\\'
    return 0


@crt('_chdir', 1)
def _chdir(box, a):
    p = box.cstr(a[0])
    c = box.vfs.canon(p)
    if c is None or not box.vfs.exists_dir(p):
        return 0xFFFFFFFF
    box.hs.cwd = c[2].rstrip('\\') + '\\'
    return 0


@crt('_getcwd', 2)
def _getcwd(box, a):
    cwd = box.hs.cwd if len(box.hs.cwd) <= 3 else box.hs.cwd.rstrip('\\')
    b = cwd.encode('latin1') + b'\0'
    if a[0] == 0:
        p = heap_alloc(box, max(len(b), a[1]))
        box.wr(p, b)
        return p
    if len(b) > a[1]:
        return 0
    box.wr(a[0], b)
    return a[0]


def _finddata(box, p, name, real):
    import os
    isdir = os.path.isdir(real)
    size = 0 if isdir else os.path.getsize(real)
    attr = 0x10 if isdir else 0x01          # _A_SUBDIR / _A_RDONLY (a CD)
    box.wr(p, struct.pack('<IiiiI', attr, 0x36000000, 0x36000000, 0x36000000, size) +
           name.encode('latin1')[:259] + b'\0')


@crt('_findfirst', 2)
def _findfirst(box, a):
    spec = box.cstr(a[0])
    names = box.vfs.listdir(spec)
    if not names:
        box.wr32(box.errno_va, 2)
        return 0xFFFFFFFF
    h = new_handle(box)
    _finddata(box, a[1], *names[0])
    box.hs.findh[h] = names[1:]
    return h


@crt('_findnext', 2)
def _findnext(box, a):
    rest = box.hs.findh.get(a[0])
    if not rest:
        return 0xFFFFFFFF
    _finddata(box, a[1], *rest.pop(0))
    return 0


@crt('_findclose', 1)
def _findclose(box, a):
    return 0 if box.hs.findh.pop(a[0], None) is not None else 0xFFFFFFFF


# ====================================================== dynamic modules ====
# DLLs the game LoadLibrary's by name.  Each maps export name -> (pop, fn);
# '*' builds a model for any decorated (_Name@N) export not listed.

def _ear_generic(name):
    def fn(box, a, _n=name):
        box.hs.ear_calls[_n] += 1
        return EAR_RET.get(_n, 0)
    return fn


# The EAR "Interactive Around-Sound" engine the game loads by name
# (earpds.dll / earias.dll, installed from data1.cab).  It is sound output
# only; each entry answers as a working engine does.  Register*/Start* hand
# back a non-zero id (br_cd.c tests RegisterChannel != 0 for success),
# InitializeEar non-zero is success (BrWindowEarStartup exits on 0), and the
# error/status queries report "no error" / "idle".
EAR_RET = {
    '_EAR_DLL_AAA_Validate@4': 1, '_EAR_DLL_AssignHwnd@4': 1,
    '_EAR_DLL_InitializeEar@4': 1, '_EAR_DLL_GetLastError@0': 0,
    '_EAR_DLL_GetVersion@0': 0x100, '_EAR_DLL_EarInactive@0': 0,
    '_EAR_DLL_GetEventStatus@8': 0,
    '_EAR_DLL_RegisterBank@8': 1, '_EAR_DLL_RegisterChannel@16': 1,
    '_EAR_DLL_RegisterEnvironment@4': 1, '_EAR_DLL_RegisterMatrix@4': 1,
    '_EAR_DLL_RegisterPreset@8': 1, '_EAR_DLL_StartEvent@4': 1,
    '_EAR_DLL_MixEvent@4': 1, '_EAR_DLL_MoveEvent@4': 1, '_EAR_DLL_StartTimer@0': 1,
    '_EAR_DLL_UpdateEar@0': 1, '_EAR_DLL_ResetEar@0': 1,
}
DYNAMIC = {
    'brstring.dll': {},
    'earias.dll': {'*': _ear_generic},
    'earpds.dll': {'*': _ear_generic},
    'ddraw.dll': {},
    'dinput.dll': {},
}


# ========================================================= install =========

def _asm_fp_snippets(box):
    """Guest routines that finish an x87-returning CRT model (see _fp_result):
    pop N operands, push the double at box.fpres, return.  And _ftol, which
    is pure guest code: the MSVCRT routine itself (truncating fistp)."""
    base = box.host_alloc(0x100, 16)
    box.fpres = base + 0xF0
    snippets = {}
    off = 0
    for pops in (0, 1, 2):
        code = b'\xDD\xD8' * pops + b'\xDD\x05' + struct.pack('<I', box.fpres) + b'\xC3'
        box.wr(base + off, code)
        snippets[pops] = base + off
        off += 0x20
    box.fp_snippet = snippets
    # floor(double): pure guest code -- the game calls it ~2,500 times a race
    # frame, so a host round trip each time would dominate the run.  frndint
    # under round-down IS floor for every finite value, and passes NaN and
    # the infinities through, as MSVCRT's does.
    box.floor_va = base + 0xC0
    box.wr(box.floor_va, bytes.fromhex(
        '83EC04' 'D93C24' '668B0424' '80E4F3' '80CC04' '6689442402' 'D96C2402'
        'DD442408' 'D9FC' 'D92C24' '83C404' 'C3'))
    ftol = base + 0x80
    box.wr(ftol, bytes.fromhex(
        '55' '8BEC' '83C4F4' '9B' 'D97DFE' '9B' '668B45FE' '80CC0C' '668945FC'
        'D96DFC' 'DF7DF4' 'D96DFE' '8B45F4' '8B55F8' 'C9' 'C3'))
    return ftol


def install(box):
    import brbox_com
    box.on_glide = None
    box.on_lfb = None
    box.vfs = brbox_fs.VFS(CD_ROOT, box.hs)
    box.errno_va = box.host_alloc(4, 4)
    box.iob_va = box.host_alloc(3 * 32, 4)
    box.std_files = {box.iob_va, box.iob_va + 32, box.iob_va + 64}
    box.adjust_fdiv_va = box.host_alloc(4, 4)
    box.wr32(box.adjust_fdiv_va, 0)
    ftol = _asm_fp_snippets(box)
    brbox_com.install(box)
    missing = []
    for slot, full in sorted(box.pe.imports.items()):
        if full == 'MSVCRT.dll!_adjust_fdiv':          # a DATA import: the slot holds &var
            box.wr32(slot, box.adjust_fdiv_va)
            continue
        if full == 'MSVCRT.dll!_ftol':
            box.wr32(slot, ftol)
            continue
        if full == 'MSVCRT.dll!floor':
            box.wr32(slot, box.floor_va)
            continue
        ent = MODELS.get(full)
        if ent is None:
            missing.append(full)
            box.wr32(slot, box.trap(full, 0, _unmodelled(full)))
            continue
        pop, fn, nargs = ent
        box.wr32(slot, box.trap(full, pop or 0, fn, nargs))
    box.unmodelled = missing


def _unmodelled(name):
    def fn(box, a):
        raise GuestFault('unmodelled import %s' % name)
    return fn


VK_CHAR = {0x0D: 13, 0x20: 32, 0x08: 8, 0x1B: 27}
for _c in range(0x30, 0x3A):
    VK_CHAR[_c] = _c
for _c in range(0x41, 0x5B):
    VK_CHAR[_c] = _c + 32
