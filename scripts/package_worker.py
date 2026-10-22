"""Code-only artifact producer after external native gates; never executes code.

No runtime/identity directories, arbitrary extras, output replacement or cleanup.
The caller must establish exact reviewed source/build provenance separately.
"""
import argparse
import hashlib
import json
import os
import pathlib
import re
import shutil
import stat
import sys

NAMES = (
    "OrganizerWorker.exe", "OrganizerWorkerConsole.exe", "OrganizerWorkerDoctor.exe",
    "OrganizerWorkerCtl.exe", "OrganizerWorkerTray.exe", "cli.md", "start-headless.ps1",
)

def plain(path):
    path = path.absolute()
    if str(path).startswith("\\\\"):
        raise ValueError("Local build path required")
    for node in (path, *path.parents):
        try:
            info = node.lstat()
        except FileNotFoundError:
            continue
        if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & 0x400:
            raise ValueError("Reparse input/output refused")
    return path

def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for data in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(data)
    return h.hexdigest()

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--input", type=pathlib.Path, required=True)
    p.add_argument("--output", type=pathlib.Path, required=True)
    p.add_argument("--revision", required=True)
    p.add_argument("--tree", required=True)
    a = p.parse_args()
    try:
        if not all(re.fullmatch(r"[a-f0-9]{40}", value) for value in (a.revision, a.tree)):
            raise ValueError("Exact source commit/tree required")
        source, output = plain(a.input), plain(a.output)
        if output.exists():
            raise ValueError("Existing output retained")
        files = {}
        for name in NAMES:
            item = plain(source / name)
            if not stat.S_ISREG(item.stat().st_mode):
                raise ValueError("Regular built artifact required")
            files[name] = digest(item)
        output.mkdir(parents=True, exist_ok=False)
        for name in NAMES:
            with (source / name).open("rb") as src, (output / name).open("xb") as dst:
                shutil.copyfileobj(src, dst)
                dst.flush()
                os.fsync(dst.fileno())
            if digest(output / name) != files[name]:
                raise ValueError("Copy changed; incomplete package retained")
        manifest = {"schema": 2, "revision": a.revision, "source_tree": a.tree, "files": files}
        with (output / "package.json").open("x", encoding="utf-8", newline="\n") as stream:
            json.dump(manifest, stream, indent=2)
        with (output / "source-revision.txt").open("x", encoding="utf-8", newline="\n") as stream:
            stream.write(a.revision)
        print(json.dumps({"ok": True, "revision": a.revision, "source_tree": a.tree,
                          "package_manifest_sha256": digest(output / "package.json"), "files": files}))
        return 0
    except (OSError, ValueError):
        print(json.dumps({"ok": False, "error": "Code-only packaging refused incomplete/reparse/existing input or output; data retained"}), file=sys.stderr)
        return 1

if __name__ == "__main__":
    sys.exit(main())
