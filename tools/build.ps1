param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [switch]$RunTests
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$CMake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

if (-not (Test-Path -LiteralPath $CMake)) {
    throw "找不到 Visual Studio 自带的 CMake：$CMake"
}

Push-Location $ProjectRoot
try {
    $ConfigurationName = $Configuration.ToLowerInvariant()
    $ConfigurePreset = if ($RunTests) {
        "windows-msvc-$ConfigurationName-tests"
    } else {
        "windows-msvc-$ConfigurationName"
    }
    $BuildPreset = if ($RunTests) { "$ConfigurationName-tests" } else { $ConfigurationName }
    $BuildDirectory = Join-Path $ProjectRoot "build\windows-msvc-$ConfigurationName$(if ($RunTests) { '-tests' } else { '' })"

    & $CMake --preset $ConfigurePreset
    if ($LASTEXITCODE -ne 0) { throw "CMake 配置失败" }

    & $CMake --build --preset $BuildPreset
    if ($LASTEXITCODE -ne 0) { throw "C++ 编译失败" }

    if ($RunTests) {
        & $CMake --test-dir $BuildDirectory --output-on-failure
        if ($LASTEXITCODE -ne 0) { throw "测试失败" }
    }
}
finally {
    Pop-Location
}
