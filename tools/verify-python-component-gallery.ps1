param(
    [Parameter(Mandatory)]
    [string]$BaselineDirectory,
    [Parameter(Mandatory)]
    [string]$ActualDirectory,
    [Parameter(Mandatory)]
    [string]$PythonExecutable,
    [double]$MaxMeanError = 2.0,
    [double]$MaxDifferentPixelRatio = 0.05
)

$ErrorActionPreference = "Stop"
$verifier = Join-Path $PSScriptRoot "verify_cross_component_gallery.py"
& $PythonExecutable $verifier `
    --cpp-directory $BaselineDirectory `
    --python-directory $ActualDirectory `
    --themes light dark light-custom-primary `
    --pages buttons text toggles inputs containers data-nav p2-experience more advanced color-icons theme setting `
    --max-mean-error $MaxMeanError `
    --max-different-pixel-ratio $MaxDifferentPixelRatio
if ($LASTEXITCODE -ne 0) {
    throw "Python component gallery visual regression failed."
}
Write-Host "Python component gallery regression verified (36 screenshots)."
