#!/usr/bin/env python3
"""Create a deterministic, source-only release archive and its SHA-256 digest."""
import argparse
import gzip
import hashlib
import io
import os
from pathlib import Path
import re
import tarfile

ROOT = Path(__file__).resolve().parents[1]
TOP = ["CMakeLists.txt", "LICENSE", "NOTICE", "README.md", "CHANGELOG.md", "UPSTREAM.json", ".gitignore", ".clang-format"]
DIRS = ["include", "src", "cmake", "tests", "examples", "scripts", "docs", ".github"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "dist")
    args = parser.parse_args()
    version = re.search(r"project\(Turbo VERSION ([0-9.]+)", (ROOT / "CMakeLists.txt").read_text())[1]
    prefix = "turbo-" + version
    files = [ROOT / name for name in TOP]
    for directory in DIRS:
        files += [p for p in (ROOT / directory).rglob("*") if p.is_file()
                  and "__pycache__" not in p.parts and p.suffix != ".pyc"]
    args.output.mkdir(parents=True, exist_ok=True)
    archive = args.output / (prefix + ".tar.gz")
    epoch = int(os.environ.get("SOURCE_DATE_EPOCH", "0"))
    with archive.open("wb") as raw, gzip.GzipFile(fileobj=raw, mode="wb", filename="", mtime=epoch) as compressed:
        with tarfile.open(fileobj=compressed, mode="w", format=tarfile.PAX_FORMAT) as tar:
            for path in sorted(files):
                if path.is_symlink():
                    raise RuntimeError("Refusing symlink in source package: " + str(path))
                data = path.read_bytes()
                info = tarfile.TarInfo(prefix + "/" + path.relative_to(ROOT).as_posix())
                info.size, info.mtime, info.mode = len(data), epoch, 0o644
                tar.addfile(info, io.BytesIO(data))
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    archive.with_suffix(archive.suffix + ".sha256").write_text(digest + "  " + archive.name + "\n")
    print(archive)
    print(digest)


if __name__ == "__main__":
    main()
