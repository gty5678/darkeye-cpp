#!/usr/bin/env python3
"""Configure and build Darkeye with CMake (Python 3.9+)."""

import argparse
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys


PROJECT_ROOT = Path(__file__).resolve().parent.parent


def visual_studio_root(env):
    """Find an installation containing the x64 C++ toolchain."""
    vswhere = Path(env.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / (
        "Microsoft Visual Studio/Installer/vswhere.exe"
    )
    if vswhere.is_file():
        result = subprocess.run(
            [str(vswhere), "-latest", "-products", "*", "-requires",
             "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
             "-property", "installationPath", "-utf8"],
            check=True, capture_output=True, encoding="utf-8",
        )
        if result.stdout.strip():
            return Path(result.stdout.strip())
    fallback = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community")
    if fallback.is_dir():
        return fallback
    raise RuntimeError("找不到 Visual Studio C++ 工具链，请安装桌面 C++ 工作负载。")


def windows_environment(root, env):
    """Import the VS x64 environment into child processes only."""
    launcher = root / "Common7/Tools/Launch-VsDevShell.ps1"
    if not launcher.is_file():
        raise RuntimeError(f"找不到 Visual Studio 开发环境脚本：{launcher}")
    powershell = shutil.which("pwsh", path=env.get("PATH")) or shutil.which(
        "powershell", path=env.get("PATH")
    )
    if not powershell:
        raise RuntimeError("找不到 PowerShell，无法初始化 MSVC 环境。")
    child_env = env.copy()
    child_env["DARKEYE_BUILD_VS_ROOT"] = str(root)
    script = r"""
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
& (Join-Path $env:DARKEYE_BUILD_VS_ROOT 'Common7/Tools/Launch-VsDevShell.ps1') `
    -VsInstallationPath $env:DARKEYE_BUILD_VS_ROOT `
    -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
$values = @{}
Get-ChildItem Env: | ForEach-Object { $values[$_.Name] = $_.Value }
Write-Output '__DARKEYE_ENV__'
$values | ConvertTo-Json -Compress
"""
    result = subprocess.run(
        [powershell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", script],
        env=child_env, capture_output=True, encoding="utf-8", errors="replace",
    )
    if result.returncode:
        raise RuntimeError(f"初始化 MSVC 环境失败：\n{result.stdout}{result.stderr}")
    marker = "__DARKEYE_ENV__"
    if marker not in result.stdout:
        raise RuntimeError("初始化 MSVC 环境未返回环境变量。")
    imported = json.loads(result.stdout.split(marker, 1)[1].strip())
    imported.pop("DARKEYE_BUILD_VS_ROOT", None)
    # VS may select its bundled vcpkg; keep the user's configured checkout.
    if env.get("VCPKG_ROOT"):
        imported["VCPKG_ROOT"] = env["VCPKG_ROOT"]
    return imported


def run(command, env, dry_run):
    print("+ " + subprocess.list2cmdline([str(arg) for arg in command]), flush=True)
    if not dry_run:
        subprocess.run(command, cwd=PROJECT_ROOT, env=env, check=True)


def clean_build_directory(build_dir, dry_run=False):
    """Remove only a direct build directory inside this project."""
    project = PROJECT_ROOT.resolve()
    build_root = project / "build"
    target = build_dir.resolve()
    if build_root.resolve() != build_root or target.parent != build_root:
        raise RuntimeError(f"拒绝清理项目 build 目录以外的路径：{target}")
    if build_dir.is_symlink() or getattr(build_dir, "is_junction", lambda: False)():
        raise RuntimeError(f"拒绝清理链接目录：{build_dir}")
    print(f"清理构建目录：{target}", flush=True)
    if not dry_run and target.exists():
        shutil.rmtree(target)


def build_project(configuration="Release", run_tests=False, dry_run=False, clean=False):
    env = os.environ.copy()
    system = platform.system().lower()
    config = configuration.lower()

    if system == "windows":
        root = visual_studio_root(env)
        if not dry_run:
            env = windows_environment(root, env)
        bundled_cmake = root / "Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
        cmake = str(bundled_cmake) if bundled_cmake.is_file() else shutil.which(
            "cmake", path=env.get("PATH")
        )
        name = f"{config}-tests" if run_tests else (
            "release-no-data" if config == "release" else "debug"
        )
        configure = [cmake, "--preset", f"windows-msvc-{name}"]
        build = [cmake, "--build", "--preset", name]
        build_dir = PROJECT_ROOT / "build" / (
            f"windows-msvc-{config}" + ("-tests" if run_tests else "")
        )
    else:
        cmake = shutil.which("cmake", path=env.get("PATH"))
        build_dir = PROJECT_ROOT / "build" / (
            f"{system}-{config}" + ("-tests" if run_tests else "")
        )
        configure = [cmake, "-S", str(PROJECT_ROOT), "-B", str(build_dir),
                     f"-DCMAKE_BUILD_TYPE={configuration}",
                     f"-DDARKEYE_BUILD_TESTS={'ON' if run_tests else 'OFF'}",
                     "-DDARKEYE_INSTALL_DATA=OFF"]
        for variable in ("Qt6_DIR", "CMAKE_TOOLCHAIN_FILE"):
            if env.get(variable):
                configure.append(f"-D{variable}={env[variable]}")
        if not env.get("CMAKE_TOOLCHAIN_FILE") and env.get("VCPKG_ROOT"):
            toolchain = Path(env["VCPKG_ROOT"]) / "scripts/buildsystems/vcpkg.cmake"
            configure.append(f"-DCMAKE_TOOLCHAIN_FILE={toolchain}")
        build = [cmake, "--build", str(build_dir), "--config", configuration, "--parallel"]

    if not cmake:
        raise RuntimeError("找不到 CMake，请安装 CMake 并加入 PATH。")
    if clean:
        clean_build_directory(build_dir, dry_run)
    run(configure, env, dry_run)
    run(build, env, dry_run)
    if run_tests:
        run([cmake, "--test-dir", str(build_dir), "-C", configuration,
             "--output-on-failure"], env, dry_run)
    return cmake, env, build_dir


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", "--configuration", type=str.capitalize,
                        choices=("Debug", "Release"), default="Release")
    parser.add_argument("--test", "--run-tests", action="store_true",
                        help="启用测试构建，编译成功后运行测试")
    parser.add_argument("--dry-run", action="store_true", help="只显示命令，不执行构建")
    args = parser.parse_args()
    build_project(args.config, args.test, args.dry_run)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except subprocess.CalledProcessError as error:
        print(f"构建命令失败（退出码 {error.returncode}）。", file=sys.stderr)
        sys.exit(error.returncode if error.returncode > 0 else 1)
    except (OSError, RuntimeError, ValueError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
