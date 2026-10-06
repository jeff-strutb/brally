"""For every function with reloc_overrides rows: regenerate lockstep rows from
the placed object and report rows that differ (offset or value)."""
import subprocess, sys, collections
rows = collections.defaultdict(dict)
for l in open('config/brally/reloc_overrides.csv', encoding='latin1'):
    p = l.strip().split(',')
    if len(p) < 3 or not p[0].startswith('0x'):
        continue
    try:
        rows[int(p[0], 16)][int(p[1], 0)] = (int(p[2], 16), ','.join(p[3:]))
    except ValueError:
        pass
for va in sorted(rows):
    out = subprocess.run(['.venv/bin/python', 'tools/brally/lockstep_rows.py', '0x%08X' % va],
                         capture_output=True, text=True).stdout
    if 'REFUSED' in out or 'Traceback' in out:
        print('0x%08X REFUSED/ERROR %s' % (va, out.strip().splitlines()[-1][:120]), flush=True)
        continue
    new = {}
    for l in out.splitlines():
        if l.startswith('0x'):
            p = l.split(',')
            new[int(p[1], 0)] = int(p[2], 16)
    old = rows[va]
    conflict = sorted(o for o, (v, c) in old.items() if o in new and new[o] != v)
    missing = [o for o in old if o not in new]
    print('0x%08X rows %d regen %d CONFLICT %d missing %d %s' % (
        va, len(old), len(new), len(conflict), len(missing),
        ' '.join('%#x' % o for o in conflict[:8])), flush=True)
