#!/usr/bin/env python3
"""Build/test/install/relocate/consume static, shared and scalar releases offline."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def run(*args, env=None):
    print("+", " ".join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), check=True, env=env)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work-dir", type=Path)
    parser.add_argument("--mode", choices=["all", "static", "shared", "scalar"], default="all")
    parser.add_argument("--jobs", default="4")
    args = parser.parse_args()
    work = (args.work_dir or Path(tempfile.mkdtemp(prefix="turbo-release-"))).resolve()
    modes = ["static", "shared", "scalar"] if args.mode == "all" else [args.mode]
    for mode in modes:
        build, install, relocated = work / mode, work / (mode + "-install"), work / (mode + "-relocated")
        if install.exists() or relocated.exists():
            raise RuntimeError("Use a fresh work directory: " + str(work))
        run("cmake", "-S", ROOT, "-B", build, "-DCMAKE_BUILD_TYPE=Release",
            "-DCMAKE_INSTALL_PREFIX=" + str(install), "-DTURBO_ENABLE_INSTALL=ON",
            "-DTURBO_BUILD_TESTS=ON", "-DTURBO_BUILD_EXAMPLES=ON",
            "-DTURBO_BUILD_SHARED=" + ("ON" if mode == "shared" else "OFF"),
            "-DTURBO_ENABLE_SIMD=" + ("OFF" if mode == "scalar" else "ON"))
        run("cmake", "--build", build, "--config", "Release", "--parallel", args.jobs)
        run("ctest", "--test-dir", build, "-C", "Release", "--output-on-failure")
        run("cmake", "--install", build, "--config", "Release")
        install.rename(relocated)
        # Copy the consumer to a separate directory: no source-tree includes.
        source = work / (mode + "-consumer-src")
        shutil.copytree(ROOT / "tests/consumer", source)
        consumer = work / (mode + "-consumer")
        run("cmake", "-S", source, "-B", consumer, "-DCMAKE_BUILD_TYPE=Release",
            "-DCMAKE_PREFIX_PATH=" + str(relocated))
        run("cmake", "--build", consumer, "--config", "Release")
        env = os.environ.copy()
        env["PATH"] = str(relocated / "bin") + os.pathsep + env.get("PATH", "")
        run("ctest", "--test-dir", consumer, "-C", "Release", "--output-on-failure", env=env)
    if args.mode == "all":
        run("cmake", "-S", ROOT / "tests/consumer", "-B", work / "subproject",
            "-DTURBO_SOURCE_DIR=" + str(ROOT), "-DCMAKE_BUILD_TYPE=Release")
        run("cmake", "--build", work / "subproject", "--config", "Release", "--parallel", args.jobs)
        run("ctest", "--test-dir", work / "subproject", "-C", "Release", "--output-on-failure")
    print("Release checks passed:", work)


if __name__ == "__main__":
    main()
