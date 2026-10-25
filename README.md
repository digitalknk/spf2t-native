# Gemmy Turbo — Puzzle Fighter native port

[gemmy-turbo/spf2t-native](https://github.com/gemmy-turbo/spf2t-native)

An unofficial native Linux port of Super Puzzle Fighter II Turbo (USA 960620).
Initial source release: `0.1.0-alpha.1`. Original game content is supplied locally
and is not included. Recovered C++ game logic compiles for Linux. Audio uses
a native sequencer and high-level QSound software mixer with original
sequence/sample data. See [KNOWN_LIMITATIONS.md](KNOWN_LIMITATIONS.md).

## Quick start with Docker or Podman

Place your supported USA960620 ROM ZIP(s) or extracted files in `roms/`, then
run from this repository:

```sh
docker build -t spf2t-native:aio .
mkdir -p output
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD/roms:/roms:ro" -v "$PWD/output:/output" spf2t-native:aio
./output/spf2t-native/puzzle-fighter-native
```

The image includes the decomp tooling project and builds both stages. For
Podman, replace `docker` with `podman` and add `--userns=keep-id` to the run
command. See [CONTAINERS.md](CONTAINERS.md) for native-only images and local-source builds.

## Build with installed tools

Requirements: Python 3.9+, CMake 3.20+, Make, a C++17 compiler with GNU-compatible
C++ runtime libraries, and shared SDL3 3.4.16+ development files. SDL must be
discoverable by CMake; set `CMAKE_PREFIX_PATH` for a custom installation.

First run the [decomp process](https://github.com/gemmy-turbo/spf2t-decomp) to
produce a complete handoff, then run:

```sh
python3 tools/build.py --decomp /path/to/output/decomp --build-dir /path/to/build --output /path/to/output/spf2t-native
```

The build uses the complete handoff from `output/decomp/`, checks its files and
checksums, and verifies the compiler/CMake/SDL environment before compiling.
See the [decomp project](https://github.com/gemmy-turbo/spf2t-decomp) for the
output format documentation.

## Repository defaults

With the complete decomp handoff in this repository's `output/decomp/`:

```sh
python3 build.py
```

This uses `output/decomp/`, builds in `build/` and writes the game to
`output/spf2t-native/`. `python3 build.py --dry-run` prints the full build command
so you can copy it and adjust the paths or options.

## Run the game

```sh
./output/spf2t-native/puzzle-fighter-native
```

Run the executable on your host. Move the complete `spf2t-native/` directory
together: executable, `data/`, `lib/`, and `licenses/`. Data and default writable `user/` paths are
resolved relative to the executable, independent of the working directory.
SDL3 and C++ runtime libraries are bundled; a compatible Linux loader/glibc
and system display/audio backends are still required. NixOS needs platform ELF
packaging or a build using its native toolchain. The output directory
contains the executable and its supporting files. See CONTAINERS.md for the
Linux runtime baseline.

A rebuild preserves the prior output beside the new one and carries player
state forward. Imported data is immutable; `--data-dir` and `--user-dir` remain
available for explicit game runtime overrides. Remove the output directory to
uninstall it, including its player state if no longer needed.

Enter starts a quick match. P1: A/D move, S drop, C/V rotate. P2: arrows move/drop,
J/K rotate. Both keyboard layouts control P1 against CPU; mapped gamepads work.
F8 enters original arcade; 5/6 insert coins; Enter/keypad Enter or gamepad Start
plays. F9 changes layout; F10 opens enhanced options. R is quick-rematch only.
Space pauses; Escape quits. `--help` lists runtime/debug options.

## License and AI assistance

Source is GPL-3.0-only unless a file says otherwise. See LICENSE,
THIRD_PARTY_NOTICES.md, UPSTREAM.md and [LICENSE_SCOPE.md](LICENSE_SCOPE.md).
The source license grants no rights over original game content. The project
is unofficial and is not affiliated with or endorsed by Capcom. Do not commit
ROMs, imported/generated game data or derived game builds to public source.
Required upstream notices remain. Windows/macOS have not been validated.

AI tools were used extensively in reverse engineering, implementation, testing
and documentation. Source recovery remains incomplete; passing bounded checks
does not establish whole-game or hardware-identical parity.
