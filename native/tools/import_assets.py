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
    ("Assets/Texture2D/playerTexture.png", "94976c8f5714a6b40b7eee3513f4f457", "player", "texture"),
    ("Assets/Texture2D/wallTexture.png", "1baec2f1eac5a674f9dc3c0f43794339", "path", "texture"),
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
