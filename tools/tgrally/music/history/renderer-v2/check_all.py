import numpy as np,soundfile as sf,sys
from analyze_mix import tuning_cents
SR=48000; TARGET={31:2.0,63:6.0,125:5.0,250:2.0,500:0.5,1000:0.0,2000:-2.0,4000:-4.5,8000:-7.5,16000:-13.0}
def octs(x):
    X=np.abs(np.fft.rfft(x.mean(1)))**2; f=np.fft.rfftfreq(len(x),1/SR)
    v={c:10*np.log10(X[(f>=c/np.sqrt(2))&(f<c*np.sqrt(2))].sum()+1e-20) for c in TARGET}; return {c:v[c]-v[1000] for c in v}
def chroma(x,hop=0.5):
    n=int(hop*SR); out=[]
    for i in range(0,len(x)-n,n):
        X=np.abs(np.fft.rfft(x[i:i+n]*np.hanning(n))); f=np.fft.rfftfreq(n,1/SR); sel=(f>50)&(f<2000)
        m=np.round(12*np.log2(f[sel]/440)).astype(int)%12; c=np.bincount(m,weights=X[sel]**2,minlength=12); out.append(c/(c.sum()+1e-12))
    return np.array(out)
for p in sys.argv[1:]:
    o,_=sf.read('pieces/%s.xm.wav'%p); m,_=sf.read('pieces/%s_mix.wav'%p); L=min(len(o),len(m))
    r=np.array([np.corrcoef(a,b)[0,1] for a,b in zip(chroma(o[:L].mean(1)),chroma(m[:L].mean(1)))]); r=r[np.isfinite(r)]
    t=tuning_cents(m); oc=octs(m); dev=max(abs(oc[c]-TARGET[c]) for c in TARGET)
    S=(m[:,0]-m[:,1])/2; M=(m[:,0]+m[:,1])/2
    lufs=None
    print('%-10s len %5.1fs/%5.1fs | tuning %+3.0fc spread %2.0f | notes vs original %.2f | tonal max dev %.1f dB | side/mid %.2f | peak %.2f'%(p,len(m)/SR,len(o)/SR,t[0],t[1],np.median(r),dev,np.mean(S**2)/np.mean(M**2),np.abs(m).max()))
