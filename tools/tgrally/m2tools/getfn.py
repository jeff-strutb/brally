import sys; sys.path.insert(0,'tools/tgrally')
import n64t3 as T
p,n,_=T.source_of(int(sys.argv[1],16)); print(T.function_text(open(p).read(),n))
