"""Bundle SDL's macOS runtime into the exported game directory."""
import hashlib
import json
from pathlib import Path
import shutil
import stat
import subprocess

# macOS ships libc++ and the frameworks SDL uses (CoreAudio, Metal, ...), so only
# SDL itself travels with the game. Anything else must come from the OS.
SYSTEM_PREFIXES = ("/usr/lib/", "/System/Library/")


def command(*args):
    return subprocess.check_output([str(arg) for arg in args], text=True).strip()


def install_name(path):
    return command("otool", "-D", path).splitlines()[-1]


def linked(path):
    # The first line names the file. For a dylib the next one is its own id.
    lines = command("otool", "-L", path).splitlines()[1:]
    names = [line.strip().split(" (")[0] for line in lines]
    if path.suffix == ".dylib" and names and names[0] == install_name(path):
        names = names[1:]
    return names


class RuntimeBundle:
    def __init__(self, roots, audio_drivers):
        self.libraries = {name: Path(path).resolve() for name, path in roots.items()}
        self.audio_drivers = audio_drivers
        for name, path in self.libraries.items():
            for dependency in linked(path):
                if not dependency.startswith(SYSTEM_PREFIXES):
                    raise ValueError("Unsupported runtime dependency of " + name + ": " + dependency)

    def install(self, package):
        executable = package / "puzzle-fighter-native"
        lib = package / "lib"
        lib.mkdir()
        rewrites = {}
        for name, source in self.libraries.items():
            target = lib / name
            shutil.copy2(source, target)
            target.chmod(target.stat().st_mode | stat.S_IWUSR)
            rewrites[install_name(source)] = "@rpath/" + name
            subprocess.run(["install_name_tool", "-id", "@rpath/" + name, str(target)], check=True, capture_output=True)
        for path in (executable, *lib.iterdir()):
            for dependency in linked(path):
                if dependency in rewrites and dependency != rewrites[dependency]:
                    subprocess.run(["install_name_tool", "-change", dependency, rewrites[dependency], str(path)], check=True, capture_output=True)
            # Editing load commands invalidates the signature, and Apple Silicon
            # refuses to run unsigned code. An ad-hoc signature is enough locally.
            subprocess.run(["codesign", "--force", "--sign", "-", str(path)], check=True, capture_output=True)
        for path in (executable, *lib.iterdir()):
            for dependency in linked(path):
                if not dependency.startswith(SYSTEM_PREFIXES + ("@rpath/",)):
                    raise ValueError("Packaged runtime still references a build-machine path: " + dependency)
        inventory = {str(path.relative_to(package)): hashlib.sha256(path.read_bytes()).hexdigest()
                     for path in sorted(lib.rglob("*")) if path.is_file()}
        (package / "runtime.json").write_text(json.dumps(
            {"audio_drivers": self.audio_drivers, "libraries": inventory, "resource_roots": []}, indent=2) + "\n")

    def copy_notices(self, destination):
        """SDL's license is already copied from third_party/; keep the same layout as Linux."""
        destination.mkdir()
        (destination / "index.json").write_text(json.dumps({name: [] for name in self.libraries}, indent=2) + "\n")
