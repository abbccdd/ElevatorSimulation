@echo off
setlocal
chcp 65001 >nul
set "DEMO_ARCH=x64"
if /i "%~1"=="x86" set "DEMO_ARCH=x86"
if /i "%~1"=="x64" set "DEMO_ARCH=x64"
if not "%~1"=="" if /i not "%~1"=="x86" if /i not "%~1"=="x64" (
    echo Usage: RunAlgorithmDemo.cmd [x64^|x86] [--all^|--step^|1..6]
    exit /b 2
)
set "DEMO_ROOT=%~dp0.."
set "DEMO_OUT=%DEMO_ROOT%\build\algorithm-demo\%DEMO_ARCH%"
set "DEMO_HASH_CURRENT=%DEMO_OUT%\source.current.sha256"
set "DEMO_HASH_BUILT=%DEMO_OUT%\source.built.sha256"
if not exist "%DEMO_OUT%" mkdir "%DEMO_OUT%"

rem Hash source contents instead of rebuilding on every launch. If hashing fails,
rem the safe fallback is a normal rebuild rather than running a stale executable.
del /q "%DEMO_HASH_CURRENT%" >nul 2>&1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0GetAlgorithmDemoFingerprint.ps1" ^
    -ProjectRoot "%DEMO_ROOT%" -OutputPath "%DEMO_HASH_CURRENT%" >nul 2>&1

set "DEMO_REBUILD=1"
if exist "%DEMO_OUT%\AlgorithmDemo.exe" if exist "%DEMO_HASH_CURRENT%" if exist "%DEMO_HASH_BUILT%" (
    fc /b "%DEMO_HASH_CURRENT%" "%DEMO_HASH_BUILT%" >nul 2>&1
    if not errorlevel 1 set "DEMO_REBUILD=0"
)
if "%DEMO_REBUILD%"=="0" goto run_demo

set "DEMO_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%DEMO_VSWHERE%" (
    echo Visual Studio Installer vswhere was not found.
    exit /b 2
)
set "DEMO_VS="
for /f "usebackq tokens=*" %%i in (`"%DEMO_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "DEMO_VS=%%i"
if not defined DEMO_VS (
    echo Installed MSVC was not found.
    exit /b 2
)
if /i "%DEMO_ARCH%"=="x86" (
    call "%DEMO_VS%\VC\Auxiliary\Build\vcvars32.bat" >nul
) else (
    call "%DEMO_VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
)
if errorlevel 1 exit /b 2
pushd "%DEMO_OUT%"
if errorlevel 1 exit /b 2
echo [1/2] Compiling the standalone demo with MSVC %DEMO_ARCH%...
cl /nologo /std:c++17 /EHsc /W4 /WX /utf-8 /MD /O2 /I"%DEMO_ROOT%\ElevatorSimulation" ^
    "%DEMO_ROOT%\Demo\AlgorithmDemo.cpp" ^
    "%DEMO_ROOT%\ElevatorSimulation\Core\Passenger.cpp" ^
    "%DEMO_ROOT%\ElevatorSimulation\Core\Floor.cpp" ^
    "%DEMO_ROOT%\ElevatorSimulation\Core\Elevator.cpp" ^
    "%DEMO_ROOT%\ElevatorSimulation\Core\EventScheduler.cpp" ^
    "%DEMO_ROOT%\ElevatorSimulation\Core\Dispatcher.cpp" ^
    "%DEMO_ROOT%\ElevatorSimulation\Core\FixedThreadPool.cpp" ^
    "%DEMO_ROOT%\ElevatorSimulation\Core\Simulation.cpp" ^
    "%DEMO_ROOT%\ElevatorSimulation\Statistics\Statistics.cpp" ^
    /Fe:AlgorithmDemo.exe >compile.log 2>&1
if errorlevel 1 (
    type compile.log
    popd
    exit /b 1
)
if exist "%DEMO_HASH_CURRENT%" copy /y "%DEMO_HASH_CURRENT%" "%DEMO_HASH_BUILT%" >nul
popd

:run_demo
pushd "%DEMO_OUT%"
if errorlevel 1 exit /b 2
if "%DEMO_REBUILD%"=="0" echo [1/2] Source unchanged; using cached %DEMO_ARCH% executable.
echo [2/2] Running the verified Core scenarios...
if "%~2"=="" (
    AlgorithmDemo.exe
) else (
    AlgorithmDemo.exe "%~2"
)
set "DEMO_RESULT=%ERRORLEVEL%"
popd
exit /b %DEMO_RESULT%
