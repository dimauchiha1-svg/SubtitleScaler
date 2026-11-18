#!/usr/bin/env python3
"""
verify_dll.py - check that a built SKSE plugin declares what Skyrim 1.7.x needs.

  python tools/verify_dll.py path/to/SubtitleScaler.dll

Reads the exported SKSEPlugin_Version structure (standard library only) and fails if
  - the DLL does not export SKSEPlugin_Version (SKSE 2.2+ and 2.3 ignore it then) or SKSEPlugin_Query (SKSE for 1.5.97),
  - the "Address Library V5" flag (versionIndependenceEx bit 1) is missing: SKSE 2.3.x for Skyrim 1.7.x refuses such a plugin,
  - the "Address Library" flag (versionIndependence bit 0) is missing,
  - fixed compatible game versions are listed without any version-independence flag.
Layout (from SKSE/Interfaces.h): dataVersion 0x00, pluginVersion 0x04, name 0x08, author 0x108, supportEmail 0x208,
versionIndependenceEx 0x304, versionIndependence 0x308, compatibleVersions 0x30C, xseMinimum 0x34C.
"""
import struct
import sys


def read_exports(data):
    if data[:2] != b"MZ":
        raise SystemExit("Not a PE file")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise SystemExit("PE signature not found")
    nsec, = struct.unpack_from("<H", data, pe + 6)
    opt_size, = struct.unpack_from("<H", data, pe + 20)
    opt = pe + 24
    magic, = struct.unpack_from("<H", data, opt)
    if magic != 0x20B:
        raise SystemExit("Not a 64-bit DLL")
    export_rva, export_size = struct.unpack_from("<II", data, opt + 112)
    sec_off = opt + opt_size
    sections = []
    for i in range(nsec):
        name, vsize, vaddr, rsize, raddr = struct.unpack_from("<8sIIII", data, sec_off + 40 * i)
        sections.append((vaddr, max(vsize, rsize), raddr))

    def rva2off(rva):
        for vaddr, size, raddr in sections:
            if vaddr <= rva < vaddr + size:
                return rva - vaddr + raddr
        raise SystemExit("RVA 0x%X is outside every section" % rva)

    if not export_rva:
        return {}, rva2off
    e = rva2off(export_rva)
    n_funcs, n_names, addr_funcs, addr_names, addr_ords = struct.unpack_from("<IIIII", data, e + 20)
    funcs = rva2off(addr_funcs)
    names = rva2off(addr_names)
    ords = rva2off(addr_ords)
    out = {}
    for i in range(n_names):
        nrva, = struct.unpack_from("<I", data, names + 4 * i)
        no = rva2off(nrva)
        end = data.index(b"\0", no)
        ordinal, = struct.unpack_from("<H", data, ords + 2 * i)
        frva, = struct.unpack_from("<I", data, funcs + 4 * ordinal)
        out[data[no:end].decode("ascii", "replace")] = frva
    return out, rva2off


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    data = open(sys.argv[1], "rb").read()
    exports, rva2off = read_exports(data)
    problems = []
    for need in ("SKSEPlugin_Version", "SKSEPlugin_Query"):
        if need not in exports:
            problems.append("missing export " + need)
    if "SKSEPlugin_Version" in exports:
        o = rva2off(exports["SKSEPlugin_Version"])
        data_version, plugin_version = struct.unpack_from("<II", data, o)
        name = data[o + 0x08:o + 0x108].split(b"\0")[0].decode("utf-8", "replace")
        author = data[o + 0x108:o + 0x208].split(b"\0")[0].decode("utf-8", "replace")
        ex, ind = struct.unpack_from("<II", data, o + 0x304)
        compat = struct.unpack_from("<16I", data, o + 0x30C)
        xse_min, = struct.unpack_from("<I", data, o + 0x34C)
        print("SKSEPlugin_Version: dataVersion=%d plugin='%s' author='%s' pluginVersion=0x%08X" % (data_version, name, author, plugin_version))
        print("  versionIndependenceEx=0x%X (bit0 NoStructUse=%d, bit1 AddressLibraryV5=%d)" % (ex, ex & 1, (ex >> 1) & 1))
        print("  versionIndependence=0x%X (bit0 AddressLibrary=%d, bit1 SignatureScanning=%d, bit2 StructsPost629=%d)" % (ind, ind & 1, (ind >> 1) & 1, (ind >> 2) & 1))
        # CommonLibSSE NG fills unused entries with 1.0.0.0 (0x01000000); SKSE does not read the list when a
        # version-independence flag (Address Library or signature scanning) is set.
        listed = [c for c in compat if c and c != 0x01000000]
        print("  compatibleVersions=%s%s  minimumSKSE=0x%08X" % (
            [hex(c) for c in listed], " (+ default 1.0.0.0 filler, ignored)" if any(c == 0x01000000 for c in compat) else "", xse_min))
        if data_version != 1:
            problems.append("dataVersion is %d, expected 1" % data_version)
        if not (ex & 2):
            problems.append("Address Library V5 flag is NOT set: SKSE 2.3.x (Skyrim 1.7.x) will not load this plugin")
        if not (ind & 1):
            problems.append("Address Library flag is NOT set")
        if listed and not (ind & 3):
            problems.append("the plugin lists fixed compatible versions and no version-independence flag, so it will not load on newer game versions")
    if problems:
        print("\nFAILED:")
        for p in problems:
            print(" -", p)
        raise SystemExit(1)
    print("\nOK: exports SKSEPlugin_Version and SKSEPlugin_Query, Address Library V5 flag set.")


if __name__ == "__main__":
    main()
