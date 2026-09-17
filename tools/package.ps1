param(
    [string]$Destination = ""
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$CMake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$WinDeployQt = "C:\Qt\6.10.3\msvc2022_64\bin\windeployqt.exe"

if (-not (Test-Path -LiteralPath $CMake)) {
    throw "找不到 Visual Studio 自带的 CMake：$CMake"
}
if (-not (Test-Path -LiteralPath $WinDeployQt)) {
    throw "找不到 Qt 部署工具：$WinDeployQt"
}

if ([string]::IsNullOrWhiteSpace($Destination)) {
    $Stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $Destination = Join-Path $ProjectRoot "out\Darkeye-$Stamp"
}

$Destination = [IO.Path]::GetFullPath($Destination)
New-Item -ItemType Directory -Path $Destination -Force | Out-Null

Push-Location $ProjectRoot
try {
    & $CMake --preset windows-msvc-release
    if ($LASTEXITCODE -ne 0) { throw "CMake 配置失败" }

    & $CMake --build --preset release
    if ($LASTEXITCODE -ne 0) { throw "Release 编译失败" }

    & $CMake --install build/windows-msvc-release --prefix $Destination
    if ($LASTEXITCODE -ne 0) { throw "安装到发布目录失败" }

    & $WinDeployQt --release --no-translations --compiler-runtime `
        (Join-Path $Destination "Darkeye.exe")
    if ($LASTEXITCODE -ne 0) { throw "Qt 运行库部署失败" }

    $DataRoot = Join-Path $Destination "data"
    New-Item -ItemType Directory -Path $DataRoot -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $ProjectRoot "data\README.md") `
        -Destination $DataRoot -Force
}
finally {
    Pop-Location
}

Write-Output "发布目录：$Destination"
