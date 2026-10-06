#!/usr/bin/env python3
"""Automated review of address-map entries (PROJECT.md §5.7) using Ghidra's view of Indy3D.exe v1.2.

Input is game/review/<x>.tsv from ExportReview.java. Each entry is compared with upstream's expectations:
  functions: Ghidra function entry at the address, callers overlapping upstream's callers,
             decompiled parameter count vs. the RTI signature, stack purge for __stdcall
  globals:   referenced (exactly or as a field) by functions that use it in upstream's C code

Confirmed entries are merged into Scripts/indy/rti_v12_reviewed.csv (committed), which rti_remap.py applies,
so a regenerated map keeps them. Everything else is listed in game/review/<x>-review.md for manual work.

Usage: python3 -I review_map.py <repo root> <ghidra export .tsv>
"""
import collections
import csv
import re
import runpy
import sys
from pathlib import Path

STDCALL = re.compile(r"__stdcall|\bWINAPI\b|\bCALLBACK\b|\bAPIENTRY\b")


def signature_params(type_str):
    """Number of parameters in an RTI *_TYPE string such as 'int (J3DAPI*)(const char*, int)'; None if variadic."""
    depth, start = 0, None
    for i in range(len(type_str) - 1, -1, -1):  # last top-level parenthesis group = parameter list
        c = type_str[i]
        if c == ")":
            if depth == 0:
                end = i
            depth += 1
        elif c == "(":
            depth -= 1
            if depth == 0:
                start = i
                break
    args = type_str[start + 1:end].strip()
    if "..." in args:
        return None
    if args in ("", "void"):
        return 0
    depth, n = 0, 1
    for c in args:
        depth += c in "(<"
        depth -= c in ")>"
        n += c == "," and depth == 0
    return n


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    root, tsv = Path(sys.argv[1]), Path(sys.argv[2])
    remap = runpy.run_path(str(root / "Scripts/indy/rti_remap.py"))
    syms = remap["read_symbols"](root)
    bodies = remap["read_source"](root, syms)

    types = {}
    for sym in list(root.glob("Libs/**/RTI/symbols.h")) + list(root.glob("Jones3D/RTI/symbols.h")):
        for name, t in re.findall(r"^#define\s+(\w+)_TYPE\s+(.+)$", sym.read_text("latin1"), re.M):
            types[name] = t.strip()
    callers, users = collections.defaultdict(set), collections.defaultdict(set)
    for fn, (calls, globs, implemented, _) in bodies.items():
        if implemented:
            for c in calls:
                callers[c].add(fn)
            for g in globs:
                users[g].add(fn)

    def module(sym):
        return sym.split("_", 1)[0]

    results = []
    for row in csv.DictReader(tsv.open(), delimiter="\t"):
        name, kind = row["name"], row["kind"]
        refs = [r for r in row["refs"].split(",") if r]
        if kind == "func":
            exp_params = signature_params(types[name]) if name in types else None
            stdcall = bool(STDCALL.search(types.get(name, "")))
            got_params = int(row["params"]) if row["params"] else None
            purge = int(row["purge"]) if row["purge"] else None
            overlap = sorted(set(refs) & callers[name])
            sig_ok = exp_params is not None and got_params == exp_params and \
                (purge == 4 * exp_params if stdcall else purge in (0, None))
            if row["entry"] != "yes":
                verdict, why = "manual", f"not a function entry (inside {row['containing'] or 'nothing'})"
            elif overlap:
                verdict, why = "confirmed", f"called by {', '.join(overlap[:3])}"
            elif sig_ok:
                verdict, why = "confirmed", f"signature matches ({exp_params} params{', stdcall' if stdcall else ''})"
            elif (not stdcall and exp_params and got_params is not None and 1 <= got_params <= exp_params
                  and any(module(r) == module(name) for r in refs)):
                # cdecl callbacks often ignore trailing parameters; a same-module caller/registrar anchors it
                same = sorted(r for r in refs if module(r) == module(name))
                verdict, why = "confirmed", f"same-module refs ({', '.join(same[:2])}), {got_params}/{exp_params} params used"
            else:
                verdict = "manual"
                why = (f"params {got_params} vs expected {exp_params}, purge {purge}"
                       f"{' (stdcall)' if stdcall else ''}; callers here: {', '.join(refs[:4]) or 'none'}; "
                       f"upstream callers: {', '.join(sorted(callers[name])[:4]) or 'none'}")
        else:
            exact = {r for r in refs if not r.startswith("+")}
            field = {r.split(":", 1)[1] for r in refs if r.startswith("+")}
            hit_exact, hit_field = sorted(exact & users[name]), sorted(field & users[name])
            same_module = sorted(r for r in exact if module(r) == module(name))
            if hit_exact:
                verdict, why = "confirmed", f"used by {', '.join(hit_exact[:3])}"
            elif hit_field:
                verdict, why = "confirmed", f"field used by {', '.join(hit_field[:3])}"
            elif same_module:
                verdict, why = "confirmed", f"exact refs from own module ({', '.join(same_module[:2])})"
            else:
                verdict = "manual"
                why = (f"exact refs: {', '.join(sorted(exact)[:4]) or 'none'}; "
                       f"nearby refs: {', '.join(sorted(field)[:3]) or 'none'}; "
                       f"upstream users: {', '.join(sorted(users[name])[:4]) or 'none'}")
        results.append((name, kind, row["v12"], row["confidence"], verdict, why))

    # sandwich rule: both nearest neighbours (same kind, v1.0 order) verified with the identical shift
    # means the layout between them is unchanged, so the entry in between is right too
    mapped = [r for r in csv.DictReader((root / "Scripts/indy/rti_v12.csv").open()) if r["v12"]]
    good = {r["name"] for r in mapped if r["confidence"] == "verified"}
    good |= {r[0] for r in results if r[4] == "confirmed"}
    order = {k: sorted((int(r["v10"], 16), int(r["v12"], 16), r["name"]) for r in mapped if r["kind"] == k)
             for k in ("func", "data")}
    index = {k: {n: i for i, (_, _, n) in enumerate(v)} for k, v in order.items()}
    for i, (name, kind, v12, conf, verdict, why) in enumerate(results):
        if verdict != "manual" or name not in index[kind]:
            continue
        seq, j = order[kind], index[kind][name]
        left = next((seq[x] for x in range(j - 1, -1, -1) if seq[x][2] in good), None)
        right = next((seq[x] for x in range(j + 1, len(seq)) if seq[x][2] in good), None)
        shift = seq[j][1] - seq[j][0]
        if left and right and left[1] - left[0] == shift == right[1] - right[0]:
            results[i] = (name, kind, v12, conf, "confirmed",
                          f"between verified {left[2]} and {right[2]}, same shift {shift:+#x}")

    # merge confirmations into the committed review file (manual entries there always win)
    reviewed_path = root / "Scripts/indy/rti_v12_reviewed.csv"
    reviewed = {}
    if reviewed_path.exists():
        reviewed = {r["name"]: r for r in csv.DictReader(reviewed_path.open())}
    for name, kind, v12, conf, verdict, why in results:
        if verdict == "confirmed" and reviewed.get(name, {}).get("reviewer") != "manual":
            reviewed[name] = dict(name=name, v12=v12, verdict="confirmed", reviewer="auto", evidence=why)
    with reviewed_path.open("w", newline="") as f:
        w = csv.DictWriter(f, ["name", "v12", "verdict", "reviewer", "evidence"], lineterminator="\n")
        w.writeheader()
        for name in sorted(reviewed):
            w.writerow(reviewed[name])

    report = tsv.with_name(tsv.stem + "-review.md")
    with report.open("w") as f:
        f.write(f"# Address map review: {tsv.name}\n\n| name | kind | v1.2 | map confidence | verdict | evidence |\n|---|---|---|---|---|---|\n")
        for r in sorted(results, key=lambda r: (r[4] != "manual", r[0])):
            f.write("| " + " | ".join(r) + " |\n")
    c = collections.Counter((r[1], r[4]) for r in results)
    print("review: " + ", ".join(f"{k}/{v}: {n}" for (k, v), n in sorted(c.items())))
    print(f"confirmed entries in {reviewed_path.name}: {len(reviewed)}; report: {report}")


if __name__ == "__main__":
    main()
