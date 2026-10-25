"""Regression tests for libraries SDL loads after application startup."""
from pathlib import Path
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from linux_runtime import RuntimeBundle


@unittest.skipUnless(sys.platform.startswith("linux") and shutil.which("cc") and shutil.which("patchelf"), "Linux compiler and patchelf required")
class RuntimeTests(unittest.TestCase):
    def library(self, root, name, source, *extra):
        path = root / (name + ".c")
        path.write_text(source)
        subprocess.run(["cc", "-shared", "-fPIC", str(path), "-o", str(root / name),
                        "-Wl,-soname," + name, "-Wl,-rpath,$ORIGIN", *extra], check=True, capture_output=True)
        return root / name

    def test_dlopen_backend_and_transitive_dependency_survive_relocation(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            sdk = root / "sdk"
            sdk.mkdir()
            child = self.library(sdk, "libpf_audio_test.so.1", "int sample(void) { return 1732; }")
            pulse = self.library(sdk, "libpulse.so.0", "extern int sample(void); int playback(void) { return sample(); }", str(child))
            sdl = self.library(sdk, "libSDL3.so.0", 'const char *backend = "libpulse.so.0";')
            for library in (child, pulse, sdl):
                library.chmod(0o444)  # Read-only SDKs must remain valid build inputs.
            plan = RuntimeBundle({"libSDL3.so.0": sdl}, ["pulseaudio", "dummy"], "cc")
            package = root / "game"
            package.mkdir()
            plan.install(package)
            sdk.rename(root / "unavailable-sdk")
            moved = root / "moved game"
            package.rename(moved)
            env = {key: value for key, value in os.environ.items() if key not in ("LD_LIBRARY_PATH", "LD_PRELOAD")}
            subprocess.run([sys.executable, "-c", 'import ctypes,sys; p=ctypes.CDLL(sys.argv[1]); assert p.playback()==1732', str(moved / "lib/libpulse.so.0")], env=env, check=True, capture_output=True)
            inventory = json.loads((moved / "runtime.json").read_text())
            self.assertIn("lib/libpulse.so.0", inventory["libraries"])
            self.assertIn("lib/libpf_audio_test.so.1", inventory["libraries"])
            self.assertNotIn(str(sdk), json.dumps(inventory))

    def test_missing_dynamic_audio_library_is_reported_before_packaging(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            sdl = self.library(root, "libSDL3.so.0", 'const char *backend = "libpulse.so.99999";')
            with self.assertRaisesRegex(ValueError, "Missing runtime library libpulse.so.99999"):
                RuntimeBundle({"libSDL3.so.0": sdl}, ["pulseaudio"], "cc")


if __name__ == "__main__":
    unittest.main()
