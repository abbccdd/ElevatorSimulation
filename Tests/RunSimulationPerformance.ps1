param(
    [ValidateSet('x64', 'x86')][string]$Architecture = 'x64'
)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$baselineCommit = '54e2db9d275e65f5f5fac6d8d60aa375cc27a082'
$outputRoot = Join-Path $projectRoot "build\simulation-performance\$Architecture"
$baselineRoot = Join-Path $outputRoot 'baseline-source'
New-Item -ItemType Directory -Force $baselineRoot | Out-Null
$archivePath = Join-Path $outputRoot 'baseline.zip'
& git -C $projectRoot archive --format=zip "--output=$archivePath" $baselineCommit ElevatorSimulation/Core ElevatorSimulation/Statistics
if ($LASTEXITCODE -ne 0) { throw 'Cannot export event-scan baseline commit.' }
Expand-Archive -LiteralPath $archivePath -DestinationPath $baselineRoot -Force

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$visualStudio) { throw 'Installed MSVC was not found.' }
$vcvars = Join-Path $visualStudio ('VC\Auxiliary\Build\vcvars' + $(if ($Architecture -eq 'x86') { '32' } else { '64' }) + '.bat')
$commonSources = @(
    'Core\Passenger.cpp', 'Core\Floor.cpp', 'Core\Elevator.cpp',
    'Core\Dispatcher.cpp', 'Core\FixedThreadPool.cpp',
    'Core\Simulation.cpp', 'Statistics\Statistics.cpp'
)

foreach ($variant in @('event-scan-baseline', 'event-calendar-current')) {
    $sourceRoot = if ($variant -eq 'event-scan-baseline') {
        Join-Path $baselineRoot 'ElevatorSimulation'
    } else {
        Join-Path $projectRoot 'ElevatorSimulation'
    }
    $variantRoot = Join-Path $outputRoot $variant
    New-Item -ItemType Directory -Force $variantRoot | Out-Null
    $sourceFiles = @($commonSources)
    if ($variant -eq 'event-calendar-current') {
        $sourceFiles += @('Core\EventScheduler.cpp', 'Core\FleetRebalancer.cpp')
    }
    $sources = $sourceFiles | ForEach-Object { '"' + (Join-Path $sourceRoot $_) + '"' }
    $compileScript = Join-Path $variantRoot 'compile.cmd'
    $compileText = @"
@echo off
chcp 65001 >nul
call "$vcvars" >nul
if errorlevel 1 exit /b 2
pushd "$variantRoot"
cl /nologo /std:c++17 /EHsc /W4 /WX /utf-8 /O2 /MD /I"$sourceRoot" "$PSScriptRoot\SimulationPerformance.cpp" $($sources -join ' ') /Fe:SimulationPerformance.exe
if errorlevel 1 exit /b 1
SimulationPerformance.exe
exit /b %ERRORLEVEL%
"@
    $compileText = $compileText.Replace("`r`n", "`n").Replace("`n", "`r`n")
    [System.IO.File]::WriteAllText($compileScript, $compileText, [System.Text.UTF8Encoding]::new($false))
    Write-Output "Variant: $variant ($Architecture)"
    & $compileScript | Tee-Object -FilePath (Join-Path $variantRoot 'results.log')
    if ($LASTEXITCODE -ne 0) { throw "$variant performance run failed." }
}
