"""gaps.py VA variant.c : show insert/delete hunks of the opcode alignment (register-blind)"""
import os,sys,tempfile,difflib,struct
sys.path.insert(0,'tools/tgrally')
import n64search as S, n64t3 as T, n64build as B
from capstone import *
md=Cs(CS_ARCH_MIPS,CS_MODE_MIPS32|CS_MODE_BIG_ENDIAN)
va=int(sys.argv[1],16); path,name,_=T.source_of(va)
tree=open(path).read(); body=T.function_text(tree,name)
src=tree.replace(body,open(sys.argv[2]).read().strip('\n'))
work=os.path.join(os.path.dirname(os.path.abspath(__file__)),'tw')
fd,tmp=tempfile.mkstemp(suffix='.c',dir=work); os.close(fd); open(tmp,'w').write(src)
obj,err=B.compile_c(tmp); os.unlink(tmp)
pieces={n:(s,e) for n,s,e in B.carve(obj) if n}
rom,fmap,syms=S.ctx()
st,nd,notes,ours,theirs=B.grade(obj,rom,name,*pieces[name],va,fmap[va],syms,{n:v for v,n,_ in B.tags_in(src)})
dis=lambda w,a: next((i.mnemonic+' '+i.op_str for i in md.disasm(struct.pack('>I',w),a)),'?')
sm=difflib.SequenceMatcher(None,[S.regblind(w) for w in ours],[S.regblind(w) for w in theirs],autojunk=False)
for t,i1,i2,j1,j2 in sm.get_opcodes():
    if t=='equal': continue
    print('==',t,'ours',i1,i2,'rom',j1,j2)
    for k in range(max(i1-2,0),i2+2): print('  O %08X %s'%(va+4*k,dis(ours[k],va+4*k)) if k<len(ours) else '')
    for k in range(max(j1-2,0),j2+2): print('  R %08X %s'%(va+4*k,dis(theirs[k],va+4*k)) if k<len(theirs) else '')
