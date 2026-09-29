#!/usr/bin/env python3
"""Analyse a BR_FRAMELOG (native/frame.m): the one pacing instrument of
ports/macos/NATIVE_RENDERER.md, section 6.

Every latency claim cites this output: n, median, p90, max, and misses.

  framelog.py LOG [--from SEQ] [--to SEQ] [--period MS]

  latency     frame start (input read) -> presented
  miss        presented later than the target refresh (by > half a period),
              or never presented; only paced frames have a target
  interval    presented -> next presented; a repeat is one > 1.5 periods
  latch gap   latch - submit: how far ahead of the latch the frame was sent
  clock rate  game-clock ms per presented ms (1.000 = race clock is wall clock)
"""
import argparse
import statistics


def pct(xs, q):
    if not xs:
        return float('nan')
    s = sorted(xs)
    return s[min(len(s) - 1, int(q * (len(s) - 1) + 0.5))]


def dist(name, xs):
    if not xs:
        print('%-10s n 0' % name)
        return
    print('%-10s n %d  median %.2f  p90 %.2f  p99 %.2f  max %.2f  min %.2f (ms)' % (
        name, len(xs), statistics.median(xs), pct(xs, 0.90), pct(xs, 0.99), max(xs), min(xs)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('log')
    ap.add_argument('--from', dest='lo', type=int, default=0)
    ap.add_argument('--to', dest='hi', type=int, default=1 << 62)
    ap.add_argument('--period', type=float, default=0.0,
                    help='refresh period in ms (default: from the targets)')
    a = ap.parse_args()
    rows = []
    for ln in open(a.log):
        if ln.startswith('#'):
            continue
        f = ln.split()
        if len(f) < 10:
            continue
        seq = int(f[0])
        if not a.lo <= seq <= a.hi:
            continue
        start, submit, gpu, pres, latch, target, lead = map(float, f[1:8])
        rows.append(dict(seq=seq, start=start, submit=submit, gpu=gpu, pres=pres,
                         latch=latch, target=target, lead=lead, clock=int(f[8]),
                         flags=f[9]))
    rows.sort(key=lambda r: r['seq'])
    if not rows:
        raise SystemExit('framelog: no frames')
    period = a.period
    if not period:
        d = [r2['latch'] - r1['latch'] for r1, r2 in zip(rows, rows[1:])
             if r1['latch'] > 0 and r2['seq'] == r1['seq'] + 1
             and 'o' not in r2['flags'] and 's' not in r2['flags']]
        period = statistics.median(d) if d else 1000.0 / 60
    shown = [r for r in rows if r['pres'] > 0]
    paced = [r for r in rows if r['target'] > 0]
    print('frames %d (seq %d..%d), presented %d, paced %d, refresh period %.3f ms' % (
        len(rows), rows[0]['seq'], rows[-1]['seq'], len(shown), len(paced), period))
    dist('latency', [r['pres'] - r['start'] for r in shown])
    dist('cpu', [r['submit'] - r['start'] for r in rows if r['submit'] > 0])
    dist('gpu', [r['gpu'] - r['submit'] for r in rows if r['gpu'] > 0 and r['submit'] > 0])
    iv = [b['pres'] - a_['pres'] for a_, b in zip(shown, shown[1:]) if b['seq'] == a_['seq'] + 1]
    dist('interval', iv)
    print('repeats    %d intervals > 1.5 periods' % sum(1 for x in iv if x > 1.5 * period))
    if paced:
        miss = [r for r in paced if r['pres'] <= 0 or r['pres'] > r['target'] + period / 2]
        early = [r for r in paced if 0 < r['pres'] < r['target'] - period / 2]
        print('misses     %d of %d paced frames%s' % (
            len(miss), len(paced),
            (' (seq ' + ' '.join(str(r['seq']) for r in miss[:12]) + (' ...' if len(miss) > 12 else '') + ')')
            if miss else ''))
        print('early      %d' % len(early))
        dist('latch gap', [r['latch'] - r['submit'] for r in paced if r['submit'] > 0])
        dist('lead', [r['lead'] for r in paced])
        dist('depth', [(r['target'] - r['latch']) / period for r in paced])
        dist('shown at', [(r['pres'] - r['latch']) / period for r in paced if r['pres'] > 0])
        print('overruns   %d, skips %d' % (sum('o' in r['flags'] for r in paced),
                                           sum('s' in r['flags'] for r in paced)))
    if len(shown) > 1:
        span = shown[-1]['pres'] - shown[0]['pres']
        print('presents/s %.2f over %.1f s' % ((len(shown) - 1) / span * 1000.0, span / 1000.0))
        c = [r for r in shown if r['clock'] >= 0]
        if len(c) > 1 and c[-1]['pres'] > c[0]['pres']:
            print('clock rate %.5f game ms per presented ms' % (
                (c[-1]['clock'] - c[0]['clock']) / (c[-1]['pres'] - c[0]['pres'])))


if __name__ == '__main__':
    main()
