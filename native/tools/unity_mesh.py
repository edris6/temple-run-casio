#!/usr/bin/env python3
"""Convert the position and triangle streams of an exported Unity YAML mesh.

This handles the uncompressed float-position Mesh layout used by the pinned
runner and temple path, not every Unity mesh or its skinning/material data.
"""
import argparse
import math
import re
import struct
from pathlib import Path


FORMAT_SIZE = {0: 4, 1: 2, 2: 1, 3: 1, 4: 1, 5: 1,
               6: 2, 7: 2, 8: 2, 9: 2, 10: 4, 11: 4}


def field(text, name):
    match = re.search(r"^\s*" + re.escape(name) + r":\s*(\S+)\s*$", text, re.M)
    if not match:
        raise ValueError("missing " + name)
    return match.group(1)


def read_mesh(path):
    text = Path(path).read_text(encoding="utf-8")
    if "--- !u!43 " not in text or int(field(text, "m_MeshCompression")) != 0:
        raise ValueError("expected an uncompressed Unity Mesh")
    index_format = int(field(text, "m_IndexFormat"))
    if index_format not in (0, 1):
        raise ValueError("unsupported index format")
    ib = bytes.fromhex(field(text, "m_IndexBuffer"))
    vertex_text = text.split("  m_VertexData:", 1)[1].split("  m_CompressedMesh:", 1)[0]
    count = int(field(vertex_text, "m_VertexCount"))
    channels_text = vertex_text.split("    m_Channels:", 1)[1].split("    m_DataSize:", 1)[0]
    channels = [tuple(map(int, m)) for m in re.findall(
        r"- stream: (\d+)\s+offset: (\d+)\s+format: (\d+)\s+dimension: (\d+)", channels_text)]
    if not channels or channels[0][0] != 0 or channels[0][2:] != (0, 3):
        raise ValueError("expected float32 position channel in stream zero")
    active = [c for c in channels if c[0] == 0 and c[3] > 0]
    if any(c[2] not in FORMAT_SIZE for c in active):
        raise ValueError("unsupported vertex channel format")
    stride = max(offset + FORMAT_SIZE[fmt] * dim for _, offset, fmt, dim in active)
    raw = bytes.fromhex(field(vertex_text, "_typelessdata"))
    if len(raw) != int(field(vertex_text, "m_DataSize")) or count * stride > len(raw):
        raise ValueError("truncated vertex data")
    vertices = [struct.unpack_from("<3f", raw, i * stride + channels[0][1]) for i in range(count)]
    if any(not math.isfinite(v) for vertex in vertices for v in vertex):
        raise ValueError("non-finite vertex")
    if len(channels) <= 4 or channels[4][2:] != (0, 2):
        raise ValueError("expected float32 UV0 channel")
    uv_stream, uv_offset, _, _ = channels[4]
    streams = {}
    for stream, offset, fmt, dimension in channels:
        if dimension:
            if fmt not in FORMAT_SIZE:
                raise ValueError("unsupported vertex channel format")
            streams[stream] = max(streams.get(stream, 0), offset + FORMAT_SIZE[fmt] * dimension)
    stream_start = 0
    for stream in sorted(streams):
        if stream == uv_stream:
            break
        stream_start = (stream_start + streams[stream] * count + 15) & ~15
    uv_stride = streams[uv_stream]
    if stream_start + uv_stride * count > len(raw):
        raise ValueError("truncated UV data")
    uvs = [struct.unpack_from("<2f", raw, stream_start + i * uv_stride + uv_offset)
           for i in range(count)]
    if any(not math.isfinite(v) for uv in uvs for v in uv):
        raise ValueError("non-finite UV")
    submeshes = text.split("  m_SubMeshes:", 1)[1].split("  m_Shapes:", 1)[0]
    blocks = re.split(r"\n  - serializedVersion:", submeshes)[1:]
    indices = []
    width = 2 if index_format == 0 else 4
    for block in blocks:
        first = int(field(block, "firstByte"))
        number = int(field(block, "indexCount"))
        base = int(field(block, "baseVertex"))
        if int(field(block, "topology")) != 0 or number % 3 or first + number * width > len(ib):
            raise ValueError("unsupported or truncated triangle submesh")
        indices.extend(base + value[0] for value in struct.iter_unpack(
            "<H" if width == 2 else "<I", ib[first:first + number * width]))
    if not indices or max(indices) >= count or count > 65535:
        raise ValueError("invalid triangle indices or too many vertices")
    return vertices, uvs, indices


def write_header(path, name, vertices, uvs, indices):
    if not re.fullmatch(r"[a-zA-Z_][a-zA-Z0-9_]*", name):
        raise ValueError("name must be a C identifier")
    def cfloat(value):
        number = format(value, ".9g")
        return number + (".0f" if "." not in number and "e" not in number else "f")
    lines = ["/* Generated from a user-provided Unity mesh; do not commit. */",
             "#include <stdint.h>",
             "static const float " + name + "_vertices[][3] = {"]
    lines.extend("    {" + ", ".join(cfloat(x) for x in vertex) + "}," for vertex in vertices)
    lines.extend(["};", "static const float " + name + "_uvs[][2] = {"])
    lines.extend("    {" + ", ".join(cfloat(x) for x in uv) + "}," for uv in uvs)
    lines.extend(["};", "static const uint16_t " + name + "_indices[] = {"])
    lines.extend("    " + ", ".join(map(str, indices[i:i + 12])) + ","
                 for i in range(0, len(indices), 12))
    lines.extend(["};", "#define " + name.upper() + "_VERTEX_COUNT " + str(len(vertices)),
                  "#define " + name.upper() + "_INDEX_COUNT " + str(len(indices)), ""])
    Path(path).write_text("\n".join(lines), encoding="ascii")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mesh", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--name", required=True)
    args = parser.parse_args()
    vertices, uvs, indices = read_mesh(args.mesh)
    write_header(args.output, args.name, vertices, uvs, indices)
    print("{}: {} vertices, {} triangles".format(args.mesh, len(vertices), len(indices) // 3))


if __name__ == "__main__":
    main()
