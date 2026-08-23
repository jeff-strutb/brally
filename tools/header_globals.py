#!/usr/bin/env python3
"""Harvest documented global addresses from the tree's own headers/sources:
`extern <type> NAME...; /* 0xADDR ... */` and `NAME = ...;  /* 0xADDR */`
comment forms. Writes build/header_globals.csv (symbol,addr). Wrong entries
cannot slip through silently: image_build.py diffs the assembled image
against the original, so a bad address turns into visible byte diffs."""
import re, glob, csv, os

pat = re.compile(
    r'^\s*(?:extern\s+)?[A-Za-z_][\w \t*]*?\b([A-Za-z_]\w+)\s*(?:\[[^\]]*\])?\s*;'
    r'\s*/\*\s*(0x1[0-9A-Fa-f]{7})\b', re.M)
out = {}
for f in glob.glob('include/*.h') + glob.glob('src/core/**/*.c', recursive=True):
    s = open(f, encoding='latin1').read()
    for m in pat.finditer(s):
        name, addr = m.group(1), int(m.group(2), 16)
        if name in out and out[name] != addr:
            out[name] = None          # conflicting docs -> unusable
        elif name not in out:
            out[name] = addr
os.makedirs('build', exist_ok=True)
with open('build/header_globals.csv', 'w', newline='') as fh:
    w = csv.writer(fh); w.writerow(['symbol', 'addr'])
    n = 0
    for k, v in sorted(out.items()):
        if v is not None:
            w.writerow([k, '0x%08X' % v]); n += 1
print(n, 'documented globals harvested')
