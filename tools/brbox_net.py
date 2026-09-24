"""brbox_net.py -- two boxed games on one virtual network.

Multiplayer races only start with a second player in the session (the host's
START RACE refuses dwCurrentPlayers <= 1, as the original does), so the code
that encodes, ships and interpolates car state is reachable only with a real
peer.  The peer here is a second copy of the original DLL in its own Box,
driven by its own script, on its own thread.

Conservative virtual-time synchronisation.  Every message a box sends is
stamped with the sender's virtual time and arrives LATENCY_MS later.  A box
may run at most LATENCY_MS of virtual time ahead of the other box's published
time, so when it looks for messages due by its current time, every message
that could be due has already been sent: what it reads never depends on how
the two threads were scheduled.  (A game that polls the network in a loop
inside one frame -- the race loader does -- simply waits here for the peer's
clock to move.)

Each box's view of the network -- its session, players, inbound queue, and a
cursor into the log addressed to it -- lives in its own HostState, so a
live-oracle sub-run snapshots and rolls it back like any other state.  Sends
are held in the box's outbox and published only outside sub-runs; inside a
sub-run delivery is frozen at the bound in force when the sub-run began, so
both sides of a capture see exactly the same messages.
"""
from __future__ import print_function

import threading

LATENCY_MS = 20.0

DPSYS_CREATEPLAYERORGROUP = 0x0003
DPSYS_DESTROYPLAYERORGROUP = 0x0005


def dp_state(box):
    """The box's DirectPlay view (created on first use, lives in HostState)."""
    d = box.hs.__dict__.get('dp')
    if d is None:
        d = {'session': None,        # DPSESSIONDESC2 bytes (0x50) when open
             'name': b'',            # the session name string
             'host': False,          # this box created the session
             'local': {},            # pid -> {'short','long','flags','event'}
             'players': {},          # every announced remote pid -> info (+'guid')
             'sessions': {},         # guid -> (desc, name): sessions others host
             'inbox': [],            # [(from, to, bytes | ('sys', kind, pid, info))]
             'outbox': [],           # [(ts, kind, payload)] not yet published
             'cursor': 0,            # next entry of the log addressed to this box
             'next_pid': 0}
        box.hs.__dict__['dp'] = d
    return d


def session_guid(d):
    return d['session'][8:24] if d['session'] else None


def remote_players(d):
    g = session_guid(d)
    if g is None:
        return {}
    return {pid: p for pid, p in d['players'].items() if p.get('guid') == g}


def send(box, kind, payload):
    """Queue an announcement or message; published at the box's next poll
    outside a sub-run."""
    dp_state(box)['outbox'].append((box.hs.ms, kind, payload))


class Net(object):
    def __init__(self, boxes):
        self.boxes = boxes
        self.cv = threading.Condition()
        self.published = [0.0] * len(boxes)
        self.done = [False] * len(boxes)
        # per destination, kept SORTED by (arrival, source box, sequence): the
        # order two threads happen to publish in must not decide the order a
        # box hears them (a box's own echo and its peer's message could land
        # in either order, and the original ran differently against itself)
        self.log = [[] for _ in boxes]          # (arrival, src, seq, kind, payload)
        self.seq = 0
        for i, b in enumerate(boxes):
            b.net = self
            b.net_index = i
            b.net_freeze = None

    def stop(self, i=None):
        with self.cv:
            if i is None:
                self.done = [True] * len(self.done)
            else:
                self.done[i] = True
            self.cv.notify_all()

    def _bound(self, i):
        """How far box i may run: the others' published time + latency."""
        others = [self.published[j] for j in range(len(self.boxes))
                  if j != i and not self.done[j]]
        return min(others) + LATENCY_MS if others else float('inf')

    # ------------------------------------------------------------ polling --
    def poll(self, box):
        """Publish, wait until the peers have caught up, deliver what is due."""
        i = box.net_index
        d = dp_state(box)
        if box.subrun:
            if box.net_freeze is None:
                with self.cv:
                    box.net_freeze = self._bound(i)
            self._deliver(box, d, min(box.hs.ms, box.net_freeze))
            return
        box.net_freeze = None
        with self.cv:
            self._publish(box, d)
            self.published[i] = box.hs.ms
            self.cv.notify_all()
            while box.hs.ms > self._bound(i):
                self.cv.wait(5.0)
        self._deliver(box, d, box.hs.ms)

    def idle(self, box, deadline=None):
        """Every thread of `box` is blocked: move its clock to the earlier of
        its own next deadline and the next message due for it, waiting for
        the peers until that time is safe.  Returns False when nothing can
        ever happen."""
        i = box.net_index
        d = dp_state(box)
        with self.cv:
            self._publish(box, d)
            while True:
                log = self.log[i]
                cands = [] if deadline is None else [deadline]
                if d['cursor'] < len(log):
                    cands.append(log[d['cursor']][0])
                if cands:
                    t = max(box.hs.ms, min(cands))
                    if t <= self._bound(i):
                        box.hs.ms = t
                        break
                elif all(self.done[j] for j in range(len(self.boxes)) if j != i):
                    return False
                # nothing yet: run our clock up to the bound so a peer that is
                # waiting on us can proceed, then wait for it
                b = self._bound(i)
                if b != float('inf') and b > box.hs.ms:
                    box.hs.ms = b
                self.published[i] = box.hs.ms
                self.cv.notify_all()
                self.cv.wait(5.0)
            self.published[i] = box.hs.ms
            self.cv.notify_all()
        self._deliver(box, d, box.hs.ms)
        return True

    def _publish(self, box, d):
        """Move the box's outbox into the logs (caller holds cv)."""
        i = box.net_index
        import bisect
        out, d['outbox'] = d['outbox'], []
        for ts, kind, payload in out:
            self.seq += 1
            e = (ts + LATENCY_MS, i, self.seq, kind, payload)
            for j in range(len(self.boxes)):
                if j != i:
                    self._insert(j, e)
            if kind == 'msg':
                # this box's OTHER local players hear it too (DirectPlay does)
                self._insert(i, e)

    def _insert(self, j, e):
        log = self.log[j]
        k = len(log)
        while k > 0 and log[k - 1][:3] > e[:3]:
            k -= 1
        cur = dp_state(self.boxes[j])['cursor']
        if k < cur:
            raise RuntimeError('brbox_net: a message would arrive in box %d\'s past '
                               '(%.2f < delivered)' % (j, e[0]))
        log.insert(k, e)

    def _deliver(self, box, d, upto):
        log = self.log[box.net_index]
        while d['cursor'] < len(log) and log[d['cursor']][0] <= upto:
            _t, _src, _seq, kind, payload = log[d['cursor']]
            d['cursor'] += 1
            _apply(box, d, kind, payload)


def _apply(box, d, kind, p):
    g = session_guid(d)
    if kind == 'session':
        guid, desc, name = p
        if desc is None:
            d['sessions'].pop(guid, None)
        else:
            d['sessions'][guid] = (desc, name)
    elif kind == 'player+':
        pid, info = p
        d['players'][pid] = info
        if g is not None and info.get('guid') == g:
            d['inbox'].append((0, 0, ('sys', DPSYS_CREATEPLAYERORGROUP, pid, info)))
            _signal_all(box, d)
    elif kind == 'player-':
        pid = p
        info = d['players'].pop(pid, None)
        if info is not None and g is not None and info.get('guid') == g:
            d['inbox'].append((0, 0, ('sys', DPSYS_DESTROYPLAYERORGROUP, pid, info)))
            _signal_all(box, d)
    elif kind == 'msg':
        guid, frm, to, data = p
        if g is None or guid != g:
            return
        for pid, pl in sorted(d['local'].items()):
            if pid == frm:
                continue
            if to == 0 or to == pid:
                d['inbox'].append((frm, pid, data))
                _signal(box, pl.get('event'))


def _signal_all(box, d):
    for p in d['local'].values():
        _signal(box, p.get('event'))


def _signal(box, h):
    if h:
        ev = box.hs.events.get(h)
        if ev is not None:
            ev['set'] = True


# ---------------------------------------------------------- paired runs --
def peer_script(steps):
    """The `peer SCRIPT` directive of a script, or None."""
    for op, args, _line in steps:
        if op == 'peer':
            return args[0]
    return None


def start_peer(host_box, script_path, log=lambda m: None, shots=None):
    """Build the peer box for `host_box`, link them, and run the peer on a
    thread.  Returns (thread, peer_box, net, result).  The host's run loop
    must call net.stop() when it ends so the peer does not wait on it."""
    import brbox_drive
    from brbox import GuestFault, Stop
    peer = brbox_drive.make_box(log=log)
    drv = brbox_drive.Driver(brbox_drive.parse_script(script_path), shots=shots, log=log)
    brbox_drive.attach(peer, drv)
    net = Net([host_box, peer])
    result = {}

    def run():
        try:
            peer.boot()
            peer.rally_main()
            result['end'] = 'returned'
        except Stop as e:
            result['end'] = 'stopped: %s' % e
        except GuestFault as e:
            result['end'] = 'fault: %s' % str(e).split('\n')[0]
        finally:
            net.stop(1)
            log('peer %s' % result.get('end'))
    th = threading.Thread(target=run, daemon=True)
    th.start()
    return th, peer, net, result
