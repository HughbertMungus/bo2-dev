@echo off
rem Press a button on a virtual pad of the running game (it follows mp_cmd.txt, app vpad.cpp).
rem Usage:  pad.cmd p2 a          press A on pad 2 (joins player 2 in a lobby)
rem         pad.cmd p3 start 300  hold Start on pad 3 for 300 ms
rem Pads p2-p4 (the virtual pads, BO2MP_VPADS=1). Buttons: a b x y up down left right start back
rem lb rb ls rs
if "%~2"=="" (
  echo usage: pad.cmd p^<2-4^> ^<button^> [hold_ms]
  exit /b 1
)
set "HOLD=%~3"
if "%HOLD%"=="" set "HOLD=250"
>>"%~dp0mp_cmd.txt" echo %~1 %~2 %HOLD%
rem The profiling build (run\prof.cmd) runs in run-prof\.
if exist "%~dp0..\run-prof\defaultmp.exe" >>"%~dp0..\run-prof\mp_cmd.txt" echo %~1 %~2 %HOLD%
