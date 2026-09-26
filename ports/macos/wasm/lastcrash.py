#!/usr/bin/env python3
"""Print the newest macOS crash report for build/wasm/brally (debugging)."""
import glob
import json
import os
import sys
d = os.path.expanduser('~/Library/Logs/DiagnosticReports')
fs = sorted(glob.glob(os.path.join(d, 'brally-*.ips')), key=os.path.getmtime)
if not fs:
    sys.exit('no crash reports')
t = open(fs[-1]).read()
hdr, body = t.split('\n', 1)
j = json.loads(body)
print(os.path.basename(fs[-1]), j['exception'].get('type'), j['exception'].get('subtype'))
th = [x for x in j['threads'] if x.get('triggered')][0]
n = int(sys.argv[1]) if len(sys.argv) > 1 else 16
for f in th['frames'][:n]:
    print('  %s + %s' % (f.get('symbol', '?'), f.get('symbolLocation', '')))
