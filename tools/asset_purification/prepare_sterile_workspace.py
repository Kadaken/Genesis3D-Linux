#!/usr/bin/env python3
# Subject to the Genesis3D Public License v1.01 in g3dlicense.txt.
# Original Code: Genesis3D, released March 25, 1999.
# Copyright (C) 1996-1999 Eclipse Entertainment, L.L.C. All Rights Reserved.
# Contributor(s): Kadaken (native Linux port, 2026). Provided without warranty.
"""Create a dereferenced asset workspace and a hash-addressed patch manifest."""
from __future__ import annotations

import hashlib
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "Assets"
WORK = ROOT / "PurificationWorkspace" / "Assets"
EVIDENCE = ROOT / "Reports" / "evidence" / "sterile_asset_purification"

ROWS = {
    "C01": (7, [0xde8de, 0xeed92], "NODE_A"),
    "C02": (14, [0xe3998,0xe3def,0xe4246,0xe46a3,0xe4afc,0xeb75a,0xebb50,0xebf5b,0xec36a,0xec777,0xecb84,0xecf92,0xed42c,0xed823,0xedc19], "ENTITY_01"),
    "C03": (14, [0xe39b7,0xe3e0e,0xe4265,0xe46c2,0xe4b1b,0xeaad1,0xeaefd,0xeb779,0xed44b,0xed842,0xedc38], "ENTITY_02"),
    "C04": (15, [0xe39e9,0xe3e40,0xe4297,0xe46f4,0xe4b57,0xebba6,0xebfb1,0xec3c0,0xec7cd,0xecbda,0xecfe8], "ENTITY_003"),
    "C05": (17, [0xeaaaf,0xeaedb], "ENTITY_00001"),
    "C06": (15, [0xeaaf1,0xeaf1d], "ENTITY_004"),
    "C07": (15, [0xeab28,0xeaf54], "ENTITY_005"),
    "C08": (22, [0xeab4f,0xeaf7b], "ENTITY_PLACEHOLDER"),
    "C09": (12, [0xeadb3,0xeb1df], "ENTITY01.ACT"),
    "C10": (13, [0xeba2b,0xed6fd,0xedaf3,0xedeea], "ENTITY_02.ACT"),
    "C11": (19, [0xebb6f,0xebf7a,0xec389,0xec796,0xecba3,0xecfb1], "ENTITY_NEUTRAL"),
    "C12": (15, [0xebe34,0xec242,0xec64f,0xeca5c,0xece6a,0xed278], "ENTITY_0003.ACT"),
    "C13": (15, [0xee5aa], "NODE_GENERIC"),
    "C14": (12, [0xee5ca], "MOTION_0001"),
    "C15": (13, [0xee614], "ENTITY_08"),
    "C16": (11, [0xee899], "NODE001.ACT"),
}

def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()

def main() -> None:
    if WORK.parent.exists():
        raise SystemExit(f"refusing to overwrite existing workspace: {WORK.parent}")
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    links = sorted(p for p in SOURCE.rglob("*") if p.is_symlink())
    if len(links) != 47:
        raise SystemExit(f"expected 47 source links, found {len(links)}")
    records = []
    for link in links:
        target = link.resolve(strict=True)
        rel = link.relative_to(SOURCE)
        out = WORK / rel
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(target, out, follow_symlinks=True)
        records.append({"logical": str(link.relative_to(ROOT)), "source": str(target), "copy": str(out.relative_to(ROOT)), "source_sha256": digest(target), "copy_sha256": digest(out)})

    bsp = WORK / "Maps" / "rfedit1.bsp"
    raw = bsp.read_bytes()
    actor_renames = {}
    patches = []
    actor_ids = {"C09", "C10", "C12", "C16"}
    for cid, (length, offsets, replacement) in ROWS.items():
        original = raw[offsets[0]:offsets[0] + length]
        for offset in offsets:
            if raw[offset:offset + length] != original:
                raise SystemExit(f"inconsistent bytes for {cid} at {offset:#x}")
            patches.append({"label": cid, "offset": hex(offset), "original_length": length,
                            "expected_sha256": hashlib.sha256(original).hexdigest(),
                            "replacement_ascii": replacement})
        if cid in actor_ids:
            matches = [p for p in (WORK / "Actors").iterdir() if p.name.encode("ascii").lower() == original.lower()]
            if len(matches) != 1:
                raise SystemExit(f"{cid}: expected one case-insensitive actor match, found {len(matches)}")
            destination = matches[0].with_name(replacement)
            if destination.exists():
                raise SystemExit(f"rename collision: {destination}")
            matches[0].rename(destination)
            actor_renames[cid] = replacement

    manifest = {"format": 1, "assets": [{"target": str(bsp.relative_to(ROOT)),
        "expected_asset_sha256": digest(bsp), "patches": patches}]}
    manifest_path = ROOT / "tools" / "asset_purification" / "sterile_manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    (EVIDENCE / "source_links.json").write_text(json.dumps(records, indent=2) + "\n")
    (EVIDENCE / "actor_renames.json").write_text(json.dumps(actor_renames, indent=2) + "\n")
    print(f"copied {len(records)} regular files; generated {len(patches)} fixed-width patches")

if __name__ == "__main__":
    main()
