import pickle,os,json,sys,numpy as np
import build2 as B, stems as ST
from concurrent.futures import ThreadPoolExecutor as ProcessPoolExecutor
SR=48000
INS=sorted(B.SMP)
BANDS=[50*2**(k/3) for k in range(25)]            # 50 Hz .. ~12.7 kHz, third-octave
def band_db(x):
    X=np.abs(np.fft.rfft(x))**2; f=np.fft.rfftfreq(len(x),1/SR)
    return np.array([10*np.log10(X[(f>=c*2**-(1/6))&(f<c*2**(1/6))].sum()+1e-20) for c in BANDS])
def rmsdb(x): return 10*np.log10(np.mean(x**2)+1e-20)
def render_stem(args):
    mod,i=args; return i,ST.render(ST.solo(mod,i),'stems/it_%d.xm'%i).astype(np.float32)
if os.path.exists('zones.pkl'): Z,info=pickle.load(open('zones.pkl','rb'))
else:
    Z,info=B.base_zones(); pickle.dump((Z,info),open('zones.pkl','wb'))
print('voicings:',json.dumps({k:v for k,v in info.items()},default=str)[:900])
O={i:np.load('stems/orig_%02d.npy'%i).astype(float) for i in INS}
Ob={i:band_db(O[i]) for i in INS}; Ol={i:rmsdb(O[i]) for i in INS}
params=json.load(open('params.json')) if os.path.exists('params.json') else {}
for it in range(int(sys.argv[1]) if len(sys.argv)>1 else 4):
    mod,hr=B.build(Z,params)
    with ProcessPoolExecutor(8) as ex: R=dict(ex.map(render_stem,[(mod,i) for i in INS]))
    lv={i:rmsdb(R[i])-Ol[i] for i in INS}; ref=np.median(list(lv.values()))
    print('iteration',it,'headroom %.3f'%hr)
    for i in INS:
        p=params.setdefault(str(i),{'gain_db':0.0,'eq_f':BANDS,'eq_db':[0.0]*len(BANDS)})
        d=band_db(R[i])-Ob[i]-lv[i]
        ob=Ob[i]; valid=ob>ob.max()-35
        fmax=max([c for c,v in zip(BANDS,valid) if v]); valid&=np.array(BANDS)<=fmax*1.01
        eq=np.array(p['eq_db'],float)
        corr=np.where(valid,-0.7*d,0.0)
        last=np.nonzero(valid)[0]
        if len(last): corr[last[-1]+1:]=corr[last[-1]]          # hold the top band's correction above it
        eq=np.clip(eq+corr,-18,18); eq=np.convolve(np.pad(eq,1,mode='edge'),[0.25,0.5,0.25],'valid')
        p['eq_db']=list(map(float,eq)); p['gain_db']=float(p['gain_db']-(lv[i]-ref))
        rms_band=np.sqrt(np.mean(d[valid]**2)) if valid.any() else 0
        print('  ins %2d level %+5.1f dB  band error rms %4.1f dB (%d bands to %.0f Hz)'%(i,lv[i]-ref,rms_band,valid.sum(),fmax))
    json.dump(params,open('params.json','w'))
mod,hr=B.build(Z,params); open('jungle_remaster2.xm','wb').write(mod); print('written, headroom %.3f'%hr)
