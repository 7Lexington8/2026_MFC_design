@echo off
setlocal EnableExtensions
cd /d "%~dp0"

echo [1/4] Initializing Visual Studio 2022 build environment...
where cmake >nul 2>nul
if errorlevel 1 (
  set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
  if not exist "%VSWHERE%" (
    echo [ERROR] cmake is not in PATH, and vswhere.exe was not found.
    echo Please open "Developer Command Prompt for VS 2022" and run this script there,
    echo or install "C++ CMake tools for Windows" from Visual Studio Installer.
    pause
    exit /b 1
  )

  set "VSINSTALL="
  for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%I"

  if not defined VSINSTALL (
    echo [ERROR] Visual Studio C++ toolchain was not found.
    echo Please install the "Desktop development with C++" workload.
    pause
    exit /b 1
  )

  if not exist "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" (
    echo [ERROR] VsDevCmd.bat was not found under:
    echo %VSINSTALL%
    pause
    exit /b 1
  )

  call "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
)

where cmake >nul 2>nul
if errorlevel 1 (
  echo [ERROR] CMake is still unavailable after initializing Visual Studio.
  echo Open Visual Studio Installer ^> Modify ^> Individual components,
  echo and install "C++ CMake tools for Windows".
  pause
  exit /b 1
)

for /f "delims=" %%I in ('where cmake') do (
  echo Using CMake: %%I
  goto :cmake_found
)
:cmake_found

echo [2/4] Checking llama.cpp...
if not exist "third_party\llama.cpp\CMakeLists.txt" (
  echo [ERROR] llama.cpp is missing. Run setup_llama.bat first.
  pause
  exit /b 1
)

echo [3/4] Configuring CPU build...
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
  -DGGML_NATIVE=OFF ^
  -DGGML_OPENMP=OFF ^
  -DGGML_CUDA=OFF
if errorlevel 1 goto :fail

echo [4/4] Building Release...
cmake --build build --config Release --target LocalSenseNova
if errorlevel 1 goto :fail

echo.
echo ========================================
echo Build complete.
echo EXE: build\Release\LocalSenseNova.exe
echo ========================================
pause
exit /b 0

:fail
echo.
echo [ERROR] Build failed. Check the messages above.
pause
exit /b 1
