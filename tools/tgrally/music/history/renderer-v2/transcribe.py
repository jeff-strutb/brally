import numpy as np,romsamp,modsamp
from scipy.optimize import nnls
nn=['C','C#','D','D#','E','F','F#','G','G#','A','A#','B']
def name(m): r=int(round(m)); return '%s%d%+d'%(nn[r%12],r//12-1,round((m-r)*100))
def transcribe(pcm,rate,t0=0.08,t1=0.6,lo=28,hi=96,thresh=0.12):
    a,b=int(t0*rate),min(len(pcm),int(t1*rate)); seg=pcm[a:b]*np.hanning(b-a)
    N=1<<int(np.ceil(np.log2(len(seg)*8))); X=np.abs(np.fft.rfft(seg,N)); f=np.fft.rfftfreq(N,1/rate)
    band=(f>25)&(f<0.45*rate); X=X[band]; f=f[band]; X=X/X.max()
    # global tuning offset: the cents that best align spectral peaks to the equal-tempered grid
    best=None
    for c in np.arange(-50,50,2):
        cols=[];ms=[]
        for m in range(lo,hi):
            f0=440*2**((m+c/100-69)/12)
            if f0>0.45*rate: break
            col=np.zeros(len(f))
            for h in range(1,40):
                fh=f0*h
                if fh>=f[-1]: break
                col+=np.exp(-0.5*((f-fh)/(fh*0.004+1.5))**2)/h
            cols.append(col); ms.append(m)
        A=np.array(cols).T; w,res=nnls(A,X)
        if best is None or res<best[0]: best=(res,c,w,ms)
    res,c,w,ms=best
    top=[(ms[i]+c/100,w[i]) for i in np.argsort(-w) if w[i]>thresh*w.max()]
    return c,sorted(top)
if __name__=='__main__':
    m=dict(romsamp.rom_modules())['0x17fd10']; S=modsamp.xm(m)
    for k in [0,1,7,3,4,5,9,13,14,2]:
        s=S[k]; c,top=transcribe(s['pcm'],s['c5'])
        print(k,s['name'][:22],c,[(name(m),round(w,2)) for m,w in top])
