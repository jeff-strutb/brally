#!/usr/bin/env python3
"""dsrun.py -- run a PS-EXE (or a disc's .cue) in DuckStation and collect
what it writes: the BIOS TTY and the files the program creates over PCDRV.

  dsrun.py [--ram8] [--timeout S] [--speed N] [--gdb-at S [--gdb CMD]...] RUNDIR IMAGE

RUNDIR becomes the PCDRV root (the program's host files land there).  The run
ends when the program creates RUNDIR/done, or at the timeout; DuckStation is
then stopped.  The TTY goes to RUNDIR/tty.txt.  DuckStation is the portable
copy ports/tgrally-ps1/tools/duckstation.sh sets up; its settings are written
here for each run (software renderer, PCDRV on, the auto-updater off).
--gdb-at S attaches the R3000 gdb to DuckStation's GDB server S seconds in,
runs the --gdb commands (default: the registers and a backtrace) against the
ELF beside IMAGE, and prints what it says."""
import argparse
import configparser
import os
import re
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
APP = os.path.join(ROOT, 'build/tools/ps1/DuckStation.app/Contents/MacOS')
EXE = os.path.join(APP, 'DuckStation')


GDB = os.path.join(ROOT, 'build/toolchains/ps1/bin/mipsel-none-elf-gdb')
GDB_PORT = 2345


def settings(rundir, ram8, speed, gdb=False, cpu='Recompiler'):
    path = os.path.join(APP, 'settings.ini')
    c = configparser.RawConfigParser(strict=False)
    c.optionxform = str
    if os.path.exists(path):
        c.read(path)
    want = {
        'Main': {'ConfirmPowerOff': 'false', 'SaveStateOnExit': 'false', 'StartPaused': 'false',
                 'PauseOnFocusLoss': 'false', 'EmulationSpeed': str(speed), 'SetupWizardIncomplete': 'false'},
        'AutoUpdater': {'CheckAtStartup': 'false'},
        'BIOS': {'TTYLogging': 'true', 'PatchFastBoot': 'true', 'SearchDirectory': 'bios'},
        'Console': {'Enable8MBRAM': 'true' if ram8 else 'false', 'Region': 'NTSC-U'},
        'GPU': {'Renderer': 'Software'},
        'CPU': {'ExecutionMode': cpu},
        'PCDrv': {'Enabled': 'true', 'EnableWrites': 'true', 'Root': rundir},
        'Logging': {'LogToFile': 'true', 'LogLevel': 'Info', 'LogToConsole': 'false'},
        'Debug': {'EnableGDBServer': 'true' if gdb else 'false', 'GDBServerPort': str(GDB_PORT)},
    }
    for sec, kv in want.items():
        if not c.has_section(sec):
            c.add_section(sec)
        for k, v in kv.items():
            c.set(sec, k, v)
    with open(path, 'w') as f:
        c.write(f, space_around_delimiters=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--ram8', action='store_true', help='the 8 MB dev-kit RAM')
    ap.add_argument('--timeout', type=float, default=60)
    ap.add_argument('--speed', type=float, default=0, help='emulation speed (0: unlimited)')
    ap.add_argument('--cpu', default='Recompiler', help='Recompiler | CachedInterpreter | Interpreter')
    ap.add_argument('--gdb-at', type=float)
    ap.add_argument('--gdb', action='append', default=[])
    ap.add_argument('rundir')
    ap.add_argument('image')
    a = ap.parse_args()
    if not os.path.exists(EXE):
        sys.exit('dsrun: run ports/tgrally-ps1/tools/duckstation.sh first')
    rundir = os.path.abspath(a.rundir)
    os.makedirs(rundir, exist_ok=True)
    done = os.path.join(rundir, 'done')
    if os.path.exists(done):
        os.remove(done)
    settings(rundir, a.ram8, a.speed, a.gdb_at is not None, a.cpu)
    log = os.path.join(APP, 'duckstation.log')
    if os.path.exists(log):
        os.remove(log)
    err = open(os.path.join(rundir, 'duckstation.err'), 'w')
    p = subprocess.Popen([EXE, '-batch', '-fastboot', '--', os.path.abspath(a.image)],
                         stdout=err, stderr=subprocess.STDOUT)
    t0 = time.time()
    gdb_done = a.gdb_at is None
    while time.time() - t0 < a.timeout and p.poll() is None and not os.path.exists(done):
        time.sleep(0.2)
        if not gdb_done and time.time() - t0 >= a.gdb_at:
            gdb_done = True
            elf = os.path.splitext(os.path.abspath(a.image))[0] + '.elf'
            cmds = a.gdb or ['info registers', 'bt 30']
            args = [GDB, '-q', '-batch', '-nx', '-ex', 'set architecture mips:3000', '-ex', 'set endian little',
                    '-ex', 'target remote :%d' % GDB_PORT]
            for c in cmds:
                args += ['-ex', c]
            args += ['-ex', 'detach', elf]
            r = subprocess.run(args, capture_output=True, text=True, timeout=60)
            print(r.stdout + r.stderr)
    finished = os.path.exists(done)
    if p.poll() is not None and not finished:
        print('dsrun: DuckStation exited by itself (status %d)' % p.returncode)
    if p.poll() is None:
        p.terminate()
        try:
            p.wait(5)
        except subprocess.TimeoutExpired:
            p.kill()
    tty = []
    if os.path.exists(log):
        for line in open(log, errors='replace'):
            m = re.search(r'TTY: (.*)$', line.rstrip('\n'))
            if m:
                tty.append(m.group(1))
    open(os.path.join(rundir, 'tty.txt'), 'w').write('\n'.join(tty) + ('\n' if tty else ''))
    print('dsrun: %s after %.1f s' % ('done' if finished else 'TIMEOUT', time.time() - t0))
    sys.exit(0 if finished else 1)


if __name__ == '__main__':
    main()
