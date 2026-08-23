#!/usr/bin/env python3
# Ghidra headless script: decompile functions at known addresses, export C.
# Run via: analyzeHeadless ... -postScript ghidra_export.py <output_dir> <csv_path>
#
# Ghidra Jython (Python 2.7) script — no f-strings, no pathlib.
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
import csv
import os
import sys

args = getScriptArgs()
if len(args) < 2:
    print("Usage: -postScript ghidra_export.py <output_dir> <functions_csv>")
    sys.exit(1)

out_dir = args[0]
csv_path = args[1]

if not os.path.exists(out_dir):
    os.makedirs(out_dir)

# Read function addresses from CSV (va,size,name)
targets = []
with open(csv_path) as f:
    reader = csv.reader(f)
    header = next(reader)
    for row in reader:
        va = int(row[0], 16)
        size = int(row[1])
        name = row[2].strip() if len(row) > 2 else ''
        targets.append((va, size, name))

print("Ghidra export: %d functions to decompile" % len(targets))

# Set up decompiler
decomp = DecompInterface()
decomp.openProgram(currentProgram)
monitor = ConsoleTaskMonitor()

listing = currentProgram.getListing()
fm = currentProgram.getFunctionManager()
space = currentProgram.getAddressFactory().getDefaultAddressSpace()

success = 0
failed = 0
skipped = 0

for va, size, name in targets:
    addr = space.getAddress(va)
    func = fm.getFunctionContaining(addr)

    if func is None:
        # Create function at this address if Ghidra didn't auto-detect it
        from ghidra.app.cmd.function import CreateFunctionCmd
        cmd = CreateFunctionCmd(addr)
        cmd.applyTo(currentProgram)
        func = fm.getFunctionContaining(addr)
        if func is None:
            failed += 1
            continue

    result = decomp.decompileFunction(func, 30, monitor)
    if result is None or not result.decompileCompleted():
        failed += 1
        continue

    c_code = result.getDecompiledFunction()
    if c_code is None:
        failed += 1
        continue

    sig = c_code.getSignature()
    body = c_code.getC()

    fname = name if name else ("FUN_%08x" % va)
    out_path = os.path.join(out_dir, "0x%08X.c" % va)
    with open(out_path, 'w') as f:
        f.write("/* Ghidra decompilation of %s at 0x%08X (%d bytes) */\n" % (fname, va, size))
        f.write(body)

    success += 1
    if success % 100 == 0:
        print("  %d/%d done" % (success, len(targets)))

print("Ghidra export complete: %d success, %d failed, %d skipped" % (success, failed, skipped))
