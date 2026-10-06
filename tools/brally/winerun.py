"""subprocess.run for Wine commands, with a timeout that actually stops them.

cl.exe under Wine is not one process.  The wine loader we launch starts cl.exe,
and cl.exe starts c1.exe and c2.exe as further Wine processes, which the host
reparents to init.  subprocess.run(timeout=...) kills only the loader, so a
wedged c2.exe survives the timeout and runs until someone notices: on
2026-10-03 eleven of them, from one statement-permutation sweep over
0x10001CF0, had each burned over two hours of CPU after the sweep had exited.

Every one of those children stays in the loader's process group.  run() starts
the command in a new session and, on timeout (or any interruption), kills the
whole group before re-raising, so callers keep their existing
`except subprocess.TimeoutExpired` handling.
"""
import os
import signal
import subprocess


def _killpg(p):
    try:
        os.killpg(p.pid, signal.SIGKILL)
    except (ProcessLookupError, PermissionError):
        pass


def run(cmd, timeout=None, capture_output=False, **kw):
    if capture_output:
        kw['stdout'] = kw['stderr'] = subprocess.PIPE
    kw['start_new_session'] = True
    with subprocess.Popen(cmd, **kw) as p:
        try:
            out, err = p.communicate(timeout=timeout)
        except BaseException:
            _killpg(p)
            try:
                p.communicate(timeout=10)
            except subprocess.TimeoutExpired:
                pass
            raise
    return subprocess.CompletedProcess(p.args, p.returncode, out, err)
