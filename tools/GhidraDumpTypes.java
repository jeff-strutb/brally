// Dump every defined data symbol's address, label, data type and size to CSV.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.DataIterator;
import java.io.*;

public class GhidraDumpTypes extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        PrintWriter pw = new PrintWriter(new FileWriter(args[0]));
        pw.println("addr,label,type,size");
        DataIterator it = currentProgram.getListing().getDefinedData(true);
        int n = 0;
        while (it.hasNext()) {
            Data d = it.next();
            if (!d.getAddress().isMemoryAddress()) continue;
            String label = d.getLabel();
            String tn = d.getDataType().getName().replace(",", ";");
            pw.printf("0x%s,%s,%s,%d%n", d.getAddress(), label == null ? "" : label,
                      tn, d.getLength());
            n++;
        }
        pw.close();
        println("dumped " + n + " data items");
    }
}
