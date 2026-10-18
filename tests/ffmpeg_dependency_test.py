"""Offline distribution contracts; no Windows runtime or real accounts."""
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from urllib.error import HTTPError
from urllib.request import Request

sys.dont_write_bytecode = True

ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "scripts/check_ffmpeg_dependency.py"
APPROVED = "f78373b58fa0a239ca062cbd9e03130285ec4e835e57abc22a5d4025e293d2b4"


class DistributionContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        spec = importlib.util.spec_from_file_location("distribution", TOOL)
        cls.policy = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.policy)

    def setUp(self):
        self.payload = b"synthetic converter; never executed"
        self.digest = hashlib.sha256(self.payload).hexdigest()
        self.entry = dict(version="reviewed-build", format="exe", sha256=self.digest,
                          bytes=len(self.payload), distribution="operator-retained",
                          url=f"https://artifacts.example.test/ffmpeg/{self.digest}/ffmpeg.exe",
                          license="reviewed GPL build", license_url="https://ffmpeg.org/legal.html",
                          redistribution_url="https://artifacts.example.test/ffmpeg/source-and-notices.html")

    def validate(self, entry=None, **kwargs):
        return self.policy.validate_entry(self.entry if entry is None else entry, **kwargs)

    def verify(self, payload=None, **kwargs):
        data = self.payload if payload is None else payload
        return self.policy.verify_download(self.entry, open_url=lambda request, **kw: io.BytesIO(data), **kwargs)

    def test_retained_content_addressed_executable_is_valid(self):
        self.validate()

    def test_pending_publication_is_explicitly_blocked(self):
        self.entry.update(url=None, redistribution_url=None)
        with self.assertRaisesRegex(ValueError, "publication"):
            self.validate()
        self.validate(allow_unpublished=True)

    def test_pending_does_not_relax_integrity(self):
        self.entry.update(url=None, redistribution_url=None, sha256="")
        with self.assertRaises(ValueError):
            self.validate(allow_unpublished=True)

    def test_partial_publication_is_rejected(self):
        self.entry["redistribution_url"] = None
        with self.assertRaises(ValueError):
            self.validate(allow_unpublished=True)

    def test_upstream_autobuild_and_latest_are_not_durable_sources(self):
        for url in (f"https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2026-10-17/{self.digest}/ffmpeg.exe",
                    f"https://artifacts.example.test/latest/{self.digest}/ffmpeg.exe",
                    f"https://artifacts.example.test/%6catest/{self.digest}/ffmpeg.exe"):
            with self.subTest(url=url), self.assertRaises(ValueError):
                self.validate(dict(self.entry, url=url))

    def test_url_must_bind_full_content_digest(self):
        with self.assertRaises(ValueError):
            self.validate(dict(self.entry, url="https://artifacts.example.test/version/ffmpeg.exe"))

    def test_unsafe_or_temporary_urls_are_rejected(self):
        for value in ("http://host.test/file", "https://user:secret@host.test/file",
                      "https://host.test/file?token=secret", "https://host.test/file#fragment",
                      "file:///tmp/file", "https://host.test:8443/file", "https://host.test/a\nb"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                self.validate(dict(self.entry, url=value))

    def test_size_format_and_consent_metadata_are_required(self):
        for field, value in (("bytes", 0), ("bytes", True), ("bytes", 2**31 + 1),
                             ("format", "zip"), ("sha256", "a" * 63),
                             ("license", ""), ("distribution", "upstream")):
            with self.subTest(field=field), self.assertRaises(ValueError):
                self.validate(dict(self.entry, **{field: value}))

    def test_license_locations_need_safe_https(self):
        for field in ("license_url", "redistribution_url"):
            with self.subTest(field=field), self.assertRaises(ValueError):
                self.validate(dict(self.entry, **{field: "http://host.test/license"}))

    def test_verified_download_matches_bytes_and_digest(self):
        self.verify()

    def test_changed_same_size_payload_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "checksum"):
            self.verify(b"x" * len(self.payload))

    def test_truncated_and_oversized_downloads_are_rejected(self):
        for data in (self.payload[:-1], self.payload + b"x"):
            with self.subTest(data=data), self.assertRaisesRegex(ValueError, "size"):
                self.verify(data)

    def test_pending_never_attempts_network(self):
        self.entry.update(url=None, redistribution_url=None)
        def forbidden(*args, **kwargs):
            self.fail("unpublished dependency must not contact network")
        with self.assertRaises(ValueError):
            self.policy.verify_download(self.entry, open_url=forbidden)

    def test_404_is_a_failure_without_leaking_url(self):
        def missing(*args, **kwargs):
            raise HTTPError("https://host.test/?secret=hidden", 404, "missing", {}, None)
        with self.assertRaisesRegex(ValueError, "HTTP 404") as caught:
            self.policy.verify_download(self.entry, open_url=missing)
        self.assertNotIn("hidden", str(caught.exception))

    def test_redirects_are_rejected_by_publication_gate(self):
        handler = self.policy.NoRedirect()
        with self.assertRaises(ValueError):
            handler.redirect_request(None, None, 302, "Found", {}, "http://unsafe.test/file")

    def test_local_file_check_never_changes_lock_or_executes_artifact(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)
            lock = path / "lock.json"
            artifact = path / "ffmpeg.exe"
            lock.write_text(json.dumps({"schema": 1, "ffmpeg": self.entry}))
            artifact.write_bytes(self.payload)
            before = lock.read_bytes()
            result = subprocess.run([sys.executable, str(TOOL), "--lock", str(lock),
                                     "--artifact-file", str(artifact)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(lock.read_bytes(), before)
            artifact.write_bytes(b"corrupt")
            result = subprocess.run([sys.executable, str(TOOL), "--lock", str(lock),
                                     "--artifact-file", str(artifact)], capture_output=True)
            self.assertEqual(result.returncode, 1)

    def test_repository_preserves_exact_approved_executable(self):
        entry = json.loads((ROOT / "config/dependencies.lock.json").read_text())["ffmpeg"]
        self.assertEqual(entry["sha256"], APPROVED)
        self.assertEqual(entry["bytes"], 165167104)
        self.assertEqual(entry["format"], "exe")
        self.validate(entry, allow_unpublished=True)

    def test_github_release_download_allows_only_the_asset_cdn_redirect(self):
        original = f"https://github.com/ricardo39985/worker-client/releases/download/{self.digest}/ffmpeg.exe"
        handler = self.policy.NoRedirect(original)
        redirected = handler.redirect_request(Request(original), None, 302, "Found", {},
            "https://release-assets.githubusercontent.com/github-production-release-asset-2e65be/1/file?sig=temporary")
        self.assertEqual(redirected.host, "release-assets.githubusercontent.com")

    def test_github_namespaced_digest_tag_supports_real_release_downloads(self):
        original = f"https://github.com/ricardo39985/worker-client/releases/download/ffmpeg/{self.digest}/ffmpeg.exe"
        self.validate(dict(self.entry, url=original))
        redirected = self.policy.NoRedirect(original).redirect_request(Request(original), None, 302, "Found", {},
            "https://release-assets.githubusercontent.com/file?sig=temporary")
        self.assertEqual(redirected.host, "release-assets.githubusercontent.com")
        self.assertFalse(redirected.has_header("Authorization"))

    def test_github_namespace_does_not_relax_digest_binding_or_redirect_scope(self):
        for tag in (f"ffmpeg/{self.digest[:-1]}", f"ffmpeg-{self.digest}", f"other/{self.digest}"):
            original = f"https://github.com/ricardo39985/worker-client/releases/download/{tag}/ffmpeg.exe"
            with self.subTest(tag=tag), self.assertRaises(ValueError):
                self.policy.NoRedirect(original).redirect_request(Request(original), None, 302, "Found", {},
                    "https://release-assets.githubusercontent.com/file?sig=temporary")

    def test_github_redirect_does_not_allow_downgrade_foreign_host_or_credentials(self):
        original = f"https://github.com/ricardo39985/worker-client/releases/download/{self.digest}/ffmpeg.exe"
        for destination in ("http://release-assets.githubusercontent.com/file", "https://evil.test/file",
                            "https://user:secret@release-assets.githubusercontent.com/file",
                            "https://release-assets.githubusercontent.com:8443/file"):
            with self.subTest(destination=destination), self.assertRaises(ValueError):
                self.policy.NoRedirect(original).redirect_request(Request(original), None, 302, "Found", {}, destination)

    def test_github_redirect_is_bounded_and_never_forwards_authorization(self):
        original = f"https://github.com/ricardo39985/worker-client/releases/download/{self.digest}/ffmpeg.exe"
        handler = self.policy.NoRedirect(original)
        request = Request(original, headers={"Authorization": "secret", "Cookie": "secret"})
        destination = "https://release-assets.githubusercontent.com/file?sig=hidden"
        redirected = handler.redirect_request(request, None, 302, "Found", {}, destination)
        self.assertFalse(redirected.has_header("Authorization"))
        self.assertFalse(redirected.has_header("Cookie"))
        with self.assertRaises(ValueError):
            handler.redirect_request(redirected, None, 302, "Found", {}, destination)

    def test_generic_origin_cannot_redirect_into_github_cdn(self):
        with self.assertRaises(ValueError):
            self.policy.NoRedirect(self.entry["url"]).redirect_request(Request(self.entry["url"]), None, 302,
                "Found", {}, "https://release-assets.githubusercontent.com/file?sig=hidden")


if __name__ == "__main__":
    unittest.main()
