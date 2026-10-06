#!/usr/bin/env python3
"""Stage 3 progress: how much of Indy3D.exe v1.2 still runs as original code.

A function counts as still original when our C code calls it through J3D_TRAMPOLINE_CALL (C files and headers,
comments ignored, the unused DX6 backend skipped). Sizes are v1.2 code bytes, from the distance to the next mapped
function in Scripts/indy/rti_v12.csv.

Usage: python3 -I Scripts/indy/progress.py [repo root=.]
"""
import bisect
import collections
import csv
import re
import sys
from pathlib import Path


def main():
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".")
    rows = list(csv.DictReader((root / "Scripts/indy/rti_v12.csv").open()))
    funcs = {r["name"]: int(r["v12"], 16) for r in rows if r["kind"] == "func" and r["v12"]}
    starts = sorted(funcs.values())

    def size(name):
        k = bisect.bisect_right(starts, funcs[name])
        return starts[k] - funcs[name] if k < len(starts) else 0

    original = set()
    for src in list(root.glob("Libs/**/*.[ch]")) + list(root.glob("Jones3D/**/*.[ch]")):
        if "external" in src.parts or "DX6" in src.parts or src.parent.name == "RTI":
            continue
        text = src.read_text("latin1")
        text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
        text = re.sub(r"//[^\n]*", " ", text)
        original.update(re.findall(r"J3D_TRAMPOLINE_CALL\(\s*(\w+)", text))
    original &= set(funcs)

    total = sum(size(n) for n in funcs)
    left = sum(size(n) for n in original)
    print(f"reimplemented in C: {len(funcs) - len(original)} of {len(funcs)} functions "
          f"({100 * (1 - len(original) / len(funcs)):.1f}%), {(total - left) / 1024:.0f} of {total / 1024:.0f} KiB "
          f"({100 * (1 - left / total):.1f}%)")
    print(f"still original: {len(original)} functions, {left / 1024:.0f} KiB")
    by = collections.defaultdict(lambda: [0, 0])
    for n in original:
        module = n.split("_")[0]
        by[module][0] += 1
        by[module][1] += size(n)
    for module, (count, nbytes) in sorted(by.items(), key=lambda kv: -kv[1][1]):
        print(f"  {module:22} {count:3} functions {nbytes / 1024:6.1f} KiB")


if __name__ == "__main__":
    main()
