@echo off
rem Per-machine launcher defaults. Copy this file to machine.cmd in the same folder and edit it.
rem play.cmd and run_local.cmd call machine.cmd first if it exists. It is kept out of git and out of
rem release folders (tools\make_dist.ps1), so installing a new release never overwrites it.
rem Only fill in unset variables ("if not defined"), so a value set on the command line still wins
rem and bench_uncapped.cmd keeps its fixed resolution scale 1.
rem
rem Example - a slower GPU (the peer's GTX 1050 Ti is GPU-bound at scale 2, ~22 fps in 4p):
rem if not defined BO2MP_RES_SCALE set "BO2MP_RES_SCALE=1"
rem
rem Extra forced game dvars, added to the launcher's own (play120's com_maxfps etc.). Example -
rem dynamic lights off (muzzle flashes, explosions; run-net's settings.txt r_dlightLimit 0):
rem if not defined BO2MP_DVARS_EXTRA set "BO2MP_DVARS_EXTRA=r_dlightLimit=i:0"
rem
rem Async replay off - fallback only; the NVIDIA static it caused is fixed (run_local.cmd):
rem if not defined BO2MP_ASYNC_REPLAY set "BO2MP_ASYNC_REPLAY=false"
