"""Regression tests for the macOS runtime bundle."""
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
from macos_runtime import RuntimeBundle


@unittest.skipUnless(sys.platform == "darwin" and shutil.which("cc") and shutil.which("install_name_tool"), "macOS compiler tools required")
class MacRuntimeTests(unittest.TestCase):
    def test_absolute_install_name_is_relocated(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            sdk = root / "sdk"
            sdk.mkdir()
            (root / "sdl.c").write_text("int sample(void) { return 1732; }")
            (root / "main.c").write_text("extern int sample(void); int main(void) { return sample() == 1732 ? 0 : 1; }")
            # Homebrew-style: the dylib's id is an absolute build-machine path.
            sdl = sdk / "libSDL3.0.dylib"
            subprocess.run(["cc", "-dynamiclib", str(root / "sdl.c"), "-o", str(sdl), "-install_name", str(sdl)], check=True, capture_output=True)
            package = root / "game"
            package.mkdir()
            subprocess.run(["cc", str(root / "main.c"), str(sdl), "-o", str(package / "puzzle-fighter-native"),
                            "-Wl,-rpath,@executable_path/lib"], check=True, capture_output=True)
            RuntimeBundle({"libSDL3.0.dylib": sdl}, ["coreaudio"]).install(package)
            sdk.rename(root / "unavailable-sdk")
            moved = root / "moved game"
            package.rename(moved)
            env = {key: value for key, value in os.environ.items() if not key.startswith("DYLD_")}
            subprocess.run([str(moved / "puzzle-fighter-native")], env=env, check=True)
            inventory = json.loads((moved / "runtime.json").read_text())
            self.assertIn("lib/libSDL3.0.dylib", inventory["libraries"])
            self.assertNotIn(str(sdk), json.dumps(inventory))

    def test_non_system_dependency_is_reported_before_packaging(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "dep.c").write_text("int dep(void) { return 1; }")
            (root / "sdl.c").write_text("extern int dep(void); int sample(void) { return dep(); }")
            dep = root / "libdep.dylib"
            subprocess.run(["cc", "-dynamiclib", str(root / "dep.c"), "-o", str(dep), "-install_name", str(dep)], check=True, capture_output=True)
            sdl = root / "libSDL3.0.dylib"
            subprocess.run(["cc", "-dynamiclib", str(root / "sdl.c"), str(dep), "-o", str(sdl), "-install_name", str(sdl)], check=True, capture_output=True)
            with self.assertRaisesRegex(ValueError, "Unsupported runtime dependency"):
                RuntimeBundle({"libSDL3.0.dylib": sdl}, ["coreaudio"])


if __name__ == "__main__":
    unittest.main()
