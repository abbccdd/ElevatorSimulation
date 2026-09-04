param(
    [ValidateSet('x64', 'x86')][string]$Architecture = 'x64'
)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
# 固定上一版：贪心分配、固定归属；只导出源文件，不 checkout、不改分支或工作区代码。
$baselineCommit = '0fade614d5095eb14b3cb63916af876f3d2e1aa3'
$outputRoot = Join-Path $projectRoot "build\dispatch-comparison\$Architecture"
$baselineRoot = Join-Path $outputRoot 'baseline-source'
New-Item -ItemType Directory -Force $baselineRoot | Out-Null
$archivePath = Join-Path $outputRoot 'baseline.zip'
& git -C $projectRoot archive --format=zip "--output=$archivePath" $baselineCommit ElevatorSimulation/Core ElevatorSimulation/Statistics
if ($LASTEXITCODE -ne 0) { throw 'Cannot export baseline commit.' }
Expand-Archive -LiteralPath $archivePath -DestinationPath $baselineRoot -Force
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$visualStudio) { throw 'Installed MSVC was not found.' }
$vcvars = Join-Path $visualStudio ('VC\Auxiliary\Build\vcvars' + $(if ($Architecture -eq 'x86') { '32' } else { '64' }) + '.bat')
foreach ($variant in @('baseline', 'current')) {
    $sourceRoot = if ($variant -eq 'baseline') { Join-Path $baselineRoot 'ElevatorSimulation' } else { Join-Path $projectRoot 'ElevatorSimulation' }
    $variantRoot = Join-Path $outputRoot $variant
    New-Item -ItemType Directory -Force $variantRoot | Out-Null
    $sourceFiles = @('Core\Passenger.cpp','Core\Floor.cpp','Core\Elevator.cpp','Core\Dispatcher.cpp','Core\Simulation.cpp','Statistics\Statistics.cpp')
    if ($variant -eq 'current') { $sourceFiles += @('Core\EventScheduler.cpp', 'Core\FixedThreadPool.cpp', 'Core\FleetRebalancer.cpp') }
    $sources = $sourceFiles |
        ForEach-Object { '"' + (Join-Path $sourceRoot $_) + '"' }
    $sourceArguments = $sources -join ' '
    $harness = Join-Path $PSScriptRoot 'DispatchComparison.cpp'
    $compileScript = Join-Path $variantRoot 'compile.cmd'
    # 生成文件仅存于忽略的 build 目录；两版使用同一 harness、/O2、MSVC 和种子。
    $compileText = @"
@echo off
chcp 65001 >nul
call "$vcvars" >nul
if errorlevel 1 exit /b 2
pushd "$variantRoot"
cl /nologo /std:c++17 /EHsc /W4 /WX /utf-8 /O2 /MD /I"$sourceRoot" "$harness" $sourceArguments /Fe:DispatchComparison.exe
if errorlevel 1 exit /b 1
DispatchComparison.exe
exit /b %ERRORLEVEL%
"@
    $compileText = $compileText.Replace("`r`n", "`n").Replace("`n", "`r`n")
    [System.IO.File]::WriteAllText($compileScript, $compileText, [System.Text.UTF8Encoding]::new($false))
    Write-Output "Variant: $variant ($Architecture)"
    & $compileScript | Tee-Object -FilePath (Join-Path $variantRoot 'results.log')
    if ($LASTEXITCODE -ne 0) { throw "$variant comparison failed." }
}
