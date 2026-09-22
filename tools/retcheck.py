#!/usr/bin/env python3
"""retcheck.py -- return-type consistency lint (catches oracle-blind dropped
returns).

The A5 equivalence oracle runs a function on synthetic inputs and compares the
return register only when the transcription's signature says there is one.  A
function whose result is communicated ONLY through eax, on a path the seeds do
not exercise, can pass EQUIVALENT while a `void` transcription silently drops
the return.  That is exactly what shipped for BrSndBankPickSlot: defined
`void`, but the original returns the picked render slot in eax, and BrRaceStep
(`int sub_10013F20(); loc18 = sub_10013F20();`) used it as an array index --
garbage slot, cars copied to the wrong buffer, the rendered slot left with an
unset camera pointer, and the game faulted entering the race.  The oracle could
not see it (with synthetic memory `chosen` degenerates to a constant), but the
signature conflict is unambiguous statically.

This flags every function whose @implements DEFINITION returns `void` while a
caller both DECLARES it value-returning AND USES the result (assigns it).
Names are resolved to VA (via @implements and the sub_/FUN_/BrSub/BrExt_<VA>
convention) so a def and a differently-named caller at one address compare.
Returns non-zero when any are found, so it can gate the build.
"""
import os, re, glob, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = glob.glob(os.path.join(ROOT,'src','**','*.c'), recursive=True) \
    + glob.glob(os.path.join(ROOT,'src','**','*.cpp'), recursive=True)
HDR = glob.glob(os.path.join(ROOT,'include','**','*.h'), recursive=True) \
    + glob.glob(os.path.join(ROOT,'src','**','*.h'), recursive=True)
RAW = {f: open(f,errors='replace').read() for f in SRC+HDR}
def _strip_comments(s):
    s = re.sub(r'/\*.*?\*/', '', s, flags=re.S)   # block comments
    s = re.sub(r'//[^\n]*', '', s)                 # line comments
    return s
# @implements anchors are read from RAW (they live in comments); usage scans
# use comment-stripped text so a commented-out call is not counted.
TXT = {f: _strip_comments(t) for f, t in RAW.items()}

IMPL = re.compile(r'@implements\s+0x([0-9A-Fa-f]+)\s+\w+\s+([A-Za-z_]\w*)')
def retcls(rt):
    rt=re.sub(r'__fastcall|__stdcall|__cdecl|BR_THISCALL1?|BR_FASTCALL|extern|static',' ',rt).strip()
    if not rt: return None
    if '*' in rt: return 'ptr'
    return 'void' if rt.split()[-1]=='void' else 'value'

# @implements DEFINITION that returns void -> {va: (name, relpath)}
defvoid={}
for f,txt in RAW.items():            # @implements lives in comments -> use RAW
    L=txt.splitlines()
    for i,ln in enumerate(L):
        m=IMPL.search(ln)
        if not m: continue
        va=int(m.group(1),16); nm=m.group(2)
        for j in range(i+1,min(i+9,len(L))):
            sm=re.match(r'^\s*([A-Za-z_][\w \t\*]*?)\b%s\s*\('%re.escape(nm),L[j])
            if sm:
                if retcls(sm.group(1))=='void':
                    defvoid[va]=(nm, os.path.relpath(f,ROOT))
                break

def names_for(va, defname):
    ns={defname}
    for pat in ('sub_%08X','sub_%08x','FUN_%08X','FUN_%08x','BrSub%08X','BrExt_%08X'):
        ns.add(pat%va)
    return ns

bugs=[]
for va,(nm,df) in sorted(defvoid.items()):
    for cn in names_for(va,nm):
        hit=None
        for f,txt in TXT.items():
            m=re.search(r'=\s*(?:\([^)]*\)\s*)?'+re.escape(cn)+r'\s*\(', txt)
            if m:
                hit=(os.path.relpath(f,ROOT), txt[:m.start()].count('\n')+1, cn); break
        if hit:
            bugs.append((va,nm,df,hit)); break

for va,nm,df,(uf,ul,cn) in bugs:
    print("%#010x %s: DEFINED void (%s) but return USED at %s:%d as %s()"
          % (va,nm,df,uf,ul,cn))
print("\n%d function(s) defined void whose return value is used (declare the real "
      "return type)" % len(bugs))
sys.exit(1 if bugs else 0)
