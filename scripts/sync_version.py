#!/usr/bin/env python3

from __future__ import annotations

import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
VERSION = (ROOT / "version.txt").read_text(encoding="utf-8").strip()

if not re.fullmatch(r"\d+\.\d+\.\d+", VERSION):
    raise SystemExit(f"Invalid version in version.txt: {VERSION!r}")


def write_text_if_changed(path: Path, content: str) -> None:
    old = read_text(path)
    if old != content:
        write_text(path, content)


def read_text(path: Path) -> str:
    data = path.read_bytes()
    for encoding in ("utf-8", "utf-8-sig", "cp1252", "latin-1"):
        try:
            return data.decode(encoding).replace("\r\n", "\n").replace("\r", "\n")
        except UnicodeDecodeError:
            continue
    raise SystemExit(f"Could not decode text file: {path}")


def write_text(path: Path, content: str) -> None:
    path.write_text(content, encoding="utf-8", newline="\n")


def update_json(path: Path, updater) -> None:
    data = json.loads(read_text(path))
    updater(data)
    formatted = json.dumps(data, ensure_ascii=False, indent=2) + "\n"
    write_text_if_changed(path, formatted)


def update_regex(path: Path, pattern: str, replacement: str) -> None:
    old = read_text(path)
    new, count = re.subn(pattern, replacement, old, flags=re.MULTILINE)
    if count == 0:
        raise SystemExit(f"Pattern not found in {path}: {pattern}")
    if old != new:
        write_text(path, new)


def sync_cmake() -> None:
    path = ROOT / "CMakeLists.txt"
    content = read_text(path)
    legacy_pattern = r"project\(avioflow VERSION \d+\.\d+\.\d+ LANGUAGES CXX\)"
    modern_pattern = (
        r'file\(READ "\$\{CMAKE_CURRENT_SOURCE_DIR\}/version\.txt" AVIOFLOW_VERSION_RAW\)\r?\n'
        r'string\(STRIP "\$\{AVIOFLOW_VERSION_RAW\}" AVIOFLOW_VERSION\)\r?\n'
        r'project\(avioflow VERSION \$\{AVIOFLOW_VERSION\} LANGUAGES CXX\)'
    )
    modern_snippet = (
        'file(READ "${CMAKE_CURRENT_SOURCE_DIR}/version.txt" AVIOFLOW_VERSION_RAW)\n'
        'string(STRIP "${AVIOFLOW_VERSION_RAW}" AVIOFLOW_VERSION)\n'
        'project(avioflow VERSION ${AVIOFLOW_VERSION} LANGUAGES CXX)'
    )
    if re.search(legacy_pattern, content):
        new = re.sub(legacy_pattern, modern_snippet, content, flags=re.MULTILINE)
        write_text_if_changed(path, new)
    elif not re.search(modern_pattern, content):
        raise SystemExit(f"Unsupported CMake version block in {path}")


def sync_python() -> None:
    path = ROOT / "python" / "pyproject.toml"
    update_regex(
        path,
        r'^\s*version\s*=\s*"\d+\.\d+\.\d+"\r?$',
        f'version = "{VERSION}"',
    )


def sync_node_package() -> None:
    node_platform_packages = (
        "@lxp3/linux-x64",
        "@lxp3/linux-arm64",
        "@lxp3/darwin-x64",
        "@lxp3/darwin-arm64",
        "@lxp3/win32-x64",
        "@lxp3/win32-arm64",
    )

    def update_node_root(data: dict) -> None:
        data["version"] = VERSION
        optional_deps = data.setdefault("optionalDependencies", {})
        for package_name in node_platform_packages:
            optional_deps[package_name] = VERSION

    update_json(ROOT / "nodejs" / "package.json", update_node_root)

    for package_name in node_platform_packages:
        package_dir = package_name.removeprefix("@lxp3/")
        update_json(
            ROOT / "nodejs" / "npm-packages" / "@lxp3" / package_dir / "package.json",
            lambda data: data.__setitem__("version", VERSION),
        )

    def update_package_lock(data: dict) -> None:
        data["version"] = VERSION
        root_pkg = data.get("packages", {}).get("")
        if isinstance(root_pkg, dict):
            root_pkg["version"] = VERSION

    update_json(ROOT / "nodejs" / "package-lock.json", update_package_lock)


def sync_wasm() -> None:
    update_json(
        ROOT / "wasm" / "package.json",
        lambda data: data.__setitem__("version", VERSION),
    )


def sync_vscode() -> None:
    update_json(
        ROOT / "vscode-extension" / "package.json",
        lambda data: data.__setitem__("version", VERSION),
    )
    update_regex(
        ROOT / "vscode-extension" / "README.md",
        r"avioflow-\d+\.\d+\.\d+\.vsix",
        f"avioflow-{VERSION}.vsix",
    )


def main() -> None:
    sync_cmake()
    sync_python()
    sync_node_package()
    sync_wasm()
    sync_vscode()
    print(f"Synchronized repository version to {VERSION}")


if __name__ == "__main__":
    main()
