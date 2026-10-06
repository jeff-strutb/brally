"""gbiexp.py in.c out.c [mode] : rewrite simple one-command gbi macro statements as explicit blocks.
mode 'dw0' (default): decl + w0 on one line, w1 on the next."""
import re, subprocess, sys, tempfile, os
src = open(sys.argv[1]).read().split('\n')
mode = sys.argv[3] if len(sys.argv) > 3 else 'dw0'
inc = os.path.abspath('src/tgrally/include')
sep = set(int(x) for x in os.environ.get('GBI_SEP','').split(',') if x)
only = set(int(x) for x in os.environ.get('GBI_LINES','').split(',') if x)
out = []
for ln, line in enumerate(src, 1):
    m = re.match(r'^(\s*)(gDP\w+|gSP\w+)\((D_8028A858\+\+.*)\);\s*$', line)
    if not m or 'TextureRectangle' in m.group(2) or (only and ln not in only):
        out.append(line); continue
    ind = m.group(1)
    d = tempfile.mkdtemp(); p = os.path.join(d, 'm.c')
    open(p, 'w').write('#include "tgr/common.h"\n#include "tgr/gbi.h"\nvoid f(void){%s(%s);}\n' % (m.group(2), m.group(3)))
    r = subprocess.run([os.path.abspath('tools/toolchains/ido53/cc'), '-E', '-I', inc, p], capture_output=True, text=True).stdout
    body = r[r.rindex('void f(void){') + len('void f(void){'):].strip()
    body = re.sub(r'\s+', ' ', body)
    mm = re.match(r'^\{ ?Gfx \*_g = \(Gfx \*\)\((D_8028A858\+\+)\); _g->words\.w0 = (.*?) ?; _g->words\.w1 = (.*?) ?; \} ?;? ?\}$', body)
    if not mm:
        out.append(line); continue
    out.append(ind + '{')
    if ln in sep:
        out.append(ind + '  Gfx *_g = (Gfx *)(D_8028A858++);'); out.append(''); out.append(ind + '  _g->words.w0 = %s;' % mm.group(2))
    else:
        out.append(ind + '  Gfx *_g = (Gfx *)(D_8028A858++); _g->words.w0 = %s;' % mm.group(2))
    out.append(ind + '  _g->words.w1 = %s;' % mm.group(3))
    out.append(ind + '}')
open(sys.argv[2], 'w').write('\n'.join(out))
