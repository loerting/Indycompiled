#!/usr/bin/env python3
"""Cross-reference check of weakly evidenced hooks (PROJECT.md §5.7).

For each function in game/review/weak_reachable_hooks.md (or the names given), compare upstream's C body with the
v1.2 body at the mapped address:
  globals: every exe global the C code uses must be referenced (exactly or as a field) by the v1.2 body
  callees: every RTI function the C code calls must be a call or tail-call target of the v1.2 body
Callees that upstream declares `static inline` are skipped (the original compiler inlined them too).

Verdicts: CONFIRMED (something matched, nothing missing), CONTRADICTED (something missing: inlining, an upstream
addition or a wrong address; check by hand), no-evidence (the C body uses no mapped global or callee).
With --apply, CONFIRMED entries are merged into Scripts/indy/rti_v12_reviewed.csv (reviewer "xref"), which
rti_remap.py rates as strong evidence. Entries reviewed by hand ("manual", "semantic") are never overwritten.

Usage: python3 -I verify_xref.py <repo root> <Indy3D.exe v1.2> [--apply] [function names...]
"""
import bisect
import csv
import re
import runpy
import struct
import sys
from pathlib import Path


def main():
    args = [a for a in sys.argv[1:] if a != "--apply"]
    apply = "--apply" in sys.argv[1:]
    if len(args) < 2:
        sys.exit(__doc__)
    root, exe_path, names = Path(args[0]), args[1], args[2:]

    rr = runpy.run_path(str(root / "Scripts/indy/rti_remap.py"), run_name="rti_remap")
    exe = rr["Exe"](rr["read_pe"](exe_path))
    rows = {r["name"]: r for r in csv.DictReader((root / "Scripts/indy/rti_v12.csv").open())}
    bodies = rr["read_source"](root, {n: (int(r["v10"], 16), r["kind"]) for n, r in rows.items()})

    inline = set()
    for hdr in list(root.glob("Libs/**/*.h")) + list(root.glob("Jones3D/**/*.h")):
        inline.update(re.findall(r"static\s+(?:inline|__inline|J3D_INLINE)\b[^;(]*?\b(\w+)\s*\(", hdr.read_text("latin1")))

    fstarts = sorted(int(r["v12"], 16) for r in rows.values() if r["kind"] == "func" and r["v12"])
    gstarts = sorted(int(r["v12"], 16) for r in rows.values() if r["kind"] == "data" and r["v12"])

    def body(a):
        k = bisect.bisect_right(fstarts, a)
        end = min(fstarts[k] if k < len(fstarts) else exe.hi, a + 0x4000)
        return exe.code[a - exe.base:end - exe.base]

    def global_end(g):
        k = bisect.bisect_right(gstarts, g)
        return gstarts[k] if k < len(gstarts) else g + 0x10000

    if not names:
        review = root / "game/review/weak_reachable_hooks.md"
        names = [line.split("|")[1].strip() for line in review.open()
                 if line.startswith("| ") and not line.startswith("| function")]

    confirmed = {}
    for n in names:
        r = rows[n]
        if not r["v12"]:
            print(f"{'unmapped':13} {n}")
            continue
        a = int(r["v12"], 16)
        b = body(a)
        imms = {struct.unpack_from("<I", b, i)[0] for i in range(len(b) - 3)}
        targets = {a + i + 5 + struct.unpack_from("<i", b, i + 1)[0] for i in range(len(b) - 4) if b[i] in (0xE8, 0xE9)}
        calls, globs, _, _ = bodies.get(n, ([], set(), None, set()))
        g_ok, g_bad, c_ok, c_bad = [], [], [], []
        for g in sorted(globs):
            if rows[g]["v12"]:
                gv = int(rows[g]["v12"], 16)
                (g_ok if any(gv <= x < global_end(gv) for x in imms) else g_bad).append(g)
        for c in sorted(set(calls) - inline - {n}):
            if rows[c]["v12"]:
                (c_ok if int(rows[c]["v12"], 16) in targets else c_bad).append(c)
        if g_bad or c_bad:
            verdict = "CONTRADICTED"
        elif g_ok or c_ok:
            verdict = "CONFIRMED"
            confirmed[n] = (r["v12"], "references " + ", ".join(g_ok + c_ok))
        else:
            verdict = "no-evidence"
        print(f"{verdict:13} {n} @{r['v12']} globals ok={g_ok} missing={g_bad} calls ok={c_ok} missing={c_bad}")

    print(f"verify_xref: {len(names)} checked, {len(confirmed)} confirmed")
    if apply and confirmed:
        path = root / "Scripts/indy/rti_v12_reviewed.csv"
        reviewed = list(csv.DictReader(path.open()))
        by_name = {r["name"]: r for r in reviewed}
        changed = 0
        for n, (v12, evidence) in confirmed.items():
            old = by_name.get(n)
            if old and old["reviewer"] in ("manual", "semantic"):
                continue
            new = {"name": n, "v12": v12, "verdict": "confirmed", "reviewer": "xref", "evidence": evidence}
            if old:
                old.update(new)
            else:
                reviewed.append(new)
            changed += 1
        reviewed.sort(key=lambda r: r["name"])
        with path.open("w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=["name", "v12", "verdict", "reviewer", "evidence"], lineterminator="\n")
            w.writeheader()
            w.writerows(reviewed)
        print(f"verify_xref: {changed} entries merged into {path}")


if __name__ == "__main__":
    main()
