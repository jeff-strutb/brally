// Ghidra headless script: decompile functions at known addresses, export C.
// Run via: analyzeHeadless ... -postScript GhidraExport.java <output_dir> <csv_path>
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.decompiler.DecompiledFunction;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSpace;
import ghidra.util.task.ConsoleTaskMonitor;
import java.io.*;
import java.util.*;

public class GhidraExport extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            println("Usage: -postScript GhidraExport.java <output_dir> <csv_path>");
            return;
        }
        String outDir = args[0];
        String csvPath = args[1];

        new File(outDir).mkdirs();

        // Read function addresses from CSV
        List<long[]> targets = new ArrayList<>();
        List<String> names = new ArrayList<>();
        BufferedReader br = new BufferedReader(new FileReader(csvPath));
        String line = br.readLine(); // skip header
        while ((line = br.readLine()) != null) {
            String[] parts = line.split(",", -1);
            long va = Long.parseLong(parts[0].replace("0x", "").replace("0X", ""), 16);
            int size = Integer.parseInt(parts[1]);
            String name = parts.length > 2 ? parts[2].trim() : "";
            targets.add(new long[]{va, size});
            names.add(name);
        }
        br.close();
        println("Ghidra export: " + targets.size() + " functions to decompile");

        // Set up decompiler
        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);

        FunctionManager fm = currentProgram.getFunctionManager();
        AddressSpace space = currentProgram.getAddressFactory().getDefaultAddressSpace();

        int success = 0;
        int failed = 0;

        for (int i = 0; i < targets.size(); i++) {
            long va = targets.get(i)[0];
            int size = (int)targets.get(i)[1];
            String name = names.get(i);

            Address addr = space.getAddress(va);
            Function func = fm.getFunctionContaining(addr);

            if (func == null) {
                CreateFunctionCmd cmd = new CreateFunctionCmd(addr);
                cmd.applyTo(currentProgram);
                func = fm.getFunctionContaining(addr);
                if (func == null) {
                    failed++;
                    continue;
                }
            }

            DecompileResults result = decomp.decompileFunction(func, 30, monitor);
            if (result == null || !result.decompileCompleted()) {
                failed++;
                continue;
            }

            DecompiledFunction cf = result.getDecompiledFunction();
            if (cf == null) {
                failed++;
                continue;
            }

            String cBody = cf.getC();
            String fname = (name != null && !name.isEmpty()) ? name : String.format("FUN_%08x", va);
            String outPath = outDir + String.format("/0x%08X.c", va);

            PrintWriter pw = new PrintWriter(new FileWriter(outPath));
            pw.printf("/* Ghidra decompilation of %s at 0x%08X (%d bytes) */\n", fname, va, size);
            pw.print(cBody);
            pw.close();

            success++;
            if (success % 100 == 0) {
                println("  " + success + "/" + targets.size() + " done");
            }
        }

        decomp.dispose();
        println("Ghidra export complete: " + success + " success, " + failed + " failed");
    }
}
