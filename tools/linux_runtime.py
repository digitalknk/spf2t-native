"""Collect SDL's Linux runtime dependencies into the exported game directory."""
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess

# These are the client libraries for SDL's Linux audio drivers, not a preferred
# backend. SDL keeps choosing the device/backend at runtime in its normal order.
AUDIO_LIBRARIES = {
    "alsa": "libasound.so", "pulseaudio": "libpulse.so",
    "pipewire": "libpipewire-0.3.so", "jack": "libjack.so", "sndio": "libsndio.so",
}
# Keep the host's libc/loader and graphics driver stack. Client libraries,
# including SDL's dlopen dependencies, travel with the game.
SYSTEM_LIBRARIES = re.compile(r"^(?:ld-linux.*|lib(?:c|m|mvec|dl|pthread|rt|resolv|util|anl)\.so(?:\..*)?)$")
VIDEO_CLIENTS = ("libwayland-", "libxkbcommon.so", "libX", "libudev.so", "libusb-1.0.so", "libdbus-1.so")


def command(*args):
    return subprocess.check_output([str(arg) for arg in args], text=True).strip()


def rpaths(path):
    origin = str(path.parent)
    return [Path(value.replace("${ORIGIN}", origin).replace("$ORIGIN", origin))
            for value in command("patchelf", "--print-rpath", path).split(":") if value]


def dynamic_names(path):
    # SDL compiles the SONAMEs passed to SDL_LoadObject into its shared object.
    # Read whole string constants, avoiding partial matches and diagnostic text.
    return set(value.decode("ascii") for value in re.findall(
        rb"(?:^|\x00)(lib[A-Za-z0-9_+.-]+\.so(?:\.[A-Za-z0-9_.-]+)*)(?=\x00)", path.read_bytes()))


class RuntimeBundle:
    def __init__(self, roots, audio_drivers, compiler):
        self.libraries = dict(roots)
        self.audio_drivers = audio_drivers
        self.resources = []
        self.search = []
        self.cache = {}
        for path in roots.values():
            self.search.extend([path.parent, *rpaths(path)])
        search = command(compiler, "-print-search-dirs")
        for line in search.splitlines():
            if line.startswith("libraries: ="):
                self.search.extend(Path(value) for value in line.split("=", 1)[1].split(os.pathsep) if value)
        if shutil.which("ldconfig"):
            cache = subprocess.run(["ldconfig", "-p"], text=True, capture_output=True)
            for line in cache.stdout.splitlines() if cache.returncode == 0 else ():
                match = re.match(r"\s*(\S+)\s+.*=>\s+(\S+)", line)
                if match:
                    self.cache.setdefault(match[1], []).append(Path(match[2]))
        sdl = roots["libSDL3.so.0"]
        names = dynamic_names(sdl)
        for driver in audio_drivers:
            if driver in ("disk", "dummy", "dsp"):
                continue
            prefix = AUDIO_LIBRARIES.get(driver)
            if prefix is None:
                raise ValueError("Runtime packaging does not yet describe this SDL audio driver: " + driver)
            candidates = sorted(name for name in names if name == prefix or name.startswith(prefix + "."))
            # A driver may be directly linked rather than loaded dynamically.
            candidates += [name for name in command("patchelf", "--print-needed", sdl).splitlines()
                           if name == prefix or name.startswith(prefix + ".")]
            if not candidates:
                raise ValueError("Cannot identify the SDL client library for audio driver: " + driver)
            for name in set(candidates):
                self.libraries[name] = self.resolve(name, sdl)
        for name in sorted(names):
            if name.startswith(VIDEO_CLIENTS):
                try:
                    self.libraries[name] = self.resolve(name, sdl)
                except ValueError:
                    # SDL also contains optional compatibility SONAMEs (udev0,
                    # X11-xcb, etc.). Do not require a second ABI when absent.
                    pass
        self._resources()
        owners = list(self.libraries.values())
        for source, _ in self.resources:
            owners.extend(path for path in source.rglob("*.so*") if path.is_file())
        processed = set()
        while owners:
            owner = owners.pop().resolve()
            if owner in processed:
                continue
            processed.add(owner)
            for name in command("patchelf", "--print-needed", owner).splitlines():
                if SYSTEM_LIBRARIES.match(Path(name).name):
                    continue
                resolved = self.resolve(name, owner)
                basename = Path(name).name
                # A process loads one instance per SONAME. Reuse the first
                # selection, then check symbol resolution after relocation.
                if basename not in self.libraries:
                    self.libraries[basename] = resolved
                owners.append(self.libraries[basename])

    def resolve(self, name, owner):
        path = Path(name)
        if path.is_absolute() and path.is_file():
            return path.resolve()
        directories = [owner.parent, *rpaths(owner), *self.search]
        candidates = [directory / name for directory in directories] + self.cache.get(name, [])
        for candidate in candidates:
            if candidate.is_file():
                return candidate.resolve()
        raise ValueError("Missing runtime library " + name + " required by " + str(owner) +
                         ". Install the SDL backend runtime/development package before building.")

    def _resources(self):
        def add(source, destination, required=False):
            if source.is_dir():
                self.resources.append((source, Path(destination)))
            elif required:
                raise ValueError("Missing audio runtime data directory: " + str(source))
        def data(path, name):
            for prefix in path.parents:
                candidate = prefix / "share" / name
                if candidate.is_dir():
                    return candidate
            raise ValueError("Missing " + name + " data for " + str(path) + ". Install the audio backend runtime data package before building.")
        for name, path in list(self.libraries.items()):
            if name.startswith("libasound.so"):
                add(data(path, "alsa"), "share/alsa", required=True)
                add(path.parent / "alsa-lib", "lib/alsa-lib")
            if name.startswith("libpipewire-0.3.so"):
                add(path.parent / "pipewire-0.3", "lib/pipewire-0.3", required=True)
                add(path.parent / "spa-0.2", "lib/spa-0.2", required=True)
                add(data(path, "pipewire"), "share/pipewire", required=True)
        # Multiarch distributions place data above lib/<triplet>/ instead.
        # resolve resources independently from library search/host configuration.

    def install(self, package):
        lib = package / "lib"
        lib.mkdir()
        for name, source in self.libraries.items():
            shutil.copy2(source, lib / name)
        for source, relative in self.resources:
            shutil.copytree(source, package / relative, dirs_exist_ok=True)
        for path in lib.rglob("*"):
            path.chmod(path.stat().st_mode | stat.S_IWUSR)
            if not path.is_file():
                continue
            with path.open("rb") as incoming:
                if incoming.read(4) != b"\x7fELF":
                    continue
            for needed in command("patchelf", "--print-needed", path).splitlines():
                if Path(needed).is_absolute() and Path(needed).name in self.libraries:
                    subprocess.run(["patchelf", "--replace-needed", needed, Path(needed).name, str(path)], check=True)
            relative = os.path.relpath(lib, path.parent)
            value = "$ORIGIN" if relative == "." else "$ORIGIN:$ORIGIN/" + relative
            subprocess.run(["patchelf", "--set-rpath", value, str(path)], check=True)
        environment = dict(os.environ, LD_LIBRARY_PATH=str(lib))
        environment.pop("LD_PRELOAD", None)
        for name in ("libSDL3.so.0", *[name for name in self.libraries if any(name.startswith(prefix) for prefix in AUDIO_LIBRARIES.values())]):
            result = subprocess.run(["ldd", "-r", str(lib / name)], env=environment, text=True, capture_output=True)
            if result.returncode or re.search(r"not found|undefined symbol:", result.stdout + result.stderr):
                raise ValueError("Packaged runtime does not resolve for " + name + ":\n" + result.stdout + result.stderr)
        # An inventory lists only portable relative paths, never build-machine
        # paths. It also makes package dependency completeness reviewable.
        inventory = {str(path.relative_to(package)): hashlib.sha256(path.read_bytes()).hexdigest()
                     for path in sorted(lib.rglob("*")) if path.is_file()}
        (package / "runtime.json").write_text(json.dumps(
            {"audio_drivers": self.audio_drivers, "libraries": inventory}, indent=2) + "\n")

    def copy_notices(self, destination):
        """Retain installed package notices when the build system provides them."""
        destination.mkdir()
        index = {}
        seen = {}
        for name, source in self.libraries.items():
            candidates = []
            if shutil.which("dpkg-query"):
                result = subprocess.run(["dpkg-query", "-S", str(source)], text=True, capture_output=True)
                if result.returncode == 0:
                    for line in result.stdout.splitlines():
                        package = line.split(": ", 1)[0].split(":", 1)[0]
                        candidates.append(Path("/usr/share/doc") / package / "copyright")
            # SDK layouts such as /prefix/lib plus /prefix/share/doc also work
            # without requiring a particular distribution's package manager.
            prefix = source.parent.parent
            for pattern in ("share/doc/*/copyright", "share/doc/*/COPYING*", "share/licenses/*/LICENSE*"):
                candidates.extend(prefix.glob(pattern))
            names = []
            for path in candidates:
                if not path.is_file():
                    continue
                data = path.read_bytes()
                digest = hashlib.sha256(data).hexdigest()
                if digest not in seen:
                    filename = digest[:16] + "-" + path.name
                    (destination / filename).write_bytes(data)
                    seen[digest] = filename
                names.append(seen[digest])
            index[name] = sorted(set(names))
        (destination / "index.json").write_text(json.dumps(index, indent=2) + "\n")
