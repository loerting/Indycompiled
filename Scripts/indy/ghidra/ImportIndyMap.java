// Applies Indycompiled's v1.0 -> v1.2 address map (Scripts/indy/rti_v12.csv, PROJECT.md §5.7) to Indy3D.exe v1.2:
// names every mapped function and global, and bookmarks what still needs review.
//   "Indy: review CRITICAL"  not verified, and used at runtime (trampoline call or live global)
//   "Indy: review"           not verified
//   "Indy: changed in 1.2"   verified, but the function's size differs from v1.0 (possible 1.2 fix)
// Usage (headless): -postScript ImportIndyMap.java <path to rti_v12.csv>
//@category Indycompiled
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.BookmarkManager;
import ghidra.program.model.listing.BookmarkType;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;
import java.io.File;
import java.nio.file.Files;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class ImportIndyMap extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        File csv = args.length > 0 ? new File(args[0]) : askFile("Indycompiled address map (rti_v12.csv)", "Import");
        List<String> lines = Files.readAllLines(csv.toPath());
        Map<String, Integer> col = new HashMap<>();
        String[] header = lines.get(0).split(",", -1);
        for (int i = 0; i < header.length; i++) {
            col.put(header[i], i);
        }

        BookmarkManager bookmarks = currentProgram.getBookmarkManager();
        for (String category : new String[] {"Indy: review CRITICAL", "Indy: review"}) {
            bookmarks.removeBookmarks(BookmarkType.WARNING, category, monitor);   // re-apply from scratch
        }
        bookmarks.removeBookmarks(BookmarkType.NOTE, "Indy: changed in 1.2", monitor);
        int named = 0, created = 0, skipped = 0, failed = 0, critical = 0, review = 0, changed = 0;
        for (String line : lines.subList(1, lines.size())) {
            String[] f = line.split(",", -1);
            String name = f[col.get("name")], kind = f[col.get("kind")], v12 = f[col.get("v12")];
            String conf = f[col.get("confidence")], method = f[col.get("method")];
            String delta = f[col.get("size_delta")], usage = f[col.get("usage")];
            if (v12.isEmpty()) {
                skipped++;
                continue;
            }
            Address addr = toAddr(Long.decode(v12));
            try {
                if (kind.equals("func")) {
                    Function fn = getFunctionAt(addr);
                    if (fn == null) {
                        fn = createFunction(addr, name);
                        if (fn != null) {
                            created++;
                        }
                    }
                    if (fn != null) {
                        fn.setName(name, SourceType.IMPORTED);
                    } else {
                        createLabel(addr, name, true, SourceType.IMPORTED);
                    }
                } else {
                    createLabel(addr, name, true, SourceType.IMPORTED);
                }
                named++;
            } catch (Exception e) {
                failed++;
                printerr("could not name " + name + " at " + v12 + ": " + e.getMessage());
            }

            boolean runtimeCritical = usage.contains("trampoline") || usage.contains("live");
            String info = conf + " via " + method + (delta.isEmpty() ? "" : ", size delta " + delta)
                + (usage.isEmpty() ? "" : ", usage " + usage);
            if (!conf.equals("verified")) {
                bookmarks.setBookmark(addr, BookmarkType.WARNING,
                    runtimeCritical ? "Indy: review CRITICAL" : "Indy: review", name + ": " + info);
                if (runtimeCritical) {
                    critical++;
                } else {
                    review++;
                }
            } else if (!delta.isEmpty()) {
                bookmarks.setBookmark(addr, BookmarkType.NOTE, "Indy: changed in 1.2", name + ": " + info);
                changed++;
            }
        }
        println(String.format("ImportIndyMap: named %d (created %d functions), unmapped %d, failed %d; "
            + "bookmarks: %d critical review, %d review, %d changed in 1.2", named, created, skipped, failed,
            critical, review, changed));
    }
}
