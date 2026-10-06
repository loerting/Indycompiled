#!/usr/bin/env python3
"""Print values stored in Indy3D.exe v1.2 at virtual addresses (for constants the decompiler shows as DAT_xxxxxxxx).

Shows each address as float, double, int32 and the raw bytes, plus the section; .bss addresses (no file data) say so,
and the address map row covering the address (if any) is listed.
Usage: python3 -I Scripts/indy/exe_value.py <0xaddress> [...] [--exe game/run/Resource/Indy3D.exe]
"""
import csv
import struct
import sys
from pathlib import Path


def main():
    args = sys.argv[1:]
    exe = Path("game/run/Resource/Indy3D.exe")
    if "--exe" in args:
        i = args.index("--exe")
        exe = Path(args[i + 1])
        del args[i:i + 2]
    if not args:
        sys.exit(__doc__)
    d = exe.read_bytes()
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    nsec, = struct.unpack_from("<H", d, pe + 6)
    optsize, = struct.unpack_from("<H", d, pe + 20)
    base, = struct.unpack_from("<I", d, pe + 24 + 28)
    secs = [struct.unpack_from("<8sIIII", d, pe + 24 + optsize + 40 * i) for i in range(nsec)]
    rows = []
    mapfile = Path(__file__).with_name("rti_v12.csv")
    if mapfile.exists():
        rows = [(int(r["v12"], 16), r["name"]) for r in csv.DictReader(mapfile.open()) if r["v12"]]
        rows.sort()
    for a in args:
        va = int(a, 16)
        name = next(((n.rstrip(b"\0").decode(), raw + va - base - sva, rsize, va - base - sva)
                     for n, vsize, sva, rsize, raw in secs if sva <= va - base < sva + max(vsize, rsize)), None)
        owner = [n for addr, n in rows if addr <= va][-1:] if rows else []
        owner_addr = max((addr for addr, n in rows if addr <= va), default=None)
        where = f"{owner[0]}+0x{va - owner_addr:x}" if owner and va - owner_addr < 0x10000 else "-"
        if not name:
            print(f"0x{va:08x}: not in the image")
            continue
        sec, off, rsize, rel = name
        if rel >= rsize:
            print(f"0x{va:08x}: {sec} (no file data: zero-initialised .bss)  nearest map entry below: {where}")
            continue
        b = d[off:off + 8]
        f, = struct.unpack_from("<f", b)
        dbl, = struct.unpack_from("<d", b)
        i32, = struct.unpack_from("<i", b)
        print(f"0x{va:08x}: {sec} float {f!r} double {dbl!r} int {i32} (0x{i32 & 0xffffffff:08x}) bytes {b.hex()}  nearest map entry below: {where}")


if __name__ == "__main__":
    main()
