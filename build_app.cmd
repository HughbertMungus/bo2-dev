@echo off
rem Build defaultmp.exe (BO2 MP, bo2-dev\app) against the SDK installed by build_sdk.cmd.
rem The build runs `rexglue codegen` on app\defaultmp_manifest.toml first (rexglue_project), so a
rem fresh tree regenerates app\generated\default_mp from game\default_mp.xex (your own copy).
rem
rem   build_app.cmd            Release, configure if needed, build
rem   set BO2DEV_JOBS=N        parallel jobs (default 16)
rem   set BO2DEV_APP_CONFIG=X  Release (default) / RelWithDebInfo / Debug
rem Output: app\out\build\win-amd64\<config>\defaultmp.exe plus the staged SDK DLLs.
setlocal EnableExtensions
for %%I in ("%~dp0.") do set "ROOT=%%~fI"
call "%ROOT%\tools\env.cmd"
if errorlevel 1 exit /b 1
if not defined BO2DEV_JOBS set "BO2DEV_JOBS=16"
if not defined BO2DEV_APP_CONFIG set "BO2DEV_APP_CONFIG=Release"
if not defined VULKAN_SDK set "VULKAN_SDK=%ROOT%\tools\vulkan-sdk"
set "SDK=%ROOT%\rexglue-sdk\out\install\win-amd64"


if not exist "%SDK%\lib\cmake\rexglue" (
  echo [build_app] SDK not installed at %SDK% - run build_sdk.cmd first
  exit /b 1
)
cd /d "%ROOT%\app"
set "BLD=out\build\win-amd64"
if not exist %BLD%\CMakeCache.txt (
  cmake -S . -B %BLD% -G "Ninja Multi-Config" ^
    -DCMAKE_C_COMPILER="%LLVM%\clang.exe" -DCMAKE_CXX_COMPILER="%LLVM%\clang++.exe" ^
    -DCMAKE_CONFIGURATION_TYPES="Debug;Release;RelWithDebInfo" ^
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ^
    -DCMAKE_PREFIX_PATH="%SDK%"
  if errorlevel 1 exit /b 1
)
rem Fresh tree: generated\default_mp\sources.cmake does not exist at the first configure, so the
rem generated sources are not in the build and CMake does not notice when codegen writes the file
rem (the link then fails on kPPCImageConfig). Run codegen alone, then configure again.
if not exist generated\default_mp\sources.cmake (
  cmake --build %BLD% --config %BO2DEV_APP_CONFIG% --target defaultmp_codegen
  if errorlevel 1 exit /b 1
  cmake -S . -B %BLD%
  if errorlevel 1 exit /b 1
)
cmake --build %BLD% --config %BO2DEV_APP_CONFIG% --parallel %BO2DEV_JOBS%
if errorlevel 1 exit /b 1
echo APP BUILD OK (%BO2DEV_APP_CONFIG%) -^> %ROOT%\app\%BLD%\%BO2DEV_APP_CONFIG%\defaultmp.exe
