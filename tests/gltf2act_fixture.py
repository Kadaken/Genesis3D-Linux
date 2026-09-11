#!/usr/bin/env python3
"""Asset-free deterministic GLB-to-F2-ACT conversion fixture."""
import json
import binascii
import pathlib
import struct
import subprocess
import sys
import tempfile
import zlib


def align(data: bytearray) -> None:
    while len(data) % 4:
        data.append(0)


def append(data: bytearray, payload: bytes) -> tuple[int, int]:
    align(data)
    offset = len(data)
    data.extend(payload)
    return offset, len(payload)


def build_glb(path: pathlib.Path) -> None:
    binary = bytearray()
    views = []
    accessors = []

    def add(payload: bytes, component_type: int, count: int, kind: str,
            *, target: int | None = None) -> int:
        offset, size = append(binary, payload)
        view = {"buffer": 0, "byteOffset": offset, "byteLength": size}
        if target is not None:
            view["target"] = target
        views.append(view)
        accessors.append({"bufferView": len(views) - 1, "componentType": component_type,
                          "count": count, "type": kind})
        return len(accessors) - 1

    positions = add(struct.pack("<9f", 1, 0, 0, 2, 0, 0, 1, 1, 0),
                    5126, 3, "VEC3", target=34962)
    normals = add(struct.pack("<9f", *(1, 0, 0) * 3), 5126, 3, "VEC3", target=34962)
    texcoords = add(struct.pack("<6f", 0, 0, 1, 0, 0, 1),
                    5126, 3, "VEC2", target=34962)
    joints = add(struct.pack("<12H", *(0, 1, 0, 0) * 3),
                 5123, 3, "VEC4", target=34962)
    weights = add(struct.pack("<12f", *(0.5, 0.5, 0, 0) * 3),
                  5126, 3, "VEC4", target=34962)
    indices = add(struct.pack("<3H", 0, 1, 2), 5123, 3, "SCALAR", target=34963)
    identity = (1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)
    inverse_binds = add(struct.pack("<32f", *(identity + identity)), 5126, 2, "MAT4")
    times = add(struct.pack("<2f", 0, 1), 5126, 2, "SCALAR")
    rotations = add(struct.pack("<8f", 0, 0, 0, 1, 0, 0, 0.70710678, 0.70710678),
                    5126, 2, "VEC4")

    def png_chunk(kind: bytes, payload: bytes) -> bytes:
        return (struct.pack(">I", len(payload)) + kind + payload
                + struct.pack(">I", binascii.crc32(kind + payload) & 0xFFFFFFFF))

    png = (b"\x89PNG\r\n\x1a\n"
           + png_chunk(b"IHDR", struct.pack(">IIBBBBB", 2, 1, 8, 6, 0, 0, 0))
           + png_chunk(b"IDAT", zlib.compress(b"\x00\xff\x20\x10\xff\x08\x40\xe0\x80"))
           + png_chunk(b"IEND", b""))
    image_offset, image_size = append(binary, png)
    views.append({"buffer": 0, "byteOffset": image_offset, "byteLength": image_size})
    image_view = len(views) - 1

    document = {
        "asset": {"version": "2.0", "generator": "Kadaken asset-free fixture"},
        "scene": 0,
        "scenes": [{"nodes": [0, 1]}],
        "nodes": [
            {"name": "mesh", "mesh": 0, "skin": 0},
            {"name": "root", "children": [2]},
            {"name": "joint", "translation": [0, 0, 0]},
        ],
        "skins": [{"joints": [1, 2], "skeleton": 1,
                   "inverseBindMatrices": inverse_binds}],
        "materials": [{"name": "neutral", "pbrMetallicRoughness": {
            "baseColorFactor": [0.25, 0.5, 0.75, 1], "baseColorTexture": {"index": 0}}}],
        "textures": [{"source": 0}],
        "images": [{"bufferView": image_view, "mimeType": "image/png"}],
        "meshes": [{"primitives": [{
            "attributes": {"POSITION": positions, "NORMAL": normals,
                           "TEXCOORD_0": texcoords, "JOINTS_0": joints,
                           "WEIGHTS_0": weights},
            "indices": indices, "material": 0, "mode": 4,
        }]}],
        "animations": [{"name": "turn", "samplers": [{
            "input": times, "output": rotations, "interpolation": "LINEAR"}],
            "channels": [{"sampler": 0, "target": {"node": 2, "path": "rotation"}}]}],
        "buffers": [{"byteLength": len(binary)}],
        "bufferViews": views,
        "accessors": accessors,
    }
    json_bytes = json.dumps(document, separators=(",", ":"), sort_keys=True).encode()
    while len(json_bytes) % 4:
        json_bytes += b" "
    align(binary)
    total = 12 + 8 + len(json_bytes) + 8 + len(binary)
    path.write_bytes(
        struct.pack("<4sII", b"glTF", 2, total)
        + struct.pack("<I4s", len(json_bytes), b"JSON") + json_bytes
        + struct.pack("<I4s", len(binary), b"BIN\0") + binary
    )


def main() -> int:
    converter = pathlib.Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="genesis-gltf2act-") as directory:
        root = pathlib.Path(directory)
        source, first, second = root / "fixture.glb", root / "first.act", root / "second.act"
        build_glb(source)
        for output in (first, second):
            completed = subprocess.run([converter, source, output], text=True,
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if completed.returncode:
                sys.stderr.write(completed.stderr)
                return completed.returncode
        if first.read_bytes() != second.read_bytes():
            sys.stderr.write("gltf2act fixture: nondeterministic ACT output\n")
            return 1
        if first.stat().st_size == 0:
            sys.stderr.write("gltf2act fixture: empty ACT output\n")
            return 1
    print("gltf2act fixture: PASS weighted geometry, skeleton, motion, material, deterministic output")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
