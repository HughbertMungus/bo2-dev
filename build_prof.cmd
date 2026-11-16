@echo off
rem Profiling build (bo2-dev P3): rexglue-sdk compiles Tracy zones only outside Release
rem ($<NOT:$<CONFIG:Release>>:REXGLUE_ENABLE_PROFILING), so this builds the SDK and the app as
rem RelWithDebInfo into their own install / build folders, leaving the Release build alone.
rem Then: powershell -ExecutionPolicy Bypass -File tools\deploy_local.ps1 -Prof   (to run-prof\)
rem       run\prof.cmd                                                            (play + Tracy)
setlocal EnableExtensions
for %%I in ("%~dp0.") do set "ROOT=%%~fI"
call "%ROOT%\tools\env.cmd"
if errorlevel 1 exit /b 1
if not defined BO2DEV_JOBS set "BO2DEV_JOBS=16"
if not defined VULKAN_SDK set "VULKAN_SDK=%ROOT%\tools\vulkan-sdk"
set "CFG=RelWithDebInfo"
set "SDK=%ROOT%\rexglue-sdk\out\install\win-amd64-prof"


rem SDK: same build tree as build_sdk.cmd (multi-config), installed to its own prefix.
if not exist "%ROOT%\rexglue-sdk\out\build\win-amd64\CMakeCache.txt" (
  echo [build_prof] run build_sdk.cmd once first
  exit /b 1
)
cd /d "%ROOT%\rexglue-sdk"
cmake --build out\build\win-amd64 --config %CFG% --parallel %BO2DEV_JOBS%
if errorlevel 1 exit /b 1
rem The codegen tool (rex::rexglue) is only exported for Release and every consumer config maps to
rem it, so install Release first (needs build_sdk.cmd's Release build), then RelWithDebInfo over it:
rem the DLLs and libs end up profiling builds, rexglue.exe stays the Release one.
cmake --install out\build\win-amd64 --config Release --prefix "%SDK%"
if errorlevel 1 exit /b 1
cmake --install out\build\win-amd64 --config %CFG% --prefix "%SDK%"
if errorlevel 1 exit /b 1

rem App: its own build folder against the profiling SDK; shares app\generated with the Release build.
if not exist "%ROOT%\app\generated\default_mp\sources.cmake" (
  echo [build_prof] run build_app.cmd once first - it generates the recompiled code
  exit /b 1
)
cd /d "%ROOT%\app"
set "BLD=out\build\win-amd64-prof"
if not exist %BLD%\CMakeCache.txt (
  cmake -S . -B %BLD% -G "Ninja Multi-Config" ^
    -DCMAKE_C_COMPILER="%LLVM%\clang.exe" -DCMAKE_CXX_COMPILER="%LLVM%\clang++.exe" ^
    -DCMAKE_CONFIGURATION_TYPES="%CFG%" ^
    -DCMAKE_PREFIX_PATH="%SDK%"
  if errorlevel 1 exit /b 1
)
cmake --build %BLD% --config %CFG% --parallel %BO2DEV_JOBS%
if errorlevel 1 exit /b 1
echo PROF BUILD OK -^> %ROOT%\app\%BLD%\%CFG%\defaultmp.exe
