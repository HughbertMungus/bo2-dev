@echo off
rem Records a Tracy trace of the running game (started with run\prof.cmd) to dumps\traces\<name>.tracy.
rem Usage: tracy_capture.cmd [seconds=30] [name=trace_<time>]
rem Uses Tracy 0.13.1's capture tools (the version of rexglue-sdk's client) from tools\tracy, unless
rem TRACY_TOOLS names another folder. Summaries: %%TRACY_TOOLS%%\tracy-csvexport.exe <trace>.
setlocal
if not defined TRACY_TOOLS set "TRACY_TOOLS=%~dp0tracy"
set "SECS=%~1"
if "%SECS%"=="" set "SECS=30"
set "NAME=%~2"
if "%NAME%"=="" for /f %%t in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set "NAME=trace_%%t"
if not exist "%~dp0..\dumps\traces" mkdir "%~dp0..\dumps\traces"
"%TRACY_TOOLS%\tracy-capture.exe" -o "%~dp0..\dumps\traces\%NAME%.tracy" -s %SECS% -f
