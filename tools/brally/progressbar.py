#!/usr/bin/env python3
"""Regenerate the README progress block (M1/M2 bars) and the treemap SVG from
the live tier counts, in one step.

The two milestone bars in README.md used to be transcribed by hand off
tools/brally/tiers.py every time progress moved -- four times in one afternoon once the
other sessions started landing matches.  This mints that chore: it reads the T3
and T4 byte/function totals straight from tiers.py, rewrites everything between
the `<!-- PROGRESS:BEGIN -->` and `<!-- PROGRESS:END -->` markers in README.md
(Current Status: the bars and the per-binary table) and between the
`<!-- PROGRESS-DETAIL:... -->` markers (the PC section: what the bars measure),
and regenerates docs/brally/progress-map.svg.  Nothing outside the markers is touched.

    M1  contract-valid  = T3 (certified, not byte-exact) + T4 (byte-exact)
    M2  byte-exact       = T4

Both are quoted against the HAND-WRITTEN target tiers.py reports (the game's
own functions), not the whole BRGlide `.text`.  The rest of `.text` -- padding
and jump tables between functions, linker/EH code, and functions the retail
game never runs -- is produced by the build or deliberately out of scope, so
dividing by all of `.text` capped the bars near 93% even with nothing left to
write.  The block itemises that rest so the two numbers reconcile.

    python3 tools/brally/progressbar.py            # rewrite README + regenerate SVG
    python3 tools/brally/progressbar.py --check     # exit 1 if README is stale, write nothing
    python3 tools/brally/progressbar.py --no-svg    # skip the treemap regen
"""
import datetime
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
README = os.path.join(ROOT, 'README.md')
SVG = os.path.join(ROOT, 'docs', 'brally', 'progress-map.svg')
N64_SVG = os.path.join(ROOT, 'docs', 'tgrally', 'progress-map.svg')
PY = os.path.join(ROOT, '.venv', 'bin', 'python')
if not os.path.exists(PY):
    PY = sys.executable

# BRGlide .text, the whole primary binary. A fixed fact of the image (the
# README scope table).  The bars divide by the hand-written target instead;
# this is only used to itemise what the rest of .text is.
BRGLIDE_TEXT = 480853
BAR_W = 40
BEGIN = '<!-- PROGRESS:BEGIN'
END = '<!-- PROGRESS:END -->'
DETAIL_BEGIN = '<!-- PROGRESS-DETAIL:BEGIN'
DETAIL_END = '<!-- PROGRESS-DETAIL:END -->'
N64_BEGIN = '<!-- N64-PROGRESS:BEGIN'
N64_END = '<!-- N64-PROGRESS:END -->'


def tier_counts():
    """(t3_fns, t3_bytes, t4_fns, t4_bytes, target_fns, target_bytes, t2_fns,
    t2_bytes, excluded_fns, excluded_bytes) from tiers.py's output.

    Parsed from stdout rather than imported: tiers.main() computes these inside
    itself and prints them; the lines it prints are stable and carry their own
    denominators, which is exactly what this needs.
    """
    out = subprocess.run([PY, os.path.join(ROOT, 'tools', 'brally', 'tiers.py')],
                         capture_output=True, text=True, cwd=ROOT).stdout
    def grab(pat):
        m = re.search(pat, out)
        if not m:
            sys.exit('progressbar: could not parse tiers.py output for /%s/\n\n%s'
                     % (pat, out))
        return m
    tgt = grab(r'hand-C target:\s+(\d+)\s+functions\s+\((\d+)\s+B')
    t2 = grab(r'T2\s+in progress \(not done\)\s+(\d+)\s+fns\s+(\d+)\s+B')
    t3 = grab(r'T3\s+certified, not byte-exact\s+(\d+)\s+fns\s+(\d+)\s+B')
    t4 = grab(r'T4\s+done \(byte-exact\)\s+(\d+)\s+fns\s+(\d+)\s+B')
    ex = grab(r'EXCLUDED\s+never run by the game\s+(\d+)\s+fns\s+(\d+)\s+B')
    return (int(t3.group(1)), int(t3.group(2)),
            int(t4.group(1)), int(t4.group(2)),
            int(tgt.group(1)), int(tgt.group(2)),
            int(t2.group(1)), int(t2.group(2)),
            int(ex.group(1)), int(ex.group(2)))


def text_breakdown():
    """(fenced_bytes, mapped_function_bytes) of BRGlide .text: the linker/EH
    code in config/brally/fenced.csv, and every function in the glide map (the rest of
    .text is padding and in-line data between functions)."""
    import csv
    fenced = sum(int(r['size']) for r in
                 csv.DictReader(open(os.path.join(ROOT, 'config', 'brally', 'fenced.csv'))))
    mapped = sum(int(r['size']) for r in
                 csv.DictReader(open(os.path.join(ROOT, 'config', 'brally', 'functions_glide.csv')))
                 if r.get('size'))
    return fenced, mapped


def n64_counts():
    """(t3_fns, t3_b, t4_fns, t4_b, target_fns, target_bytes) from
    tools/tgrally/n64tiers.py -- the Top Gear Rally lane. Same parse-the-stdout
    approach as tier_counts(); the numbers carry commas here, so strip them."""
    out = subprocess.run([PY, os.path.join(ROOT, 'tools', 'tgrally', 'n64tiers.py')],
                         capture_output=True, text=True, cwd=ROOT).stdout

    def grab(pat):
        m = re.search(pat, out)
        if not m:
            sys.exit('progressbar: could not parse n64tiers.py for /%s/\n\n%s'
                     % (pat, out))
        return m

    def num(s):
        return int(s.replace(',', ''))
    tgt = grab(r'game code:\s+([\d,]+)\s+functions\s+\(([\d,]+)\s+B')
    t3 = grab(r'T3\s+certified, not byte-exact\s+([\d,]+)\s+fns\s+([\d,]+)\s+B')
    t4 = grab(r'T4\s+done \(byte-exact\)\s+([\d,]+)\s+fns\s+([\d,]+)\s+B')
    return (num(t3.group(1)), num(t3.group(2)),
            num(t4.group(1)), num(t4.group(2)),
            num(tgt.group(1)), num(tgt.group(2)))


def n64_block(t3_fns, t3_b, t4_fns, t4_b, target_fns, target_b):
    """The N64 (Top Gear Rally) M1/M2 bars, same rendering as the PC block so
    the two read as one system. Denominator is the ROM's whole game-code
    .text (fenced library not yet separated); both bars quote against it."""
    m1_b, m1_fns = t3_b + t4_b, t3_fns + t4_fns
    m1_pct = 100 * m1_b / target_b if target_b else 0
    m2_pct = 100 * t4_b / target_b if target_b else 0
    today = datetime.date.today().isoformat()
    return (
        '_Snapshot %s._\n\n'
        '```\n'
        'M1  Contract-valid (T3 + T4)\n'
        '    %s  %.1f%%   %s / %s B   %s / %s fns\n'
        'M2  Byte-exact (T4)\n'
        '    %s  %.1f%%   %s / %s B   %s / %s fns\n'
        '```\n'
        % (today,
           bar(m1_pct), m1_pct, f'{m1_b:,}', f'{target_b:,}',
           f'{m1_fns:,}', f'{target_fns:,}',
           bar(m2_pct), m2_pct, f'{t4_b:,}', f'{target_b:,}',
           f'{t4_fns:,}', f'{target_fns:,}'))


# report_exe.csv's `exe` column -> the shipped filename, in the order the
# per-binary table lists them (the three EXEs, then the DLL).
EXES = [('bossrally', 'BossRally.exe'),
        ('brally', 'BRally.exe'),
        ('setvideo', 'SetVideo.exe')]


def exe_counts():
    """{exe: (match_fns, match_bytes, diff_fns, diff_bytes)} from report_exe.csv.

    The EXEs have no T3 lane (no certification workstream runs on them), so for
    an EXE M1 == M2: a function is either byte-exact or still open.  match+diff
    is the in-scope game code; the static CRT that fills out each image is fenced
    (reproduced at link, not decompiled) and is not in this file at all.
    """
    out = {e: [0, 0, 0, 0] for _tag, e in EXES}
    disp = {tag: e for tag, e in EXES}
    p = os.path.join(ROOT, 'build', 'brally', 'win32', 'match', 'report_exe.csv')
    if not os.path.exists(p):
        return out
    import csv
    for r in csv.DictReader(open(p)):
        e = disp.get(r.get('exe', ''))
        if not e:
            continue
        b = int(r['orig_size']) if r.get('orig_size') else 0
        if r.get('status') == 'match':
            out[e][0] += 1
            out[e][1] += b
        elif r.get('status') == 'diff':
            out[e][2] += 1
            out[e][3] += b
    return out


def bar(pct, w=BAR_W):
    filled = round(pct / 100 * w)
    return '█' * filled + '░' * (w - filled)


def _table(t3_fns, t3_b, t4_fns, t4_b, exes, target_b):
    """The per-binary M1/M2 table, mini-bars 20 wide."""
    rows = ['| Area | M1: contract-valid | M2: byte-exact |',
            '|---|---|---|']
    for _tag, name in EXES:
        mf, mb, df, db = exes[name]
        tgt_b = mb + db
        pct = 100 * mb / tgt_b if tgt_b else 100.0
        cell = '`%s` %.0f%%: %d/%d fns, %s B' % (
            bar(pct, 20), pct, mf, mf + df, f'{mb:,}')
        rows.append('| **%s** | %s | %s |' % (name, cell, cell))
    # BRGlide.dll: M1 = T3+T4, M2 = T4, both vs the hand-written target.
    m1_b, m1_fns = t3_b + t4_b, t3_fns + t4_fns
    m1p, m2p = 100 * m1_b / target_b, 100 * t4_b / target_b
    rows.append('| **BRGlide.dll** | `%s` %.1f%%: %s B, %s fns | '
                '`%s` %.1f%%: %s B, %s fns |'
                % (bar(m1p, 20), m1p, f'{m1_b:,}', f'{m1_fns:,}',
                   bar(m2p, 20), m2p, f'{t4_b:,}', f'{t4_fns:,}'))
    return '\n'.join(rows)


def block(t3_fns, t3_b, t4_fns, t4_b, target, target_b, exes):
    """Current Status: the two milestone bars and the per-binary table."""
    m1_b, m1_fns = t3_b + t4_b, t3_fns + t4_fns
    m2_b, m2_fns = t4_b, t4_fns
    m1_pct, m2_pct = 100 * m1_b / target_b, 100 * m2_b / target_b
    today = datetime.date.today().isoformat()
    return (
        '_Snapshot %s._\n\n'
        '```\n'
        'M1  Contract-valid: compiles & ports (T3 + T4)\n'
        '    %s  %.1f%%   %s / %s B   %s / %s fns\n'
        'M2  Byte-exact (T4)\n'
        '    %s  %.1f%%   %s / %s B   %s / %s fns\n'
        '```\n\n'
        '%s\n'
        % (today,
           bar(m1_pct), m1_pct, f'{m1_b:,}', f'{target_b:,}',
           f'{m1_fns:,}', f'{target:,}',
           bar(m2_pct), m2_pct, f'{m2_b:,}', f'{target_b:,}',
           f'{m2_fns:,}', f'{target:,}',
           _table(t3_fns, t3_b, t4_fns, t4_b, exes, target_b)))


def detail_block(t3_fns, t3_b, t4_fns, t4_b, target, target_b,
                 ex_fns, ex_b, fenced_b, mapped_b):
    """PC section: what the bars measure and the rest of the DLL."""
    m1_b, m1_fns = t3_b + t4_b, t3_fns + t4_fns
    pad_b = BRGLIDE_TEXT - mapped_b
    open_fns = target - m1_fns
    left = ('%d function%s still open' % (open_fns, '' if open_fns == 1 else 's')
            if open_fns else 'complete')
    return (
        '**What the bars measure.** Both bars count the game\'s own functions in\n'
        'BRGlide.dll: the code that has to be written by hand. M1 has %s.\n'
        'M2 trails it by %d functions (%s B) that are certified to behave exactly like\n'
        'the original but do not yet compile to identical bytes.\n\n'
        '**The rest of the DLL.** Its code section is %s B; the bars leave out %s B\n'
        'of it, all of which the finished DLL still contains but none of which is\n'
        'hand-written:\n\n'
        '| Bytes | What it is | Where it comes from |\n'
        '|---:|---|---|\n'
        '| %s | padding and jump tables between functions | emitted by the compiler with the functions |\n'
        '| %s | import stubs and C++ exception-handling glue | generated by the compiler and linker |\n'
        '| %s | %d functions the retail game never runs | written, but left out of the count (config/brally/excluded.csv) |\n\n'
        'By binary: the three EXEs are complete at both milestones (their game '
        'code is fully\n'
        'byte-exact; the static CRT filling out each image is reproduced by '
        'linking, not\n'
        'decompiled, and is out of scope). All remaining work is in BRGlide.dll.\n'
        % (left, t3_fns, f'{t3_b:,}',
           f'{BRGLIDE_TEXT:,}', f'{BRGLIDE_TEXT - target_b:,}',
           f'{pad_b:,}', f'{fenced_b:,}', f'{ex_b:,}', ex_fns))


def splice(text, new_block, begin=BEGIN, end=END):
    i = text.find(begin)
    j = text.find(end)
    if i < 0 or j < 0:
        sys.exit('progressbar: %s markers not found in README.md' % begin)
    head_end = text.find('\n', i) + 1          # keep the BEGIN marker line
    return text[:head_end] + new_block + text[j:]


def main():
    argv = sys.argv[1:]
    (t3_fns, t3_b, t4_fns, t4_b, target, target_b,
     t2_fns, t2_b, ex_fns, ex_b) = tier_counts()
    fenced_b, mapped_b = text_breakdown()
    old = open(README, encoding='utf-8').read()
    updated = splice(old, block(t3_fns, t3_b, t4_fns, t4_b, target, target_b,
                                exe_counts()))
    updated = splice(updated,
                     detail_block(t3_fns, t3_b, t4_fns, t4_b, target, target_b,
                                  ex_fns, ex_b, fenced_b, mapped_b),
                     DETAIL_BEGIN, DETAIL_END)

    # N64 (Top Gear Rally) lane -- its own markers, same rendering.
    n3f, n3b, n4f, n4b, ntf, ntb = n64_counts()
    updated = splice(updated, n64_block(n3f, n3b, n4f, n4b, ntf, ntb),
                     N64_BEGIN, N64_END)

    if '--check' in argv:
        if updated != old:
            print('README progress block is STALE -- run tools/brally/progressbar.py')
            return 1
        print('README progress block is current.')
        return 0

    m1_pct = 100 * (t3_b + t4_b) / target_b
    m2_pct = 100 * t4_b / target_b
    if updated != old:
        open(README, 'w', encoding='utf-8').write(updated)
        print('README: M1 %.1f%% / M2 %.1f%%  (T3 %d fns/%s B, T4 %d fns/%s B)'
              % (m1_pct, m2_pct, t3_fns, f'{t3_b:,}', t4_fns, f'{t4_b:,}'))
    else:
        print('README progress block already current (M1 %.1f%% / M2 %.1f%%).'
              % (m1_pct, m2_pct))
    n_m1 = 100 * (n3b + n4b) / ntb if ntb else 0
    n_m2 = 100 * n4b / ntb if ntb else 0
    print('  N64 (Top Gear Rally): M1 %.1f%% / M2 %.1f%%  (T3 %d, T4 %d of %d fns)'
          % (n_m1, n_m2, n3f, n4f, ntf))

    if '--no-svg' not in argv:
        py = 'python3' if not os.path.exists(PY) else PY
        subprocess.run([py, os.path.join(ROOT, 'tools', 'brally', 'progressmap.py'),
                        '--svg', SVG], cwd=ROOT, check=True)
        subprocess.run([py, os.path.join(ROOT, 'tools', 'tgrally', 'n64map.py'),
                        '--svg', N64_SVG], cwd=ROOT, check=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
