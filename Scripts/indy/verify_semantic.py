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

    # --- 1b. message handlers: push <handler>; push <SITHDSS id>; call sithMessage_RegisterFunction -------------
    enum, last = {}, -1
    for name, val in re.findall(r"\b(SITHDSS_\w+)\s*(?:=\s*(0x[0-9A-Fa-f]+|\d+))?\s*,", (root / "Libs/sith/Dss/sithDSS.h").read_text("latin1")):
        last = int(val, 0) if val else last + 1
        enum[name] = last
    msg_c = {}  # id -> C handler
    for fn, body in cbodies.items():
        for idname, handler in re.findall(r"sithMessage_RegisterFunction\(\s*(SITHDSS_\w+)\s*,\s*(\w+)\s*\)", body):
            if idname in enum:
                msg_c[enum[idname]] = handler
    msg_bin = {}  # id -> v1.2 handler address
    reg_msg = funcs.get("sithMessage_RegisterFunction")
    for site, tgt in exe.sites:
        if tgt != reg_msg:
            continue
        o = site - exe.base
        c = exe.code
        if c[o - 2] == 0x6A and c[o - 7] == 0x68:
            msg_bin[c[o - 1]] = struct.unpack_from("<I", c, o - 6)[0]
        elif c[o - 5] == 0x68 and c[o - 10] == 0x68:
            msg_bin[struct.unpack_from("<I", c, o - 4)[0]] = struct.unpack_from("<I", c, o - 9)[0]
    for mid, fn in msg_c.items():
        if mid in msg_bin and fn in rows and fn not in cog:
            cog[fn] = ("cog-match" if funcs.get(fn) == msg_bin[mid] else "cog-MISMATCH", msg_bin[mid], f"message id {mid}")

    # --- 1c. AI instincts: push <function>; push "<name>"; call sithAI_RegisterInstinct ------------------------
    # The registering function is still original v1.2 code; pair the instinct name with upstream's function name.
    instinct_fns = {n.split("_", 1)[1].lower(): n for n in rows if n.startswith("sithAIInstinct_") and rows[n]["kind"] == "func"}
    reg_inst = funcs.get("sithAI_RegisterInstinct")
    for site, tgt in exe.sites:
        if tgt != reg_inst:
            continue
        o = site - exe.base
        c = exe.code
        if c[o - 5] == 0x68 and c[o - 10] == 0x68:
            name = cstring(struct.unpack_from("<I", c, o - 4)[0])
            addr = struct.unpack_from("<I", c, o - 9)[0]
            fn = instinct_fns.get(re.sub(r"[^a-z0-9]", "", (name or "").lower()))
            if fn and fn not in cog:
                cog[fn] = ("cog-match" if funcs.get(fn) == addr else "cog-MISMATCH", addr, f'AI instinct "{name}"')

    # --- 1d. console commands: push <flags>; push <name or ciphered name>; push <function>; call RegisterCommand ---
    con_c = {}  # name literal (plain or the CipherText input) -> C function
    for fn, body in cbodies.items():
        for func, lit in re.findall(r'sithConsole_RegisterCommand\(\s*(\w+)\s*,\s*(?:sithCommand_CipherText\(\s*)?"([^"]*)"', body):
            con_c[c_unescape(lit)] = func
    reg_con = funcs.get("sithConsole_RegisterCommand")

    def any_string(va):  # console names can be very short ("mem"), so no length filter here
        b = exe.read(va, 128).split(b"\0")[0]
        return b.decode("latin1") if b and all(32 <= ch < 127 for ch in b) else None

    con_bin = {}
    for site, tgt in exe.sites:
        if tgt != reg_con:
            continue
        o = site - exe.base
        c = exe.code
        if c[o - 5] != 0x68:
            continue
        func_ptr = struct.unpack_from("<I", c, o - 4)[0]
        name = None
        if c[o - 10] == 0x68:                      # push "<name>"; push <function>; call
            name = any_string(struct.unpack_from("<I", c, o - 9)[0])
        elif c[o - 6] == 0x50:                     # push eax (name from sithCommand_CipherText); push <function>
            window = c[max(0, o - 40):o - 6]
            pushes = [struct.unpack_from("<I", window, i + 1)[0] for i in range(len(window) - 4) if window[i] == 0x68]
            strs = [any_string(v) for v in pushes]
            strs = [x for x in strs if x]
            name = strs[-1] if strs else None
        if name and exe.lo <= func_ptr < exe.hi:
            con_bin[name] = func_ptr
    for lit, fn in con_c.items():
        if lit in con_bin and fn in rows and fn not in cog:
            cog[fn] = ("cog-match" if funcs.get(fn) == con_bin[lit] else "cog-MISMATCH", con_bin[lit], f"console command {lit!r}")

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

    # --- 3. contradiction detectors (flag only) --------------------------------------------------------------
    # 3a. cdecl argument count from caller stack cleanup ("add esp, 4*N" right after "call F") vs the RTI signature
    types = {}
    for sym in list(root.glob("Libs/**/RTI/symbols.h")) + list(root.glob("Jones3D/RTI/symbols.h")):
        for name, t in re.findall(r"^#define\s+(\w+)_TYPE\s+(.+)$", sym.read_text("latin1"), re.M):
            types[name] = t.strip()
    review_mod = runpy.run_path(str(root / "Scripts/indy/review_map.py"))
    cleanup = collections.defaultdict(collections.Counter)
    for site, tgt in exe.sites:
        o = site - exe.base + 5
        if exe.code[o] == 0x83 and exe.code[o + 1] == 0xC4:
            cleanup[tgt][exe.code[o + 2] // 4] += 1
        elif exe.code[o] == 0x59:          # pop ecx: one argument
            cleanup[tgt][1] += 1
    argcount_bad = {}
    for fn, a in funcs.items():
        t = types.get(fn, "")
        if not t or "__stdcall" in t or "WINAPI" in t or "CALLBACK" in t:
            continue
        try:
            exp = review_mod["signature_params"](t) if "(" in t else None
        except TypeError:
            exp = None
        votes = cleanup.get(a)
        if exp is None or not votes:
            continue
        n, cnt = votes.most_common(1)[0]
        if cnt >= 2 and n != exp and sum(votes.values()) == cnt:
            argcount_bad[fn] = (exp, n, cnt)

    # 3b. live globals: declared float must be accessed by x87 float instructions, other types by integer ones
    decl = {}
    for src in list(root.glob("Libs/**/*.[ch]")) + list(root.glob("Jones3D/**/*.[ch]")):
        if "external" in src.parts:
            continue
        for name, typ in re.findall(r"J3D_DECL_FAR_VAR\(\s*(\w+)\s*,\s*([^)]+?)\s*\)", src.read_text("latin1")):
            decl[name] = typ
    fpu_ops = {(0xD9, 0x05), (0xD9, 0x1D), (0xD9, 0x15), (0xD8, 0x05), (0xD8, 0x0D), (0xD8, 0x1D), (0xD8, 0x25),
               (0xD8, 0x35), (0xD8, 0x15), (0xD8, 0x2D), (0xD8, 0x3D)}
    access_bad = {}
    for name, typ in decl.items():
        r = rows.get(name)
        if not r or not r["v12"] or typ.strip() not in ("float", "int", "uint32_t", "int32_t", "size_t", "unsigned int"):
            continue
        a = int(r["v12"], 16)
        fpu = integer = 0
        for p_ in exe.dimm_at.get(a, ()):
            i = p_ - exe.base
            pair = (exe.code[i - 2], exe.code[i - 1]) if i >= 2 else None
            if pair in fpu_ops:
                fpu += 1
            else:
                integer += 1
        if typ.strip() == "float" and integer and not fpu:
            access_bad[name] = f"float, but {integer} integer-style accesses and no x87 accesses"
        elif typ.strip() != "float" and fpu and not integer:
            access_bad[name] = f"{typ}, but only x87 float accesses ({fpu})"

    # --- report ----------------------------------------------------------------------------------------------
    out = root / "game/review/semantic.md"
    out.parent.mkdir(parents=True, exist_ok=True)
    cm = collections.Counter(v[0] for v in cog.values())
    lines = ["# Semantic verification of the address map", "",
             f"Registrations (COG verbs: {len(reg_bin)} in v1.2 / {len(reg_c)} in C; message handlers: {len(msg_bin)} / {len(msg_c)}; "
             f"AI instincts) compared {len(cog)}: " + ", ".join(f"{k} {n}" for k, n in cm.items()),
             f"String fingerprints: {len(support)} functions supported, {len(contra)} contradicted", "",
             "## Registration mismatches", "", "| function | registered as | map | v1.2 registration |", "|---|---|---|---|"]
    for fn, (v, a, verb) in sorted(cog.items()):
        if v != "cog-match":
            lines.append(f"| {fn} | {verb} | {funcs.get(fn, 0):#x} | {a:#x} |")
    lines += ["", "## String contradictions", "", "| function | map | string | used in v1.2 by | names there |", "|---|---|---|---|---|"]
    for fn, items in sorted(contra.items()):
        for s, a in items[:2]:
            lines.append(f"| {fn} | {funcs.get(fn, 0):#x} | `{s[:50]!r}` | {a:#x} | {', '.join(by_addr.get(a, ['?']))} |")
    lines += ["", "## Argument-count contradictions (cdecl caller cleanup vs signature)", "",
              "| function | v1.2 | expected args | callers clean | sites |", "|---|---|---|---|---|"]
    for fn, (exp, n, cnt) in sorted(argcount_bad.items()):
        lines.append(f"| {fn} | {funcs[fn]:#x} | {exp} | {n} | {cnt} |")
    lines += ["", "## Live-global access contradictions", "", "| global | v1.2 | problem |", "|---|---|---|"]
    for name, why in sorted(access_bad.items()):
        lines.append(f"| {name} | {rows[name]['v12']} | {why} |")
    out.write_text("\n".join(lines) + "\n")
    print(f"argument-count contradictions: {len(argcount_bad)}, live-global access contradictions: {len(access_bad)}")
    print(lines[2]); print(lines[3]); print(f"report: {out}")

    if apply:
        path = root / "Scripts/indy/rti_v12_reviewed.csv"
        rev = {r["name"]: r for r in csv.DictReader(path.open())} if path.exists() else {}
        fixed = confirmed = 0
        string_owner = {}  # v1.2 address -> C function, from unique string fingerprints
        for s_, fns in c_strings.items():
            if len(fns) == 1 and len(bin_strings.get(s_, ())) == 1:
                string_owner[next(iter(bin_strings[s_]))] = next(iter(fns))
        for fn, (v, a, verb) in cog.items():
            if rev.get(fn, {}).get("reviewer") == "manual":
                continue
            if string_owner.get(a, fn) != fn:
                print(f"NOT applied (contradicts string fingerprint of {string_owner[a]}): {fn} -> {a:#x} ({verb})")
                continue
            # registrations are authoritative: always record the registered address as a fix, so it overrides
            # whatever the position-based mapper computes (also on later runs, when the map already agrees)
            what = verb if verb.startswith(("message", "AI", "console")) else "COG verb " + repr(verb)
            rev[fn] = dict(name=fn, v12=f"{a:#010x}", verdict="fixed", reviewer="semantic",
                           evidence=f"v1.2 registers it as {what}")
            if v == "cog-MISMATCH":
                fixed += 1
            else:
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
