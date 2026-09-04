#!/usr/bin/env python3
# Subject to the Genesis3D Public License v1.01 in g3dlicense.txt.
# Original Code: Genesis3D, released March 25, 1999.
# Copyright (C) 1996-1999 Eclipse Entertainment, L.L.C. All Rights Reserved.
# Contributor(s): Kadaken (native Linux port, 2026). Provided without warranty.
"""Repair project-local #include spelling for case-sensitive filesystems."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
from collections import defaultdict
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent.parent
SKIP_DIRS = {".git", "CMakeFiles", "build", "cmake-build-debug", "cmake-build-release"}
CANONICAL_BASETYPE = PROJECT_ROOT / "basetype.h"
INCLUDE_ROOTS = [
    PROJECT_ROOT,
    PROJECT_ROOT / "Actor",
    PROJECT_ROOT / "Bitmap",
    PROJECT_ROOT / "Engine",
    PROJECT_ROOT / "Engine" / "Drivers",
    PROJECT_ROOT / "Math",
    PROJECT_ROOT / "Support",
    PROJECT_ROOT / "VFile",
    PROJECT_ROOT / "World",
]
SOURCE_SUFFIXES = {
    ".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx",
    ".inl", ".inc", ".rc", ".def", "._h",
}
INCLUDE_RE = re.compile(
    r"^(?P<prefix>[ \t]*#[ \t]*include[ \t]*)"
    r"(?P<open>[<\"])(?P<target>[^>\"\r\n]+)(?P<close>[>\"])(?P<suffix>[^\r\n]*)$",
    re.MULTILINE,
)


def project_files() -> list[Path]:
    files: list[Path] = []
    for path in PROJECT_ROOT.rglob("*"):
        if any(part in SKIP_DIRS for part in path.relative_to(PROJECT_ROOT).parts):
            continue
        if path.is_file():
            files.append(path)
    return files


def source_files(files: list[Path]) -> list[Path]:
    result = []
    for path in files:
        suffix = path.suffix.lower()
        if suffix in SOURCE_SUFFIXES or path.name.lower().endswith("._h"):
            result.append(path)
    return result


def compiler_include_roots() -> list[Path]:
    """Read the active C compiler's angle-bracket include search path."""
    result = subprocess.run(
        ["cc", "-E", "-x", "c", "-", "-v"],
        input=b"",
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        check=False,
    )
    roots: list[Path] = []
    collecting = False
    for raw_line in result.stderr.decode("utf-8", errors="replace").splitlines():
        line = raw_line.strip()
        if line == "#include <...> search starts here:":
            collecting = True
            continue
        if line == "End of search list.":
            break
        if collecting:
            candidate = Path(line)
            if candidate.is_dir() and candidate not in roots:
                roots.append(candidate)
    return roots


def block_comment_line_starts(text: str) -> set[int]:
    """Return line offsets that begin inside a C block comment."""
    blocked: set[int] = set()
    in_block = False
    offset = 0
    for line in text.splitlines(keepends=True):
        if in_block:
            blocked.add(offset)
        cursor = 0
        while cursor < len(line):
            if in_block:
                end = line.find("*/", cursor)
                if end < 0:
                    break
                in_block = False
                cursor = end + 2
                continue
            line_comment = line.find("//", cursor)
            block_comment = line.find("/*", cursor)
            if line_comment >= 0 and (block_comment < 0 or line_comment < block_comment):
                break
            if block_comment < 0:
                break
            in_block = True
            cursor = block_comment + 2
        offset += len(line)
    return blocked


def casefold_child(parent: Path, name: str) -> Path | None:
    if not parent.is_dir():
        return None
    matches = [child for child in parent.iterdir() if child.name.casefold() == name.casefold()]
    return matches[0] if len(matches) == 1 else None


def casefold_path(base: Path, target: str) -> Path | None:
    current = base
    for component in target.replace("\\", "/").split("/"):
        if not component or component == ".":
            continue
        if component == "..":
            current = current.parent
            continue
        current = casefold_child(current, component)
        if current is None:
            return None
    return current if current.is_file() else None


def include_spelling(base: Path, matched: Path) -> str:
    return matched.relative_to(base).as_posix()


def resolve_target(
    source: Path,
    target: str,
    basename_index: dict[str, list[Path]],
    relative_index: dict[str, list[Path]],
    system_include_roots: list[Path],
) -> tuple[str | None, str | None]:
    normalized = target.replace("\\", "/")
    search_bases = [source.parent, *INCLUDE_ROOTS]

    # If the compiler can already find this exact spelling, it is not a case error.
    for base in search_bases:
        if (base / normalized).is_file():
            return target, None

    # Follow compiler-like search order using case-insensitive component lookup.
    ordered_matches: list[tuple[Path, Path]] = []
    for base in search_bases:
        matched = casefold_path(base, normalized)
        if matched is not None and all(matched != item[1] for item in ordered_matches):
            ordered_matches.append((base, matched))
    if ordered_matches:
        base, matched = ordered_matches[0]
        return include_spelling(base, matched), None

    # Check headers available to the active Linux compiler. This catches
    # legacy spellings such as <Assert.h> without guessing unavailable Win32
    # or DirectX SDK headers.
    for base in system_include_roots:
        exact = base / normalized
        if exact.is_file():
            return target, None
        matched = casefold_path(base, normalized)
        if matched is not None:
            return include_spelling(base, matched), None

    # Fall back to a repository-wide spelling only when it is unambiguous.
    if "/" in normalized:
        candidates = relative_index.get(normalized.casefold(), [])
        spellings = {path.relative_to(PROJECT_ROOT).as_posix() for path in candidates}
    else:
        candidates = basename_index.get(normalized.casefold(), [])
        spellings = {path.name for path in candidates}

    if len(spellings) == 1:
        return next(iter(spellings)), None
    if len(spellings) > 1:
        return None, f"ambiguous candidates: {', '.join(sorted(spellings))}"
    return None, None


def sweep(check_only: bool) -> dict[str, object]:
    if not CANONICAL_BASETYPE.is_file():
        raise SystemExit(f"canonical header missing: {CANONICAL_BASETYPE}")

    files = project_files()
    system_include_roots = compiler_include_roots()
    basename_index: dict[str, list[Path]] = defaultdict(list)
    relative_index: dict[str, list[Path]] = defaultdict(list)
    for path in files:
        basename_index[path.name.casefold()].append(path)
        relative_index[path.relative_to(PROJECT_ROOT).as_posix().casefold()].append(path)

    changes: list[dict[str, object]] = []
    ambiguities: list[dict[str, str]] = []
    scanned_includes = 0

    for source in source_files(files):
        try:
            original = source.read_bytes().decode("latin-1")
        except OSError:
            continue

        file_changes: list[dict[str, object]] = []
        blocked_lines = block_comment_line_starts(original)

        def replace(match: re.Match[str]) -> str:
            nonlocal scanned_includes
            line_start = original.rfind("\n", 0, match.start()) + 1
            if line_start in blocked_lines:
                return match.group(0)
            scanned_includes += 1
            target = match.group("target")
            line = original.count("\n", 0, match.start()) + 1

            if Path(target.replace("\\", "/")).name.casefold() == "basetype.h":
                corrected = "basetype.h"
                problem = None
            else:
                corrected, problem = resolve_target(
                    source,
                    target,
                    basename_index,
                    relative_index,
                    system_include_roots,
                )

            if problem:
                ambiguities.append({
                    "file": source.relative_to(PROJECT_ROOT).as_posix(),
                    "line": str(line),
                    "include": target,
                    "reason": problem,
                })
                return match.group(0)
            if corrected is None or corrected == target:
                return match.group(0)

            file_changes.append({"line": line, "from": target, "to": corrected})
            return (
                f"{match.group('prefix')}{match.group('open')}{corrected}"
                f"{match.group('close')}{match.group('suffix')}"
            )

        updated = INCLUDE_RE.sub(replace, original)
        if file_changes:
            changes.append({
                "file": source.relative_to(PROJECT_ROOT).as_posix(),
                "replacements": file_changes,
            })
            if not check_only:
                source.write_bytes(updated.encode("latin-1"))

    return {
        "project_root": str(PROJECT_ROOT),
        "mode": "check" if check_only else "write",
        "scanned_includes": scanned_includes,
        "system_include_roots": [str(path) for path in system_include_roots],
        "changed_files": len(changes),
        "replacements": sum(len(item["replacements"]) for item in changes),
        "changes": changes,
        "ambiguities": ambiguities,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="report without writing")
    parser.add_argument("--report", type=Path, help="write a JSON report")
    args = parser.parse_args()

    report = sweep(args.check)
    rendered = json.dumps(report, indent=2)
    if args.report:
        args.report.write_text(rendered + "\n", encoding="utf-8")
    print(rendered)
    return 1 if args.check and report["replacements"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
