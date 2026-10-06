import sys,os,subprocess,numpy as np,soundfile as sf
from remaster import parse,assemble
def solo(m,keep):
    head,inst,tail=parse(m); rebuilt={}
    for i,(hdr,smp) in enumerate(inst):
        if i+1==keep or not smp: continue
        rebuilt[i]=hdr+b''.join(s for s,d in smp)+b''.join(b'\0'*len(d) for s,d in smp)
    return assemble(head,inst,tail,rebuilt)
def render(m,path):
    open(path,'wb').write(m); subprocess.run(['openmpt123','--quiet','--render','--output-type','wav','--samplerate','48000','--float','--force',path],check=True)
    x,sr=sf.read(path+'.wav'); os.remove(path+'.wav'); os.remove(path); return x.mean(axis=1) if x.ndim==2 else x
if __name__=='__main__':
    tag,src=sys.argv[1],sys.argv[2]; ins=[int(v) for v in sys.argv[3].split(',')]
    m=open(src,'rb').read(); os.makedirs('stems',exist_ok=True)
    for i in ins: np.save('stems/%s_%02d.npy'%(tag,i),render(solo(m,i),'stems/tmp_%s_%d.xm'%(tag,i)).astype(np.float32))
