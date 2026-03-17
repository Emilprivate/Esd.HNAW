@echo off
setlocal

set CONFIG=Release
if not "%~1"=="" set CONFIG=%~1

if not exist build mkdir build

cmake --fresh -S . -B build -A x64
if errorlevel 1 goto :error

cmake --build build --config %CONFIG%
if errorlevel 1 goto :error

echo Build succeeded.
exit /b 0

:error
echo Build failed.
exit /b %errorlevel%
