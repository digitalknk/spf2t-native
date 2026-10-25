#!/usr/bin/env python3
"""Preflight, compile and package a native game from a complete decomp output."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MISSING = "Run spf2t-decomp first with the same output directory, or use the native AIO image."


def verify_input(path):
    try:
        manifest = json.loads((path / "manifest.json").read_text())
    except FileNotFoundError:
        raise ValueError("Missing decomp output: " + str(path) + ". " + MISSING)
    if manifest.get("format") != "spf2t-decomp-v1" or manifest.get("api_version") != 1 or manifest.get("game_revision") != "USA960620":
        raise ValueError("Unsupported decomp output format/API/revision. " + MISSING)
    required = {"data/manifest.json", "data/tables/main-program.bin", "data/tables/main-opcodes.bin", "data/assets/graphics.bin", "data/assets/samples.bin", "data/assets/sound-program.bin", "data/generated/puzzle_fighter/generated/source_howto_sequence.inc", "licenses/LICENSE", "licenses/LICENSE_SCOPE.md", "licenses/THIRD_PARTY_NOTICES.md", "licenses/UPSTREAM.md", "licenses/BSD-3-Clause.txt"}
    for source in (ROOT / "src").glob("*"):
        if source.suffix in (".hpp", ".cpp"):
            required.update("source/include/puzzle_fighter/" + name for name in re.findall(r"#include\s*<puzzle_fighter/([^>]+)>", source.read_text()))
    if not required.issubset(manifest["files"]):
        raise ValueError("Incomplete decomp output inventory. " + MISSING)
    for relative, record in manifest["files"].items():
        item = (path / relative).resolve()
        item.relative_to(path.resolve())
        if not item.is_file():
            raise ValueError("Missing decompiled file: " + relative + ". " + MISSING)
        if item.stat().st_size != record["size"] or hashlib.sha256(item.read_bytes()).hexdigest() != record["sha256"]:
            raise ValueError("Decomp output checksum mismatch: " + relative + ". " + MISSING)
    return manifest


def preflight(args):
    errors = []
    if sys.version_info < (3, 9):
        errors.append("Python 3.9 or newer is required")
    try:
        verify_input(args.decomp.resolve())
    except (ValueError, KeyError, OSError) as error:
        errors.append(str(error))
    for name in (args.cmake, args.cxx, "make"):
        if shutil.which(name) is None:
            errors.append("Required build tool not found: " + name)
    for directory in (args.output, args.build_dir):
        parent = directory.resolve()
        while not parent.exists():
            parent = parent.parent
        if not parent.is_dir() or not os.access(parent, os.W_OK | os.X_OK):
            errors.append("Directory is not writable: " + str(directory))
    output = args.output.resolve()
    decomp = args.decomp.resolve()
    if ROOT.is_relative_to(output) or output.is_relative_to(decomp) or decomp.is_relative_to(output):
        errors.append("Native output must be separate from project source and decomp input")
    runtimes = {}
    if shutil.which(args.cxx):
        for name in ("libstdc++.so.6", "libgcc_s.so.1"):
            result = subprocess.run([args.cxx, "-print-file-name=" + name], capture_output=True, text=True)
            path = Path(result.stdout.strip())
            if result.returncode or not path.is_file():
                errors.append("Compiler runtime library not found: " + name)
            else:
                runtimes[name] = path
    for relative in ("LICENSE", "LICENSE_SCOPE.md", "THIRD_PARTY_NOTICES.md", "UPSTREAM.md", "third_party/BSD-3-Clause.txt", "third_party/SDL3-LICENSE.txt", "third_party/GCC-runtime-copyright.txt"):
        if not (ROOT / relative).is_file():
            errors.append("Required notice is missing: " + relative)
    if errors:
        raise ValueError("Preflight failed:\n- " + "\n- ".join(errors))
    # Probe the actual compiler/CMake/SDL combination without starting the build
    # or writing to the user's selected build/output directories.
    with tempfile.TemporaryDirectory(prefix="spf2t-preflight-") as temporary:
        probe = Path(temporary)
        (probe / "CMakeLists.txt").write_text('cmake_minimum_required(VERSION 3.20)\nproject(preflight LANGUAGES CXX)\nfile(WRITE "${CMAKE_BINARY_DIR}/check.cpp" "#include <filesystem>\\nint main() { std::filesystem::path p; }\\n")\nadd_executable(check "${CMAKE_BINARY_DIR}/check.cpp")\ntarget_compile_features(check PRIVATE cxx_std_17)\nfind_package(SDL3 3.4.16 REQUIRED CONFIG)\nfile(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/sdl-runtime.txt" CONTENT "$<TARGET_FILE:SDL3::SDL3>")\n')
        result = subprocess.run([args.cmake, "-S", str(probe), "-B", str(probe / "build"), "-DCMAKE_CXX_COMPILER=" + args.cxx], text=True, capture_output=True)
        if result.returncode:
            raise ValueError("Build environment preflight failed (C++17/CMake/SDL3):\n" + result.stdout + result.stderr)
        check = subprocess.run([args.cmake, "--build", str(probe / "build")], text=True, capture_output=True)
        if check.returncode:
            raise ValueError("Compiler/linker preflight failed:\n" + check.stdout + check.stderr)
        sdl = Path((probe / "build/sdl-runtime.txt").read_text().strip())
        if not sdl.is_file() or ".so" not in sdl.name:
            raise ValueError("A shared Linux SDL3 library is required for native packaging")
        runtimes["libSDL3.so.0"] = sdl
    return runtimes


def build(args):
    runtimes = preflight(args)
    decomp = args.decomp.resolve()
    build_dir = args.build_dir.resolve()
    output = args.output.resolve()
    subprocess.run([args.cmake, "-S", str(ROOT), "-B", str(build_dir), "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_CXX_COMPILER=" + args.cxx, "-DPF_DECOMP_OUTPUT=" + str(decomp), "-DCMAKE_INSTALL_RPATH=$ORIGIN/lib"], check=True)
    subprocess.run([args.cmake, "--build", str(build_dir), "--parallel", "2"], check=True)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".native-package-", dir=output.parent) as temporary:
        stage = Path(temporary)
        package = stage / "game"
        subprocess.run([args.cmake, "--install", str(build_dir), "--prefix", str(package)], check=True)
        shutil.copytree(decomp / "data", package / "data")
        lib = package / "lib"
        lib.mkdir()
        for name, path in runtimes.items():
            shutil.copy2(path, lib / name)
        notices = package / "licenses"
        notices.mkdir()
        for name in ("LICENSE", "LICENSE_SCOPE.md", "THIRD_PARTY_NOTICES.md", "UPSTREAM.md"):
            shutil.copy2(ROOT / name, notices / name)
        for name in ("BSD-3-Clause.txt", "SDL3-LICENSE.txt", "GCC-runtime-copyright.txt"):
            shutil.copy2(ROOT / "third_party" / name, notices / name)
        shutil.copytree(decomp / "licenses", notices / "decomp")
        if output.exists():
            backup = Path(tempfile.mkdtemp(prefix=output.name + ".previous-", dir=output.parent))
            backup.rmdir()
            output.rename(backup)
            try:
                if (backup / "user").exists():
                    shutil.copytree(backup / "user", package / "user")
                package.rename(output)
            except BaseException:
                backup.rename(output)
                raise
            print("Previous output preserved: " + str(backup))
        else:
            package.rename(output)
    print("Run the native executable directly: " + str(output / "puzzle-fighter-native"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--decomp", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--cxx", default=os.environ.get("CXX", "c++"))
    args = parser.parse_args()
    try:
        build(args)
    except (ValueError, KeyError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + "\n")


if __name__ == "__main__":
    main()
