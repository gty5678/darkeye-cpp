param(
    [Parameter(Mandatory)]
    [string]$PythonRoot,
    [Parameter(Mandatory)]
    [string]$PythonExecutable,
    [string]$OutputDirectory = "",
    [string]$CustomPrimary = "#336699"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $ProjectRoot "build\python-component-gallery-matrix"
}
& $PythonExecutable (Join-Path $PSScriptRoot "capture_python_component_gallery.py") `
    --python-root $PythonRoot `
    --output-dir $OutputDirectory `
    --custom-primary $CustomPrimary
if ($LASTEXITCODE -ne 0) {
    throw "Python component snapshot capture failed."
}

$Expected = 3 * 12
$Actual = (Get-ChildItem -LiteralPath $OutputDirectory -Filter *.png -Recurse).Count
if ($Actual -ne $Expected) {
    throw "Python component snapshot count mismatch: expected $Expected, got $Actual"
}
Write-Host "Python component visual matrix generated: $OutputDirectory ($Actual images)"
