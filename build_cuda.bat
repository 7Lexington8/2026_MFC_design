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
    pause
    exit /b 1
  )

  call "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
)

where cmake >nul 2>nul
if errorlevel 1 (
  echo [ERROR] CMake is unavailable. Install "C++ CMake tools for Windows".
  pause
  exit /b 1
)

echo [2/4] Checking llama.cpp...
if not exist "third_party\llama.cpp\CMakeLists.txt" (
  echo [ERROR] llama.cpp is missing. Run setup_llama.bat first.
  pause
  exit /b 1
)

echo [3/4] Configuring CUDA build...
cmake -S . -B build-cuda -G "Visual Studio 17 2022" -A x64 ^
  -DGGML_NATIVE=OFF ^
  -DGGML_CUDA=ON
if errorlevel 1 goto :fail

echo [4/4] Building Release...
cmake --build build-cuda --config Release --target LocalSenseNova
if errorlevel 1 goto :fail

echo.
echo ========================================
echo CUDA build complete.
echo EXE: build-cuda\Release\LocalSenseNova.exe
echo ========================================
pause
exit /b 0

:fail
echo.
echo [ERROR] CUDA build failed. Check the messages above.
echo Make sure CUDA Toolkit and a supported NVIDIA GPU are available.
pause
exit /b 1
