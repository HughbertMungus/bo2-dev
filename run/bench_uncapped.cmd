@echo off
rem Uncapped frame-rate benchmark (bo2-dev P2), as bo2-bisect's bench_uncapped.cmd: com_maxfps
rem forced to 1000 (BO2MP_DVARS, app dvar_force.cpp), emulated display refresh 165 (this PC's
rem display) unless given, vsync off, one line per second in mp_fps.txt (REX_FPS_LOG=1). The
rem previous run's log is kept as mp_fps_prev.txt. Summarize with toolsps_summary.py.
rem Usage: bench_uncapped.cmd [refresh]
setlocal
if exist "%~dp0mp_fps.txt" move /y "%~dp0mp_fps.txt" "%~dp0mp_fps_prev.txt" >nul
set "REX_FPS_LOG=1"
rem 4 players on one controller: virtual pads + mirror (BO2MP_VPADS=0 before calling to turn off).
if not defined BO2MP_VPADS set "BO2MP_VPADS=1"
rem Resolution scale 1 for comparable CPU-side measurements (run_local.cmd now defaults to 3;
rem set BO2MP_RES_SCALE before calling to measure another scale).
if not defined BO2MP_RES_SCALE set "BO2MP_RES_SCALE=1"
if not defined BO2MP_DVARS set "BO2MP_DVARS=com_maxfps=i:1000"
set "BO2MP_REFRESH=%~1"
if "%BO2MP_REFRESH%"=="" set "BO2MP_REFRESH=165"
call "%~dp0run_local.cmd"
