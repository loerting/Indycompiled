#!/usr/bin/env python3
"""Meaning-based verification of the v1.0 -> v1.2 address map (PROJECT.md §5.7).

Position-based evidence (shifts, interpolation, parameter counts) can be fooled where LucasArts reordered code
in 1.2. This script checks what functions *do*:

1. COG verb registration: v1.2 registers every script verb with
   push "<verb>"; push <function>; push <table>; call sithCog_RegisterFunction
   and upstream's C code registers its implementation under the same verb name. The pair (verb -> address)
   from the binary must match the map's address for the C function of that verb.
2. String fingerprints: upstream's reimplementation keeps the original log, error and assert strings. A string
   used by exactly one function in v1.2 and by exactly one function in upstream's C code ties those two
   together. If the map puts that C function elsewhere, the mapping is wrong.

Results: game/review/semantic.md (report). With --apply, COG mismatches are written to
Scripts/indy/rti_v12_reviewed.csv as fixes, and string- or COG-confirmed entries as confirmed (reviewer=semantic).

Usage: python3 -I verify_semantic.py <repo root> <Indy3D.exe v1.2> [--apply]
"""
import bisect
import collections
import csv
import re
import runpy
import struct
import sys
from pathlib import Path

STRING_LITERAL = re.compile(r'"((?:\\.|[^"\\])*)"')


def c_unescape(s):
    return (s.replace(r"\n", "\n").replace(r"\t", "\t").replace(r"\"", '"').replace(r"\\", "\\"))


def c_functions(root):
    """Function name -> body text (comments removed, strings kept) for upstream's C code."""
    out = {}
    for src in list(root.glob("Libs/**/*.c")) + list(root.glob("Jones3D/**/*.c")):
        if "external" in src.parts:
            continue
        text = src.read_text("latin1")
        text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
        text = re.sub(r"//[^\n]*", " ", text)
        for m in re.finditer(r"^[A-Za-z_][^\n;{}]*?\b(\w+)\s*\([^;{}]*\)\s*\n\{(.*?)^\}", text, re.M | re.S):
            out[m.group(1)] = m.group(2)
    return out


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    root, exe_path, apply = Path(sys.argv[1]), sys.argv[2], "--apply" in sys.argv
    remap = runpy.run_path(str(root / "Scripts/indy/rti_remap.py"))
    secs = remap["read_pe"](exe_path)
    exe = remap["Exe"](secs)
    rows = {r["name"]: r for r in csv.DictReader((root / "Scripts/indy/rti_v12.csv").open())}
    funcs = {n: int(r["v12"], 16) for n, r in rows.items() if r["kind"] == "func" and r["v12"]}
    by_addr = collections.defaultdict(list)
    for n, a in funcs.items():
        by_addr[a].append(n)
    starts = sorted(set(exe.starts) | set(funcs.values()))

    def body_range(a):
        k = bisect.bisect_right(starts, a)
        return a, (starts[k] if k < len(starts) else exe.hi)

    def cstring(va):
        b = exe.read(va, 256)
        s = b.split(b"\0")[0]
        if len(s) >= 6 and all(32 <= c < 127 or c in (9, 10) for c in s):
            return s.decode("latin1")
        return None

    # --- 1. COG verb registration ---------------------------------------------------------------------------
    cbodies = c_functions(root)
    reg_c = {}  # verb -> C function
    for fn, body in cbodies.items():
        for m in re.finditer(r'(?:sithCog_RegisterFunction|sithCogFunction\w*_Register\w*)\s*\(\s*\w+\s*,\s*(\w+)\s*,\s*"([^"]+)"', body):
            reg_c[m.group(2)] = m.group(1)
    reg_addr = funcs.get("sithCog_RegisterFunction")
    reg_bin = {}  # verb -> v1.2 address
    for site, tgt in exe.sites:
        if tgt != reg_addr:
            continue
        o = site - exe.base
        window = exe.code[max(0, o - 32):o]
        imms = [struct.unpack_from("<I", window, i + 1)[0] for i in range(len(window) - 4) if window[i] == 0x68]
        if len(imms) >= 2:
            func_ptr, name_ptr = imms[-1], imms[-2]
            verb = cstring(name_ptr)
            if verb and exe.lo <= func_ptr < exe.hi:
                reg_bin[verb] = func_ptr
    cog = {}  # C function -> (verdict, v1.2 address from registration)
    for verb, fn in reg_c.items():
        if verb in reg_bin and fn in rows:
            cog[fn] = ("cog-match" if funcs.get(fn) == reg_bin[verb] else "cog-MISMATCH", reg_bin[verb], verb)

    # --- 2. string fingerprints ------------------------------------------------------------------------------
    bin_strings = collections.defaultdict(set)  # string -> v1.2 function starts using it
    for a in sorted(set(funcs.values())):
        lo, hi = body_range(a)
        for i in range(lo - exe.base, max(lo - exe.base, hi - exe.base - 3)):
            v = struct.unpack_from("<I", exe.code, i)[0]
            if exe.drange[0] <= v < exe.drange[1]:
                s = cstring(v)
                if s:
                    bin_strings[s].add(a)
    c_strings = collections.defaultdict(set)  # string -> C functions using it
    for fn, body in cbodies.items():
        if fn not in rows:
            continue
        for lit in STRING_LITERAL.findall(body):
            s = c_unescape(lit)
            if len(s) >= 6:
                c_strings[s].add(fn)
    support, contra = collections.defaultdict(list), collections.defaultdict(list)
    for s, fns in c_strings.items():
        if len(fns) != 1 or len(bin_strings.get(s, ())) != 1:
            continue
        fn, (a,) = next(iter(fns)), tuple(bin_strings[s])
        if funcs.get(fn) == a:
            support[fn].append(s)
        else:
            contra[fn].append((s, a))

    # --- report ----------------------------------------------------------------------------------------------
    out = root / "game/review/semantic.md"
    out.parent.mkdir(parents=True, exist_ok=True)
    cm = collections.Counter(v[0] for v in cog.values())
    lines = ["# Semantic verification of the address map", "",
             f"COG verbs: {len(reg_bin)} registrations found in v1.2, {len(reg_c)} in upstream C; "
             f"compared {len(cog)}: " + ", ".join(f"{k} {n}" for k, n in cm.items()),
             f"String fingerprints: {len(support)} functions supported, {len(contra)} contradicted", "",
             "## COG mismatches", "", "| function | verb | map | v1.2 registration |", "|---|---|---|---|"]
    for fn, (v, a, verb) in sorted(cog.items()):
        if v != "cog-match":
            lines.append(f"| {fn} | {verb} | {funcs.get(fn, 0):#x} | {a:#x} |")
    lines += ["", "## String contradictions", "", "| function | map | string | used in v1.2 by | names there |", "|---|---|---|---|---|"]
    for fn, items in sorted(contra.items()):
        for s, a in items[:2]:
            lines.append(f"| {fn} | {funcs.get(fn, 0):#x} | `{s[:50]!r}` | {a:#x} | {', '.join(by_addr.get(a, ['?']))} |")
    out.write_text("\n".join(lines) + "\n")
    print(lines[2]); print(lines[3]); print(f"report: {out}")

    if apply:
        path = root / "Scripts/indy/rti_v12_reviewed.csv"
        rev = {r["name"]: r for r in csv.DictReader(path.open())} if path.exists() else {}
        fixed = confirmed = 0
        for fn, (v, a, verb) in cog.items():
            if rev.get(fn, {}).get("reviewer") == "manual":
                continue
            if v == "cog-MISMATCH":
                rev[fn] = dict(name=fn, v12=f"{a:#010x}", verdict="fixed", reviewer="semantic",
                               evidence=f'v1.2 registers it as COG verb "{verb}"')
                fixed += 1
            else:
                rev[fn] = dict(name=fn, v12=f"{a:#010x}", verdict="confirmed", reviewer="semantic",
                               evidence=f'v1.2 registers it as COG verb "{verb}"')
                confirmed += 1
        for fn, strs in support.items():
            if fn in cog or fn in contra or rev.get(fn, {}).get("reviewer") == "manual":
                continue
            rev[fn] = dict(name=fn, v12=f"{funcs[fn]:#010x}", verdict="confirmed", reviewer="semantic",
                           evidence=f"unique string shared with upstream: {strs[0][:40]!r}")
            confirmed += 1
        with path.open("w", newline="") as f:
            w = csv.DictWriter(f, ["name", "v12", "verdict", "reviewer", "evidence"], lineterminator="\n")
            w.writeheader()
            for n in sorted(rev):
                w.writerow(rev[n])
        print(f"applied: {fixed} COG fixes, {confirmed} semantic confirmations")


if __name__ == "__main__":
    main()
