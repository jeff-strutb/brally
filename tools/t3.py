#!/usr/bin/env python3
"""Hand-certified T3 functions: complete and verified, not yet byte-exact.

    .venv/bin/python tools/t3.py            # list every @t3 tag, validate, exit 1 on a bad one
    .venv/bin/python tools/t3.py --vas      # just the VAs (lower-case), one per line

THE TAG.  Directly above a function's @implements line (below WHAT IT DOES):

    /* @t3 0x1000EAF0 2026-09-06 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
     * <measured residue: bytes, regions, multiset rows>; every row mapped to
     * a compiler decision (<classes>). No missing semantic operation.
     * Dossier: <where the dead-probe list lives>. Do not reopen before the
     * end-grind. */

The first line is machine-read: `@t3 <VA> <YYYY-MM-DD>` and the phrase
`CERTIFIED COMPLETE`.  The rest is for the reader.

WHAT IT MEANS (CLAUDE.md rule 12).  The function is transcribed from the
original's bytes, every structural element verified, and its residue is
accounted row by row as compiler decisions only.  It is fully usable by the
port.  It is NOT byte-exact and it is NOT done: the tag is a parking receipt
with the evidence attached, so that no session re-litigates it before the
end-grind.  It is never an excuse to stop short: a function whose residue
still contains a missing or extra semantic operation does not qualify.

WHAT THIS TOOL CHECKS.  Every tag has a VA, a date and the certification
phrase; the VA has an @implements in the same file; and the function is still
`diff` in report.csv -- a certified function that has since become byte-exact
is STALE and must lose the tag (the @implements stays).  claim_lane.py and
tiers.py import certified() so certified rows are never handed out as
targets and are reported with their own denominator.
"""
import csv, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'src')
REPORT = os.path.join(ROOT, 'build', 'match', 'report.csv')
TAG = re.compile(r'@t3\s+(0x[0-9A-Fa-f]{8})\s+(\d{4}-\d{2}-\d{2})\b')
IMPL = re.compile(r'@implements\s+(0x[0-9A-Fa-f]{8})\b')


def certified():
    """va(lower) -> {'date','file','line','ok','why'}"""
    out = {}
    for dp, _, fs in os.walk(SRC):
        for fn in fs:
            if not fn.endswith(('.c', '.cpp', '.h')):
                continue
            path = os.path.join(dp, fn)
            try:
                text = open(path, encoding='utf-8', errors='replace').read()
            except OSError:
                continue
            if '@t3' not in text:
                continue
            impls = set(m.group(1).lower() for m in IMPL.finditer(text))
            for ln, line in enumerate(text.splitlines(), 1):
                if '@t3' not in line:
                    continue
                m = TAG.search(line)
                rel = os.path.relpath(path, ROOT)
                if not m:
                    out['?%s:%d' % (rel, ln)] = dict(date='', file=rel, line=ln, ok=False,
                                                    why='malformed: need `@t3 0xVA YYYY-MM-DD`')
                    continue
                va, date = m.group(1).lower(), m.group(2)
                why = ''
                if 'CERTIFIED COMPLETE' not in line:
                    why = 'missing the phrase CERTIFIED COMPLETE on the tag line'
                elif va not in impls:
                    why = 'no @implements %s in this file' % va
                out[va] = dict(date=date, file=rel, line=ln, ok=not why, why=why)
    return out


def report_status():
    st = {}
    if os.path.exists(REPORT):
        for r in csv.DictReader(open(REPORT)):
            if r.get('va'):
                st[r['va'].lower()] = r.get('status', '')
    return st


def main(argv):
    cert = certified()
    if '--vas' in argv:
        for va in sorted(cert):
            if not va.startswith('?'):
                print(va)
        return 0
    st = report_status()
    bad = 0
    for va, info in sorted(cert.items()):
        flag = 'ok'
        if not info['ok']:
            flag = 'BAD: ' + info['why']; bad += 1
        elif st.get(va) == 'match':
            flag = 'STALE: byte-exact now -- remove the @t3 tag'; bad += 1
        elif va not in st:
            flag = 'unswept (no report.csv row yet)'
        print('%s  %s  %s:%d  %s' % (va, info['date'], info['file'], info['line'], flag))
    n = sum(1 for v in cert if not v.startswith('?'))
    print('T3-certified functions: %d  (hand-certified; not counted as matched)' % n)
    if bad:
        print('FAIL: %d bad or stale @t3 tag(s).' % bad)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
