// Exports what Ghidra sees at each address of the Indycompiled address map, so Scripts/indy/review_map.py can
// compare it with upstream's expectations (PROJECT.md §5.7):
//   functions: is it a function entry, decompiled parameter count, stack purge (ret N)
//   data:      which functions reference the address (exactly, or a field within 0x100 bytes)
// Usage (headless): -postScript ExportReview.java <rti_v12.csv> <out.tsv> [all|critical]
//@category Indycompiled
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.pcode.HighFunction;
import ghidra.program.model.symbol.Reference;
import java.io.File;
import java.io.PrintWriter;
import java.nio.file.Files;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeSet;

public class ExportReview extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        List<String> lines = Files.readAllLines(new File(args[0]).toPath());
        boolean all = args.length > 2 && args[2].equals("all");
        Map<String, Integer> col = new HashMap<>();
        String[] header = lines.get(0).split(",", -1);
        for (int i = 0; i < header.length; i++) {
            col.put(header[i], i);
        }
        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);

        int n = 0;
        try (PrintWriter out = new PrintWriter(new File(args[1]))) {
            out.println("name\tkind\tv12\tconfidence\tentry\tcontaining\tparams\tpurge\trefs");
            for (String line : lines.subList(1, lines.size())) {
                String[] f = line.split(",", -1);
                String name = f[col.get("name")], kind = f[col.get("kind")], v12 = f[col.get("v12")];
                String conf = f[col.get("confidence")], usage = f[col.get("usage")];
                boolean critical = usage.contains("trampoline") || usage.contains("live");
                if (v12.isEmpty() || (!all && (!critical || conf.equals("verified")))) {
                    continue;
                }
                Address addr = toAddr(Long.decode(v12));
                String entry = "", containing = "", params = "", purge = "";
                TreeSet<String> refs = new TreeSet<>();
                if (kind.equals("func")) {
                    Function fn = getFunctionAt(addr);
                    Function inside = getFunctionContaining(addr);
                    entry = fn != null ? "yes" : "no";
                    containing = inside != null ? inside.getName() : "";
                    if (fn != null) {
                        purge = Integer.toString(fn.getStackPurgeSize());
                        DecompileResults res = decomp.decompileFunction(fn, 30, monitor);
                        HighFunction hf = res.getHighFunction();
                        if (hf != null) {
                            params = Integer.toString(hf.getFunctionPrototype().getNumParams());
                        }
                        for (Reference r : getReferencesTo(addr)) {
                            Function from = getFunctionContaining(r.getFromAddress());
                            if (from != null) {
                                refs.add(from.getName());
                            }
                        }
                    }
                } else {
                    for (int off = 0; off < 0x100; off++) {
                        for (Reference r : getReferencesTo(addr.add(off))) {
                            Function from = getFunctionContaining(r.getFromAddress());
                            if (from != null) {
                                refs.add((off == 0 ? "" : "+" + Integer.toHexString(off) + ":") + from.getName());
                            }
                        }
                    }
                }
                out.println(String.join("\t", name, kind, v12, conf, entry, containing, params, purge,
                    String.join(",", refs)));
                n++;
            }
        }
        decomp.dispose();
        println("ExportReview: wrote " + n + " entries to " + args[1]);
    }
}
