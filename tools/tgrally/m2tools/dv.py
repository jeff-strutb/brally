"""dv.py VA variant.c : print the diff of one variant"""
import os,sys,tempfile
sys.path.insert(0,'tools/tgrally')
import n64search as S, n64t3 as T, n64build as B
va=int(sys.argv[1],16); path,name,_=T.source_of(va)
tree=open(path).read(); body=T.function_text(tree,name)
nb=open(sys.argv[2]).read().strip('\n')
src=tree.replace(body,nb)
work=os.path.join(os.path.dirname(os.path.abspath(__file__)),'tw')
fd,tmp=tempfile.mkstemp(suffix='.c',dir=work); os.close(fd); open(tmp,'w').write(src)
obj,err=B.compile_c(tmp); os.unlink(tmp)
if obj is None: sys.exit(err)
pieces={n:(s,e) for n,s,e in B.carve(obj) if n}
rom,fmap,syms=S.ctx()
st,nd,notes,ours,theirs=B.grade(obj,rom,name,*pieces[name],va,fmap[va],syms,{n:v for v,n,_ in B.tags_in(src)})
print(st,nd); B.print_diff(va,ours,theirs)
