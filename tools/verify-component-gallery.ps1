param(
    [Parameter(Mandatory)]
    [string]$BaselineDirectory,
    [Parameter(Mandatory)]
    [string]$ActualDirectory,
    [string]$PythonExecutable = "python",
    [double]$MaxMeanError = 2.0,
    [double]$MaxDifferentPixelRatio = 0.05
)

$ErrorActionPreference = "Stop"

$baselineRoot = [IO.Path]::GetFullPath($BaselineDirectory)
$actualRoot = [IO.Path]::GetFullPath($ActualDirectory)
$visualVerifier = Join-Path $PSScriptRoot "verify_cross_component_gallery.py"

# Timer-driven controls may encode visually identical frames differently.  Use a
# strict perceptual threshold rather than fragile PNG byte hashes.
& $PythonExecutable $visualVerifier `
    --cpp-directory $baselineRoot `
    --python-directory $actualRoot `
    --max-mean-error $MaxMeanError `
    --max-different-pixel-ratio $MaxDifferentPixelRatio
if ($LASTEXITCODE -ne 0) {
    throw "Component gallery visual regression failed."
}

Write-Host "Component gallery regression verified (96 screenshots; perceptual thresholds)"
