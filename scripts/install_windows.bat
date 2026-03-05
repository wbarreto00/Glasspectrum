@echo off
REM Glasspectrum OFX Plugin Installer — Windows
REM Run as Administrator

echo.
echo   ===================================================
echo        Glasspectrum — Windows Installer
echo        Lens Emulation for DaVinci Resolve
echo   ===================================================
echo.

set "BUNDLE=%~dp0Glasspectrum.ofx.bundle"
set "DEST=C:\Program Files\Common Files\OFX\Plugins"

if not exist "%BUNDLE%" (
    echo   ERROR: Glasspectrum.ofx.bundle not found.
    echo   Make sure this script is next to the bundle folder.
    pause
    exit /b 1
)

echo   Installing to %DEST% ...

if exist "%DEST%\Glasspectrum.ofx.bundle" (
    rmdir /s /q "%DEST%\Glasspectrum.ofx.bundle"
)

if not exist "%DEST%" mkdir "%DEST%"
xcopy /e /i /y "%BUNDLE%" "%DEST%\Glasspectrum.ofx.bundle"

echo.
echo   Installed! Restart DaVinci Resolve to use Glasspectrum.
echo.
pause
