#!/usr/bin/env python3
"""Anonymous read-only release verification. Downloads into a temporary directory."""
import hashlib
import json
from pathlib import Path
import tarfile
import tempfile
from urllib.parse import urlsplit
from urllib.request import HTTPRedirectHandler, Request, build_opener
import zipfile


class AssetRedirect(HTTPRedirectHandler):
    def __init__(self, original):
        self.original = original
        self.redirected = False

    def redirect_request(self, req, fp, code, msg, headers, newurl):
        target = urlsplit(newurl)
        if (self.redirected or req.full_url != self.original
                or code not in (302, 303, 307, 308)
                or target.scheme != "https"
                or target.hostname != "release-assets.githubusercontent.com"
                or target.port not in (None, 443)
                or target.username or target.password or target.fragment):
            raise ValueError("Rejected release redirect")
        self.redirected = True
        # Neither credentials nor signed CDN query URLs are recorded/forwarded.
        return Request(newurl, headers={"User-Agent": "OrganizerWorker-publication-verification"})


def main():
    manifest = json.loads(Path(__file__).with_name("manifest.json").read_text())
    tag = manifest["tag"]
    digest_pin = tag.removeprefix("ffmpeg/")
    prefix = f"https://github.com/ricardo39985/worker-client/releases/download/{tag}/"
    with tempfile.TemporaryDirectory(prefix="ffmpeg-public-verification-") as temp:
        downloaded = Path(temp)
        for asset in manifest["assets"]:
            name = asset["name"]
            assert Path(name).name == name
            assert asset["url"] == prefix + name
            request = Request(asset["url"], headers={"User-Agent": "OrganizerWorker-publication-verification"})
            opener = build_opener(AssetRedirect(asset["url"]))
            digest = hashlib.sha256()
            total = 0
            with opener.open(request, timeout=60) as response, (downloaded / name).open("wb") as output:
                assert response.status == 200
                while chunk := response.read(1024 * 1024):
                    total += len(chunk)
                    assert total <= asset["bytes"], "Oversize response"
                    digest.update(chunk)
                    output.write(chunk)
            assert total == asset["bytes"]
            assert digest.hexdigest() == asset["sha256"]
            print(f"HTTP 200 {name} bytes={total} sha256={digest.hexdigest()} MATCH", flush=True)

        bundle = downloaded / "ffmpeg-source-and-notices.tar.gz"
        with tarfile.open(bundle) as archive:
            root = "ffmpeg-source-and-notices/"
            checksums = archive.extractfile(root + "SOURCE-SHA256SUMS").read().decode()
            count = 0
            for line in checksums.splitlines():
                expected, name = line.split("  ", 1)
                payload = archive.extractfile(root + name).read()
                assert hashlib.sha256(payload).hexdigest() == expected
                if (downloaded / name).is_file():
                    assert payload == (downloaded / name).read_bytes()
                count += 1
            print(f"PASS source bundle internal checksums: {count}/{count}")

        root = "ffmpeg-n9.0.2-22-g46d8f462ee-win64-gpl-9.0/"
        with zipfile.ZipFile(downloaded / (root[:-1] + "-retained.zip")) as archive:
            assert archive.testzip() is None
            payload = archive.read(root + "bin/ffmpeg.exe")
            assert len(payload) == 165167104
            assert hashlib.sha256(payload).hexdigest() == digest_pin
            assert payload == (downloaded / "ffmpeg.exe").read_bytes()
            assert archive.read(root + "NOTICE.md") == (downloaded / "NOTICE.md").read_bytes()
            print(f"PASS downloaded ZIP executable: bytes={len(payload)} sha256={digest_pin}")

        with zipfile.ZipFile(downloaded / "licenses.zip") as archive:
            assert archive.testzip() is None
            assert "licenses/COPYING.GPLv3" in archive.namelist()
            assert "licenses/BtbN-MIT.txt" in archive.namelist()
        print(f"PASS anonymous hosted assets: {len(manifest['assets'])}/{len(manifest['assets'])}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
