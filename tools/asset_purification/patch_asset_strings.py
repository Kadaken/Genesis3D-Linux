#!/usr/bin/env python3
# Subject to the Genesis3D Public License v1.01 in g3dlicense.txt.
# Original Code: Genesis3D, released March 25, 1999.
# Copyright (C) 1996-1999 Eclipse Entertainment, L.L.C. All Rights Reserved.
# Contributor(s): Kadaken (native Linux port, 2026). Provided without warranty.
"""Safely replace fixed-width byte strings in legacy Genesis3D assets."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import sys
from pathlib import Path
from typing import Any


PROJECT_ROOT = Path(__file__).resolve().parents[2]


class ManifestError(ValueError):
    """A manifest or target failed a safety check."""


def parse_offset(value: object) -> int:
    if isinstance(value, int):
        offset = value
    elif isinstance(value, str):
        offset = int(value, 0)
    else:
        raise ManifestError(f"invalid offset type: {type(value).__name__}")
    if offset < 0:
        raise ManifestError("offsets cannot be negative")
    return offset


def load_manifest(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        value = json.load(stream)
    if not isinstance(value, dict) or not isinstance(value.get("assets"), list):
        raise ManifestError("manifest must contain an 'assets' list")
    return value


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def resolve_target(raw_target: object, allow_external: bool) -> tuple[Path, Path]:
    if not isinstance(raw_target, str) or not raw_target:
        raise ManifestError("each asset needs a non-empty target path")
    logical = Path(raw_target)
    if not logical.is_absolute():
        logical = PROJECT_ROOT / logical
    logical = logical.absolute()
    resolved = logical.resolve(strict=True)
    try:
        resolved.relative_to(PROJECT_ROOT)
    except ValueError:
        if not allow_external:
            raise ManifestError(
                f"{logical} resolves outside the project ({resolved}); "
                "pass --allow-external-target only after reviewing the link"
            )
    if not resolved.is_file():
        raise ManifestError(f"target is not a regular file: {resolved}")
    return logical, resolved


def prepare_patches(asset: dict[str, Any], size: int) -> list[dict[str, Any]]:
    raw_patches = asset.get("patches")
    if not isinstance(raw_patches, list) or not raw_patches:
        raise ManifestError("each asset needs a non-empty patches list")
    prepared: list[dict[str, Any]] = []
    for index, raw in enumerate(raw_patches):
        if not isinstance(raw, dict):
            raise ManifestError(f"patch {index} must be an object")
        offset = parse_offset(raw.get("offset"))
        length = raw.get("original_length")
        expected = raw.get("expected_sha256")
        replacement_text = raw.get("replacement_ascii")
        if not isinstance(length, int) or length <= 0:
            raise ManifestError(f"patch {index}: original_length must be positive")
        if offset + length > size:
            raise ManifestError(f"patch {index}: byte range exceeds target size")
        if not isinstance(expected, str) or len(expected) != 64:
            raise ManifestError(f"patch {index}: expected_sha256 must be 64 hex digits")
        try:
            int(expected, 16)
        except ValueError as error:
            raise ManifestError(f"patch {index}: expected_sha256 is not hex") from error
        if not isinstance(replacement_text, str):
            raise ManifestError(f"patch {index}: replacement_ascii must be text")
        try:
            replacement = replacement_text.encode("ascii", errors="strict")
        except UnicodeEncodeError as error:
            raise ManifestError(f"patch {index}: replacement is not ASCII") from error
        if b"\x00" in replacement:
            raise ManifestError(f"patch {index}: replacement cannot contain NUL")
        if len(replacement) > length:
            raise ManifestError(
                f"patch {index}: {len(replacement)} replacement bytes exceed {length}"
            )
        prepared.append(
            {
                "offset": offset,
                "length": length,
                "expected": expected.lower(),
                "replacement": replacement.ljust(length, b"\x00"),
                "label": raw.get("label", f"patch-{index}"),
            }
        )
    prepared.sort(key=lambda patch: patch["offset"])
    for previous, current in zip(prepared, prepared[1:]):
        if previous["offset"] + previous["length"] > current["offset"]:
            raise ManifestError(
                f"overlapping patches near offsets {previous['offset']:#x} and "
                f"{current['offset']:#x}"
            )
    return prepared


def verify_ranges(stream: Any, patches: list[dict[str, Any]]) -> None:
    for patch in patches:
        stream.seek(patch["offset"])
        original = stream.read(patch["length"])
        actual = sha256_bytes(original)
        if actual != patch["expected"]:
            raise ManifestError(
                f"{patch['label']} at {patch['offset']:#x}: expected hash "
                f"{patch['expected']}, found {actual}"
            )


def process_asset(
    asset: dict[str, Any], apply: bool, allow_external: bool, backup_suffix: str
) -> int:
    logical, resolved = resolve_target(asset.get("target"), allow_external)
    before_size = resolved.stat().st_size
    expected_asset_hash = asset.get("expected_asset_sha256")
    if expected_asset_hash and sha256_file(resolved) != expected_asset_hash:
        raise ManifestError(f"whole-file hash mismatch: {logical}")
    patches = prepare_patches(asset, before_size)

    # rb+ is intentional: dry-runs exercise the exact access mode required for
    # application, while writes remain gated behind --apply.
    with resolved.open("rb+") as stream:
        verify_ranges(stream, patches)
        if not apply:
            print(f"DRY-RUN verified {len(patches)} patches: {logical} -> {resolved}")
            return len(patches)

        backup = logical.with_name(logical.name + backup_suffix)
        if backup.exists() or backup.is_symlink():
            raise ManifestError(f"refusing to overwrite existing backup: {backup}")
        shutil.copy2(resolved, backup, follow_symlinks=True)
        for patch in patches:
            stream.seek(patch["offset"])
            stream.write(patch["replacement"])
        stream.flush()
        os.fsync(stream.fileno())

    if resolved.stat().st_size != before_size:
        raise ManifestError(f"structural shift detected after writing: {logical}")
    print(f"APPLIED {len(patches)} patches: {logical}; backup: {backup}")
    return len(patches)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument(
        "--apply", action="store_true", help="write changes (default is verification only)"
    )
    parser.add_argument(
        "--allow-external-target",
        action="store_true",
        help="permit a project symlink to resolve outside the repository",
    )
    parser.add_argument(
        "--backup-suffix", default=".pre-purification.bak", help="exclusive backup suffix"
    )
    arguments = parser.parse_args()

    try:
        manifest = load_manifest(arguments.manifest)
        count = 0
        for asset in manifest["assets"]:
            if not isinstance(asset, dict):
                raise ManifestError("asset entries must be objects")
            count += process_asset(
                asset,
                apply=arguments.apply,
                allow_external=arguments.allow_external_target,
                backup_suffix=arguments.backup_suffix,
            )
        mode = "applied" if arguments.apply else "verified"
        print(f"{mode} {count} fixed-width patches; no file length changes")
        return 0
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
