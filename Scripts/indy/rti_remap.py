#!/usr/bin/env python3
"""Map OpenJones3D's v1.0 addresses (RTI headers) onto Indy3D.exe v1.2.

Evidence, strongest first:
  1. local shift consensus: an RTI function and its RTI neighbours land on v1.2 call targets with one shift
  2. assert strings: a function's assert messages name its source file (drops wrong anchors)
  3. call graph: if upstream's C code of A calls B, the v1.2 body of A must call the v1.2 address of B
  4. interpolation: unmapped symbols between two mapped neighbours take a neighbour's shift,
     if that lands on a plausible function start (code) or keeps the layout consistent (data)

Data symbols are verified against upstream's C code too: a function that uses global X must reference
the v1.2 address of X (or a field of it) in its v1.2 body.

Usage: python3 -I rti_remap.py <openjones3d-checkout> <Indy3D.exe v1.2> <out.csv>
"""
import bisect
import collections
import csv
import re
import struct
import sys
from pathlib import Path

V10_TEXT = (0x00401000, 0x00505000)  # from upstream RTI (EXE_TEXT_START/END_ADDR)


# ---------------------------------------------------------------- inputs

def read_pe(path):
    data = Path(path).read_bytes()
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    optsize = struct.unpack_from("<H", data, pe + 20)[0]
    image_base = struct.unpack_from("<I", data, pe + 24 + 28)[0]
    secs = {}
    for i in range(nsec):
        off = pe + 24 + optsize + i * 40
        name = data[off:off + 8].rstrip(b"\0").decode("latin1")
        vsize, va, rawsize, rawptr = struct.unpack_from("<IIII", data, off + 8)
        secs[name] = dict(va=image_base + va, vsize=vsize, raw=data[rawptr:rawptr + rawsize])
    return secs


def read_symbols(root):
    """name -> (v1.0 address, kind, module) from every *_ADDR define in upstream headers."""
    syms = {}
    for hdr in list(root.glob("Libs/**/*.h")) + list(root.glob("Jones3D/**/*.h")):
        if "external" in hdr.parts:
            continue
        rel = hdr.relative_to(root).parts
        module = rel[1] if rel[0] == "Libs" else rel[0]
        for name, addr in re.findall(r"^#define\s+(\w+)_ADDR\s+(0x[0-9A-Fa-f]+)", hdr.read_text("latin1"), re.M):
            if name.startswith("EXE_"):
                continue
            a = int(addr, 16)
            kind = "func" if V10_TEXT[0] <= a < V10_TEXT[1] else "data"
            syms[name] = (a, kind, module)
    return syms


def read_usage(root):
    """name -> set of runtime uses: hook (patched), trampoline (called in the original exe), live (global read from the exe)."""
    usage = collections.defaultdict(set)
    pats = {"hook": r"J3D_HOOKFUNC\(\s*(\w+)", "trampoline": r"J3D_TRAMPOLINE_CALL\(\s*(\w+)",
            "live": r"J3D_DECL_FAR_(?:ARRAY)?VAR\(\s*(\w+)"}
    for src in list(root.glob("Libs/**/*.[ch]")) + list(root.glob("Jones3D/**/*.[ch]")):
        if "external" in src.parts or src.name == "j3dhook.h":
            continue
        text = src.read_text("latin1")
        for kind, pat in pats.items():
            for name in re.findall(pat, text):
                usage[name].add(kind)
    return usage


def read_source(root, names):
    """Function name -> (called RTI functions, used RTI globals, implemented?, referenced RTI functions) from upstream C code."""
    bodies = {}
    ident = re.compile(r"[A-Za-z_]\w*")
    for src in list(root.glob("Libs/**/*.c")) + list(root.glob("Jones3D/**/*.c")):
        if "external" in src.parts:
            continue
        text = src.read_text("latin1")
        text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
        text = re.sub(r"//[^\n]*", " ", text)
        text = re.sub(r'"(?:\\.|[^"\\])*"', '""', text)
        for m in re.finditer(r"^[A-Za-z_][^\n;{}]*?\b(\w+)\s*\([^;{}]*\)\s*\n\{(.*?)^\}", text, re.M | re.S):
            fname, body = m.group(1), m.group(2)
            if fname not in names:
                continue
            calls = [t for t in re.findall(r"\b(\w+)\s*\(", body) if names.get(t, (0, ""))[1] == "func"]
            called = set(calls)
            refs = {t for t in ident.findall(body) if names.get(t, (0, ""))[1] == "func" and t not in called and t != fname}
            globs = {t for t in ident.findall(body) if names.get(t, (0, ""))[1] == "data"}
            bodies[fname] = (calls, globs, "J3D_TRAMPOLINE_CALL" not in body, refs)
    return bodies


# ---------------------------------------------------------------- v1.2 analysis

class Exe:
    def __init__(self, secs):
        t = secs[".text"]
        self.secs, self.code, self.base = secs, t["raw"], t["va"]
        self.lo, self.hi = self.base, self.base + len(self.code)
        code, base = self.code, self.base
        self.sites = []  # (call site, target)
        for i in range(len(code) - 5):
            if code[i] == 0xE8:
                tgt = base + i + 5 + struct.unpack_from("<i", code, i + 1)[0]
                if self.lo <= tgt < self.hi:
                    self.sites.append((base + i, tgt))
        self.site_va = [s for s, _ in self.sites]
        self.targets = {t for _, t in self.sites}
        # function pointers: 16-aligned .text addresses used as immediates or stored in data sections
        refs = set()
        for i in range(len(code) - 3):
            v = struct.unpack_from("<I", code, i)[0]
            if self.lo <= v < self.hi and v % 16 == 0:
                refs.add(v)
        for name in (".rdata", ".data"):
            raw = secs[name]["raw"]
            for i in range(0, len(raw) - 3, 4):
                v = struct.unpack_from("<I", raw, i)[0]
                if self.lo <= v < self.hi and v % 16 == 0:
                    refs.add(v)
        self.refs = refs
        self.starts = sorted(self.targets | refs)
        dlo = min(secs[n]["va"] for n in (".rdata", ".data"))
        dhi = max(secs[n]["va"] + secs[n]["vsize"] for n in (".rdata", ".data", ".data1"))
        self.drange = (dlo, dhi)
        self.dimm_at = {}  # immediate value -> sorted offsets in .text
        for i in range(len(code) - 3):
            v = struct.unpack_from("<I", code, i)[0]
            if dlo <= v < dhi:
                self.dimm_at.setdefault(v, []).append(base + i)
        self.dimm = sorted(self.dimm_at)

    def is_boundary(self, x):
        if not (self.lo <= x < self.hi) or x % 16:
            return False
        if x in self.targets or x in self.refs:
            return True
        o = x - self.base
        prev = self.code[o - 1] if o >= 1 else 0
        return prev in (0xC3, 0xCC, 0x90) or (o >= 3 and self.code[o - 3] == 0xC2) or \
            (o >= 5 and self.code[o - 5] == 0xE9) or (o >= 2 and self.code[o - 2] == 0xEB)

    def body_end(self, start, mapped_starts):
        k = bisect.bisect_right(self.starts, start)
        e1 = self.starts[k] if k < len(self.starts) else self.hi
        k = bisect.bisect_right(mapped_starts, start)
        e2 = mapped_starts[k] if k < len(mapped_starts) else self.hi
        return min(e1, e2, start + 0x4000)

    def code_refs_in(self, start, end):
        o0, o1 = start - self.base, end - self.base
        out = set()
        for i in range(o0, max(o0, o1 - 3)):
            v = struct.unpack_from("<I", self.code, i)[0]
            if self.lo <= v < self.hi and v % 16 == 0:
                out.add(v)
        return out

    def calls_in(self, start, end):
        i, j = bisect.bisect_left(self.site_va, start), bisect.bisect_left(self.site_va, end)
        return [t for _, t in self.sites[i:j]]

    def read(self, va, n):
        for s in self.secs.values():
            if s["va"] <= va < s["va"] + len(s["raw"]):
                o = va - s["va"]
                return s["raw"][o:o + n]
        return b""

    def assert_files(self, start, end):
        files = set()
        o0, o1 = start - self.base, end - self.base
        for i in range(o0, max(o0, o1 - 3)):
            v = struct.unpack_from("<I", self.code, i)[0]
            if self.drange[0] <= v < self.drange[1]:
                s = self.read(v, 120).split(b"\0")[0]
                if re.fullmatch(rb"[A-Za-z]:\\[\x20-\x7e]+\.c", s):
                    files.add(s.decode().rsplit("\\", 1)[1][:-2].lower())
        return files


# ---------------------------------------------------------------- mapping

def consensus(addrs, hits, candidates, window):
    """For each address, the shift that maps it and >= 3 of its 16 neighbours onto `hits`.
    Collisions (several addresses on one target) keep only the best-supported address."""
    out, support = {}, {}
    for idx, a in enumerate(addrs):
        nb = addrs[max(0, idx - 8):idx + 9]
        j0, j1 = bisect.bisect_left(candidates, a - window), bisect.bisect_right(candidates, a + window)
        best_d, best_n = None, 0
        for t in candidates[j0:j1]:
            d = t - a
            n = sum(1 for x in nb if x + d in hits)
            if n > best_n:
                best_d, best_n = d, n
        if best_d is not None and best_n >= 3 and a + best_d in hits:
            out[a] = a + best_d
            support[a] = best_n
    by_target = collections.defaultdict(list)
    for a, b in out.items():
        by_target[b].append(a)
    for b, claim in by_target.items():
        if len(claim) > 1:
            claim.sort(key=lambda a: support[a], reverse=True)
            losers = claim if support[claim[0]] == support[claim[1]] else claim[1:]
            for a in losers:
                del out[a]
    return out


def increasing_subset(pairs):
    """Longest subsequence of (v10, v12) pairs, sorted by v10, whose v12 also increases (linker order)."""
    pairs = sorted(pairs)
    tails, tail_idx, prev = [], [], [None] * len(pairs)
    for i, (_, b) in enumerate(pairs):
        k = bisect.bisect_left(tails, b)
        if k == len(tails):
            tails.append(b)
            tail_idx.append(i)
        else:
            tails[k] = b
            tail_idx[k] = i
        prev[i] = tail_idx[k - 1] if k else None
    keep, i = set(), tail_idx[-1] if tail_idx else None
    while i is not None:
        keep.add(pairs[i][0])
        i = prev[i]
    return keep


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    root, out_csv = Path(sys.argv[1]), Path(sys.argv[3])
    exe = Exe(read_pe(sys.argv[2]))
    syms = read_symbols(root)
    by_addr = collections.defaultdict(list)
    for n, (a, k, m) in syms.items():
        by_addr[a].append(n)
    bodies = read_source(root, syms)
    usage = read_usage(root)
    EMPTY = ((), set(), False, set())

    fa = sorted({a for a, k, _ in syms.values() if k == "func"})
    fname = {a: by_addr[a][0] for a in fa}
    callers = collections.defaultdict(set)   # callee -> callers (implemented C bodies)
    pointers = collections.defaultdict(set)  # function -> functions that take its address
    for fn, (calls, _, implemented, refs) in bodies.items():
        if implemented:
            for c in set(calls):
                callers[syms[c][0]].add(syms[fn][0])
            for r in refs:
                pointers[syms[r][0]].add(syms[fn][0])

    # 1. consensus anchors, minus those whose assert strings name another module
    fmap = consensus(fa, exe.targets, sorted(exe.targets), 0x800)
    method = {a: "consensus" for a in fmap}
    starts = sorted(fmap.values())
    for a in list(fmap):
        b = fmap[a]
        files = exe.assert_files(b, exe.body_end(b, starts))
        if files and fname[a].split("_")[0].lower() not in files:
            del fmap[a], method[a]
    in_order = increasing_subset(fmap.items())
    for a in list(fmap):
        if a not in in_order:
            del fmap[a], method[a]

    def neighbour_shift(a):
        i = bisect.bisect_left(fa, a)
        for k in range(1, 60):
            for j in (i - k, i + k):
                if 0 <= j < len(fa) and fa[j] in fmap:
                    return fmap[fa[j]] - fa[j]
        return None

    body_cache = {}

    def body(b, starts):
        key = (b, len(starts))
        if key not in body_cache:
            e = exe.body_end(b, starts)
            body_cache[key] = (set(exe.calls_in(b, e)), exe.code_refs_in(b, e))
        return body_cache[key]

    def votes_for(a, prior, starts):
        """v1.2 candidates for function a: called by its mapped callers / referenced by mapped pointer users."""
        votes = collections.Counter()
        for src, idx in ((callers[a], 0), (pointers[a], 1)):
            for c in src:
                if c in fmap:
                    for t in body(fmap[c], starts)[idx]:
                        if abs((t - a) - prior) <= 0x800:
                            votes[t] += 1
        return votes

    # 2. caller/pointer voting (adds and corrects), 3. interpolation; repeat until stable
    for _round in range(6):
        changed = 0
        starts = sorted(fmap.values())
        used = {b: a for a, b in fmap.items()}
        for a in fa:
            prior = fmap[a] - a if a in fmap else neighbour_shift(a)
            if prior is None:
                continue
            v = votes_for(a, prior, starts)
            if not v:
                continue
            (w, nw), *rest = v.most_common(2) + [(None, 0)]
            runner = rest[0][1]
            strong = nw >= 2 and nw >= 2 * runner
            unique = nw == 1 and runner == 0 and abs((w - a) - prior) <= 0x100
            if not (strong or unique) or w == fmap.get(a):
                continue
            if w in used and used[w] != a:
                continue
            if a in fmap and v.get(fmap[a], 0) >= nw:
                continue
            if a in fmap:
                del used[fmap[a]]
            fmap[a], method[a] = w, "callers"
            used[w] = a
            changed += 1
        starts = sorted(fmap.values())
        used = set(fmap.values())
        for i, a in enumerate(fa):
            if a in fmap:
                continue
            p = next((fa[j] for j in range(i - 1, -1, -1) if fa[j] in fmap), None)
            n = next((fa[j] for j in range(i + 1, len(fa)) if fa[j] in fmap), None)
            shifts = {fmap[x] - x for x in (p, n) if x is not None}
            ok = [d for d in shifts
                  if exe.is_boundary(a + d) and a + d not in used
                  and (p is None or a + d > fmap[p]) and (n is None or a + d < fmap[n])]
            if len(ok) == 1:
                fmap[a], method[a] = a + ok[0], "interpolated"
                used.add(a + ok[0])
                changed += 1
        if not changed:
            break

    # final consistency: one function per address, linker order preserved (violators become doubtful)
    owners = collections.Counter(fmap.values())
    assert all(n == 1 for n in owners.values()), "duplicate v1.2 addresses after mapping"
    out_of_order = set(fmap) - increasing_subset(fmap.items())

    # verification per function: callers/pointer users agree, or its own callees are found
    starts = sorted(fmap.values())
    fver = {}
    edges = verified = 0
    for a, b in fmap.items():
        v = votes_for(a, b - a, starts)
        incoming = v.get(b, 0)
        calls, _, implemented, _ = bodies.get(fname[a], EMPTY)
        out_total = out_ok = 0
        if implemented and calls:
            tgts = body(b, starts)[0]
            e = [syms[c][0] for c in set(calls) if syms[c][0] in fmap]
            out_total, out_ok = len(e), sum(1 for c in e if fmap[c] in tgts)
            edges, verified = edges + out_total, verified + out_ok
        if a in out_of_order:
            fver[a] = "doubtful"    # breaks linker order: needs a look whatever else says
        elif incoming or out_ok:
            fver[a] = "verified"
        elif exe.is_boundary(b) and not (out_total >= 2) and not v:
            fver[a] = "plausible"   # no evidence either way, lands on a function boundary
        else:
            fver[a] = "doubtful"    # evidence exists but does not confirm this address

    # ---------------------------------------------------------------- data
    da = sorted({a for a, k, _ in syms.values() if k == "data"})
    dname = {a: by_addr[a][0] for a in da}
    dmap = consensus(da, set(exe.dimm), exe.dimm, 0x800)
    dmethod = {a: "consensus" for a in dmap}
    users = collections.defaultdict(list)
    for fn, (_, globs, implemented, _) in bodies.items():
        if implemented and syms[fn][0] in fmap:
            for g in globs:
                users[syms[g][0]].append(fmap[syms[fn][0]])

    def exact_refs(x, user_starts):
        """users whose v1.2 body references exactly address x"""
        n = 0
        for b in user_starts:
            e = exe.body_end(b, starts)
            if any(b <= p < e for p in exe.dimm_at.get(x, ())):
                n += 1
        return n

    def dneighbour(i, direction):
        j = i + direction
        while 0 <= j < len(da):
            if da[j] in dmap:
                return dmap[da[j]] - da[j]
            j += direction
        return None

    for i, a in enumerate(da):
        prior = dmap[a] - a if a in dmap else dneighbour(i, -1)
        if prior is None:
            prior = dneighbour(i, 1)
        if prior is None:
            continue
        cands = [prior] + [d for d in (dneighbour(i, -1), dneighbour(i, 1)) if d is not None and d != prior]
        if a in users:
            scored = [(exact_refs(a + d, users[a]), -abs(d - prior), d) for d in cands]
            best = max(scored)
            if best[0] > 0 and a + best[2] != dmap.get(a):
                dmap[a], dmethod[a] = a + best[2], "usage"
        elif a in dmap:
            l, r = dneighbour(i, -1), dneighbour(i, 1)
            if l is not None and l == r and l != prior:
                dmap[a], dmethod[a] = a + l, "smoothed"
        if a not in dmap:
            dmap[a], dmethod[a] = a + prior, "interpolated"

    dclaims = collections.defaultdict(list)
    for a in da:
        dclaims[dmap[a]].append(a)
    dcollide = {a for c in dclaims.values() if len(c) > 1 for a in c}

    dver = {}
    for i, a in enumerate(da):
        if a in dcollide:
            dver[a] = "doubtful"    # shares its v1.2 address with another global
            continue
        if a not in users:
            dver[a] = "plausible"
            continue
        size = min((da[i + 1] - a) if i + 1 < len(da) else 0x100, 0x10000)
        lo, hi = dmap[a], dmap[a] + size
        j0, j1 = bisect.bisect_left(exe.dimm, lo), bisect.bisect_left(exe.dimm, hi)
        refs = [p for v in exe.dimm[j0:j1] for p in exe.dimm_at[v]]
        hit = any(b <= p < exe.body_end(b, starts) for b in users[a] for p in refs)
        dver[a] = "verified" if hit else "doubtful"

    # ---------------------------------------------------------------- report
    nf, nd = len(fa), len(da)
    print(f"functions: {len(fmap)}/{nf} mapped ({len(fmap) / nf:.1%})  methods: " +
          ", ".join(f"{k} {v}" for k, v in collections.Counter(method.values()).most_common()))
    print("  confidence: " + ", ".join(f"{k} {v}" for k, v in collections.Counter(fver.values()).most_common()))
    print(f"  source call edges found in v1.2: {verified}/{edges} ({verified / max(1, edges):.1%})")
    print(f"data:      {len(dmap)}/{nd} mapped  methods: " +
          ", ".join(f"{k} {v}" for k, v in collections.Counter(dmethod.values()).most_common()))
    print("  confidence: " + ", ".join(f"{k} {v}" for k, v in collections.Counter(dver.values()).most_common()))

    mapped_fa = [a for a in fa if a in fmap]
    changed_funcs = [(fname[a], (fmap[b] - fmap[a]) - (b - a)) for a, b in zip(mapped_fa, mapped_fa[1:])
                     if (fmap[b] - fmap[a]) != (b - a)]
    print(f"functions whose size differs in v1.2 (or followed by unknown code): {len(changed_funcs)}")
    mapped_da = [a for a in da if a in dmap]
    breaks = [(dname[a], (dmap[b] - b) - (dmap[a] - a)) for a, b in zip(mapped_da, mapped_da[1:])
              if (dmap[b] - b) != (dmap[a] - a)]
    print(f"data layout breaks (symbol resized or data inserted after it): {len(breaks)}")
    for n, d in breaks[:15]:
        print(f"  {n:<48} {d:+#x}")
    doubt = [fname[a] for a in fa if fver.get(a) == "doubtful"]
    unmapped = [fname[a] for a in fa if a not in fmap]
    print(f"doubtful functions: {len(doubt)}  e.g. {doubt[:6]}")
    print(f"unmapped functions: {len(unmapped)}  e.g. {unmapped[:6]}")

    # reviewed entries (Scripts/indy/rti_v12_reviewed.csv): confirmed -> verified; manual fixes override the address
    reviewed_path = Path(__file__).with_name("rti_v12_reviewed.csv")
    if reviewed_path.exists():
        applied = stale = 0
        for r in csv.DictReader(reviewed_path.open()):
            if r["name"] not in syms:
                continue
            a, k, _ = syms[r["name"]]
            mp, conf, meth = (fmap, fver, method) if k == "func" else (dmap, dver, dmethod)
            if r["verdict"] == "fixed":
                mp[a], conf[a], meth[a] = int(r["v12"], 16), "verified", "manual"
                applied += 1
            elif r["verdict"] == "confirmed" and a in mp and mp[a] == int(r["v12"], 16):
                conf[a] = "verified"
                meth[a] = meth.get(a, "") + "+review"
                applied += 1
            elif r["verdict"] == "confirmed":
                stale += 1
        print(f"review file: {applied} entries applied, {stale} stale (map changed since review)")

    with out_csv.open("w", newline="") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(["name", "kind", "module", "v10", "v12", "method", "confidence", "size_delta", "usage"])
        delta = dict(changed_funcs)
        for n, (a, k, m) in sorted(syms.items(), key=lambda kv: kv[1][0]):
            if k == "func":
                b, how, conf = fmap.get(a), method.get(a, ""), fver.get(a, "unmapped")
                sd = delta.get(fname[a], 0)
            else:
                b, how, conf = dmap.get(a), dmethod.get(a, ""), dver.get(a, "unmapped")
                sd = ""
            w.writerow([n, k, m, f"{a:#010x}", f"{b:#010x}" if b else "", how, conf, f"{sd:+#x}" if sd else "",
                        "+".join(sorted(usage.get(n, ())))])
    crit = collections.Counter()
    for n, (a, k, m) in syms.items():
        u = usage.get(n, set())
        conf = (fver if k == "func" else dver).get(a, "unmapped")
        if "trampoline" in u or "live" in u:
            crit[conf] += 1
    print("runtime-critical (trampolines + live globals): " + ", ".join(f"{k} {v}" for k, v in crit.most_common()))
    print(f"written: {out_csv}")


if __name__ == "__main__":
    main()
