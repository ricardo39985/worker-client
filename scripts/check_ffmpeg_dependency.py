#!/usr/bin/env python3
"""Read-only publication gate for the retained converter. Never uploads or runs it."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
from urllib.error import HTTPError, URLError
from urllib.parse import unquote, urlsplit
from urllib.request import HTTPRedirectHandler, Request, build_opener


def safe_url(value):
    if not isinstance(value, str) or not value or any(ord(c) <= 32 for c in value):
        raise ValueError("Dependency URLs must be permanent HTTPS locations")
    url = urlsplit(value)
    if (url.scheme != "https" or not url.hostname or url.username or url.password
            or url.query or url.fragment or url.port not in (None, 443) or "\\" in value):
        raise ValueError("Dependency URLs require HTTPS/443 without credentials, query or fragment")
    return url


def validate_entry(entry, *, allow_unpublished=False):
    for field in ("version", "license"):
        if not isinstance(entry.get(field), str) or not entry[field].strip():
            raise ValueError(f"Missing converter {field}")
    if entry.get("format") != "exe" or entry.get("distribution") != "operator-retained":
        raise ValueError("Converter must use an operator-retained executable")
    digest = entry.get("sha256", "")
    if not isinstance(digest, str) or not re.fullmatch(r"[a-f0-9]{64}", digest):
        raise ValueError("Invalid converter checksum pin")
    size = entry.get("bytes")
    if type(size) is not int or not 0 < size <= 2**31:
        raise ValueError("Invalid converter byte count")
    safe_url(entry.get("license_url"))
    if entry.get("url") is None and entry.get("redistribution_url") is None:
        if allow_unpublished:
            return
        raise ValueError("FFmpeg publication pending: publish retained binary and matching source/notices; see docs/ffmpeg-distribution.md")
    url = safe_url(entry.get("url"))
    safe_url(entry.get("redistribution_url"))
    path = unquote(url.path).lower()
    if digest not in path.split("/") or not path.endswith("/ffmpeg.exe"):
        raise ValueError("Converter URL must contain the full checksum directory and end in /ffmpeg.exe")
    if "autobuild-" in path or "latest" in path.split("/") or url.hostname.lower() == "github.com" and path.startswith("/btbn/"):
        raise ValueError("Expiring upstream/autobuild/latest URLs are not retained distribution")


def verify_stream(entry, stream):
    digest = hashlib.sha256()
    total = 0
    while chunk := stream.read(1024 * 1024):
        total += len(chunk)
        if total > entry["bytes"]:
            raise ValueError("Converter download size exceeds pin")
        digest.update(chunk)
    if total != entry["bytes"]:
        raise ValueError("Converter download size does not match pin")
    if digest.hexdigest() != entry["sha256"]:
        raise ValueError("Converter download checksum does not match pin")


class NoRedirect(HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        raise ValueError("Publication gate requires a direct HTTPS 200 artifact URL")


def verify_download(entry, *, open_url=None):
    validate_entry(entry)
    # No credentials, cookies, redirects, TLS exceptions or response URL logging.
    if open_url is None:
        open_url = build_opener(NoRedirect()).open
    try:
        with open_url(Request(entry["url"], headers={"User-Agent": "OrganizerWorker-dependency-gate"}), timeout=60) as stream:
            if getattr(stream, "status", 200) != 200:
                raise ValueError("Artifact endpoint did not return HTTP 200")
            verify_stream(entry, stream)
    except HTTPError as error:
        raise ValueError(f"Artifact endpoint returned HTTP {error.code}") from None
    except (URLError, OSError):
        raise ValueError("Artifact HTTPS download failed") from None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lock", type=Path, default=Path(__file__).resolve().parents[1] / "config/dependencies.lock.json")
    parser.add_argument("--allow-unpublished", action="store_true", help="offline source-only check; never a release pass")
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--artifact-file", type=Path)
    group.add_argument("--download", action="store_true", help="verify the published HTTPS bytes without executing")
    args = parser.parse_args()
    try:
        lock = json.loads(args.lock.read_text(encoding="utf-8"))
        if lock["schema"] != 1:
            raise ValueError("Unsupported dependency lock schema")
        entry = lock["ffmpeg"]
        validate_entry(entry, allow_unpublished=args.allow_unpublished)
        if args.download:
            verify_download(entry)
        elif args.artifact_file:
            with args.artifact_file.open("rb") as stream:
                verify_stream(entry, stream)
    except (ValueError, KeyError, TypeError, OSError):
        # Do not echo user-supplied URL credentials, signed queries or filesystem secrets.
        print("FFmpeg dependency gate FAILED: publication, lock policy or artifact integrity incomplete. See docs/ffmpeg-distribution.md.", file=sys.stderr)
        return 1
    if entry["url"] is None:
        print("PASS source-only policy; FFmpeg publication PENDING (release blocked)")
    else:
        print("PASS FFmpeg dependency policy" + (" and artifact integrity" if args.download or args.artifact_file else "; download not checked"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
