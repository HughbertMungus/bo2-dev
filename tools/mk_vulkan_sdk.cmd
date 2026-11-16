@echo off
rem Assemble tools\vulkan-sdk, a minimal stand-in for the LunarG Vulkan SDK that rexglue-sdk
rem development's find_package(Vulkan 1.4.313) needs, for a machine without the Vulkan SDK:
rem   Include\vulkan, Include\vk_video  Vulkan-Headers 1.4.357  (VULKAN_THIRDPARTY)
rem   Include\vma\vk_mem_alloc.h        VMA 3.3.0               (same)
rem   Lib\vulkan-1.lib                  import lib generated from the system vulkan-1.dll exports
rem The SDK loads the Vulkan loader at runtime; only headers + this lib are used at build time.
setlocal
for %%I in ("%~dp0..") do set "ROOT=%%~fI"
call "%ROOT%\tools\env.cmd"
if errorlevel 1 exit /b 1
rem VULKAN_THIRDPARTY: a folder with vulkan-headers and vulkan-memory-allocator checkouts.
if not defined VULKAN_THIRDPARTY (
  echo [mk_vulkan_sdk] set VULKAN_THIRDPARTY ^(or install the LunarG Vulkan SDK instead^)
  exit /b 1
)
set "SRC=%VULKAN_THIRDPARTY%"
set "V=%~dp0vulkan-sdk"
if not exist "%V%\Include\vma" mkdir "%V%\Include\vma"
if not exist "%V%\Lib" mkdir "%V%\Lib"
xcopy /e /i /y /q "%SRC%\vulkan-headers\include" "%V%\Include" >nul || exit /b 1
copy /y "%SRC%\vulkan-memory-allocator\include\vk_mem_alloc.h" "%V%\Include\vma\" >nul || exit /b 1
set "D=%V%\Lib"
dumpbin /nologo /exports C:\Windows\System32\vulkan-1.dll > "%D%\exports.txt" || exit /b 1
echo LIBRARY vulkan-1> "%D%\vulkan-1.def"
echo EXPORTS>> "%D%\vulkan-1.def"
for /f "skip=19 tokens=4" %%A in (%D%\exports.txt) do echo %%A>> "%D%\vulkan-1.def"
lib /nologo /def:"%D%\vulkan-1.def" /machine:x64 /out:"%D%\vulkan-1.lib" || exit /b 1
echo VULKAN STAND-IN OK -^> %V%
