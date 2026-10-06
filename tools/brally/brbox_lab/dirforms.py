import itertools, os, re, subprocess, sys, difflib, concurrent.futures as cf
sys.path.insert(0, 'tools'); sys.path.insert(0, 'build/brally/win32/brbox')
from match_diff import parse_coff_obj
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
SRC, SYM, VA, OPT = sys.argv[1:5]
base = open(SRC).read()
orig = open('build/brally/win32/match/orig/%s.bin' % VA, 'rb').read()
MD = Cs(CS_ARCH_X86, CS_MODE_32); NRM = lambda s: re.sub(r'0x[0-9a-f]{3,}', 'A', s)
O = ['%s %s' % (i.mnemonic, NRM(i.op_str)) for i in MD.disasm(orig, 0)]
IDX = {'fx': 0, 'fy': 1, 'fz': 2}
def gen(forms):
    t = base
    decl_f = [v for v in ('fy', 'fz', 'fx') if forms[v] == 'f']
    decl_i = [v for v in ('fy', 'fz', 'fx') if forms[v] == 'i']
    d = ''
    if decl_f: d += '    float %s;\n' % ', '.join(decl_f)
    if decl_i: d += '    int %s;\n' % ', '.join('i' + v[1] for v in decl_i)
    t = t.replace('    float fy, fz, fx;\n', d, 1)
    for v in ('fy', 'fz', 'fx'):
        line = '            %s = (float)DAT_105ccc78[0].dir[%d];\n' % (v, IDX[v])
        if forms[v] == 'f': continue
        if forms[v] == 'i':
            t = t.replace(line, '            i%s = DAT_105ccc78[0].dir[%d];\n' % (v[1], IDX[v]))
            use = '(float)i%s' % v[1]
        else:
            t = t.replace(line, '')
            use = '(float)DAT_105ccc78[0].dir[%d]' % IDX[v]
        t = re.sub(r'(\* )%s\b' % v, r'\g<1>' + use.replace('\\', '\\\\'), t)
    return t
def score(forms, idx):
    t = gen(forms)
    dd = 'build/brally/win32/match/t3d/df/%d' % idx; os.makedirs(dd, exist_ok=True)
    src = os.path.join(dd, os.path.basename(SRC)); open(src, 'w').write(t)
    obj = os.path.join(dd, 'o.obj')
    if os.path.exists(obj): os.unlink(obj)
    subprocess.run(['sh', 'tools/toolchains/wine.sh', 'tools/toolchains/msvc5/bin/cl.exe', '/nologo'] + OPT.split() + ['/W3', '/I', os.path.dirname(SRC), '/I', 'include', '/I', 'tools/toolchains/msvc5-compat', '/I', 'tools/toolchains/msvc5/include', '/DBR_MATCHING_BUILD', '/c', src, '/Fo' + obj.replace('/', '\\')], capture_output=True)
    if not os.path.exists(obj): return (9999, 9999), forms
    code = parse_coff_obj(obj)[SYM][0].rstrip(b'\x90')
    b = ['%s %s' % (i.mnemonic, NRM(i.op_str)) for i in MD.disasm(code, 0)]
    sm = difflib.SequenceMatcher(None, O, b, autojunk=False)
    import xdiff; return xdiff.split_score(orig, code), forms
combos = [dict(zip(('fx', 'fy', 'fz'), c)) for c in itertools.product('fie', repeat=3)]
with cf.ThreadPoolExecutor(12) as ex:
    res = sorted(ex.map(lambda a: score(a[1], a[0]), enumerate(combos)), key=lambda r: r[0])
for s, f in res[:8]: print(s, f)
open('build/brally/win32/match/t3d/df/best.c', 'w').write(gen(res[0][1]))
