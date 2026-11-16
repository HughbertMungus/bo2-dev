@echo off
rem bo2-dev play launcher: the build as last tested (2026-11-12) - native 1280x720 scene and native
rem split-screen views (no MSAA), rendered at resolution scale 2 (2560x1440 internal), page clear off,
rem vsync off, plus the virtual pads + player 1 pad mirror for one-controller 4p bot setups
rem (Y = join, X = back out of class select, RB = spectate; run mirror_off.cmd once in the match).
rem Everything else comes from run_local.cmd.
setlocal
rem This machine's own defaults (machine.cmd, not in git or release folders; see machine.example.cmd).
if exist "%~dp0machine.cmd" call "%~dp0machine.cmd"
if not defined BO2MP_RES_SCALE set "BO2MP_RES_SCALE=2"
if not defined BO2MP_VPADS set "BO2MP_VPADS=1"
call "%~dp0run_local.cmd" %*
