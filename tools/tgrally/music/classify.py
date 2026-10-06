import numpy as np,collections,json,sys
import modsamp
from remaster import parse
from render_remaster import load_events
from transcribe import transcribe,name
from scipy.signal import find_peaks
def inst_samples(m):
    head,inst,tail=parse(m); out={}; flat=modsamp.xm(m); k=0
    for i,(hdr,smp) in enumerate(inst):
        n=len(smp); out[i+1]=list(range(k,k+n)); k+=n
    return out,flat
def feats(s):
    x=s['pcm']; sr=s['c5']; n=len(x)
    X=np.abs(np.fft.rfft(x*np.hanning(n)))+1e-9; f=np.fft.rfftfreq(n,1/sr)
    flat=np.exp(np.mean(np.log(X)))/np.mean(X); cen=(X*f).sum()/X.sum(); low=(X[f<150]**2).sum()/(X**2).sum()
    hop=int(sr*0.01); w=int(sr*0.03); fr=[np.abs(np.fft.rfft(x[i:i+w]*np.hanning(w))) for i in range(0,max(1,n-w),hop)]
    flux=np.array([np.maximum(fr[i]-fr[i-1],0).sum() for i in range(1,len(fr))]) if len(fr)>2 else np.zeros(1)
    on,_=find_peaks(flux,height=flux.max()*0.35,distance=6) if flux.max()>0 else ([],None)
    env=np.sqrt(np.convolve(x**2,np.ones(max(1,int(sr*0.01)))/max(1,int(sr*0.01)),'same')); pk=np.argmax(env)
    t_dec=(np.argmax(env[pk:]<env[pk]*0.1) if (env[pk:]<env[pk]*0.1).any() else n-pk)/sr
    try: c,top=transcribe(x,sr); mx=max(w for _,w in top); notes=[(name(m),round(w/mx,2)) for m,w in top if w/mx>=0.18]
    except Exception: notes=[]
    return dict(dur=round(n/sr,2),loop=s['loop'] is not None,flat=round(float(flat),2),cen=int(cen),low=round(float(low),2),onsets=len(on),attack_ms=int(pk/sr*1000),decay10=round(float(t_dec),2),notes=notes)
if __name__=='__main__':
    for p in sys.argv[1:]:
        m=open('pieces/%s.xm'%p,'rb').read(); im,flat=inst_samples(m)
        notes,total=load_events('pieces/%s.csv'%p); by=collections.defaultdict(list)
        for n in notes: by[n['instr']].append(n)
        print('=== %s'%p)
        for i in sorted(by):
            ns=by[i]; smp=collections.Counter(n['sample'] for n in ns).most_common(1)[0][0]; s=flat[im[i][smp]]
            f=feats(s); nn=[n['note'] for n in ns]; mov=sum(1 for n in ns if np.ptp([t[1] for t in n['ticks']])>0.5)
            print('ins %2d [%s] %-24s n=%4d notes %d..%d(%d) dur %.2fs mov %3d vol %.2f | %s'%(i,'#%d'%im[i][smp],s['name'].split(' / ')[-1][:24],len(ns),min(nn),max(nn),len(set(nn)),np.median([(n['end']-n['start'])/48000 for n in ns]),mov,np.median([n['ticks'][0][2] for n in ns]),json.dumps(f)))
