# This file is public domain, it can be freely copied without restrictions.
# SPDX-License-Identifier: CC0-1.0

from __future__ import annotations

from pathlib import Path
import subprocess


def run_cmd(cmd: list[str]) -> None:
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(
            "Command failed:\n"
            + " ".join(cmd)
            + f"\nstdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )


def build_cpp_test_lib(proj_path: Path, target: str, output_name: str) -> Path:
    repo_root = proj_path.parent
    build_dir = repo_root / "build" / "pytest-cmake-tests"
    run_cmd([
        "cmake",
        "-S",
        str(proj_path),
        "-B",
        str(build_dir),
        f"-DCOCOTB_CPP_SOURCE_DIR={repo_root}",
        "-DCMAKE_BUILD_TYPE=Release",
    ])
    run_cmd(["cmake", "--build", str(build_dir), "--target", target, "--parallel"])
    return (build_dir / f"{output_name}.so").resolve()
