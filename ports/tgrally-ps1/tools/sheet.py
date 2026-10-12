#!/usr/bin/env python3
"""sheet.py RUNDIR -- one contact sheet of a shots.sh run: each retrace's
PlayStation frame beside the native port's, at half size (RUNDIR/sheet.png)."""
import glob
import os
import re
import sys

from PIL import Image

d = sys.argv[1]
rows = []
for p in sorted(glob.glob(os.path.join(d, 'ps1', 'shot*.png'))):
    f = int(re.search(r'shot(\d+)_', p).group(1))
    m = os.path.join(d, 'mac', 'frame%05d.png' % f)
    a = Image.open(p).convert('RGB').resize((320, 240))
    b = Image.open(m).convert('RGB').resize((320, 240)) if os.path.exists(m) else Image.new('RGB', (320, 240))
    rows.append((a, b))
sheet = Image.new('RGB', (650, 245 * len(rows)), (40, 40, 40))
for i, (a, b) in enumerate(rows):
    sheet.paste(a, (0, i * 245))
    sheet.paste(b, (330, i * 245))
sheet.save(os.path.join(d, 'sheet.png'))
print(os.path.join(d, 'sheet.png'))
