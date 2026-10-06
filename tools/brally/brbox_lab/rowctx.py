import sys, struct, os
sys.path.insert(0,'tools')
import t3, reloc_fill, pe
from capstone import *
md=Cs(CS_ARCH_X86,CS_MODE_32)
orig=pe.load('reference/brally/orig/BRGlide.dll'); img=pe.load('build/brally/win32/brbox/image/BRGlide.T3.dll')
rows=t3.report_rows()
def show(va, off):
    r=rows['0x%08x'%va]
    code=img.read(va, int(r['orig_size'])); base=va
    if code[0]==0xE9:
        base=va+5+struct.unpack('<i',code[1:5])[0]; code=img.read(base,0x4000)
    ins=[i for i in md.disasm(code,base)]
    for i in ins:
        if i.address-base<=off<i.address-base+i.size:
            print('  T3  +%X %s %s'%(i.address-base,i.mnemonic,i.op_str)); break
def origfind(va, val):
    r=rows['0x%08x'%va]; code=orig.read(va,int(r['orig_size']))
    hits=[i for i in md.disasm(code,va) if ('0x%x'%val) in i.op_str]
    for i in hits[:4]: print('  ORIG +%X %s %s'%(i.address-va,i.mnemonic,i.op_str))
for spec in sys.argv[1:]:
    va,off,val=[int(x,16) for x in spec.split(':')]
    print('%08X +%X row %08X'%(va,off,val)); show(va,off); origfind(va,val)
