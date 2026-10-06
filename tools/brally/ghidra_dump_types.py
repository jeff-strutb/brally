# Ghidra headless postScript (Jython): dump every defined data symbol's
# address, label, data type and size to CSV for the matching pipeline.
# Run: analyzeHeadless <proj> -process BRGlide.dll -noanalysis \
#        -postScript ghidra_dump_types.py <out_csv>
import sys
args = getScriptArgs()
out = open(args[0], 'w')
out.write('addr,label,type,size\n')
listing = currentProgram.getListing()
it = listing.getDefinedData(True)
n = 0
while it.hasNext():
    d = it.next()
    a = d.getAddress()
    if not a.isMemoryAddress():
        continue
    dt = d.getDataType()
    label = d.getLabel() or ''
    tn = dt.getName().replace(',', ';')
    out.write('0x%s,%s,%s,%d\n' % (a, label, tn, d.getLength()))
    n += 1
out.close()
print('dumped %d data items' % n)
