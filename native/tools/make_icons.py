#!/usr/bin/env python3
"""Generate simple original 92x64 add-in icons using the standard library."""
import struct
import zlib
from pathlib import Path


def png(path, selected):
    rows = []
    for y in range(64):
        row = bytearray([0])
        for x in range(92):
            bg = (28, 59, 37) if not selected else (49, 90, 51)
            if 16 <= x < 76 and 12 <= y < 56:
                bg = (151, 127, 75)
            if 25 <= x < 67 and 21 <= y < 56:
                bg = (37, 43, 25)
            if 43 <= x < 49 and 30 <= y < 53:
                bg = (220, 181, 92)
            row.extend(bg)
        rows.append(row)
    def chunk(kind, payload):
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload))
    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 92, 64, 8, 2, 0, 0, 0))
    data += chunk(b"IDAT", zlib.compress(b"".join(rows))) + chunk(b"IEND", b"")
    Path(path).write_bytes(data)


if __name__ == "__main__":
    directory = Path(__file__).resolve().parents[1] / "assets"
    directory.mkdir(exist_ok=True)
    png(directory / "icon-uns.png", False)
    png(directory / "icon-sel.png", True)
