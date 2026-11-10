#!/usr/bin/env python3
"""Pack a folder into a DayZ .pbo (uncompressed, with SHA1 footer).

Usage: python3 tools/pack_pbo.py Source/FeralHordes @FeralHordes/Addons/FeralHordes.pbo
The prefix is read from $PBOPREFIX$ in the source folder (not packed itself).
"""
import hashlib, os, struct, sys, time

def pack(src, out):
    prefix_file = os.path.join(src, "$PBOPREFIX$")
    prefix = open(prefix_file).read().strip() if os.path.exists(prefix_file) else os.path.basename(src)
    files = []
    for root, _, names in os.walk(src):
        for n in sorted(names):
            full = os.path.join(root, n)
            rel = os.path.relpath(full, src)
            if rel == "$PBOPREFIX$":
                continue
            files.append((rel.replace("/", "\\"), open(full, "rb").read(), int(os.path.getmtime(full))))
    files.sort(key=lambda f: f[0].lower())

    h = bytearray()
    # product/version entry carrying the prefix
    h += b"\0" + struct.pack("<5I", 0x56657273, 0, 0, 0, 0)
    h += b"prefix\0" + prefix.encode() + b"\0" + b"\0"
    for name, data, mtime in files:
        h += name.encode() + b"\0" + struct.pack("<5I", 0, len(data), 0, mtime, len(data))
    h += b"\0" + struct.pack("<5I", 0, 0, 0, 0, 0)
    body = bytes(h) + b"".join(d for _, d, _ in files)
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    with open(out, "wb") as f:
        f.write(body + b"\0" + hashlib.sha1(body).digest())
    print(f"Packed {len(files)} files into {out} (prefix '{prefix}')")

if __name__ == "__main__":
    pack(sys.argv[1], sys.argv[2])
