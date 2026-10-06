#!/usr/bin/env python3
"""tags.txt lines: `sheet material r,c r,c ...` or `sheet material rowN` (whole
row) or `sheet material all` -> texmat.csv (hash,material,sheet/row/col)."""
import sys, collections
idx = {}
for ln in open(sys.argv[1]):
    s, r, c, h, fmt, w, hh, mean = ln.strip().split(',')
    idx[(int(s), int(r), int(c))] = (h, '%sx%s f%s' % (w, hh, fmt))
tags = {}
for ln in open(sys.argv[2]):
    ln = ln.split('#')[0].split()
    if len(ln) < 3: continue
    s, mat = int(ln[0]), ln[1]
    for tok in ln[2:]:
        if tok == 'all':
            cells = [k for k in idx if k[0] == s]
        elif tok.startswith('row'):
            cells = [k for k in idx if k[0] == s and k[1] == int(tok[3:])]
        else:
            r, c = map(int, tok.split(','))
            cells = [(s, r, c)]
        for k in cells:
            if k in idx: tags[k] = mat
out = open(sys.argv[3], 'w')
out.write('# Remastered material of every game texture, by the FNV-1a hash of its\n'
          '# large level (RGBA8, as BR_TEXDUMP names it): hash,material,size\n')
cnt = collections.Counter()
for k in sorted(tags, key=lambda k: idx[k][0]):
    h, desc = idx[k]
    out.write('%s,%s,%s\n' % (h, tags[k], desc)); cnt[tags[k]] += 1
print(len(tags), 'of', len(idx), 'tagged;', dict(cnt))
