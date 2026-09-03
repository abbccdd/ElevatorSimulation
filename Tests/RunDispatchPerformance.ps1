param(
    [ValidateSet('x64', 'x86')][string]$Architecture = 'x64'
)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$outputRoot = Join-Path $projectRoot "build\dispatch-performance\$Architecture"
New-Item -ItemType Directory -Force $outputRoot | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$visualStudio) { throw 'Installed MSVC was not found.' }
$vcvarsName = if ($Architecture -eq 'x86') { 'vcvars32.bat' } else { 'vcvars64.bat' }
$vcvars = Join-Path $visualStudio "VC\Auxiliary\Build\$vcvarsName"
$harness = Join-Path $PSScriptRoot 'DispatchPerformance.cpp'
$sourceRoot = Join-Path $projectRoot 'ElevatorSimulation'
$compileScript = Join-Path $outputRoot 'compile.cmd'
$compileText = @"
@echo off
chcp 65001 >nul
call "$vcvars" >nul
if errorlevel 1 exit /b 2
pushd "$outputRoot"
cl /nologo /std:c++17 /EHsc /W4 /WX /utf-8 /O2 /MD /I"$sourceRoot" "$harness" "$sourceRoot\Core\Elevator.cpp" "$sourceRoot\Core\Dispatcher.cpp" "$sourceRoot\Core\FixedThreadPool.cpp" /Fe:DispatchPerformance.exe
if errorlevel 1 exit /b 1
DispatchPerformance.exe
exit /b %ERRORLEVEL%
"@
$compileText = $compileText.Replace("`r`n", "`n").Replace("`n", "`r`n")
[System.IO.File]::WriteAllText($compileScript, $compileText, [System.Text.UTF8Encoding]::new($false))
& $compileScript | Tee-Object -FilePath (Join-Path $outputRoot 'results.log')
if ($LASTEXITCODE -ne 0) { throw 'Dispatcher performance comparison failed.' }
