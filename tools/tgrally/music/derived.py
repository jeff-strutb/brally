import numpy as np,json,glob,strict,verify,wave
from scipy.signal import resample_poly
from fractions import Fraction
def analyse(rom,D,rt):
    R=verify.rom_sample(rom); r=np.asarray(R['raw']).astype(float)*256; d=np.asarray(D['raw']).astype(float)*(1 if D['bits']==16 else 256)
    n=len(d); t=np.arange(n)/rt
    best=None
    for ph in np.linspace(0,1,21):
        idx=t+ph-0.5
        for nm in ('linear','nearest'):
            if nm=='linear': y=np.interp(idx,np.arange(len(r)),r)
            else: y=r[np.clip(np.round(idx).astype(int),0,len(r)-1)]
            A=np.vstack([y,np.ones(n)]).T; (g,c),*_=np.linalg.lstsq(A,d,rcond=None); res=d-(g*y+c)
            db=10*np.log10(max((res**2).mean(),1e-9)/max(((d-d.mean())**2).mean(),1e-9))
            if best is None or db<best[0]: best=(db,nm,ph,g)
    f=Fraction(rt).limit_denominator(400); lo=resample_poly(d,f.denominator,f.numerator)[:len(r)]
    L=min(len(lo),len(r)); A=np.vstack([r[:L],np.ones(L)]).T; (g,c),*_=np.linalg.lstsq(A,lo[:L],rcond=None)
    band=10*np.log10(max(((lo[:L]-g*r[:L]-c)**2).mean(),1e-9)/max(((lo[:L]-lo[:L].mean())**2).mean(),1e-9))
    return best,band,len(np.unique(d))
C=[('0xebc00#6','Impulsetracker/Phoenix (VE)/nameless.it',13),('0xebc00#7','Fasttracker 2/Patosz/rh\'s commando.xm',40),('0xebc00#7','Impulsetracker/Pro-XeX/ne sam obiadwal.it',2),
   ('0x149c80#6','Impulsetracker/DJ AIL/the night.it',11),('0xebc00#0','Fasttracker 2/DJ Keen/toxic 99.xm',32),('0xebc00#5','Fasttracker 2/Komah/permian organism.xm',4),
   ('0xebc00#13','Screamtracker 3/Lord Mystic/stars.s3m',12),('0xebc00#1','Screamtracker 3/Terrorfinger/lavawall.s3m',1),('0x12eab0#6','Impulsetracker/Pro-XeX/ne sam obiadwal.it',2)]
U={(x['rom'],x['path'],x['j']):x for f in glob.glob('upcheck_*.json') for x in json.load(open(f))}
for rom,p,j in C:
    x=U[(rom,p,j)]; D=strict.load(p,j,x['len'])
    (db,nm,ph,g),band,nd=analyse(rom,D,x['ratio'])
    print('%-11s %2db x%.3f distinct %6d | rebuilt-from-ROM residual %6.1f dB (%s) | same-band diff %6.1f dB | %s'%(rom,D['bits'],x['ratio'],nd,db,nm,band,p))
