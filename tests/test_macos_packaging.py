"""Keep the macOS bundle from loading libraries off the build machine.

macdeployqt leaves the build machine's rpaths and install names behind, and on
a Mac with Homebrew Qt one rpath into /opt/homebrew was enough for dyld to load
a second QtCore next to the bundled one. build-dmg.sh strips them;
check-bundle-paths.sh is the guard it and verify-dmg.sh run.

The fixtures are real Mach-O files built with clang, so otool,
install_name_tool and ad-hoc codesign run for real. macdeployqt, dmgbuild and
hdiutil are stand-ins that log their calls.

Run with: python3 -m unittest discover -s tests -p 'test_macos_packaging.py' -v
"""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
APP = "Sony Device Center.app"
MOCK = r'''#!/usr/bin/env python3
import json, os, pathlib, shutil, sys
tool = pathlib.Path(sys.argv[0]).name
args = sys.argv[1:]
with open(os.environ["MOCK_LOG"], "a") as log:
    log.write(json.dumps([tool, *args]) + "\n")
real = os.environ.get("MOCK_REAL_" + tool)
if real:
    os.execv(real, [real, *args])
if tool == "macdeployqt":
    shutil.copytree(os.environ["MOCK_DEPLOY"], pathlib.Path(args[0]) / "Contents", dirs_exist_ok=True)
elif tool == "dmgbuild":
    pathlib.Path(args[-1]).touch()
elif tool == "hdiutil" and args[0] == "attach":
    destination = args[args.index("-mountpoint") + 1]
    shutil.copytree(os.environ["MOCK_STAGING"], destination, dirs_exist_ok=True, symlinks=True)
elif tool == "hdiutil" and args[0] == "detach":
    shutil.rmtree(args[1])
'''
INFO_PLIST = '''<?xml version="1.0" encoding="UTF-8"?>
<plist version="1.0"><dict>
<key>CFBundleExecutable</key><string>sony-device-center</string>
<key>CFBundleIdentifier</key><string>test.sony-device-center</string>
<key>CFBundlePackageType</key><string>APPL</string>
</dict></plist>
'''
REPORT = '#include <stdio.h>\nvoid report(void) { puts("%s"); }\n'
MAIN = 'void report(void);\nint main(void) { report(); return 0; }\n'


def load_paths(binary, command):
    """The paths of one kind of load command, each slice of a universal file folded."""
    output = subprocess.run(["otool", "-l", str(binary)], check=True, capture_output=True, text=True).stdout
    paths, current = [], None
    for line in output.splitlines():
        field, _, value = line.strip().partition(" ")
        if field == "cmd":
            current = value
        elif current == command and field in ("path", "name"):
            path = value.rsplit(" (offset ", 1)[0]
            if path not in paths:
                paths.append(path)
    return paths


@unittest.skipUnless(sys.platform == "darwin" and shutil.which("clang"), "needs macOS and clang")
class MacBundlePathTests(unittest.TestCase):
    def setUp(self):
        # The space checks that every path survives the scripts' quoting.
        self.temp = tempfile.TemporaryDirectory(prefix="sony packaging ")
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.bin = self.directory / "bin"
        self.bin.mkdir()
        for tool in ("macdeployqt", "dmgbuild", "codesign", "install_name_tool", "hdiutil"):
            path = self.bin / tool
            path.write_text(MOCK)
            path.chmod(0o755)
        self.log = self.directory / "calls.jsonl"
        self.env = {key: value for key, value in os.environ.items() if not key.startswith("SONY_")}
        self.env.update(PATH=f"{self.bin}:{os.environ['PATH']}", MOCK_LOG=str(self.log),
                        MOCK_REAL_install_name_tool=shutil.which("install_name_tool"))

    def calls(self):
        return [json.loads(line) for line in self.log.read_text().splitlines()] if self.log.exists() else []

    def run_script(self, script, *args):
        return subprocess.run(["bash", str(ROOT / "packaging/macos" / script), *map(str, args)],
                              env=self.env, capture_output=True, text=True)

    def compile(self, output, source, *flags):
        output.parent.mkdir(parents=True, exist_ok=True)
        source_file = self.directory / f"{output.name}.c"
        source_file.write_text(source)
        subprocess.run(["clang", "-arch", "arm64", "-arch", "x86_64", "-o", str(output), str(source_file), *flags],
                       check=True, capture_output=True)
        return output

    def library(self, output, install_name, *flags, says="bundled"):
        return self.compile(output, REPORT % says, "-dynamiclib", "-install_name", install_name, *flags)

    def make_build(self):
        """A build tree whose app finds its Qt stand-in through a Homebrew-style
        build rpath, plus what macdeployqt would copy into the bundle."""
        homebrew = self.directory / "homebrew" / "lib"
        self.library(homebrew / "libqt.dylib", "@rpath/libqt.dylib", says="homebrew")
        build = self.directory / "build"
        app = build / "apps/device-center/sony-device-center.app/Contents"
        self.compile(app / "MacOS/sony-device-center", MAIN, "-L", str(homebrew), "-lqt", f"-Wl,-rpath,{homebrew}")
        (app / "Info.plist").write_text(INFO_PLIST)
        for name in ("sonyd", "sonyctl"):
            self.compile(build / "apps" / name / name, "int main(void) { return 0; }\n")
        deploy = self.directory / "deploy"
        self.library(deploy / "Frameworks/libqt.dylib", f"{homebrew}/libqt.dylib",
                     "-Wl,-rpath,/opt/homebrew/Cellar/qtbase/6.11.2/lib", "-Wl,-rpath,@loader_path")
        self.compile(deploy / "Resources/qml/Fake/libfakeplugin.dylib", "void plugin(void) {}\n",
                     "-bundle", "-Wl,-rpath,/usr/local/lib")
        self.env["MOCK_DEPLOY"] = str(deploy)
        return build

    def test_build_strips_paths_to_the_build_machine_before_signing(self):
        build = self.make_build()
        built = build / "apps/device-center/sony-device-center.app/Contents/MacOS/sony-device-center"
        # The fixture has the bug: the build-tree binary resolves the decoy.
        self.assertEqual(subprocess.run([built], capture_output=True, text=True).stdout, "homebrew\n")

        self.env["MOCK_REAL_codesign"] = shutil.which("codesign")
        result = self.run_script("build-dmg.sh", build)
        self.assertEqual(result.returncode, 0, result.stderr)

        app = build / "dmg-staging" / APP
        contents = app / "Contents"
        self.assertEqual(load_paths(contents / "MacOS/sony-device-center", "LC_RPATH"),
                         ["@executable_path/../Frameworks"])
        self.assertEqual(load_paths(contents / "Frameworks/libqt.dylib", "LC_RPATH"), ["@loader_path"])
        self.assertEqual(load_paths(contents / "Frameworks/libqt.dylib", "LC_ID_DYLIB"),
                         ["@executable_path/../Frameworks/libqt.dylib"])
        self.assertEqual(load_paths(contents / "Resources/qml/Fake/libfakeplugin.dylib", "LC_RPATH"), [])
        check = self.run_script("check-bundle-paths.sh", app)
        self.assertEqual((check.returncode, check.stdout), (0, ""))
        launched = subprocess.run([contents / "MacOS/sony-device-center"], capture_output=True, text=True)
        self.assertEqual(launched.stdout, "bundled\n", launched.stderr)

        # Every edit comes before the bundle's signature, and every edited file
        # ends up with a valid one, the QML plugin under Resources included.
        calls = self.calls()
        bundle_signature = next(index for index, call in enumerate(calls)
                                if call[0] == "codesign" and "--sign" in call and call[-1] == str(app))
        edits = [index for index, call in enumerate(calls) if call[0] == "install_name_tool"]
        self.assertLess(max(edits), bundle_signature)
        edited = {calls[index][-1] for index in edits}
        self.assertEqual(edited, {str(contents / name) for name in (
            "MacOS/sony-device-center", "Frameworks/libqt.dylib", "Resources/qml/Fake/libfakeplugin.dylib")})
        for path in edited:
            verified = subprocess.run(["codesign", "--verify", "--strict", path], capture_output=True, text=True)
            self.assertEqual(verified.returncode, 0, verified.stderr)

    def test_build_refuses_a_link_to_the_build_machine(self):
        build = self.make_build()
        brotli = self.library(self.directory / "brotli/libbrotlicommon.1.dylib",
                              "/opt/homebrew/opt/brotli/lib/libbrotlicommon.1.dylib")
        self.compile(self.directory / "deploy/Frameworks/libbrotlidec.1.dylib", "void decode(void) {}\n",
                     "-dynamiclib", "-install_name", "@rpath/libbrotlidec.1.dylib", str(brotli))
        result = self.run_script("build-dmg.sh", build)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Contents/Frameworks/libbrotlidec.1.dylib\tLC_LOAD_DYLIB\t"
                      "/opt/homebrew/opt/brotli/lib/libbrotlicommon.1.dylib", result.stderr)
        calls = self.calls()
        self.assertFalse(any(call[0] == "codesign" and "--deep" in call for call in calls))
        self.assertFalse(any(call[0] == "dmgbuild" for call in calls))

    def test_check_lists_every_path_outside_the_bundle(self):
        app = self.directory / APP
        library = self.library(self.directory / "local/libx.dylib", "/usr/local/opt/x/lib/libx.dylib")
        self.library(app / "Contents/Frameworks/liby.dylib", "/opt/homebrew/opt/y/lib/liby.dylib",
                     "-Wl,-rpath,@loader_path/../Frameworks")
        self.compile(app / "Contents/MacOS/sony-device-center", "int main(void) { return 0; }\n", str(library),
                     "-Wl,-rpath,/usr/local/lib", "-Wl,-rpath,@executable_path/../Frameworks")
        result = self.run_script("check-bundle-paths.sh", app)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(sorted(result.stdout.splitlines()), [
            "Contents/Frameworks/liby.dylib\tLC_ID_DYLIB\t/opt/homebrew/opt/y/lib/liby.dylib",
            "Contents/MacOS/sony-device-center\tLC_LOAD_DYLIB\t/usr/local/opt/x/lib/libx.dylib",
            "Contents/MacOS/sony-device-center\tLC_RPATH\t/usr/local/lib",
        ])

    def test_check_accepts_bundle_relative_and_system_paths(self):
        app = self.directory / APP
        library = self.library(app / "Contents/Frameworks/libqt.dylib", "@rpath/libqt.dylib",
                               "-Wl,-rpath,@loader_path")
        self.compile(app / "Contents/MacOS/sony-device-center", MAIN, str(library),
                     "-framework", "CoreFoundation", "-Wl,-rpath,@executable_path/../Frameworks")
        (app / "Contents/Info.plist").write_text(INFO_PLIST)
        result = self.run_script("check-bundle-paths.sh", app)
        self.assertEqual((result.returncode, result.stdout), (0, ""), result.stderr)

    def test_verification_rejects_a_homebrew_rpath(self):
        staging = self.directory / "staging"
        contents = staging / APP / "Contents"
        for name in ("MacOS/sonyd", "MacOS/sonyctl",
                     "Frameworks/QtCore.framework", "Frameworks/QtQuick.framework",
                     "Frameworks/QtQuickControls2.framework", "Resources/qml/QtQuick/Controls",
                     "Resources/qml/QtQuick/Layouts", "Resources/qml/QtQuick/Shapes",
                     "PlugIns/platforms/libqcocoa.dylib", "Resources/AppIcon.icns"):
            path = contents / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.touch()
        self.compile(contents / "MacOS/sony-device-center", "int main(void) { return 0; }\n",
                     "-Wl,-rpath,/opt/homebrew/opt/qt/lib")
        (staging / "Applications").symlink_to("/Applications")
        self.env["MOCK_STAGING"] = str(staging)
        result = self.run_script("verify-dmg.sh", self.directory / "test.dmg")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Contents/MacOS/sony-device-center\tLC_RPATH\t/opt/homebrew/opt/qt/lib", result.stderr)
        self.assertIn("loads libraries from outside itself", result.stderr)


if __name__ == "__main__":
    unittest.main()
