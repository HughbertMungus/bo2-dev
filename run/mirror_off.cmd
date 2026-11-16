@echo off
rem Pad mirror off (live): player 1's Y / X / RB no longer press on the virtual pads.
<nul set /p "=0" > "%~dp0padmirror.txt"
