@echo off
REM Glasspectum OFX Plugin Installer — Windows
REM Run as Administrator

echo.
echo   ===================================================
echo        Glasspectum — Windows Installer
echo        Lens Emulation for DaVinci Resolve
echo   ===================================================
echo.

set "BUNDLE=%~dp0Glasspectum.ofx.bundle"
set "DEST=C:\Program Files\Common Files\OFX\Plugins"

if not exist "%BUNDLE%" (
    echo   ERROR: Glasspectum.ofx.bundle not found.
    echo   Make sure this script is next to the bundle folder.
    pause
    exit /b 1
)

echo   Installing to %DEST% ...

if exist "%DEST%\Glasspectum.ofx.bundle" (
    rmdir /s /q "%DEST%\Glasspectum.ofx.bundle"
)

if not exist "%DEST%" mkdir "%DEST%"
xcopy /e /i /y "%BUNDLE%" "%DEST%\Glasspectum.ofx.bundle"

echo.
echo   Installed! Restart DaVinci Resolve to use Glasspectum.
echo.
pause
