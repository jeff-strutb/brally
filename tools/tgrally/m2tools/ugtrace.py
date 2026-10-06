"""ugtrace.py file.c FuncName : ugen free-list trace for one function (ALLOC/FREE lines)"""
import sys,os,subprocess
sys.path.insert(0,'tools/tgrally'); import n64build as B
R=os.getcwd(); p=os.path.abspath(sys.argv[1]); src=open(p).read()
S=os.path.dirname(os.path.abspath(__file__))
o=os.path.join(S,'ug.o')
cmd=[os.path.join(R,'build/tgrally/ext/instr/out/cc')]+B.BASE_FLAGS+B.cflags_for(src)+['-o',o,p]
r=subprocess.run(cmd,capture_output=True,text=True,env=dict(os.environ,DKWB_UGEN_TRACE='1'),cwd=S)
names=[x[0] for x in B.carve(B.Obj(o))]; k=names.index(sys.argv[2])
on=False
for line in r.stderr.splitlines():
    if line.startswith('DKWB-PROC BEGIN'):
        on = line.endswith('proc=%d'%k)
        continue
    if on and 'DKWB-CALL' not in line: print(line)
