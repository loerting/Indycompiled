// Decompiles the named functions of Indy3D.exe v1.2 into <outdir>/<name>.c for analysis.
// Output belongs in the git-ignored game/ folder (PROJECT.md §7.4: no decompiler output in git).
// Usage (headless): -postScript DecompileFunctions.java <outdir> <name or 0xaddress> [...]
//@category Indycompiled
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Symbol;
import java.io.File;
import java.io.PrintWriter;

public class DecompileFunctions extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        File outDir = new File(args[0]);
        outDir.mkdirs();
        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);
        for (int i = 1; i < args.length; i++) {
            String key = args[i];
            Function fn = null;
            if (key.startsWith("0x")) {
                fn = getFunctionAt(toAddr(Long.decode(key)));
            } else {
                for (Symbol s : currentProgram.getSymbolTable().getSymbols(key)) {
                    fn = getFunctionAt(s.getAddress());
                    if (fn != null) {
                        break;
                    }
                }
            }
            if (fn == null) {
                printerr("not found: " + key);
                continue;
            }
            DecompileResults res = decomp.decompileFunction(fn, 60, monitor);
            String text = res.getDecompiledFunction() != null ? res.getDecompiledFunction().getC() : "// decompilation failed\n";
            try (PrintWriter w = new PrintWriter(new File(outDir, fn.getName() + ".c"))) {
                w.println("// " + fn.getName() + " @ " + fn.getEntryPoint() + ", " + fn.getBody().getNumAddresses() + " bytes (Indy3D.exe v1.2)");
                w.print(text);
            }
            println("decompiled " + fn.getName());
        }
        decomp.dispose();
    }
}
