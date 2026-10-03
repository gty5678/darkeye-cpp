#!/usr/bin/env python3
"""Build Release, install the application and create a 7z (without user data)."""

import argparse
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import time

from build import PROJECT_ROOT, build_project, run


EXCLUDED_RUNTIME_FILES = ("vc_redist.x64.exe", "opengl32sw.dll")


def project_version():
    """Read the same project version used by the application's DARKEYE_VERSION."""
    source = (PROJECT_ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    source = re.sub(r"#[^\n]*", "", source)
    match = re.search(
        r"\bproject\s*\(\s*Darkeye\b[^)]*?\bVERSION\s+"
        r"([0-9]+(?:\.[0-9]+){0,3})(?=\s|\))",
        source, re.IGNORECASE,
    )
    if not match:
        raise RuntimeError("无法读取 CMakeLists.txt 中的 project(Darkeye VERSION ...) 版本号。")
    return match.group(1)


def remove_unused_runtime(destination):
    """Remove the optional deployed files before creating the archive."""
    for name in EXCLUDED_RUNTIME_FILES:
        path = destination / name
        if path.is_file():
            path.unlink()
            print(f"移除：{path}")


def find_seven_zip():
    for name in ("7zz", "7z", "7za"):
        executable = shutil.which(name)
        if executable:
            return executable
    for root in (Path("C:/Program Files"), Path("C:/Program Files (x86)")):
        executable = root / "7-Zip/7z.exe"
        if executable.is_file():
            return str(executable)
    raise RuntimeError("找不到 7-Zip，请安装并将 7z / 7zz 加入 PATH。")


def archive_install(destination, archive, seven_zip):
    """Keep one top-level directory in the 7z and test archive integrity."""
    files = sorted(path for path in destination.rglob("*") if path.is_file())
    if not files:
        raise RuntimeError("安装目录为空，无法打包。")
    temporary = archive.with_suffix(".7z.tmp")
    if temporary.exists() or archive.exists():
        raise RuntimeError(f"压缩目标已存在：{archive}")
    total_bytes = sum(path.stat().st_size for path in files)
    print(f"开始 7z 压缩：{len(files)} 个文件，"
          f"原始大小 {total_bytes / 1024 / 1024:.1f} MiB", flush=True)
    started = time.perf_counter()
    subprocess.run(
        [seven_zip, "a", "-t7z", "-mx=7", "-bsp1", str(temporary), destination.name],
        cwd=destination.parent, check=True,
    )
    print(f"压缩用时：{time.perf_counter() - started:.1f} 秒", flush=True)
    print("正在校验压缩包完整性……", flush=True)
    subprocess.run([seven_zip, "t", "-bsp1", str(temporary)], check=True)
    temporary.rename(archive)
    return len(files)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--destination", type=Path, help="指定新的安装目录，7z 放在其旁边")
    parser.add_argument("--dry-run", action="store_true", help="只显示构建、安装和打包步骤")
    args = parser.parse_args()
    started = time.perf_counter()
    try:
        return package_project(args)
    finally:
        if not args.dry_run:
            elapsed = time.perf_counter() - started
            minutes, seconds = divmod(elapsed, 60)
            print(f"总用时：{int(minutes)} 分 {seconds:.1f} 秒"
                  f"（{elapsed:.1f} 秒）", flush=True)


def package_project(args):
    seven_zip = find_seven_zip()
    version = project_version()
    print(f"软件版本：{version}", flush=True)
    destination = (args.destination or PROJECT_ROOT / "out" / f"Daryeye-{version}").resolve()
    archive = destination.parent / f"{destination.name}.7z"
    for path in (destination, archive, archive.with_suffix(".7z.tmp")):
        if path.exists():
            raise RuntimeError(f"目标已存在，请指定新的 --destination：{path}")

    cmake, env, build_dir = build_project("Release", dry_run=args.dry_run, clean=True)
    if not args.dry_run:
        destination.mkdir(parents=True)
    run([cmake, "--install", str(build_dir), "--config", "Release",
         "--prefix", str(destination)], env, args.dry_run)
    # CMake's install rules deploy Qt and the vcpkg DLL dependencies.
    if args.dry_run:
        for name in EXCLUDED_RUNTIME_FILES:
            print(f"移除（如果存在）：{destination / name}")
        print(f"7z: {archive}（工具：{seven_zip}）")
        return 0
    remove_unused_runtime(destination)
    if platform.system() == "Windows":
        for relative in ("Darkeye.exe", "Qt6Core.dll", "Qt6Gui.dll",
                         "Qt6Widgets.dll", "plugins/platforms/qwindows.dll"):
            if not (destination / relative).is_file():
                raise RuntimeError(f"安装缺少必要文件：{relative}")
    count = archive_install(destination, archive, seven_zip)
    print(f"安装目录：{destination}")
    print(f"7z：{archive}（{count} 个文件，{archive.stat().st_size / 1024 / 1024:.1f} MiB）")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except subprocess.CalledProcessError as error:
        print(f"构建、安装或压缩失败（退出码 {error.returncode}）。", file=sys.stderr)
        sys.exit(error.returncode if error.returncode > 0 else 1)
    except (OSError, RuntimeError, ValueError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
