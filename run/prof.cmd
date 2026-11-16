@echo off
rem Play the profiling build (build_prof.cmd, deployed to run-prof\ by deploy_local.ps1 -Prof) with
rem the Tracy profiler available (perf.profiler, on demand: near-free until a capture connects) and
rem the per-second fps log. Capture with tools\tracy_capture.cmd [seconds] [name]. Takes the same
rem environment as run_local.cmd (BO2MP_VPADS, BO2MP_RES_SCALE, ...); live.txt, mp_fps.txt and the
rem logs are in run-prof\. The Release exe has no Tracy zones (rexglue-sdk compiles them out).
setlocal
if not exist "%~dp0..\run-prof\defaultmp.exe" (
  echo no profiling build in run-prof\ - run build_prof.cmd, then tools\deploy_local.ps1 -Prof
  exit /b 1
)
set "BO2MP_BIN=%~dp0..\run-prof\"
set "REX_FPS_LOG=1"
set "BO2MP_ARGS=%BO2MP_ARGS% --perf.profiler=true"
call "%~dp0run_local.cmd" %*
