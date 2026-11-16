@echo off
rem Build environment for build_sdk.cmd, build_app.cmd and build_prof.cmd (called, not run alone).
rem Sets LLVM (the clang folder), runs the Visual Studio 2022 x64 environment with the MSVC 14.44
rem toolset and puts LLVM, CMake and Ninja on PATH.
rem
rem Per-machine settings go in local.cmd next to the build scripts (kept out of git), for example:
rem   set "BO2DEV_LLVM=D:\llvm\bin"
rem Without BO2DEV_LLVM, clang comes from PATH, then from %ProgramFiles%\LLVM\bin.
if exist "%ROOT%\local.cmd" call "%ROOT%\local.cmd"
if defined BO2DEV_LLVM (
  set "LLVM=%BO2DEV_LLVM%"
) else (
  for %%I in (clang.exe) do if not "%%~$PATH:I"=="" set "LLVM=%%~dp$PATH:I"
)
if not defined LLVM set "LLVM=%ProgramFiles%\LLVM\bin"
for %%I in ("%LLVM%\.") do set "LLVM=%%~fI"
if not exist "%LLVM%\clang.exe" (
  echo [env] clang.exe not found in %LLVM% - install LLVM or set BO2DEV_LLVM in local.cmd
  exit /b 1
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VS="
if defined BO2DEV_VS set "VS=%BO2DEV_VS%"
if not defined VS if exist "%VSWHERE%" (
  for /f "usebackq delims=" %%I in (`"%VSWHERE%" -version [17.0^,18.0^) -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS=%%I"
)
if not defined VS (
  echo [env] Visual Studio 2022 with C++ tools not found - install it or set BO2DEV_VS in local.cmd
  exit /b 1
)
rem MSVC 14.44: older STLs lack the constexpr std::mutex constructor rexglue-sdk needs.
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44 >nul 2>&1
if errorlevel 1 (
  echo [env] the MSVC 14.44 toolset is missing - add "MSVC v143 - VS 2022 C++ x64/x86 build tools (v14.44)" in the Visual Studio Installer
  exit /b 1
)
set "PATH=%LLVM%;%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
exit /b 0
