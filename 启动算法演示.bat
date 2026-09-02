@echo off
setlocal
chcp 65001 >nul
cd /d "%~dp0"

echo ============================================================
echo       Elevator Dispatch Algorithm Demo
echo ============================================================
echo.
call "%~dp0Demo\RunAlgorithmDemo.cmd" x64 --step
set "DEMO_EXIT_CODE=%ERRORLEVEL%"

echo.
if not "%DEMO_EXIT_CODE%"=="0" (
    echo Demo failed with exit code %DEMO_EXIT_CODE%.
    echo Please check the Visual Studio 2022 C++ installation.
) else (
    echo All six demo scenarios completed successfully.
)
echo.
pause
exit /b %DEMO_EXIT_CODE%
