#!/usr/bin/env python3
"""Compare two world snapshots (INDY_DUMP_WORLD output) with a float tolerance and name the differing fields.

Snapshots list every thing template (T<n>) and thing (O<n>) as words: hex values, or tokens for pointers (what they
point to). Simulation A/B runs (INDY_DUMP_WORLD_FRAME) may differ in the last bits of floats when float code was
compiled differently, so 32-bit words that are both finite floats count as equal within a relative tolerance.
Field names come from the compiler's record layout of SithThing (clang -fdump-record-layouts, cached in Build/).

Usage: python3 -I Scripts/indy/compare_world.py <orig> <ours> [--tol 1e-4] [--max 40]
Exit code 0: equal (within tolerance), 1: different.
"""
import json
import math
import re
import shlex
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "Build/mingw-dx9-release"


def thing_layout():
    """offset -> innermost field path of SithThing (from clang's record layout dump), cached."""
    cache = BUILD / "indy_thing_layout.json"
    types_h = ROOT / "Libs/sith/types.h"
    if cache.exists() and cache.stat().st_mtime > types_h.stat().st_mtime:
        return {int(k): v for k, v in json.loads(cache.read_text()).items()}
    entry = next(e for e in json.loads((BUILD / "compile_commands.json").read_text())
                 if e["file"].endswith("Libs/sith/World/sithThing.c"))
    args = shlex.split(entry["command"])
    keep, skip = [], False
    for a in args[1:]:
        if skip:
            skip = False
            continue
        if a in ("-o", "-c", "-MF", "-MT"):
            skip = True
            continue
        if a.endswith(".c") or a.startswith("-M"):
            continue
        keep.append(a)
    probe = "#include <sith/types.h>\nSithThing indy_layout_probe;\n"
    out = subprocess.run([args[0], *keep, "-fsyntax-only", "-Xclang", "-fdump-record-layouts", "-x", "c", "-"],
                         input=probe, capture_output=True, text=True, cwd=entry["directory"]).stdout
    block = out[out.index("| struct sSithThing\n") - 20:]
    block = block[:block.index("[sizeof=")]
    layout, stack = {}, []  # stack: (name, offset, is union)
    for line in block.splitlines():
        m = re.match(r"\s*(\d+)(?::\d+-\d+)? \|( *)(\S.*?)\s*$", line)
        if not m:
            continue
        off, depth, decl = int(m.group(1)), len(m.group(2)) // 2, m.group(3)
        name = "" if "(anonymous" in decl else decl.split()[-1]
        del stack[depth:]
        stack.append((name, off, decl.startswith("union ")))
        if depth < 1 or off % 4:
            continue
        parts = []
        for i, (n, o, is_union) in enumerate(stack[1:], 1):
            if n:
                parts.append(n)
            if n and is_union and i < len(stack) - 1:  # inside a named union: the union and the offset in it
                parts[-1] += f"+0x{off - o:x}"
                break
        layout[off] = ".".join(parts)  # the innermost field at this offset wins
    cache.write_text(json.dumps(layout))
    return layout


def load(path):
    records = {}
    for line in open(path, encoding="latin1"):
        if line[:1] in "TO" and line[1:2].isdigit():
            key, _, rest = line.partition(" ")
            records[key] = tokenize(rest)
    return records


def tokenize(text):
    """Split a record into top-level words; {B..: ...} blocks stay one token."""
    tokens, depth, cur = [], 0, []
    for part in text.split():
        cur.append(part)
        depth += part.count("{") - part.count("}")
        if depth == 0:
            tokens.append(" ".join(cur))
            cur = []
    return tokens


def as_float(token):
    if re.fullmatch(r"[0-9a-f]{8}", token):
        f = struct.unpack("<f", bytes.fromhex(token)[::-1])[0]
        if math.isfinite(f):
            return f
    return None


def main():
    args = sys.argv[1:]
    tol = float(args[args.index("--tol") + 1]) if "--tol" in args else 1e-4
    limit = int(args[args.index("--max") + 1]) if "--max" in args else 40
    files = [a for i, a in enumerate(args) if not a.startswith("--") and (i == 0 or not args[i - 1].startswith("--"))]
    a, b = load(files[0]), load(files[1])
    layout = thing_layout()
    exact = close = 0
    diffs = []
    for key in sorted(set(a) | set(b), key=lambda k: (k[0], int(k[1:]))):
        ta, tb = a.get(key), b.get(key)
        if ta is None or tb is None:
            diffs.append((key, -1, "record missing", str(ta is not None), str(tb is not None)))
            continue
        for i, (x, y) in enumerate(zip(ta, tb)):
            if x == y:
                exact += 1
                continue
            fx, fy = as_float(x), as_float(y)
            if fx is not None and fy is not None and abs(fx - fy) <= tol * max(1.0, abs(fx), abs(fy)):
                close += 1
                continue
            diffs.append((key, i * 4, layout.get(i * 4, "?"), x, y))
        if len(ta) != len(tb):
            diffs.append((key, -1, "length", str(len(ta)), str(len(tb))))
    print(f"{len(a)} records; words equal {exact}, within tolerance {close}, different {len(diffs)}")
    fields = {}
    for key, off, field, x, y in diffs:
        fields.setdefault(field, []).append(key)
    for field, keys in sorted(fields.items(), key=lambda kv: -len(kv[1]))[:20]:
        print(f"  {field}: {len(keys)} ({', '.join(keys[:6])}{' ...' if len(keys) > 6 else ''})")
    for key, off, field, x, y in diffs[:limit]:
        fx, fy = as_float(x), as_float(y)
        extra = f"  ({fx:g} vs {fy:g})" if fx is not None and fy is not None else ""
        print(f"  {key} +0x{off:x} {field}: {x[:60]} | {y[:60]}{extra}")
    sys.exit(1 if diffs else 0)


if __name__ == "__main__":
    main()
