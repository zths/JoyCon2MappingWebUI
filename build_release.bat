@echo off
setlocal

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "BUILD_DIR=%ROOT%\build"
set "CONFIG=Release"
set "GENERATOR="

if not defined VSCMD_VER (
  call :ensure_vs_env
  if errorlevel 1 exit /b %errorlevel%
)

where cmake >nul 2>nul
if errorlevel 1 (
  echo [error] cmake not found in PATH.
  exit /b 1
)

if not exist "%BUILD_DIR%" (
  mkdir "%BUILD_DIR%"
)

if exist "%BUILD_DIR%\CMakeCache.txt" (
  echo [info] Reusing existing build directory: "%BUILD_DIR%"
) else (
  echo [info] Configuring CMake project...
  if defined GENERATOR (
    cmake -S "%ROOT%" -B "%BUILD_DIR%" -G "%GENERATOR%" -A x64
  ) else (
    cmake -S "%ROOT%" -B "%BUILD_DIR%"
  )
  if errorlevel 1 exit /b %errorlevel%
)

echo [info] Building %CONFIG%...
cmake --build "%BUILD_DIR%" --config %CONFIG%
if errorlevel 1 exit /b %errorlevel%

echo [ok] Build completed.
exit /b 0

:ensure_vs_env
if exist "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat" (
  set "GENERATOR=Visual Studio 18 2026"
  call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"
  exit /b %errorlevel%
)

if exist "C:\Program Files\Microsoft Visual Studio\18\Preview\VC\Auxiliary\Build\vcvars64.bat" (
  set "GENERATOR=Visual Studio 18 2026"
  call "C:\Program Files\Microsoft Visual Studio\18\Preview\VC\Auxiliary\Build\vcvars64.bat"
  exit /b %errorlevel%
)

if exist "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" (
  set "GENERATOR=Visual Studio 18 2026"
  call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
  exit /b %errorlevel%
)

if exist "C:\Program Files\Microsoft Visual Studio\17\Community\VC\Auxiliary\Build\vcvars64.bat" (
  set "GENERATOR=Visual Studio 17 2022"
  call "C:\Program Files\Microsoft Visual Studio\17\Community\VC\Auxiliary\Build\vcvars64.bat"
  exit /b %errorlevel%
)

if exist "C:\Program Files\Microsoft Visual Studio\17\Professional\VC\Auxiliary\Build\vcvars64.bat" (
  set "GENERATOR=Visual Studio 17 2022"
  call "C:\Program Files\Microsoft Visual Studio\17\Professional\VC\Auxiliary\Build\vcvars64.bat"
  exit /b %errorlevel%
)

if exist "C:\Program Files\Microsoft Visual Studio\17\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
  set "GENERATOR=Visual Studio 17 2022"
  call "C:\Program Files\Microsoft Visual Studio\17\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
  exit /b %errorlevel%
)

echo [error] Visual Studio build environment not found.
echo [error] Install Visual Studio C++ build tools, or run this script from a Developer Command Prompt.
exit /b 1
