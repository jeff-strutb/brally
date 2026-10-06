import os
import numpy as np,json,glob,wave,sys
sys.argv=['x']
import strict
def mirror(x,rt):
    x=x-x.mean(); X=np.abs(np.fft.rfft(x*np.hanning(len(x)))); n=len(X); c=int(n/rt)
    if 2*c>n: w=n-c
    else: w=c
    w=min(w,c)
    above=X[c:c+w]; below=X[c-w:c][::-1]          # mirror about the old limit
    a=np.log(above+1e-9); b=np.log(below+1e-9)
    corr=np.corrcoef(a,b)[0,1]
    lvl=20*np.log10(above.mean()/below.mean())
    return corr,lvl
def ref(fn):
    w=wave.open(os.path.join(os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..')), 'reference/tgrally/XM/')+fn); b=w.readframes(w.getnframes())
    return (np.frombuffer(b,'<i2').astype(float) if w.getsampwidth()==2 else np.frombuffer(b,np.uint8).astype(float)-128)
man=json.load(open(os.path.join(os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..')), 'reference/tgrally/XM/manifest.json')))
for e in man:
    if e['length_ratio']>1.05: print('in use  %-36s mirror-corr %.2f  above/below %5.1f dB'%((e['file'],)+mirror(ref(e['file']),e['length_ratio'])))
U={(x['rom'],x['path'],x['j']):x for f in glob.glob('upcheck_*.json') for x in json.load(open(f))}
for rom,p,j in [('0xebc00#6','Impulsetracker/Phoenix (VE)/nameless.it',13),('0xebc00#7','Impulsetracker/Badliz/theme for crom3.it',12),('0xebc00#13','Screamtracker 3/Lord Mystic/stars.s3m',12)]:
    x=U[(rom,p,j)]; D=strict.load(p,j,x['len'])
    print('cand    %-36s mirror-corr %.2f  above/below %5.1f dB'%((p[-36:],)+mirror(D['pcm'],x['ratio'])))
