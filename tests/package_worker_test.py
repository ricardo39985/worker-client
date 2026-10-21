"""Observable artifact publication/security; synthetic bytes only."""
import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest

SCRIPT = pathlib.Path(__file__).resolve().parents[1] / "scripts/package_worker.py"
NAMES = {"OrganizerWorker.exe", "OrganizerWorkerConsole.exe", "OrganizerWorkerDoctor.exe", "OrganizerWorkerCtl.exe", "OrganizerWorkerTray.exe", "cli.md", "start-headless.ps1"}
class PackageContracts(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="ow-code-package-")
        self.root = pathlib.Path(self.tmp.name)
        self.source, self.output = self.root / "input", self.root / "output"
        self.source.mkdir()
        for name in NAMES:
            (self.source / name).write_bytes(("synthetic-" + name).encode())
    def tearDown(self):
        self.tmp.cleanup()
    def run_package(self):
        return subprocess.run([sys.executable, str(SCRIPT), "--input", str(self.source), "--output", str(self.output), "--revision", "a"*40, "--tree", "b"*40], text=True, capture_output=True)
    def test_only_code_is_published_with_matching_full_revision_and_manifest(self):
        (self.source / "credential.dpapi").write_bytes(b"synthetic-nonsecret-sentinel")
        result = self.run_package()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse((self.output / "credential.dpapi").exists(), "private/root extras must not enter an artifact")
        manifest = json.loads((self.output / "package.json").read_text())
        self.assertEqual((manifest["schema"], manifest["revision"], manifest["source_tree"]), (2, "a"*40, "b"*40))
        self.assertEqual(set(manifest["files"]), NAMES)
        for name in NAMES:
            self.assertEqual(hashlib.sha256((self.output / name).read_bytes()).hexdigest(), manifest["files"][name])
        self.assertEqual((self.output / "source-revision.txt").read_text(), "a"*40)
        self.assertEqual(json.loads(result.stdout)["package_manifest_sha256"], hashlib.sha256((self.output / "package.json").read_bytes()).hexdigest())
    def test_existing_output_and_operator_data_are_never_overwritten(self):
        self.output.mkdir()
        sentinel = self.output / "operator.txt"
        sentinel.write_bytes(b"preserve")
        self.assertNotEqual(self.run_package().returncode, 0)
        self.assertEqual(sentinel.read_bytes(), b"preserve")
        self.assertEqual(list(self.output.iterdir()), [sentinel])
    def test_missing_companion_cannot_publish_success(self):
        (self.source / "OrganizerWorkerCtl.exe").unlink()
        self.assertNotEqual(self.run_package().returncode, 0)
        self.assertFalse(self.output.exists())
    def test_reparse_symlink_cannot_copy_an_unreviewed_file(self):
        target = self.root / "private-fixture"
        target.write_bytes(b"synthetic-do-not-publish")
        (self.source / "OrganizerWorker.exe").unlink()
        (self.source / "OrganizerWorker.exe").symlink_to(target)
        self.assertNotEqual(self.run_package().returncode, 0)
        self.assertFalse(self.output.exists())

if __name__ == "__main__":
    unittest.main()
