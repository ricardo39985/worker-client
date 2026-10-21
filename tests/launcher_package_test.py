"""Artifact bytes must not depend on git checkout vs archive line endings."""
import hashlib
import pathlib
import subprocess
import sys
import tempfile

cmake, module, source = sys.argv[1:]
canonical = pathlib.Path(source).read_bytes().replace(b"\r\n", b"\n")
canonical += b"\n# literal ${UNMODIFIED} @UNMODIFIED@\n#cmakedefine UNMODIFIED\n"
with tempfile.TemporaryDirectory(prefix="ow-launcher-package-") as tmp:
    root = pathlib.Path(tmp)
    outputs = []
    for kind, content in (("lf", canonical), ("crlf", canonical.replace(b"\n", b"\r\n"))):
        input_path, output_path = root / (kind + "-source.ps1"), root / (kind + "-artifact.ps1")
        input_path.write_bytes(content)
        subprocess.run([cmake, "-DOW_LAUNCHER_SOURCE=" + str(input_path), "-DOW_LAUNCHER_OUTPUT=" + str(output_path), "-P", module], check=True)
        outputs.append(output_path.read_bytes())
    assert outputs[0] == outputs[1] == canonical, "Same reviewed launcher differs by checkout/archive EOL; enrollment breaks on unchanged-source upgrades"
    assert hashlib.sha256(outputs[0]).digest() == hashlib.sha256(outputs[1]).digest()
print("LAUNCHER_PACKAGE_PASS canonical reviewed script bytes across git/archive EOL")
