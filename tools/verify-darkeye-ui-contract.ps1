param(
    [Parameter(Mandatory)]
    [string]$PythonRoot,
    [Parameter(Mandatory)]
    [string]$PythonExecutable,
    [string]$CppBaselineDirectory = "",
    [string]$CppActualDirectory = "",
    [string]$PythonBaselineDirectory = "",
    [string]$PythonActualDirectory = "",
    [switch]$CrossImplementationVisual,
    [double]$CrossMaxMeanError = 6.0,
    [double]$CrossMaxDifferentPixelRatio = 0.15
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$PythonRoot = [IO.Path]::GetFullPath($PythonRoot)

& $PythonExecutable (Join-Path $PSScriptRoot "verify_ui_compatibility_matrix.py") `
    --matrix (Join-Path $ProjectRoot "docs\DARKEYE_UI_COMPATIBILITY_MATRIX.json") `
    --python-components (Join-Path $PythonRoot "darkeye_ui\components\__init__.py") `
    --cpp-facade (Join-Path $ProjectRoot "libs\darkeye_ui\components\Components.h") `
    --gallery-source (Join-Path $ProjectRoot "apps\component_gallery\main.cpp") `
    --python-gallery-source (Join-Path $PythonRoot "darkeye_ui\demo.py") `
    --python-harness-source (Join-Path $PSScriptRoot "capture_python_component_gallery.py")
if ($LASTEXITCODE -ne 0) {
    throw "darkeye_ui compatibility matrix validation failed."
}

function Test-VisualBaseline([string]$Baseline, [string]$Actual, [string]$Name) {
    if ([string]::IsNullOrWhiteSpace($Baseline) -and [string]::IsNullOrWhiteSpace($Actual)) {
        return
    }
    if ([string]::IsNullOrWhiteSpace($Baseline) -or [string]::IsNullOrWhiteSpace($Actual)) {
        throw "$Name visual validation requires both baseline and actual directories."
    }
    & (Join-Path $PSScriptRoot "verify-component-gallery.ps1") `
        -BaselineDirectory $Baseline -ActualDirectory $Actual `
        -PythonExecutable $PythonExecutable
    if ($LASTEXITCODE -ne 0) {
        throw "$Name visual regression validation failed."
    }
}

Test-VisualBaseline $CppBaselineDirectory $CppActualDirectory "C++"
Test-VisualBaseline $PythonBaselineDirectory $PythonActualDirectory "Python"
if ($CrossImplementationVisual) {
    if ([string]::IsNullOrWhiteSpace($CppActualDirectory) -or
        [string]::IsNullOrWhiteSpace($PythonActualDirectory)) {
        throw "Cross-implementation visual validation requires C++ and Python actual directories."
    }
    & $PythonExecutable (Join-Path $PSScriptRoot "verify_cross_component_gallery.py") `
        --cpp-directory $CppActualDirectory `
        --python-directory $PythonActualDirectory `
        --max-mean-error $CrossMaxMeanError `
        --max-different-pixel-ratio $CrossMaxDifferentPixelRatio
    if ($LASTEXITCODE -ne 0) {
        throw "Cross-implementation visual validation failed."
    }
}
Write-Host "Darkeye UI compatibility contract verified."
