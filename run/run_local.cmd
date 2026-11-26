@echo off
rem bo2-dev P0 launcher: unmodified upstream (rexglue-sdk development) BO2 MP build.
rem Options copied from bo2-bisect\run-net\run_local.cmd, renamed to development's settings:
rem   --game_data_root           -> --title.game_root
rem   --cache_root               -> --app.cache_root
rem   --log_file / --log_level   -> --log.file_name / --log.level   (log goes to logs\)
rem   --video_mode_*             -> --ui.video_mode_*
rem   --render_target_path_d3d12 -> --gpu.d3d12.render_target_path
rem   --readback_memexport       -> --gpu.xenos.readback_memexport  (gpu.d3d12.* is already false)
rem   --occlusion_query_enable   -> --gpu.xenos.occlusion_query_enable
rem   --resolution_scale         -> --gpu.resolution_scale_x / _y
rem   perf/0003 (runtime default flipped there) -> --gpu.xenos.execute_unclipped_draw_vs_on_cpu=true:
rem     without it the 2nd sun-shadow cascade's 4x clear wraps EDRAM onto the 1st on RTV and the
rem     near dynamic shadows vanish (BISECT.md "RTV near-shadow loss").
rem   update_data_root = game root (bo2-bisect app) -> --title.update_root (patch_mp.ff etc.)
rem   --mp_pad_count=4 + BO2MP_MU_* (users 2-4 signed in) -> --kernel.signed_in_users=4 (bo2-port)
rem Dropped: --mp_signin_state / --mp_keyboard_pad / --mp_pad (bo2-bisect app
rem settings, not in the P0 app; development errors on unknown settings) and the BO2MP_MU_* /
rem BO2MP_LEGACY_XGI / REX_* environment switches (P1 ports the XGI/XNet/sign-in behaviour into
rem the bo2-port runtime instead; the REX_* perf switches are P2).
rem Usage: run_local.cmd [refresh]   (default 60)
setlocal
rem This machine's own defaults (machine.cmd, not in git or release folders; see machine.example.cmd).
rem play.cmd already called it; calling again is harmless since it only fills in unset variables.
if exist "%~dp0machine.cmd" call "%~dp0machine.cmd"
rem BO2MP_DVARS_EXTRA (e.g. from machine.cmd): dvars forced on top of the launcher's BO2MP_DVARS
rem (play120's com_maxfps, bench_uncapped's 1000), same "name=kind:value,..." format.
if defined BO2MP_DVARS_EXTRA if defined BO2MP_DVARS set "BO2MP_DVARS=%BO2MP_DVARS%,%BO2MP_DVARS_EXTRA%"
if defined BO2MP_DVARS_EXTRA if not defined BO2MP_DVARS set "BO2MP_DVARS=%BO2MP_DVARS_EXTRA%"
rem BO2MP_BIN: the folder with defaultmp.exe (default this one; run\prof.cmd uses run-prof\).
rem It is also the working folder (live.txt, logs, mp_fps.txt) and holds cache_mp.
if not defined BO2MP_BIN set "BO2MP_BIN=%~dp0"
cd /d "%BO2MP_BIN%"
rem Data root: a release layout (..\BO2) if present, otherwise the source tree's game folder.
if not defined BO2MP_DATA if exist "%~dp0..\BO2\default_mp.xex" set "BO2MP_DATA=%~dp0..\BO2"
if not defined BO2MP_DATA set "BO2MP_DATA=%~dp0..\game"
if not defined BO2MP_LOGLEVEL set "BO2MP_LOGLEVEL=info"
if not defined BO2MP_RTPATH set "BO2MP_RTPATH=rtv"
if not defined BO2MP_MEMEXP_READBACK set "BO2MP_MEMEXP_READBACK=false"
if not defined BO2MP_OCCLUSION set "BO2MP_OCCLUSION=false"
if not defined BO2MP_RES_SCALE set "BO2MP_RES_SCALE=3"
if not defined BO2MP_USERS set "BO2MP_USERS=4"
rem Page clear off (bo2-bisect perf/0052 pageclear): on, every CPU-uploaded page is marked invalid at
rem each frame end and re-uploaded. P2 A/B 2026-11-12, 4p bots uncapped, in one match: off 124.7 /
rem 139.7 fps vs on 107.0 / 98.5 / 100.3 (+29%). Live: console "gpu.xenos.clear_memory_page_state true".
if not defined BO2MP_PAGECLEAR set "BO2MP_PAGECLEAR=false"
rem P2 batch (bo2-port e6b6e437): async replay, 3 upload copy workers, 7-frame upload prefetch,
rem CPU write invalidation 2^4 pages. In-match A/B 2026-11-12, 4p bots uncapped, scale 1, 6 x 25 s:
rem 135.0 vs 113.8 fps (+18.6%%), on won 6/6. Live override: run\live.txt (see ab\all_off.txt).
rem P2 batch 2 (bo2-port 911f022d): deferred guest stores, range fast path, fetch dedup, bulk register
rem writes. Same A/B on top of batch 1: 175.3 vs 140.5 fps (+24.8%%), on won 6/6.
rem Texture resource recycling 384 MB (bo2-port 40019cbe): 236.3 vs 194.4 fps (+21.6%%), on won 6/6,
rem minimums 184-228 vs 120-153.
rem 4x views of 1x-owned EDRAM drawn into the 1x targets (bo2-port 7a60b5a8, msaa_4x_alias_as_1x): the
rem XDK clears 1x surfaces through half-size 4x views, which cost two transfers per clear with the
rem native split views. Peer (GTX 1050 Ti) 4p bots, in-match A/B 6 x 22 s: 77.0 vs 61.6 fps (+25%%),
rem on won 6/6, GPU transfers 5.54 -> 1.28 ms/frame. Live: "gpu.d3d12.msaa_4x_alias_as_1x false".
rem Fetch dedup off (2026-11-13): first blamed for the peer's red/green static, wrongly - live toggling
rem misled (the damage persists and clears on its own). Worth ~1%, so it stays off.
rem BO2MP_ASYNC_REPLAY (default true). Async replay caused the red/green static and pink objects on
rem the peer (NVIDIA) - found 2026-11-14 by fresh-launch bisection; the NVIDIA driver corrupted
rem textures when texture loads / texture descriptor creation overlapped the replay thread recording
rem a command list. Fixed in the runtime (gpu.d3d12.async_replay_lock, default 18), so replay is on
rem again everywhere; BO2MP_ASYNC_REPLAY=false remains as a fallback.
if not defined BO2MP_ASYNC_REPLAY set "BO2MP_ASYNC_REPLAY=true"
rem resolve_to_texture (2026-11-15): 1x k_8_8_8_8 resolves are blitted from the host render target
rem straight into the texture that samples them instead of being reloaded (case A; only at
rem resolution scale 1). Verified exact by readback (173 compares), user visual checks on both PCs.
rem Peer 4p A/B 6 x 25 s: 74.7 vs 72.1 fps (+3.6%%), GPU 12.81 vs 13.35 ms. Live: "... false" to undo.
rem kernel.yield_idle_sleep_us=50 (2026-11-15): BO2's idle job workers poll through SwitchToThread
rem (~1.6M yields/s); a thread yielding back to back sleeps 50 us instead. 1p same-match A/B,
rem 5 x 15 s: uncapped 6.35 -> 4.78 cores, 120 cap 3.54 -> 2.83 cores, 60 cap 2.46 -> 2.25, fps
rem unchanged (for the Odin 3 target). Live: "kernel.yield_idle_sleep_us 0" to undo.
rem kernel.wait_poll_sleep_us=50: the render thread (Backend) polls an event with ~455k zero-timeout
rem waits/s; repeated timed-out polls sleep 50 us too (Backend 84%% -> 5%% of a core). 120 cap,
rem 5 x 15 s: 2.83 -> 2.46 cores. kernel.yield_idle_gap_us=1000: idle job workers scan their queues
rem ~90 us between yields, so polls up to 1 ms apart count as back to back: 2.41 -> 2.04 cores.
rem fps unchanged in both (median ~124, p5 ~123.5). All told 3.54 -> ~2.0 cores at a 120 cap.
rem Rechecked 2026-11-26 after the upstream merge (SDK 0.12.0, cheaper locks): 1p 120 cap, 5 x 15 s
rem (tools\ab_cpu.py ab\idle_off.txt ab\idle_on.txt): off 3.22 -> on 1.82 cores, fps 123.0 -> 124.4.
rem texture_cpu_written_reload_per_frame (2026-11-14): a texture the CPU keeps writing (BO2's 2 MB 3D
rem 512x256x4, ~52% of texture load bytes) is reloaded once per frame instead of on every draw.
rem Peer 4p in-match A/B 6 x 25 s: 73.5 vs 65.8 fps (+12%%), p10 64.9 vs 55.2, GPU 13.1 vs 15.1 ms,
rem on won 5/6; user visual check (weapons, arms, props lighting): fine. Live: "... false" to undo.
rem BO2MP_LMP_BOT_LIMIT=14 (2026-11-15): Local Multiplayer bots up to 14 (18 players with 4 local),
rem like System Link. Menu: the bots Lua caps friends+enemies at 9 there (now the value given) and
rem each side at 9 (now 10, the most its labels allow; app lua_patches.cpp). Match: _bot.gsc
rem bot_local_think caps local games at 10 players (now 18; app gsc_patches.cpp). User-checked: bots
rem spawn past 9: FFA 14, TDM up to 13 a team (the scoreboard limit; lobby clamp 9 -> 13). BO2MP_LMP_BOT_LIMIT= (empty) leaves the game as shipped.
if not defined BO2MP_LMP_BOT_LIMIT set "BO2MP_LMP_BOT_LIMIT=14"
rem BO2MP_MENU_QUIT=1 (2026-11-15): the multiplayer main menu reads System Link, Local, Options,
rem Quit (no Xbox LIVE; Quit is the PC menu's own button and popup; app lua_patches.cpp).
rem User-checked. BO2MP_MENU_QUIT=0 leaves the menu as shipped.
if not defined BO2MP_MENU_QUIT set "BO2MP_MENU_QUIT=1"
rem vsync off is bo2-bisect's play default (its app set it on every launch; BO2MP_VSYNC=1 = on).
if not defined BO2MP_VSYNC set "BO2MP_VSYNC=0"
rem Native 1280x720 single-player scene without MSAA (app scene_res.cpp; BO2 renders 880x720 2x and
rem upscales). User-checked 2026-11-12 at resolution scale 2 (explosions, stuns, flashbang, smoke,
rem shadows fine; "huge improvement"). BO2MP_SCENE_1280=0 = stock 880x720 2x.
if not defined BO2MP_SCENE_1280 set "BO2MP_SCENE_1280=1"
rem Native split-screen views without MSAA: 2p 832x352 -> 1280x360, 4p 448x352 -> 640x360 (+ derived
rem targets, app scene_res.cpp). Checked in 3p only (4p targets); 2p untested. BO2MP_SPLIT_NATIVE=0 = stock.
if not defined BO2MP_SPLIT_NATIVE set "BO2MP_SPLIT_NATIVE=1"
rem BO2MP_VPADS=1: three virtual pads (app "bo2vpad" input driver) and the player 1 pad mirror
rem (Y = virtual pads press A/join, X = B, RB = spectate). mirror_off.cmd / mirror_on.cmd switch it live.
if not defined BO2MP_VPADS set "BO2MP_VPADS=0"
set "HID_DRIVERS=sdl3"
if "%BO2MP_VPADS%"=="1" set "HID_DRIVERS=sdl3,bo2vpad"
if "%BO2MP_VPADS%"=="1" if not defined BO2MP_PAD_MIRROR set "BO2MP_PAD_MIRROR=1"
if "%BO2MP_VPADS%"=="1" if exist "%BO2MP_BIN%padmirror.txt" del "%BO2MP_BIN%padmirror.txt"
set "VSYNC=false"
if "%BO2MP_VSYNC%"=="1" set "VSYNC=true"
set "REFRESH=60"
if not "%~1"=="" set "REFRESH=%~1"
if defined BO2MP_REFRESH set "REFRESH=%BO2MP_REFRESH%"
if not exist cache_mp mkdir cache_mp
"%BO2MP_BIN%defaultmp.exe" ^
  --title.game_root="%BO2MP_DATA%" --title.update_root="%BO2MP_DATA%" ^
  --kernel.signed_in_users=%BO2MP_USERS% ^
  --gpu.vsync=%VSYNC% ^
  --hid.drivers=%HID_DRIVERS% ^
  --app.cache_root="%BO2MP_BIN%cache_mp" ^
  --log.file_name=mp_run.log --log.level=%BO2MP_LOGLEVEL% ^
  --ui.video_mode_width=1280 --ui.video_mode_height=720 --ui.video_mode_refresh_rate=%REFRESH% ^
  --gpu.d3d12.render_target_path=%BO2MP_RTPATH% ^
  --gpu.xenos.readback_memexport=%BO2MP_MEMEXP_READBACK% ^
  --gpu.xenos.occlusion_query_enable=%BO2MP_OCCLUSION% ^
  --gpu.xenos.execute_unclipped_draw_vs_on_cpu=true ^
  --gpu.xenos.clear_memory_page_state=%BO2MP_PAGECLEAR% ^
  --gpu.d3d12.async_replay=%BO2MP_ASYNC_REPLAY% --gpu.d3d12.upload_copy_workers=3 ^
  --gpu.d3d12.upload_prefetch_frames=7 --gpu.xenos.cpu_write_invalidation_pages_log2=4 ^
  --gpu.d3d12.defer_guest_stores=true --gpu.xenos.shared_memory_fast_path=true ^
  --gpu.d3d12.fetch_dedup=false --gpu.xenos.bulk_register_writes=true ^
  --gpu.d3d12.texture_recycle_pool_mb=384 --gpu.d3d12.msaa_4x_alias_as_1x=true ^
  --gpu.xenos.texture_cpu_written_reload_per_frame=true --gpu.d3d12.resolve_to_texture=true ^
  --kernel.yield_idle_sleep_us=50 --kernel.wait_poll_sleep_us=50 --kernel.yield_idle_gap_us=1000 ^
  --runtime.xma_decode_threads=3 --runtime.ffmpeg_alloc_cache=true ^
  --gpu.resolution_scale_x=%BO2MP_RES_SCALE% --gpu.resolution_scale_y=%BO2MP_RES_SCALE% %BO2MP_ARGS%
