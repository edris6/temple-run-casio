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
            unity_mesh.write_header(args.output / (name + ".h"), name, vertices, uvs, indices)
            print("{}: {} vertices, {} triangles".format(source, len(vertices), len(indices) // 3))
        else:
            unity_texture.convert(source, args.output / (name + "_tex.h"), name)


if __name__ == "__main__":
    main()
