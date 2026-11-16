@echo off
rem Build and install rexglue-sdk (development / bo2-port) into rexglue-sdk\out\install\win-amd64
rem with LLVM clang and the VS 2022 toolset (MSVC 14.44); see tools\env.cmd.
rem
rem Follows upstream's win-amd64 preset layout
rem (out\build\win-amd64, out\install\win-amd64) so a project finds it via CMAKE_PREFIX_PATH /
rem find_package(rexglue). Deliberately NOT the upstream preset: it adds -march=x86-64-v2, and our
rem rule is no -march (the SDK's own rexglue_cpu_flags adds the -msse4.1 floor).
rem
rem   build_sdk.cmd            Release, configure if needed, build + install
rem   set BO2DEV_JOBS=N        parallel jobs (default 16)
rem   set BO2DEV_SDK_CONFIG=X  Release (default) / RelWithDebInfo / Debug
setlocal EnableExtensions
for %%I in ("%~dp0.") do set "ROOT=%%~fI"
call "%ROOT%\tools\env.cmd"
if errorlevel 1 exit /b 1
if not defined BO2DEV_JOBS set "BO2DEV_JOBS=16"
if not defined BO2DEV_SDK_CONFIG set "BO2DEV_SDK_CONFIG=Release"

rem development needs a Vulkan SDK >= 1.4.313 (headers + VMA + vulkan-1.lib). Without one,
rem tools\vulkan-sdk is a minimal stand-in: Vulkan-Headers 1.4.357 and VMA 3.3.0 copied
rem from a rexglue-sdk checkout's thirdparty folder, plus an import lib made by tools\mk_vulkan_lib.cmd
rem from the system vulkan-1.dll. A real Vulkan SDK (VULKAN_SDK already set) takes precedence.
if not defined VULKAN_SDK set "VULKAN_SDK=%ROOT%\tools\vulkan-sdk"


cd /d "%ROOT%\rexglue-sdk"
set "BLD=out\build\win-amd64"
set "INST=%ROOT%\rexglue-sdk\out\install\win-amd64"
if not exist %BLD%\CMakeCache.txt (
  cmake -S . -B %BLD% -G "Ninja Multi-Config" ^
    -DCMAKE_C_COMPILER="%LLVM%\clang.exe" -DCMAKE_CXX_COMPILER="%LLVM%\clang++.exe" ^
    -DCMAKE_CXX_STANDARD=23 ^
    -DCMAKE_CONFIGURATION_TYPES="Debug;Release;RelWithDebInfo" ^
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ^
    -DREXGLUE_BUILD_TESTS=OFF ^
    -DCMAKE_INSTALL_PREFIX="%INST%"
  if errorlevel 1 exit /b 1
)
cmake --build %BLD% --config %BO2DEV_SDK_CONFIG% --target install --parallel %BO2DEV_JOBS%
if errorlevel 1 exit /b 1
echo SDK BUILD OK (%BO2DEV_SDK_CONFIG%) -^> %INST%
