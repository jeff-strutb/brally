"""Apply exact line replacements to a file in the WORKING TREE and stage the
same replacements applied to HEAD's version (so unrelated uncommitted edits
by other sessions stay unstaged)."""
import subprocess, sys, json
path = sys.argv[1]
pairs = json.load(open(sys.argv[2]))
def fix(text):
    for old, new in pairs:
        assert text.count(old) == 1, ('not unique/not found', old)
        text = text.replace(old, new)
    return text
raw = open(path, 'rb').read().decode('latin1')
open(path, 'wb').write(fix(raw).encode('latin1'))
idx = subprocess.run(['git', 'show', ':' + path], capture_output=True).stdout.decode('latin1')
blob = subprocess.run(['git', 'hash-object', '-w', '--stdin'], input=fix(idx).encode('latin1'),
                      capture_output=True).stdout.decode().strip()
subprocess.run(['git', 'update-index', '--cacheinfo', '100644,%s,%s' % (blob, path)], check=True)
print('staged', path)
