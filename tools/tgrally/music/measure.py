import numpy as np,sys
SR=48000
NAMES={3:'piano chord Am',4:'piano chord G',5:'synth bass',6:'dist chord',7:'dist low Bb',8:'dist low Bb 2',9:'closed hat',10:'mobypiano',11:'snare',12:'guitar90 lead',16:'wack10',17:'kick',18:'open hat',19:'guru chord min',20:'guru chord maj'}
PITCHED={3,4,5,6,7,8,10,12,19,20}
def frames(x,n): m=len(x)//n; return x[:m*n].reshape(m,n)
def rmsdb(x): return 10*np.log10(np.mean(x**2)+1e-20)
def logspec(seg,lo=40,hi=4000,cents=5):
    X=np.abs(np.fft.rfft(seg*np.hanning(len(seg)),1<<15)); f=np.fft.rfftfreq(1<<15,1/SR)
    g=lo*2**(np.arange(0,1200*np.log2(hi/lo),cents)/1200); return np.log(np.interp(g,f,X)+1e-6)
def pitch_offset(a,b,win=0.2):
    n=int(win*SR); A=frames(a,n); B=frames(b,n); ea=np.sqrt((A**2).mean(1)); eb=np.sqrt((B**2).mean(1))
    act=(ea>ea.max()*0.05)&(eb>eb.max()*0.05); offs=[]
    for i in np.nonzero(act)[0]:
        la=logspec(A[i]); lb=logspec(B[i]); la-=la.mean(); lb-=lb.mean()
        best=max(range(-30,31),key=lambda s:np.dot(la[30:-30],np.roll(lb,-s)[30:-30]))
        offs.append(best*5)
    return np.array(offs)
def bands(x):
    X=np.abs(np.fft.rfft(x))**2; f=np.fft.rfftfreq(len(x),1/SR); out=[]
    for c in [63,125,250,500,1000,2000,4000,8000]:
        s=(f>=c/np.sqrt(2))&(f<c*np.sqrt(2)); out.append(10*np.log10(X[s].sum()+1e-20))
    return np.array(out)
def decay(a,b):
    n=int(0.01*SR); ea=np.sqrt((frames(a,n)**2).mean(1)); eb=np.sqrt((frames(b,n)**2).mean(1))
    on=[i for i in range(2,len(ea)-80) if ea[i]>ea[i-2]*2.5 and ea[i]>ea.max()*0.1]
    r=[]
    for i in on:
        wa=[ea[i:i+10].mean(),ea[i+10:i+30].mean(),ea[i+30:i+80].mean()]; wb=[eb[i:i+10].mean(),eb[i+10:i+30].mean(),eb[i+30:i+80].mean()]
        r.append([20*np.log10((wb[k]/wb[0]+1e-9)/(wa[k]/wa[0]+1e-9)) for k in (1,2)])
    return np.median(np.array(r),0) if r else (np.nan,np.nan)
tot_o=None
print('%-16s %8s %9s %11s | %-47s | %s'%('instrument','level dB','pitch c','pitch IQR','band diff dB 63 125 250 500 1k 2k 4k 8k','decay dB @0.1-0.3s,0.3-0.8s'))
for i in sorted(NAMES):
    a=np.load('stems/orig_%02d.npy'%i).astype(float); b=np.load('stems/rem_%02d.npy'%i).astype(float)
    L=min(len(a),len(b)); a,b=a[:L],b[:L]
    lv=rmsdb(b)-rmsdb(a)
    if i in PITCHED:
        po=pitch_offset(a,b); ps='%+6.0f'%np.median(po) if len(po) else '   n/a'; iq='%3.0f..%3.0f'%(np.percentile(po,25),np.percentile(po,75)) if len(po) else ''
    else: ps,iq='  -',''
    bd=(bands(b)-rmsdb(b)*0)-(bands(a))-lv
    d=decay(a,b)
    print('%-16s %+8.1f %9s %11s | %s | %+5.1f %+5.1f'%(NAMES[i],lv,ps,iq,' '.join('%+4.0f'%v for v in bd),d[0],d[1]))
