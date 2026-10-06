#!/usr/bin/env python3
"""Check whether an Indy3D.exe has the same code/data layout as the v1.0 exe that OpenJones3D targets.

It compares the exe's direct call graph (E8 rel32 calls) and absolute data references with the
v1.0 addresses in OpenJones3D's RTI headers. It also estimates per-region address shifts, so a
shifted build (e.g. v1.2 or a localized v1.0) shows how much of it could be remapped automatically.

Usage: python3 -I exe_layout_check.py <openjones3d-checkout> <Indy3D.exe>
"""
import bisect
import collections
import hashlib
import re
import struct
import sys
from pathlib import Path

V10_SHA256 = "3fbaf8cd401b4af80967cbe42e3420fb803288b336ebbe72a9a01b6dfd661a53"


def read_pe(data):
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        sys.exit("not a PE file")
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    timestamp = struct.unpack_from("<I", data, pe + 8)[0]
    optsize = struct.unpack_from("<H", data, pe + 20)[0]
    opt = pe + 24
    image_base = struct.unpack_from("<I", data, opt + 28)[0]
    sections = []
    for i in range(nsec):
        off = opt + optsize + i * 40
        name = data[off:off + 8].rstrip(b"\0").decode("latin1")
        vsize, va, rawsize, rawptr = struct.unpack_from("<IIII", data, off + 8)
        sections.append(dict(name=name, va=image_base + va, vsize=vsize, raw=data[rawptr:rawptr + rawsize]))
    return image_base, timestamp, sections


def read_rti(root):
    funcs, data_addrs = {}, {}
    typed = set()
    for sym in root.rglob("RTI/symbols.h"):
        typed.update(re.findall(r"^#define\s+(\w+)_TYPE\b", sym.read_text("latin1"), re.M))
    for hdr in root.rglob("RTI/addresses.h"):
        for name, addr in re.findall(r"^#define\s+(\w+)_ADDR\s+(0x[0-9A-Fa-f]+)", hdr.read_text("latin1"), re.M):
            if name.startswith("EXE_"):
                continue
            (funcs if name in typed else data_addrs)[int(addr, 16)] = name
    return funcs, data_addrs


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    root, exe = Path(sys.argv[1]), Path(sys.argv[2])
    data = exe.read_bytes()
    sha = hashlib.sha256(data).hexdigest()
    image_base, timestamp, sections = read_pe(data)
    funcs, data_addrs = read_rti(root)

    print(f"file      {exe}  ({len(data):,} bytes)")
    print(f"sha256    {sha}  {'== v1.0 (OpenJones3D target)' if sha == V10_SHA256 else '!= v1.0'}")
    print(f"linked    timestamp {timestamp:#x}")
    for s in sections:
        print(f"section   {s['name']:<8} va={s['va']:#010x} vsize={s['vsize']:#08x} raw={len(s['raw']):#08x}")
    print(f"RTI       {len(funcs)} functions, {len(data_addrs)} data symbols")

    text = next(s for s in sections if s["name"] == ".text")
    code, base = text["raw"], text["va"]
    lo, hi = base, base + len(code)

    calls = collections.Counter()
    for i in range(len(code) - 5):
        if code[i] == 0xE8:
            target = base + i + 5 + struct.unpack_from("<i", code, i + 1)[0]
            if lo <= target < hi:
                calls[target] += 1
    targets = set(calls)

    # 1) Identity test: do the v1.0 function addresses line up with this exe's call targets?
    ident = sum(1 for a in funcs if a in targets)
    print(f"\ncall targets in .text: {len(targets):,}")
    print(f"identity  {ident}/{len(funcs)} RTI functions ({ident / len(funcs):.1%}) are direct call targets at their v1.0 address")

    # 2) Data references: how many v1.0 data addresses appear as 32-bit immediates in .text?
    imms = {struct.unpack_from("<I", code, i)[0] for i in range(len(code) - 3)}
    dhit = sum(1 for a in data_addrs if a in imms)
    print(f"data      {dhit}/{len(data_addrs)} RTI data addresses ({dhit / max(1, len(data_addrs)):.1%}) appear as immediates in .text")

    # 3) Local shift estimate: for each RTI function, find the shift that best maps its RTI
    #    neighbours onto call targets. "Anchored" = the function itself lands on a call target
    #    with that shift and at least 3 of its 16 nearest neighbours agree.
    fa = sorted(funcs)
    tl = sorted(targets)
    anchored, shifts = 0, collections.Counter()
    for idx, a in enumerate(fa):
        nb = fa[max(0, idx - 8):idx + 9]
        j0, j1 = bisect.bisect_left(tl, a - 0x4000), bisect.bisect_right(tl, a + 0x4000)
        best_d, best_n = None, 0
        for t in tl[j0:j1]:
            d = t - a
            n = sum(1 for x in nb if x + d in targets)
            if n > best_n:
                best_d, best_n = d, n
        if best_d is not None and best_n >= 3 and a + best_d in targets:
            anchored += 1
            shifts[best_d] += 1
    print(f"anchored  {anchored}/{len(funcs)} RTI functions ({anchored / len(funcs):.1%}) map onto call targets via a locally consistent shift")
    print("top shifts (v1.0 address -> this exe):")
    for d, n in shifts.most_common(8):
        print(f"  {d:+#08x}  {n} functions")

    # 4) Same idea for data: shift that maps neighbouring RTI data symbols onto immediates in .text.
    data_lo = min(s["va"] for s in sections if s["name"] in (".rdata", ".data"))
    data_hi = max(s["va"] + s["vsize"] for s in sections if s["name"] in (".rdata", ".data", ".data1"))
    dimm = sorted(v for v in imms if data_lo <= v < data_hi)
    dimm_set = set(dimm)
    da = sorted(data_addrs)
    danchored, dshifts = 0, collections.Counter()
    for idx, a in enumerate(da):
        nb = da[max(0, idx - 8):idx + 9]
        j0, j1 = bisect.bisect_left(dimm, a - 0x800), bisect.bisect_right(dimm, a + 0x800)
        best_d, best_n = None, 0
        for t in dimm[j0:j1]:
            d = t - a
            n = sum(1 for x in nb if x + d in dimm_set)
            if n > best_n:
                best_d, best_n = d, n
        if best_d is not None and best_n >= 3 and a + best_d in dimm_set:
            danchored += 1
            dshifts[best_d] += 1
    print(f"anchored  {danchored}/{len(da)} RTI data symbols ({danchored / max(1, len(da)):.1%}) map onto immediates via a locally consistent shift")
    print("top data shifts:")
    for d, n in dshifts.most_common(6):
        print(f"  {d:+#08x}  {n} symbols")

    top = shifts.most_common(1)
    if sha == V10_SHA256:
        verdict = "EXACT v1.0: works with upstream as is"
    elif ident / len(funcs) > 0.5 and top and top[0][0] == 0:
        verdict = "SAME LAYOUT as v1.0: upstream's addresses should work (only the hash check differs)"
    else:
        verdict = "DIFFERENT LAYOUT: upstream's addresses do not apply directly; see shifts above"
    print(f"\nverdict   {verdict}")


if __name__ == "__main__":
    main()
