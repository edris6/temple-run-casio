#!/usr/bin/env python3
"""Convert an 8-bit RGB/RGBA Unity-exported PNG to a 128x128 RGB565 header."""
import argparse
from pathlib import Path
import re
import struct
import zlib


def read_png(path):
    data = Path(path).read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG")
    pos = 8
    compressed = bytearray()
    header = None
    while pos + 12 <= len(data):
        length = struct.unpack_from(">I", data, pos)[0]
        end = pos + 12 + length
        if end > len(data):
            raise ValueError("truncated PNG chunk")
        kind, payload = data[pos + 4:pos + 8], data[pos + 8:pos + 8 + length]
        expected = struct.unpack_from(">I", data, pos + 8 + length)[0]
        if zlib.crc32(kind + payload) != expected:
            raise ValueError("invalid PNG chunk CRC")
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", payload)
        elif kind == b"IDAT":
            compressed.extend(payload)
        elif kind == b"IEND":
            break
        pos = end
    if header is None:
        raise ValueError("missing PNG header")
    width, height, depth, color, compression, filtering, interlace = header
    if not width or not height or depth != 8 or color not in (2, 6) or compression or filtering or interlace:
        raise ValueError("expected non-interlaced 8-bit RGB or RGBA PNG")
    channels = 3 if color == 2 else 4
    stride = width * channels
    data = zlib.decompress(compressed)
    if len(data) != (stride + 1) * height:
        raise ValueError("invalid PNG data length")
    rows = []
    previous = bytearray(stride)
    for y in range(height):
        offset = y * (stride + 1)
        mode = data[offset]
        if mode > 4:
            raise ValueError("unsupported PNG filter")
        row = bytearray(data[offset + 1:offset + 1 + stride])
        for i in range(stride):
            left = row[i - channels] if i >= channels else 0
            up = previous[i]
            upper_left = previous[i - channels] if i >= channels else 0
            if mode == 1:
                predict = left
            elif mode == 2:
                predict = up
            elif mode == 3:
                predict = (left + up) // 2
            elif mode == 4:
                p = left + up - upper_left
                a, b, c = abs(p - left), abs(p - up), abs(p - upper_left)
                predict = left if a <= b and a <= c else up if b <= c else upper_left
            else:
                predict = 0
            row[i] = (row[i] + predict) & 255
        rows.append(row)
        previous = row
    return width, height, channels, rows


def convert(source, destination, name):
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name):
        raise ValueError("name must be a C identifier")
    width, height, channels, rows = read_png(source)
    pixels = []
    for y in range(128):
        row = rows[y * height // 128]
        for x in range(128):
            start = (x * width // 128) * channels
            r, g, b = row[start:start + 3]
            pixels.append(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))
    lines = ["/* Generated from a user-provided texture; do not commit. */",
             "#include <stdint.h>", "static const uint16_t " + name + "_texture[16384] = {"]
    lines.extend("    " + ", ".join("0x%04x" % value for value in pixels[i:i+12]) + ","
                 for i in range(0, len(pixels), 12))
    lines.append("};\n")
    Path(destination).write_text("\n".join(lines), encoding="ascii")
    print("{}: {}x{} -> 128x128 RGB565".format(source, width, height))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source")
    parser.add_argument("destination")
    parser.add_argument("--name", required=True)
    args = parser.parse_args()
    convert(args.source, args.destination, args.name)


if __name__ == "__main__":
    main()
