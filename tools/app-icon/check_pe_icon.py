#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""CI check: a Windows binary (exe / VST3 dll) carries Ohpe's own icon as a resource.

    python3 tools/app-icon/check_pe_icon.py resources/icon/Ohpe.ico <binary> [<binary> ...]

Reads the PE resource directory (no Windows API, runs anywhere), collects the RT_ICON images and
requires an RT_GROUP_ICON plus every image of the .ico byte for byte - so it fails on the framework's
default icon as well as on no icon at all.
"""
import struct, sys

RT_ICON, RT_GROUP_ICON = 3, 14


def ico_images(path):
    d = open(path, "rb").read()
    _, kind, n = struct.unpack_from("<HHH", d, 0)
    assert kind == 1, "not an .ico"
    out = []
    for i in range(n):
        w, h, _, _, _, _, size, off = struct.unpack_from("<BBBBHHII", d, 6 + 16 * i)
        out.append(((w or 256), d[off:off + size]))
    return out


def pe_resources(path):
    d = open(path, "rb").read()
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    assert d[pe:pe + 4] == b"PE\0\0", "not a PE file"
    nsec, = struct.unpack_from("<H", d, pe + 6)
    optsize, = struct.unpack_from("<H", d, pe + 20)
    opt = pe + 24
    magic, = struct.unpack_from("<H", d, opt)
    dirs = opt + (112 if magic == 0x20B else 96)
    rsrc_rva, rsrc_size = struct.unpack_from("<II", d, dirs + 8 * 2)
    if rsrc_rva == 0:
        return {}
    secs = []
    for i in range(nsec):
        s = opt + optsize + 40 * i
        vsize, va, rawsize, raw = struct.unpack_from("<IIII", d, s + 8)
        secs.append((va, max(vsize, rawsize), raw))

    def off(rva):
        for va, size, raw in secs:
            if va <= rva < va + size:
                return raw + rva - va
        raise ValueError("rva outside sections")

    base = off(rsrc_rva)

    def entries(o):
        _, _, _, _, nn, ni = struct.unpack_from("<IIHHHH", d, base + o)
        for i in range(nn + ni):
            name, ptr = struct.unpack_from("<II", d, base + o + 16 + 8 * i)
            yield name, ptr

    res = {}
    for tname, tptr in entries(0):
        if not tptr & 0x80000000:
            continue
        blobs = []
        for _, nptr in entries(tptr & 0x7FFFFFFF):
            stack = [nptr]
            while stack:
                p = stack.pop()
                if p & 0x80000000:
                    stack += [q for _, q in entries(p & 0x7FFFFFFF)]
                else:
                    rva, size = struct.unpack_from("<II", d, base + p)
                    blobs.append(d[off(rva):off(rva) + size])
        res[tname] = blobs
    return res


def main():
    ico, bins = sys.argv[1], sys.argv[2:]
    want = ico_images(ico)
    ok = True
    for b in bins:
        res = pe_resources(b)
        icons = set(res.get(RT_ICON, []))
        groups = res.get(RT_GROUP_ICON, [])
        missing = [w for w, data in want if data not in icons]
        good = bool(groups) and not missing
        ok &= good
        print(f"{'ok  ' if good else 'FAIL'} {b}: {len(groups)} icon group(s), {len(icons)} icon image(s)"
              + (f", missing sizes {missing}" if missing else f", all {len(want)} sizes of {ico}"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
