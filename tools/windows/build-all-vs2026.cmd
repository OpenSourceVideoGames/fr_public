@echo off
setlocal
powershell -ExecutionPolicy Bypass -File "%~dp0vs2026-check-and-build.ps1" -Build -Configuration Release -Platform Win32
set ERR=%ERRORLEVEL%
if not "%ERR%"=="0" (
  echo.
  echo Build finished with errors. Exit code: %ERR%
  exit /b %ERR%
)

echo.
echo Build finished successfully.
exit /b 0
