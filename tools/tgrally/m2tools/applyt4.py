"""applyt4.py VA body.c NOTE : put an EXACT body into the tree as T4.

Keeps the WHAT IT DOES text up to its RESIDUE paragraph, appends NOTE (plain
text, wrapped at 76), drops the @t4-pass / @t3 lines, and replaces the
function (whole-file drafts end in .full.c and are copied as they are)."""
import re
import sys
import textwrap

sys.path.insert(0, 'tools/tgrally')
import n64t3 as T  # noqa: E402

va = int(sys.argv[1], 16)
src, note = sys.argv[2], sys.argv[3]
path, name, _ = T.source_of(va)
t = open(path).read()
tag = '/* @implements 0x%08X tgr %s */' % (va, name)
i = t.index(tag)
w = t.rindex('/* WHAT IT DOES:', 0, i)
head = t[w:i]
head = re.sub(r'/\* @t4-pass [^\n]*\*/\n', '', head)
head = re.sub(r'/\* @t3 0x[0-9A-Fa-f]{8} \*/\n', '', head)
end = head.index('*/')
what = head[:end]
k = what.find('\n * RESIDUE')
if k >= 0:
    what = what[:k]
what = what.rstrip()
if note:
    lines = textwrap.wrap(note, 73)
    what += '\n' + '\n'.join(' * ' + l for l in lines)
new_head = what + ' */\n' + tag + '\n'
if src.endswith('.full.c'):
    print('whole-file drafts: copy by hand')
    sys.exit(1)
nb = open(src).read().strip('\n')
nb = re.sub(r'^/\* @implements[^\n]*\n', '', nb)
body = T.function_text(t, name)
fs = t.index(body, i)
t = t[:w] + new_head + nb + t[fs + len(body):]
open(path, 'w').write(t)
print('applied', name, path)
