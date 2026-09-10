#!/usr/bin/env python3
"""Regenerate the README progress block (M1/M2 bars) and the treemap SVG from
the live tier counts, in one step.

The two milestone bars in README.md used to be transcribed by hand off
tools/tiers.py every time progress moved -- four times in one afternoon once the
other sessions started landing matches.  This mints that chore: it reads the T3
and T4 byte/function totals straight from tiers.py, rewrites everything between
the `<!-- PROGRESS:BEGIN -->` and `<!-- PROGRESS:END -->` markers in README.md,
and regenerates docs/progress-map.svg.  Nothing outside the markers is touched.

    M1  contract-valid  = T3 (certified, not byte-exact) + T4 (byte-exact)
    M2  byte-exact       = T4

Both are quoted against BRGlide `.text` (480,853 B, the primary target), the
denominator the README documents -- NOT the smaller hand-C target tiers.py
prints its own percentages against.

    python3 tools/progressbar.py            # rewrite README + regenerate SVG
    python3 tools/progressbar.py --check     # exit 1 if README is stale, write nothing
    python3 tools/progressbar.py --no-svg    # skip the treemap regen
"""
import datetime
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
README = os.path.join(ROOT, 'README.md')
SVG = os.path.join(ROOT, 'docs', 'progress-map.svg')
PY = os.path.join(ROOT, '.venv', 'bin', 'python')
if not os.path.exists(PY):
    PY = sys.executable

# BRGlide .text, the whole primary binary. A fixed fact of the image (CLAUDE.md
# scope table / README), not something tiers.py reports -- it prints the smaller
# 452,733 B hand-C target. Both bars use this so they are comparable.
BRGLIDE_TEXT = 480853
BAR_W = 40
BEGIN = '<!-- PROGRESS:BEGIN'
END = '<!-- PROGRESS:END -->'


def tier_counts():
    """(t3_fns, t3_bytes, t4_fns, t4_bytes, target_fns) from tiers.py's output.

    Parsed from stdout rather than imported: tiers.main() computes these inside
    itself and prints them; the lines it prints are stable and carry their own
    denominators, which is exactly what this needs.
    """
    out = subprocess.run([PY, os.path.join(ROOT, 'tools', 'tiers.py')],
                         capture_output=True, text=True, cwd=ROOT).stdout
    def grab(pat):
        m = re.search(pat, out)
        if not m:
            sys.exit('progressbar: could not parse tiers.py output for /%s/\n\n%s'
                     % (pat, out))
        return m
    target = int(grab(r'hand-C target:\s+(\d+)\s+functions').group(1))
    t3 = grab(r'T3\s+certified, not byte-exact\s+(\d+)\s+fns\s+(\d+)\s+B')
    t4 = grab(r'T4\s+done \(byte-exact\)\s+(\d+)\s+fns\s+(\d+)\s+B')
    return (int(t3.group(1)), int(t3.group(2)),
            int(t4.group(1)), int(t4.group(2)), target)


def bar(pct):
    filled = round(pct / 100 * BAR_W)
    return '█' * filled + '░' * (BAR_W - filled)


def block(t3_fns, t3_b, t4_fns, t4_b, target):
    m1_b, m1_fns = t3_b + t4_b, t3_fns + t4_fns
    m2_b, m2_fns = t4_b, t4_fns
    m1_pct, m2_pct = 100 * m1_b / BRGLIDE_TEXT, 100 * m2_b / BRGLIDE_TEXT
    m1_fpct, m2_fpct = 100 * m1_fns / target, 100 * m2_fns / target
    today = datetime.date.today().isoformat()
    return (
        '_Snapshot %s._\n\n'
        '```\n'
        'M1  Contract-valid — compiles & ports (T3 + T4)\n'
        '    %s  %.1f%%   %s / %s B   %s / %s fns\n'
        'M2  Byte-exact (T4)\n'
        '    %s  %.1f%%   %s / %s B   %s / %s fns\n'
        '```\n\n'
        'The bars sit close by design: matching is byte-exact-first, so only %d\n'
        'certified-but-not-yet-exact functions (%s B) separate M1 from M2. Byte\n'
        'percentages trail function percentages (%.1f%% / %.1f%% of functions) '
        'because the\n'
        'functions still open are several times larger than the matched ones.\n'
        % (today,
           bar(m1_pct), m1_pct, f'{m1_b:,}', f'{BRGLIDE_TEXT:,}',
           f'{m1_fns:,}', f'{target:,}',
           bar(m2_pct), m2_pct, f'{m2_b:,}', f'{BRGLIDE_TEXT:,}',
           f'{m2_fns:,}', f'{target:,}',
           t3_fns, f'{t3_b:,}', m1_fpct, m2_fpct))


def splice(text, new_block):
    i = text.find(BEGIN)
    j = text.find(END)
    if i < 0 or j < 0:
        sys.exit('progressbar: %s markers not found in README.md' % BEGIN)
    head_end = text.find('\n', i) + 1          # keep the BEGIN marker line
    return text[:head_end] + new_block + text[j:]


def main():
    argv = sys.argv[1:]
    t3_fns, t3_b, t4_fns, t4_b, target = tier_counts()
    new = block(t3_fns, t3_b, t4_fns, t4_b, target)
    old = open(README, encoding='utf-8').read()
    updated = splice(old, new)

    if '--check' in argv:
        if updated != old:
            print('README progress block is STALE -- run tools/progressbar.py')
            return 1
        print('README progress block is current.')
        return 0

    m1_pct = 100 * (t3_b + t4_b) / BRGLIDE_TEXT
    m2_pct = 100 * t4_b / BRGLIDE_TEXT
    if updated != old:
        open(README, 'w', encoding='utf-8').write(updated)
        print('README: M1 %.1f%% / M2 %.1f%%  (T3 %d fns/%s B, T4 %d fns/%s B)'
              % (m1_pct, m2_pct, t3_fns, f'{t3_b:,}', t4_fns, f'{t4_b:,}'))
    else:
        print('README progress block already current (M1 %.1f%% / M2 %.1f%%).'
              % (m1_pct, m2_pct))

    if '--no-svg' not in argv:
        subprocess.run([sys.executable if not os.path.exists(PY) else 'python3',
                        os.path.join(ROOT, 'tools', 'progressmap.py'),
                        '--svg', SVG], cwd=ROOT, check=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
