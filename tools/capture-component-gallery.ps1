param(
    [ValidateSet("Debug", "Release", "DebugTests", "ReleaseTests")]
    [string]$Configuration = "DebugTests",
[string]$OutputDirectory = ""
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$ConfigurationName = switch ($Configuration) {
    "DebugTests" { "debug-tests" }
    "ReleaseTests" { "release-tests" }
    default { $Configuration.ToLowerInvariant() }
}
$Gallery = Join-Path $ProjectRoot "build\windows-msvc-$ConfigurationName\darkeye_component_gallery.exe"
if (-not (Test-Path -LiteralPath $Gallery -PathType Leaf)) {
    throw "找不到组件展厅，请先构建 darkeye_component_gallery：$Gallery"
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $ProjectRoot "build\component-gallery-matrix"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

$Scales = @(
    @{ Name = "dpi-100"; Value = "1.0" },
    @{ Name = "dpi-125"; Value = "1.25" },
    @{ Name = "dpi-150"; Value = "1.5" },
    @{ Name = "dpi-200"; Value = "2.0" }
)
$PreviousScale = $env:QT_SCALE_FACTOR
try {
    foreach ($Scale in $Scales) {
        $env:QT_SCALE_FACTOR = $Scale.Value
        $Target = Join-Path $OutputRoot $Scale.Name
        New-Item -ItemType Directory -Force -Path $Target | Out-Null
        & $Gallery --snapshot-dir $Target --all-themes --custom-primary "#336699"
        if ($LASTEXITCODE -ne 0) {
            throw "组件展厅快照失败：$($Scale.Name)"
        }
    }
}
finally {
    $env:QT_SCALE_FACTOR = $PreviousScale
}

$PagesPerTheme = 12
$ThemeVariants = 8 # seven standard themes plus Light with an explicit custom primary.
$Expected = $Scales.Count * $ThemeVariants * $PagesPerTheme
$Actual = (Get-ChildItem -LiteralPath $OutputRoot -Filter *.png -Recurse).Count
if ($Actual -ne $Expected) {
    throw "组件快照数量不符：期望 $Expected，实际 $Actual"
}
Write-Host "组件视觉矩阵已生成：$OutputRoot ($Actual 张)"
