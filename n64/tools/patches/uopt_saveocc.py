"""Add a per-occurrence record to the instrumented uopt's compute_save.

WHAT IT DOES: after the workbench profiles have been applied to the generated
IDO 5.3 uopt.c, inserts one CDX record per live-range occurrence at the end
of compute_save's occurrence loop (label L46f190, ido-static-recomp 9c242adc):

    [CDX] saveocc proc=P sym=S uses=U defs=D weight=W nl=N contrib=C bb=0x...

uses/defs/weight are the occurrence's counts and its block's weight; nl is
the predecessor-outside-the-web flag; contrib is what the occurrence adds to
the web's net saving after the boundary charges.  Their sum over a web is the
`totalsave` of its p1dec record.  The record only prints under CDX_LOG with
CDX_PROC selected, like the profile's own records.

    python3 n64/tools/patches/uopt_saveocc.py build/ext/instr/uopt.c
"""
import sys

ANCHOR = 'L46f190:\nt0 = MEM_U32(s1 + 28);\nf22.f[0] = f22.f[0] + f20.f[0];\n'
RECORD = ('L46f190:\n'
          'DKWB_CDX_LOG(dkwb_cdx_globalcolor_ordinal, "[CDX] saveocc proc=%d sym=%d uses=%d defs=%d weight=%d nl=%d '
          'contrib=%.6f bb=0x%x\\n", dkwb_cdx_globalcolor_ordinal, (int)MEM_U16(MEM_U32(s1 + 0) + 2), '
          '(int)MEM_U16(s0 + 16), (int)MEM_U8(s0 + 18), (int)MEM_U32(MEM_U32(s0 + 0) + 44), '
          '(int)MEM_U8(s0 + 21), (double)f20.f[0], MEM_U32(s0 + 0));\n'
          't0 = MEM_U32(s1 + 28);\nf22.f[0] = f22.f[0] + f20.f[0];\n')


def main():
    p = sys.argv[1]
    s = open(p).read()
    if 'saveocc' in s:
        print('already patched')
        return
    if s.count(ANCHOR) != 1:
        sys.exit('anchor not found exactly once: not the pinned generated uopt.c')
    s = s.replace(ANCHOR, RECORD)
    open(p, 'w').write(s)
    print('saveocc record added')


if __name__ == '__main__':
    main()
