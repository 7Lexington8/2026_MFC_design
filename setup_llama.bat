@echo off
setlocal
cd /d %~dp0

where git >nul 2>nul
if errorlevel 1 (
  echo [ERROR] Git not found. Install Git for Windows first.
  pause
  exit /b 1
)

if exist third_party\llama.cpp\CMakeLists.txt (
  echo llama.cpp already exists.
) else (
  echo Cloning official llama.cpp ...
  git clone --depth 1 --branch b10516 https://github.com/ggml-org/llama.cpp.git third_party\llama.cpp
  if errorlevel 1 (
    echo [ERROR] Failed to clone llama.cpp.
    pause
    exit /b 1
  )
)

echo Done.
pause
