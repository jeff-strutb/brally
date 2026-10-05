#!/usr/bin/env python3
"""Read C2's own register-allocator and frame-layout data for one function.

    .venv/bin/python tools/c2read/c2read.py SOURCE.c FUNCNAME [--frame] [--trace] [--rotation]
        [--blocks NAMES] [--symbols NAMES] [--json OUT] [--opts '/O2 /I tools/glide2x-sdk']

Prints every register candidate C2 colours for FUNCNAME: priority, tie key,
allowed registers and the register it got (or split / memory), and with
--frame the frame-slot packing, with --symbols the symbol ids that decide
order-only ties.  About 40 s for a 9 KB function.  A run occasionally stops
before the sorted list; rerun it.

Compiles SOURCE.c with our CL (front end) and the VC++ 5.0 SP3 C2.EXE back end
(tools/msvc5/bin-sp3/C2.EXE) spinning at its entry, attaches winedbg's gdb
server, and runs Byte Tactics' c2prio tracer through gdbshim (GDB remote
protocol, no gdb needed).  Report formatting is c2prio's own.
"""
import argparse, json, os, re, shutil, socket, struct, subprocess, sys, time, secrets, types
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
WINEBIN = ROOT + '/tools/wine/Wine Stable.app/Contents/Resources/wine/bin'
ENV = dict(os.environ, WINEPREFIX=ROOT + '/build/wineprefix', WINEDEBUG='-all')
sys.path.insert(0, HERE)
import gdbshim
import c2prio_bt as bt   # Byte Tactics tools/c2prio.py, unchanged (imported without gdb: host side)
bt.gdb = gdbshim         # its tracer then runs against the shim


def winpath(p):
    return 'Z:' + os.path.abspath(p).replace('/', '\\')


def body_line(text, fname):
    """Line number of the '{' opening FNAME's definition (C1 numbers tuples from the line before it)."""
    m = None
    for mm in re.finditer(r'^[^\n;{}]*\b%s\s*\(' % re.escape(fname), text, re.M):
        k = text.find('{', mm.end())
        semi = text.find(';', mm.end())
        if k != -1 and (semi == -1 or k < semi):
            m = k
    return text.count('\n', 0, m) + 1 if m is not None else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('source')
    ap.add_argument('func')
    ap.add_argument('--opts', default='/O2')
    ap.add_argument('--trace', action='store_true')
    ap.add_argument('--frame', action='store_true')
    ap.add_argument('--rotation', action='store_true')
    ap.add_argument('--ids', action='store_true')
    ap.add_argument('--blocks', nargs='?', const='')
    ap.add_argument('--symbols')
    ap.add_argument('--json')
    ap.add_argument('--keep', action='store_true')
    a = ap.parse_args()

    c2 = ROOT + '/tools/msvc5/bin-sp3/C2.EXE'
    import hashlib
    assert hashlib.sha256(open(c2, 'rb').read()).hexdigest() == bt.C2_SHA256
    token = secrets.token_hex(4)
    run = f'{ROOT}/build/c2read/{token}'
    os.makedirs(run)
    exe = f'c2r{token}.exe'
    data = bytearray(open(c2, 'rb').read())
    off = bt.file_offset(data, bt.ENTRY)
    assert data[off:off + 2] == bt.ENTRY_BYTES
    data[off:off + 2] = b'\xeb\xfe'
    open(f'{run}/{exe}', 'wb').write(data)
    shutil.copy(ROOT + '/tools/msvc5/bin-sp3/MSPDB50.DLL', run)
    obj = f'{run}/out.obj'
    procs = []
    t0 = time.time()
    try:
        cl = subprocess.Popen([WINEBIN + '/wine', ROOT + '/tools/msvc5/bin/cl.exe', '/nologo', *a.opts.split(), '/W3',
                               '/I', 'include', '/I', 'tools/msvc5-compat', '/I', 'tools/msvc5/include',
                               '/DBR_MATCHING_BUILD', '/c', '/B2' + winpath(f'{run}/{exe}'), '/Fo' + winpath(obj),
                               os.path.relpath(a.source, ROOT)],
                              cwd=ROOT, env=ENV, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        procs.append(cl)
        pid = None
        while pid is None:
            try:
                info = subprocess.run([WINEBIN + '/wine', 'tasklist'], env=ENV, capture_output=True, text=True,
                                      timeout=8).stdout
            except subprocess.TimeoutExpired:
                continue
            for line in info.splitlines():
                f = line.split()
                if len(f) > 1 and f[0].lower() == exe:
                    pid = int(f[1])
            if pid is None:
                if cl.poll() is not None:
                    sys.exit('compile ended before C2 ran:\n' + cl.stdout.read())
                if time.time() - t0 > 300:
                    sys.exit('C2 did not start')
                time.sleep(0.1)
        with socket.socket() as sk:
            sk.bind(('', 0))
            port = sk.getsockname()[1]
        srv = subprocess.Popen([WINEBIN + '/winedbg', '--gdb', '--no-start', '--port', str(port), str(pid)], env=ENV,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        procs.append(srv)
        gdbshim.connect(port, exe)
        cfgp = f'{run}/config.json'
        out_json = f'{run}/trace.json'
        json.dump({'relay': '', 'exe': exe, 'match': ['substr', [a.func]], 'out': out_json,
                   'blocks': a.blocks is not None, 'inline': False, 'symbols': a.symbols is not None,
                   'frame': a.frame, 'rotation': a.rotation, 'ids': a.ids}, open(cfgp, 'w'))
        os.environ['C2PRIO_CONFIG'] = cfgp
        bt.tracer()
        trace = json.load(open(out_json))
        if trace.get('error'):
            sys.exit('trace failed:\n' + trace['error'])
        log = cl.communicate(timeout=600)[0]
        if not os.path.exists(obj):
            sys.exit('compile failed:\n' + log)
        fns = [f for f in trace['functions'] if a.func in (f['name'] or '')]
        if not fns:
            sys.exit('C2 never allocated ' + a.func + '; saw ' + ', '.join(f['name'] or '?' for f in trace['functions']))
        if a.json:
            json.dump(fns, open(a.json, 'w'), indent=1)
        text = open(a.source, errors='replace').read()
        brace = body_line(text, a.func)
        print(f'{a.func}  ({a.source}, {time.time() - t0:.1f} s, C2 {trace.get("seconds")} s traced)')
        if a.symbols is not None:
            bt.print_symbols(fns[0], [n.strip() for n in a.symbols.split(',') if n.strip()])
        for n, fn in enumerate(fns):
            if len(fns) > 1:
                print(f'\nC2 allocated this function {len(fns)} times; run {n + 1}:')
            bt.report(fn, brace, a.trace, a.blocks)
            if a.frame:
                bt.print_frame(fn, brace)
            if a.rotation:
                bt.print_rotation(fn, brace)
            if a.ids:
                bt.print_ids(fn)
        shutil.copy(obj, f'{HERE}/last.obj')
    finally:
        for p in procs:
            if p.poll() is None:
                p.kill()
        subprocess.run(['pkill', '-f', exe])
        if not a.keep:
            shutil.rmtree(run, ignore_errors=True)


main()
