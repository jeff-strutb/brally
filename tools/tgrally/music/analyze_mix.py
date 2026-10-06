import numpy as np,soundfile as sf,sys
SR=48000
def tuning_cents(x):
    """Deviation of the strongest spectral peaks from the A440 equal-tempered grid, energy-weighted."""
    x=x.mean(1) if x.ndim==2 else x; n=1<<15; offs=[];w=[]
    for i in range(0,len(x)-n,n*4):
        X=np.abs(np.fft.rfft(x[i:i+n]*np.hanning(n))); f=np.fft.rfftfreq(n,1/SR)
        sel=(f>80)&(f<2000); Xs=X[sel]; fs=f[sel]
        for j in np.argsort(-Xs)[:8]:
            if 0<j<len(Xs)-1:
                if Xs[j]<=0 or not np.isfinite(Xs[j]): continue
                a,b,c=np.log(Xs[j-1]+1e-9),np.log(Xs[j]+1e-9),np.log(Xs[j+1]+1e-9); d=0.5*(a-c)/(a-2*b+c)
                fr=fs[j]+d*(fs[1]-fs[0]); cents=1200*np.log2(fr/440); offs.append((cents+50)%100-50); w.append(Xs[j])
    offs=np.array(offs); w=np.array(w); ok=np.isfinite(offs)&(w>0); offs,w=offs[ok],w[ok]
    return float(np.average(offs,weights=w)), float(np.sqrt(np.average(offs**2,weights=w)))
def bands(x):
    x=x.mean(1) if x.ndim==2 else x; X=np.abs(np.fft.rfft(x))**2; f=np.fft.rfftfreq(len(x),1/SR)
    B=[31,63,125,250,500,1000,2000,4000,8000,16000]
    v=np.array([X[(f>=c/np.sqrt(2))&(f<c*np.sqrt(2))].sum() for c in B]); return B,10*np.log10(v/v.sum())
if __name__=='__main__':
    o,_=sf.read('jungle_orig.xm.wav'); m,_=sf.read(sys.argv[1] if len(sys.argv)>1 else 'jungle_mix.wav')
    print('tuning vs A440 (cents, mean / rms spread):  original %+.0f / %.0f    remaster %+.0f / %.0f'%(*tuning_cents(o),*tuning_cents(m)))
    B,bo=bands(o); _,bm=bands(m)
    print('octave band share dB:', '  '.join('%d'%b for b in B)); print('  original ', ' '.join('%5.1f'%v for v in bo)); print('  remaster ', ' '.join('%5.1f'%v for v in bm))
    S=(m[:,0]-m[:,1])/2; M=(m[:,0]+m[:,1])/2; print('stereo: side/mid energy %.2f (original %.2f), L/R correlation %.2f'%(np.mean(S**2)/np.mean(M**2),np.mean(((o[:,0]-o[:,1])/2)**2)/np.mean(((o[:,0]+o[:,1])/2)**2),np.corrcoef(m[:,0],m[:,1])[0,1]))
