import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest


MODULE = Path(__file__).resolve().parents[1] / "native" / "tools" / "unity_mesh.py"
spec = importlib.util.spec_from_file_location("unity_mesh", MODULE)
mesh = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mesh)
sys.path.insert(0, str(MODULE.parent))
import import_assets


def fixture():
    vertices = [(0.0, 1.0, 2.0), (1.0, 2.0, 3.0), (2.0, 3.0, 4.0)]
    # Position and UV channels share stream 0; stream 1 follows them.
    stream0 = b"".join(struct.pack("<5f", *v, 0, 0) for v in vertices)
    raw = stream0 + b"\x00" * 12
    return """%YAML 1.1
--- !u!43 &4300000
Mesh:
  m_SubMeshes:
  - serializedVersion: 2
    firstByte: 0
    indexCount: 3
    topology: 0
    baseVertex: 0
  m_Shapes:
  m_MeshCompression: 0
  m_IndexFormat: 0
  m_IndexBuffer: 000001000200
  m_VertexData:
    m_VertexCount: 3
    m_Channels:
    - stream: 0
      offset: 0
      format: 0
      dimension: 3
    - stream: 0
      offset: 12
      format: 0
      dimension: 0
    - stream: 0
      offset: 0
      format: 0
      dimension: 0
    - stream: 0
      offset: 0
      format: 0
      dimension: 0
    - stream: 0
      offset: 12
      format: 0
      dimension: 2
    - stream: 1
      offset: 0
      format: 0
      dimension: 1
    m_DataSize: {size}
    _typelessdata: {data}
  m_CompressedMesh:
""".format(size=len(raw), data=raw.hex()), vertices


class UnityMeshTests(unittest.TestCase):
    def test_deduplicates_identical_vertices_without_losing_uv_seams(self):
        positions = [(0, 0, 0), (1, 0, 0), (0, 0, 0), (0, 0, 0)]
        uvs = [(0, 0), (1, 0), (0, 0), (1, 1)]
        indices = [0, 1, 2, 3, 1, 2]
        actual_positions, actual_uvs, actual_indices = import_assets.deduplicate(
            positions, uvs, indices)
        self.assertEqual(len(actual_positions), 3)
        self.assertEqual([actual_positions[i] for i in actual_indices],
                         [positions[i] for i in indices])
        self.assertEqual([actual_uvs[i] for i in actual_indices],
                         [uvs[i] for i in indices])

    def test_reads_interleaved_positions_and_triangle_indices(self):
        source, vertices = fixture()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test.asset"
            path.write_text(source)
            actual, uvs, indices = mesh.read_mesh(path)
            self.assertEqual(actual, vertices)
            self.assertEqual(uvs, [(0, 0)] * 3)
            self.assertEqual(indices, [0, 1, 2])
            output = Path(directory) / "mesh.h"
            mesh.write_header(output, "path_mesh", actual, uvs, indices)
            self.assertIn("PATH_MESH_INDEX_COUNT 3", output.read_text())

    def test_rejects_truncated_buffers_and_non_triangle_topology(self):
        source, _ = fixture()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test.asset"
            for bad in (source.replace("m_DataSize: 72", "m_DataSize: 80"),
                        source.replace("topology: 0", "topology: 2"),
                        source.replace("000001000200", "000001000900")):
                path.write_text(bad)
                with self.assertRaises(ValueError):
                    mesh.read_mesh(path)

    def test_reads_uvs_from_aligned_second_stream(self):
        source, vertices = fixture()
        uvs = [(0.25, 0.5), (0.5, 0.75), (1.0, 0.0)]
        # Stream zero occupies 36 bytes, then Unity aligns stream one to 48.
        raw = b"".join(struct.pack("<3f", *v) for v in vertices) + b"\0" * 12
        raw += b"".join(struct.pack("<2f", *uv) for uv in uvs)
        source = source.replace("- stream: 0\n      offset: 12\n      format: 0\n      dimension: 2",
                                "- stream: 1\n      offset: 0\n      format: 0\n      dimension: 2")
        # Remove the unused synthetic extra channel from stream one.
        source = source.replace("- stream: 1\n      offset: 0\n      format: 0\n      dimension: 1",
                                "- stream: 1\n      offset: 0\n      format: 0\n      dimension: 0")
        source = source.replace(source.split("_typelessdata: ")[1].splitlines()[0], raw.hex())
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test.asset"
            path.write_text(source)
            actual, actual_uvs, indices = mesh.read_mesh(path)
        self.assertEqual(actual, vertices)
        self.assertEqual(actual_uvs, uvs)
        self.assertEqual(indices, [0, 1, 2])


if __name__ == "__main__":
    unittest.main()
