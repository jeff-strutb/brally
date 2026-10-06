"""Replace every reloc_overrides.csv row of one function with the rows in a
file, in the WORKING TREE and the INDEX (other sessions' edits stay unstaged)."""
import subprocess, sys
path, va, newf = sys.argv[1], sys.argv[2].upper() + ',', sys.argv[3]
new = [l.rstrip('\r\n') + '\n' for l in open(newf) if l.strip()]
def fix(text):
    lines = text.splitlines(keepends=True)
    keep = [l for l in lines if not l.upper().startswith(va)]
    if keep and not keep[-1].endswith('\n'):
        keep[-1] += '\n'
    return ''.join(keep + new), len(lines) - len(keep)
raw = open(path, 'rb').read().decode('latin1')
t, n = fix(raw)
open(path, 'wb').write(t.encode('latin1'))
idx = subprocess.run(['git', 'show', ':' + path], capture_output=True).stdout.decode('latin1')
t2, n2 = fix(idx)
blob = subprocess.run(['git', 'hash-object', '-w', '--stdin'], input=t2.encode('latin1'),
                      capture_output=True).stdout.decode().strip()
subprocess.run(['git', 'update-index', '--cacheinfo', '100644,%s,%s' % (blob, path)], check=True)
print('dropped %d (tree) / %d (index), added %d; staged' % (n, n2, len(new)))
