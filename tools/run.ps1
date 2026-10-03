param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [string]$DataDirectory = ""
)

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$ConfigurationName = $Configuration.ToLowerInvariant()
$BuildDirectory = Join-Path $ProjectRoot "build\windows-msvc-$ConfigurationName"
$Executable = Join-Path $BuildDirectory "Darkeye.exe"
$QtBin = "C:\Qt\6.10.3\msvc2022_64\bin"

if (-not (Test-Path -LiteralPath $Executable)) {
    throw "程序尚未编译，请先运行 python .\tools\build.py --config $Configuration"
}

$env:PATH = "$QtBin;$env:PATH"
$PreviousDataDirectory = $env:DARKEYE_DATA_DIR
try {
    if (-not [string]::IsNullOrWhiteSpace($DataDirectory)) {
        $env:DARKEYE_DATA_DIR = [IO.Path]::GetFullPath($DataDirectory)
    }
    & $Executable
}
finally {
    $env:DARKEYE_DATA_DIR = $PreviousDataDirectory
}
