import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
import zlib


MODULE = Path(__file__).resolve().parents[1] / "native" / "tools" / "unity_texture.py"
spec = importlib.util.spec_from_file_location("unity_texture", MODULE)
texture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(texture)


def png_chunk(kind, payload):
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload))


def make_png():
    width, height, channels = 3, 5, 4
    rows = [bytes(((y * 41 + i * 13) & 255) for i in range(width * channels))
            for y in range(height)]
    encoded = bytearray()
    previous = bytes(width * channels)
    for mode, row in enumerate(rows):
        encoded.append(mode)
        for i, value in enumerate(row):
            left = row[i-channels] if i >= channels else 0
            up = previous[i]
            upper_left = previous[i-channels] if i >= channels else 0
            p = left + up - upper_left
            a, b, c = abs(p-left), abs(p-up), abs(p-upper_left)
            predictors = (0, left, up, (left+up)//2,
                          left if a <= b and a <= c else up if b <= c else upper_left)
            encoded.append((value - predictors[mode]) & 255)
        previous = row
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", header) +
            png_chunk(b"IDAT", zlib.compress(encoded)) + png_chunk(b"IEND", b"")), rows


class UnityTextureTests(unittest.TestCase):
    def test_decodes_all_png_filters_and_outputs_header(self):
        data, rows = make_png()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "source.png"
            output = Path(directory) / "texture.h"
            path.write_bytes(data)
            width, height, channels, decoded = texture.read_png(path)
            self.assertEqual((width, height, channels), (3, 5, 4))
            self.assertEqual(decoded, rows)
            texture.convert(path, output, "sample")
            self.assertIn("sample_texture[16384]", output.read_text())

    def test_rejects_corrupt_data(self):
        data, _ = make_png()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad.png"
            path.write_bytes(data[:30] + b"X" + data[31:])
            with self.assertRaises(ValueError):
                texture.read_png(path)


if __name__ == "__main__":
    unittest.main()
