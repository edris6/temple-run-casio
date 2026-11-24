#!/usr/bin/env python3
"""Generate local native assets from the pinned Unity reconstruction checkout."""
import argparse
from pathlib import Path
import re
import unity_mesh
import unity_texture


ASSETS = (
    ("Assets/Mesh/player.asset", "00a87548036507242bec37aca6c00243", "player", "mesh"),
    ("Assets/Mesh/Object81-Pivot.asset", "dafb6161b16610c4bbafceed09ce5209", "path", "mesh"),
    ("Assets/Mesh/Object80-Pivot.asset", "c947e917efb32be43b693440ae57aedb", "path_h", "mesh"),
    ("Assets/Mesh/Object79-Pivot.asset", "7432c34d46d566548840af1598290327", "left_corner", "mesh"),
    ("Assets/Mesh/Object77-Pivot.asset", "5fae00a8ce7f988439fce322ba3cb4e7", "right_corner", "mesh"),
    ("Assets/Mesh/Object78-Pivot.asset", "758ea34925fcdf147928d8c8226840fe", "corner_c", "mesh"),
    ("Assets/Mesh/Object76-Pivot.asset", "293dc9a2f1895a948af863f7ca38d017", "corner_d", "mesh"),
    ("Assets/Mesh/Object97-Pivot.asset", "5d3b65e3a16702a48b42d5e1554f4c1f", "jump", "mesh"),
    ("Assets/Mesh/Object102-Pivot.asset", "a7f4e1549896a8e46975806f078aa715", "slide", "mesh"),
    ("Assets/Mesh/Object104-Pivot.asset", "32ebffaaa06a73844b6f42a0465ded38", "trip", "mesh"),
    ("Assets/Mesh/Object105-Pivot.asset", "94d7e36ff31940843b030193b3f1650f", "jump_h", "mesh"),
    ("Assets/Mesh/Object99-Pivot.asset", "14867a50d29207840a674a438784dfdf", "slide_h", "mesh"),
    ("Assets/Mesh/Object100-Pivot.asset", "1ce28ac0da3fc9e4abcbfee3c960fd84", "trip_h", "mesh"),
    ("Assets/Mesh/Object21-Pivot.asset", "7201610bdb9770a4d9f29825ecf1829c", "hole", "mesh"),
    ("Assets/Mesh/Object16-Pivot.asset", "213fdaef06d96ea41ba6b35ee573a4a2", "hole_h", "mesh"),
    ("Assets/Mesh/Object47-Pivot.asset", "d404dbc6b844aa3468e6fd78eb755d9c", "narrow", "mesh"),
    ("Assets/Mesh/Object53-Pivot.asset", "8ad5eee616dfc4f47b9cf530812f7943", "narrow_h", "mesh"),
    ("Assets/Mesh/Box01.asset", "5f5788a330b828e4ba8472b9598f0c47", "coin", "mesh"),
    ("Assets/Texture2D/playerTexture.png", "94976c8f5714a6b40b7eee3513f4f457", "player", "texture"),
    ("Assets/Texture2D/wallTexture.png", "1baec2f1eac5a674f9dc3c0f43794339", "path", "texture"),
    ("Assets/Texture2D/treeTexture.png", "ff584f033757e13448d5f0635d662995", "tree", "texture"),
)

# Local prefab offsets, in the same order as the native renderer's transforms.
MESH_ORIGINS = {
    "path": (559.9522, -1335.3497), "path_h": (569.9522, -1405.3497),
    "left_corner": (629.9522, -1275.3496), "right_corner": (689.9522, -1275.3496),
    "corner_c": (639.9522, -1335.3496), "corner_d": (699.9522, -1335.3496),
    "jump": (1359.9521, -675.3496), "slide": (1349.952, -575.3496),
    "trip": (1359.9521, -415.3496), "jump_h": (1269.9521, -645.3496),
    "slide_h": (1269.952, -575.3496), "trip_h": (1269.952, -375.3496),
    "hole": (1139.9521, -925.3496), "hole_h": (1189.9521, -925.3496),
    "narrow": (1699.9521, -1235.3496), "narrow_h": (1549.9521, -1295.3496),
}


def deduplicate(vertices, uvs, indices):
    """Merge only identical position/UV pairs, preserving texture seams."""
    lookup, positions, texcoords, remapped = {}, [], [], []
    for index in indices:
        key = (vertices[index], uvs[index])
        if key not in lookup:
            lookup[key] = len(positions)
            positions.append(vertices[index])
            texcoords.append(uvs[index])
        remapped.append(lookup[key])
    return positions, texcoords, remapped


def append_fixed_mesh(path, name, vertices, uvs):
    """Bake prefab-local transforms and quantize for the CG50 renderer."""
    if name == "player":
        transformed = [(x, y - 3.6, z + 30) for x, y, z in vertices]
    elif name == "coin":
        transformed = [(x, y - 6, z) for x, y, z in vertices]
    else:
        origin_x, origin_z = MESH_ORIGINS[name]
        transformed = [(-(x + origin_x), z + 6, y + origin_z) for x, y, z in vertices]
    position_q = [tuple(round(value * 16) for value in point) for point in transformed]
    uv_q = [tuple(round(value * 2048) for value in uv) for uv in uvs]
    if any(not -32768 <= value <= 32767 for point in position_q + uv_q for value in point):
        raise ValueError("fixed-point mesh value out of range: " + name)
    with path.open("a", encoding="ascii") as output:
        for suffix, values in (("fixed_vertices", position_q), ("fixed_uvs", uv_q)):
            output.write("static const int16_t {}_{}[][{}] = {{\n".format(
                name, suffix, len(values[0])))
            for point in values:
                output.write("    {" + ", ".join(map(str, point)) + "},\n")
            output.write("};\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path, help="path to the Unity reconstruction checkout")
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parents[1] / "build")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    for relative, expected_guid, name, kind in ASSETS:
        source = args.reference / relative
        meta = Path(str(source) + ".meta").read_text(encoding="utf-8")
        guid = re.search(r"^guid:\s*([0-9a-f]{32})\s*$", meta, re.M)
        if not guid or guid.group(1) != expected_guid:
            raise ValueError("unexpected asset GUID: " + str(source))
        if kind == "mesh":
            vertices, uvs, indices = unity_mesh.read_mesh(source)
            original_count = len(vertices)
            vertices, uvs, indices = deduplicate(vertices, uvs, indices)
            output = args.output / (name + ".h")
            unity_mesh.write_header(output, name, vertices, uvs, indices)
            append_fixed_mesh(output, name, vertices, uvs)
            print("{}: {} -> {} vertices, {} triangles".format(
                source, original_count, len(vertices), len(indices) // 3))
        else:
            unity_texture.convert(source, args.output / (name + "_tex.h"), name)


if __name__ == "__main__":
    main()
