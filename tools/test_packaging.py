"""Host-independent release recipe, archive and data-preservation tests."""
import hashlib
import os
from pathlib import Path
import tarfile
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import package


class PackagingTest(unittest.TestCase):
    def setUp(self):
        output = package.PROJECT_ROOT / "out"
        output.mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="packaging-tests-", dir=output)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "out").mkdir()
        self.context = patch.object(package, "PROJECT_ROOT", self.root)
        self.context.start()
        self.addCleanup(self.context.stop)

    def test_supported_native_architectures(self):
        self.assertEqual(package.release_platform("Windows", "AMD64"), ("windows", "x64"))
        self.assertEqual(package.release_platform("Linux", "x86_64"), ("linux", "x64"))
        self.assertEqual(package.release_platform("Darwin", "arm64"), ("macos", "arm64"))
        for system, arch in [("Darwin", "x86_64"), ("Linux", "arm64"), ("Windows", "ARM64")]:
            with self.assertRaises(RuntimeError):
                package.release_platform(system, arch)

    def test_staging_cleanup_preserves_user_data_and_rejects_escape(self):
        stage = self.root / "out/stage"
        profile = stage / "DarkEye/data"
        profile.mkdir(parents=True)
        sentinel = profile / "public.db"
        sentinel.write_bytes(b"user database")
        with self.assertRaises(RuntimeError):
            package.reset_output(stage)
        self.assertEqual(sentinel.read_bytes(), b"user database")
        for target in (self.root / "outside", self.root / "out/../outside"):
            with self.assertRaises(RuntimeError):
                package.reset_output(target)

    def test_linux_portable_archive_layout_and_checksums(self):
        destination = self.root / "out/payload"
        resources = destination / "share/darkeye"
        for relative in ["bin/Darkeye", "plugins/platforms/libqxcb.so", "share/darkeye/config/update.ini", "share/darkeye/LICENSE"]:
            file = destination / relative
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_bytes(b"payload")
        (resources / "avwiki").mkdir()
        (resources / "avwiki/page.md").write_text("seed")
        base = self.root / "out/Darkeye-1.2.5-linux-x64"
        outputs = package.package_linux(SimpleNamespace(format="portable", dry_run=False), destination,
                                        self.root / "out/portable", base, "1.2.5", os.environ.copy())
        self.assertEqual(len(outputs), 1)
        with tarfile.open(outputs[0], "r:gz") as archive:
            names = archive.getnames()
            self.assertIn("DarkEye/data", names)
            marker = archive.extractfile("DarkEye/app-1.2.5/bin/portable.ini").read()
            self.assertIn(b"DataDirectory=../../data", marker)
            launcher = archive.getmember("DarkEye/Darkeye.sh")
            self.assertEqual(launcher.mode & 0o111, 0o111)
            self.assertIn(b'app-1.2.5/bin/Darkeye', archive.extractfile(launcher).read())
        self.assertFalse((destination / "bin/portable.ini").exists())
        package.write_checksums(outputs)
        self.assertEqual(Path(str(outputs[0]) + ".sha256").read_text().split()[0],
                         hashlib.sha256(outputs[0].read_bytes()).hexdigest())

    def test_native_installer_recipes_in_dry_run(self):
        args = SimpleNamespace(dry_run=True, format="both", mac_sign_identity=None, mac_installer_identity=None)
        base = self.root / "out/Darkeye-1.2.5-macos-arm64"
        with patch.object(package, "run") as run:
            outputs = package.package_macos(args, self.root / "out/payload", self.root / "out/portable",
                                            base, "1.2.5", "cmake", os.environ.copy())
        self.assertEqual([p.suffix for p in outputs], [".zip", ".pkg"])
        commands = [call.args[0] for call in run.call_args_list]
        self.assertTrue(any("--install-location" in c and "/Applications" in c for c in commands))
        self.assertTrue(any("--verify" in c for c in commands))
        with patch.object(package, "run") as run:
            outputs = package.package_linux(args, self.root / "out/payload", self.root / "out/portable",
                                            self.root / "out/Darkeye-1.2.5-linux-x64", "1.2.5", os.environ.copy())
        self.assertTrue(str(outputs[0]).endswith("-portable.tar.gz"))
        self.assertTrue(str(outputs[1]).endswith("-setup.deb"))
        self.assertIn("--root-owner-group", run.call_args.args[0])


if __name__ == "__main__":
    unittest.main()
