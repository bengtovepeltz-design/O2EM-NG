@echo off
setlocal EnableExtensions
cd /d "%~dp0"
title O2EM-NG Installer Builder

echo ========================================
echo   O2EM-NG Beta 4 Installer Builder
echo ========================================
echo.
echo BAT-filen har startat korrekt.
echo Tryck valfri tangent for att fortsatta...
pause >nul

set "ISCC="
if exist "%ProgramFiles%\Inno Setup 7\ISCC.exe" set "ISCC=%ProgramFiles%\Inno Setup 7\ISCC.exe"
if defined ISCC goto compiler_found

if exist "%ProgramFiles%\Inno Setup 7\ISCC.exe" set "ISCC=%ProgramFiles%\Inno Setup 7\ISCC.exe"
if defined ISCC goto compiler_found

if exist "%ProgramFiles(x86)%\Inno Setup 7\ISCC.exe" set "ISCC=%ProgramFiles(x86)%\Inno Setup 7\ISCC.exe"
if defined ISCC goto compiler_found

for /f "delims=" %%I in ('where /r "%ProgramFiles%" ISCC.exe 2^>nul') do if not defined ISCC set "ISCC=%%I"
if defined ISCC goto compiler_found

if defined ProgramFiles(x86) for /f "delims=" %%I in ('where /r "%ProgramFiles(x86)%" ISCC.exe 2^>nul') do if not defined ISCC set "ISCC=%%I"
if defined ISCC goto compiler_found

echo.
echo FEL: ISCC.exe hittades inte.
echo Oppna Inno Setup Compiler och valj Help - About for att kontrollera installationen.
goto finished

:compiler_found
echo.
echo Inno Setup compiler hittades:
echo "%ISCC%"
echo.

if exist "..\dist\release-info.iss" goto release_found

echo FEL: Release-versionen hittades inte:
echo "%CD%\..\dist\release-info.iss"
echo.
echo Bygg projektet som x64 Release och kor sedan filen igen.
goto finished

:release_found
if not exist "Output" mkdir "Output"

echo Bygger installationsprogrammet...
echo.
"%ISCC%" "O2EM-NG_Setup.iss"

if errorlevel 1 goto build_failed

echo.
echo ========================================
echo   Installationsprogrammet ar klart
echo ========================================
echo.
echo Output-mapp:
echo "%CD%\Output"
goto finished

:build_failed
echo.
echo FEL: Inno Setup kunde inte kompilera scriptet.
echo Las felmeddelandet ovan.

:finished
echo.
echo Tryck valfri tangent for att stanga fonstret...
pause >nul
endlocal
