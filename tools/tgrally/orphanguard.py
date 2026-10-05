"""ProcessPoolExecutor initializer: a worker exits when its parent does.

A pool worker waits for work on a queue whose write end every worker also
holds, so when the parent is killed (a timed-out shell, a ^C that reaches only
the parent) no worker ever sees end-of-file: each sits in sem_wait for good,
holding 100+ MB.  On 2026-10-04 two such sets from n64t3.py (9 and 15
processes) were found hours after their parent had gone.

Pass `initializer=orphanguard.watch_parent` to the executor; a daemon thread
in each worker polls its parent pid and exits the worker once it changes.
"""
import os
import threading
import time


def watch_parent():
    ppid = os.getppid()

    def loop():
        while os.getppid() == ppid:
            time.sleep(2)
        os._exit(1)

    threading.Thread(target=loop, daemon=True).start()
