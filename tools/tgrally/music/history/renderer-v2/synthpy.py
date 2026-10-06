import ctypes,numpy as np,os
_lib=ctypes.CDLL(os.path.join(os.path.join(os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..')), 'build', 'tgrally', 'music'),'libsynth.dylib'))
class Voice(ctypes.Structure):
    _fields_=[('unison',ctypes.c_int),('detune_cents',ctypes.c_float),('width',ctypes.c_float),('saw',ctypes.c_float),('pulse',ctypes.c_float),
              ('pulse_width',ctypes.c_float),('sub',ctypes.c_float),('noise',ctypes.c_float),('cutoff_hz',ctypes.c_float),('env_oct',ctypes.c_float),
              ('key_track',ctypes.c_float),('reso',ctypes.c_float),('drive',ctypes.c_float),('fa',ctypes.c_float),('fd',ctypes.c_float),('fs',ctypes.c_float),
              ('fr',ctypes.c_float),('aa',ctypes.c_float),('ad',ctypes.c_float),('as_',ctypes.c_float),('ar',ctypes.c_float),('rate',ctypes.c_float),('seed',ctypes.c_uint32)]
_lib.render_voice.argtypes=[ctypes.POINTER(Voice),ctypes.c_int,ctypes.POINTER(ctypes.c_float),ctypes.c_float,ctypes.POINTER(ctypes.c_float),ctypes.POINTER(ctypes.c_float)]
def render(params,freq,gate_s,rate=48000,seed=1):
    v=Voice(**{k:params.get(k,0) for k,_ in Voice._fields_ if k not in ('rate','seed')},rate=rate,seed=seed)
    f=np.ascontiguousarray(freq,dtype=np.float32); n=len(f); L=np.zeros(n,np.float32); R=np.zeros(n,np.float32)
    _lib.render_voice(ctypes.byref(v),n,f.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),gate_s,L.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),R.ctypes.data_as(ctypes.POINTER(ctypes.c_float)))
    return np.stack([L,R],1)
