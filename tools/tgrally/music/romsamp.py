import os
import sys,glob
sys.path.insert(0,os.path.join(os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..')), 'tools', 'tgrally'))
import extract_xm as x, modsamp
def rom_modules():
    p=glob.glob(os.path.join(os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..')), 'reference/tgrally/*.z64'))[0]
    rom=x.normalise_rom(open(p,'rb').read(),p)
    return [(hex(o),m) for o,m in x.find_modules(rom)]
