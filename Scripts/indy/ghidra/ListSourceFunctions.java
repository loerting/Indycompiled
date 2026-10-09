// Lists the functions of one source file in an exe without symbols (the debug build): the functions whose asserts
// reference the file's path string, and every function between the first and the last of them (the linker keeps a
// file's functions together, in source order). One line per function: entry, size, whether it references the path,
// callees, string literals used. Output belongs in the git-ignored game/ folder (PROJECT.md §7.4).
// Usage (headless): -postScript ListSourceFunctions.java <outfile> <path substring, e.g. sith\Engine\sithPhysics.c>
//@category Indycompiled
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.StringDataInstance;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;
import java.io.PrintWriter;
import java.util.Set;
import java.util.TreeSet;

public class ListSourceFunctions extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        String needle = args[1].toLowerCase();
        Set<Address> marked = new TreeSet<>();
        for (Data d : currentProgram.getListing().getDefinedData(true)) {
            if (!d.hasStringValue()) {
                continue;
            }
            String s = StringDataInstance.getStringDataInstance(d).getStringValue();
            if (s == null || !s.toLowerCase().contains(needle)) {
                continue;
            }
            for (Reference r : getReferencesTo(d.getAddress())) {
                Function f = getFunctionContaining(r.getFromAddress());
                if (f != null) {
                    marked.add(f.getEntryPoint());
                }
            }
        }
        try (PrintWriter out = new PrintWriter(args[0])) {
            if (marked.isEmpty()) {
                out.println("# no function references " + args[1]);
                return;
            }
            Address first = ((TreeSet<Address>) marked).first();
            Address last = ((TreeSet<Address>) marked).last();
            out.println("# " + args[1] + ": " + marked.size() + " functions reference the path, range " + first + "-" + last);
            for (Function f : currentProgram.getFunctionManager().getFunctions(first, true)) {
                if (f.getEntryPoint().compareTo(last) > 0) {
                    break;
                }
                StringBuilder callees = new StringBuilder();
                for (Function c : f.getCalledFunctions(monitor)) {
                    callees.append(c.getEntryPoint()).append(' ');
                }
                StringBuilder strings = new StringBuilder();
                for (Instruction ins : currentProgram.getListing().getInstructions(f.getBody(), true)) {
                    for (Reference r : ins.getReferencesFrom()) {
                        Data d = getDataAt(r.getToAddress());
                        if (d != null && d.hasStringValue()) {
                            String s = StringDataInstance.getStringDataInstance(d).getStringValue();
                            if (s != null && !s.toLowerCase().contains(needle)) {
                                strings.append('"').append(s.replace("\t", " ").replace("\n", "\\n")).append("\" ");
                            }
                        }
                    }
                }
                out.println(f.getEntryPoint() + "\t" + f.getBody().getNumAddresses() + "\t"
                    + (marked.contains(f.getEntryPoint()) ? "A" : "-") + "\t" + callees.toString().trim() + "\t"
                    + strings.toString().trim());
            }
        }
    }
}
