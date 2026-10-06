"""Check every 'lockstep SYM' row against the placed object's relocation
sites: valid only if the object has a reloc for SYM at that offset."""
import subprocess, sys, collections, os
rows = collections.defaultdict(list)
for l in open('config/brally/reloc_overrides.csv', encoding='latin1'):
    p = l.rstrip('\r\n').split(',')
    if len(p) < 4 or not p[0].startswith('0x'):
        continue
    rows[int(p[0], 16)].append((int(p[1], 0), ','.join(p[3:]), l.rstrip('\r\n')))
env = dict(os.environ, LOCKSTEP_SITES='1')
bad_all = []
for va in sorted(rows):
    out = subprocess.run(['.venv/bin/python', 'tools/brally/lockstep_rows.py', '0x%08X' % va],
                         capture_output=True, text=True, env=env).stdout
    sites = {}
    for l in out.splitlines():
        if l.startswith('# SITE '):
            _, _, off, sym = l.split(' ', 3)
            sites[int(off, 16)] = sym.lstrip('_')
    if not sites:
        print('0x%08X no sites (%s)' % (va, out.strip().splitlines()[-1][:80] if out.strip() else '?'), flush=True)
        continue
    bad = []
    for off, comment, line in rows[va]:
        if not comment.startswith('lockstep '):
            continue
        sym = comment.split()[1].lstrip('_')
        if sites.get(off) != sym:
            bad.append(line)
    print('0x%08X rows %d invalid %d' % (va, len(rows[va]), len(bad)), flush=True)
    bad_all += bad
open('build/brally/win32/brbox/invalid_rows.txt', 'w').write('\n'.join(bad_all) + '\n')
