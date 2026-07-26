@echo off
setlocal
cd /d "%~dp0"

set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"

if not exist "..\x64\Release\O2EM-NG.exe" (
    echo ERROR: Release build not found.
    echo Rebuild O2EM-NG in x64 Release first.
    pause
    exit /b 1
)

if not exist "%ISCC%" (
    echo ERROR: Inno Setup 6 was not found.
    echo Install Inno Setup 6, then run this file again.
    pause
    exit /b 1
)

if not exist Output mkdir Output
"%ISCC%" "O2EM-NG_Setup.iss"
if errorlevel 1 (
    echo.
    echo Installer build failed.
    pause
    exit /b 1
)

echo.
echo Installer created in Installer\Output
pause
