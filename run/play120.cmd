@echo off
rem Play at a 120 fps cap (the target; bo2-bisect play120.cmd): emulated display refresh 120 and the
rem game's com_maxfps forced to 120 (BO2MP_DVARS, app dvar_force.cpp). Otherwise as play.cmd.
setlocal
if not defined BO2MP_DVARS set "BO2MP_DVARS=com_maxfps=i:120"
call "%~dp0play.cmd" 120
