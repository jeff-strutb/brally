"""install.py VA draft.c : replace the function body in its tree file with the draft."""
import sys; sys.path.insert(0,'tools/tgrally')
import n64t3 as T
p,n,_=T.source_of(int(sys.argv[1],16)); tree=open(p).read(); body=T.function_text(tree,n)
open(p,'w').write(tree.replace(body,open(sys.argv[2]).read().strip('\n')))
print('installed', n, 'into', p)
