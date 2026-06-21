@echo off
setlocal

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

set "EXE=%ROOT%\build\webgui\Release\JoyCon2Mapper.exe"
if not exist "%EXE%" (
  set "EXE=%ROOT%\build\webgui\JoyCon2Mapper.exe"
)

if not exist "%EXE%" (
  echo [error] Build output not found.
  echo [error] Expected:
  echo [error]   "%ROOT%\build\webgui\Release\JoyCon2Mapper.exe"
  echo [error] or:
  echo [error]   "%ROOT%\build\webgui\JoyCon2Mapper.exe"
  echo [hint] Run build_release.bat first.
  exit /b 1
)

for %%I in ("%EXE%") do (
  set "EXE_DIR=%%~dpI"
  set "EXE_NAME=%%~nxI"
)

echo [info] Starting "%EXE%"
start "" /D "%EXE_DIR%" "%EXE_NAME%"
