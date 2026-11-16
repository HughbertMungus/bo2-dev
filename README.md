# bo2-dev

A native Windows build of the Xbox 360 multiplayer executable of *Call of Duty: Black Ops II*
(`default_mp.xex`), statically recompiled with [ReXGlue](https://github.com/rexglue/rexglue-sdk)
(rexglue-sdk), plus performance work in the runtime and a few optional changes to the game.

**No game files are included.** You need your own copy of the Xbox 360 game. The build reads
`default_mp.xex` from it to generate the C++ code, and the game reads its data from it at run time.
The patches in this repository locate the bytes they change by hashes and contain only their own
values. This project is not affiliated with or endorsed by Activision, Treyarch or Microsoft.

Provided as is, without warranty of any kind (see [LICENSE](LICENSE)).

## What it adds

- Split-screen for up to 4 local players, with native 1280x720 single-player scene and native
  split-screen views (no MSAA), rendered at a resolution scale (default 2x, 2560x1440 internal).
- Runtime performance work (D3D12): async command replay, upload batching, texture recycling,
  render-target and resolve fast paths, lower CPU use from idle polling.
- Optional game changes, on by default in `run\run_local.cmd`:
  - `BO2MP_LMP_BOT_LIMIT=14`: Local Multiplayer allows up to 14 bots (18 players with 4 local
    players). Each side's bot count gets a "Maximum" choice; team games stop at 13 per team.
    Empty leaves the game as shipped.
  - `BO2MP_MENU_QUIT=1`: the multiplayer main menu reads System Link, Local, Options, Quit
    (without the non-functional Xbox LIVE entry). `0` leaves it as shipped.
- Branch `attachments` (on top of `main`): `BO2MP_ATTACHMENTS=1` allows 3 primary and 2 secondary
  attachments in Create-a-Class without the Gunfighter wildcards.

## Requirements

- Windows 10 or 11, x64. A GPU with full Direct3D 12 support (tested on AMD and an NVIDIA GTX
  1050 Ti; other hardware is untested).
- To build:
  - Visual Studio 2022 with the C++ workload and the **MSVC v143 14.44** toolset (Visual Studio
    Installer, Individual components). CMake and Ninja come with it.
  - LLVM clang (tested with 23.1). Found on `PATH`, in `%ProgramFiles%\LLVM\bin`, or set
    `BO2DEV_LLVM` in `local.cmd` (see below).
  - The Vulkan SDK 1.4.313 or newer (`VULKAN_SDK` set by its installer). Alternatively
    `tools\mk_vulkan_sdk.cmd` assembles a minimal stand-in from Vulkan-Headers and
    VulkanMemoryAllocator checkouts (`VULKAN_THIRDPARTY`).
  - Git.

## Building

1. Clone this repository and the runtime fork next to each other, the runtime inside it:

   ```bat
   git clone https://github.com/HughbertMungus/bo2-dev.git
   cd bo2-dev
   git clone --branch bo2-port --recurse-submodules --shallow-submodules https://github.com/HughbertMungus/rexglue-sdk.git rexglue-sdk
   ```

2. Put your game files in `bo2-dev\game` (or make `game` a junction to them):
   `default_mp.xex` and the game's data (`*.ff` and the rest of the disc contents, plus the title
   update files) in one folder.

3. Optional: `local.cmd` in the repository root holds this machine's build settings and is not
   tracked, for example:

   ```bat
   @echo off
   set "BO2DEV_LLVM=D:\llvm\bin"
   ```

4. Build the runtime, then the game (code generation runs as part of the first app build):

   ```bat
   build_sdk.cmd
   build_app.cmd
   ```

   The result is `app\out\build\win-amd64\Release\defaultmp.exe` with its DLLs.

5. Copy the built `.exe` and `.dll` files into `run\` (or run `tools\deploy_local.ps1`, which does
   that and checks the hashes).

## Running

From `run\`:

- `play.cmd` - normal play (resolution scale 2, vsync off).
- `play120.cmd` - capped at 120 fps.
- `play120_kb.cmd` - as play120, with the keyboard as player 1's pad.

The game data is taken from `..\game`, or from `..\BO2` (a release layout where the game files sit
next to the `run` folder), or from `BO2MP_DATA`. The first launches build the shader pipeline cache
(`run\cache_mp`), so they stutter until it is warm.

Per-machine launcher settings go in `run\machine.cmd` (see `run\machine.example.cmd`), for example
`BO2MP_RES_SCALE=1` for a slower GPU. Other switches are documented at the top of
`run\run_local.cmd`.

## Repository layout

- `app\` - the game project: the codegen manifest and the hand-written sources (hooks, patches,
  input, render-target sizes).
- `run\` - launchers.
- `tools\` - build environment, deployment, profiling and measurement helpers.
- `ab\` - live setting files used for A/B measurements.
- `rexglue-sdk\` (cloned separately) - the runtime fork, branch `bo2-port`.

## License

BSD 3-Clause, the same license as rexglue-sdk. See [LICENSE](LICENSE).
