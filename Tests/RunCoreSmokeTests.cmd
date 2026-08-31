@echo off
setlocal
rem Use the installed Visual Studio compiler; no extra project or test framework.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" exit /b 2
set "TEST_VS="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "TEST_VS=%%i"
if not defined TEST_VS exit /b 2
call "%TEST_VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 2
if not exist "%~dp0..\build\core-tests" mkdir "%~dp0..\build\core-tests"
pushd "%~dp0..\build\core-tests"
if errorlevel 1 exit /b 2
cl /nologo /std:c++17 /EHsc /W4 /WX /utf-8 /MDd /Zi /I"%~dp0..\ElevatorSimulation" "%~dp0CoreSmokeTests.cpp" "%~dp0..\ElevatorSimulation\Core\Passenger.cpp" "%~dp0..\ElevatorSimulation\Core\Floor.cpp" "%~dp0..\ElevatorSimulation\Core\Elevator.cpp" "%~dp0..\ElevatorSimulation\Core\Dispatcher.cpp" "%~dp0..\ElevatorSimulation\Core\Simulation.cpp" "%~dp0..\ElevatorSimulation\Statistics\Statistics.cpp" /Fe:CoreSmokeTests.exe
if errorlevel 1 (
    popd
    exit /b 1
)
CoreSmokeTests.exe
set "TEST_RESULT=%ERRORLEVEL%"
popd
exit /b %TEST_RESULT%
