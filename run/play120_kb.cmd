@echo off
rem play120.cmd with the keyboard acting as player 1's pad (hid.mnk_mode), for the peer over RDP
rem (bo2-bisect play120_kb.cmd). The game window needs focus. Keys (development defaults):
rem   Space = A, Backspace = B, L = X, P = Y, Enter = Start, Tab = Back, WASD = left stick,
rem   arrows = right stick, Shift+arrows = d-pad, Q/E = triggers, 1/3 = bumpers.
setlocal
set "BO2MP_ARGS=--hid.mnk_mode=true %BO2MP_ARGS%"
call "%~dp0play120.cmd"
