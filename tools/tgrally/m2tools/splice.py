import sys; sys.path.insert(0,'tools/tgrally')
import n64t3 as T
va=int(sys.argv[1],16); p,n,_=T.source_of(va); src=open(p).read()
out=src.replace(T.function_text(src,n),open(sys.argv[2]).read().strip('\n'))
open(sys.argv[3],'w').write(out)
