import sys,os,json,struct,zipfile,io,numpy as np,modsamp,stload,wave
from scancore import hits_for
SKIP={'info','txt','doc','mid','c','h','library','rexx','guide','readme','lyr','smus','html','htm','exe','dms','lzx','pp','diz','nfo','asm','s','o','a','bat','ini','cfg','pcx','gif','jpg','ilbm','lbm','bmp','font','catalog','prefs','lst','dat','bin','dll'}
def aiff(d):
    if d[:4]!=b'FORM' or d[8:12] not in (b'AIFF',b'AIFC'): return None
    p=12; ch=1; bits=8; data=None
    while p+8<=len(d):
        cid=d[p:p+4]; ln=struct.unpack('>I',d[p+4:p+8])[0]; c=d[p+8:p+8+ln]
        if cid==b'COMM': ch,_,bits=struct.unpack('>hIh',c[:8])
        if cid==b'SSND': off=struct.unpack('>I',c[:4])[0]; data=c[8+off:]
        p+=8+ln+(ln&1)
    if data is None: return None
    if bits<=8: a=np.frombuffer(data,np.int8).astype(np.int32); b=8
    else: a=np.frombuffer(data[:len(data)//2*2],'>i2').astype(np.int32); b=16
    return [modsamp.mk('aiff',a[::max(ch,1)],b,c5=8363)]
def wav(d):
    try: w=wave.open(io.BytesIO(d))
    except Exception: return None
    ch,sw=w.getnchannels(),w.getsampwidth(); f=w.readframes(w.getnframes())
    if sw==1: a=np.frombuffer(f,np.uint8).astype(np.int32)-128; b=8
    elif sw==2: a=np.frombuffer(f[:len(f)//2*2],'<i2').astype(np.int32); b=16
    else: return None
    return [modsamp.mk('wav',a[::ch],b,c5=w.getframerate())]
def voc(d):
    if not d.startswith(b'Creative Voice File'): return None
    p=struct.unpack('<H',d[20:22])[0]; out=bytearray()
    while p<len(d) and d[p]!=0:
        t=d[p]; ln=int.from_bytes(d[p+1:p+4],'little'); c=d[p+4:p+4+ln]
        if t==1: out+=c[2:]
        elif t==2: out+=c
        p+=4+ln
    return [modsamp.mk('voc',np.frombuffer(bytes(out),np.uint8).astype(np.int32)-128,8,c5=8363)] if out else None
def au(d):
    if d[:4]!=b'.snd': return None
    off,size,enc,rate,ch=struct.unpack('>IIIII',d[4:24]); x=d[off:]
    if enc==2: return [modsamp.mk('au',np.frombuffer(x,np.int8).astype(np.int32)[::ch],8,c5=rate)]
    if enc==3: return [modsamp.mk('au',np.frombuffer(x[:len(x)//2*2],'>i2').astype(np.int32)[::ch],16,c5=rate)]
    return None
def samples_of(name,d):
    for f in (aiff,wav,voc,au):
        r=f(d)
        if r: return r
    if d[:4]==b'FORM' and d[8:12]==b'8SVX':
        return [st_from_bytes(d)]
    if d[:17]==b'Extended Module: ' or d[44:48]==b'SCRM' or d[:4]==b'IMPM' or d[1080:1084] in (b'M.K.',b'M!K!',b'FLT4',b'4CHN',b'6CHN',b'8CHN',b'FLT8'):
        try: return modsamp.load(d)
        except Exception: return None
    if len(d)<64 or len(d)>4_000_000: return None
    if sum(32<=b<127 or b in (9,10,13) for b in d[:512])>0.9*min(512,len(d)): return None   # text
    return [modsamp.mk('raw',np.frombuffer(d,np.int8).astype(np.int16),8,c5=8363)]
def st_from_bytes(d):
    p=12; rate=8363; body=b''
    while p+8<=len(d):
        cid=d[p:p+4]; ln=struct.unpack('>I',d[p+4:p+8])[0]; c=d[p+8:p+8+ln]
        if cid==b'VHDR': rate=struct.unpack('>H',c[12:14])[0]
        if cid==b'BODY': body=c
        p+=8+ln+(ln&1)
    return modsamp.mk('8svx',np.frombuffer(body,np.int8).astype(np.int16),8,c5=rate)
def items(path):
    ext=path.rsplit('.',1)[-1].lower() if '.' in os.path.basename(path) else ''
    if ext in SKIP: return
    d=open(path,'rb').read()
    if ext=='zip':
        try:
            z=zipfile.ZipFile(io.BytesIO(d))
            for n in z.namelist():
                if not n.endswith('/'): yield path+'!'+n, z.read(n)
        except Exception: pass
        return
    if ext=='lha' or ext=='lzh':
        try:
            import lhafile,tempfile
            z=lhafile.Lhafile(path)
            for i in z.infolist():
                try: yield path+'!'+i.filename, z.read(i.filename)
                except Exception: pass
        except Exception: pass
        return
    yield path,d
if __name__=='__main__':
    out=open(sys.argv[2],'a')
    for path in [l.rstrip('\n') for l in open(sys.argv[1]) if l.strip()]:
        try:
            for name,d in items(path):
                ss=samples_of(name,d)
                if not ss: continue
                ss=[s for s in ss if s['len']>=64]
                if not ss: continue
                h=hits_for(ss)
                if h: out.write(json.dumps({'path':name,'n':len(ss),'hits':h})+'\n'); out.flush()
        except Exception as e:
            out.write(json.dumps({'path':path,'err':str(e)[:100]})+'\n')
