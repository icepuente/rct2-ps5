# OpenRCT2: PS5 native port

A native PS5 port of [OpenRCT2](https://github.com/OpenRCT2/OpenRCT2) v0.5.5, the open-source re-implementation of RollerCoaster Tycoon 2. It launches from the home screen of a jailbroken console. Bring your own game files.

> **Status: early but playable.** On hardware it boots to the title screen with music. All 57 RCT2 scenarios load and run, parks play at full speed, and saving and loading work. It's played with the DualSense. Long play sessions and every menu have not been tested yet.

[Download](https://github.com/icepuente/rct2-ps5/releases) · [Requirements](#requirements) · [Installation](#installation) · [Controls](#controls) · [Build from source](#building-and-layout)

## Features

- **A native folder title** (`PPSA99702`). ShadowMountPlus registers it and you start it from the home screen like any game. No PKG is needed.
- **OpenRCT2 v0.5.5** is built from upstream source as a pinned submodule. PS5 differences are handled at link time in [`platform/`](platform), and a few small upstream bug fixes are applied from [`patches/openrct2`](patches/openrct2).
- **1920×1080 output** through the PS5 SDL2 port, with the interface scaled 2× for the TV. The title screen runs at about 60 fps, and the simulation keeps the fastest speed (8×) even with 1,700 guests.
- **DualSense controls:**
  - the left stick drives a drawn mouse cursor;
  - the D-pad jumps between buttons, dropdown items and list rows;
  - the face buttons cover clicking, rotating, zooming and closing windows.

  See [Controls](#controls).
- **Sound effects and music** through SDL2 audio.
- **Up to 4 GiB of memory.** Large parks fit; the default PS5 heap for homebrew does not.
- **Saves and settings** are stored in the title's own storage (`/download0`), so updates keep them.
- **Original home-screen icon and loading screen.** The first launch indexes about 2,500 objects in about 20 seconds behind the loading screen.
- **OpenRCT2's "What's new" changelog** and licence are included.
- **Plugin scripting** (QuickJS) is enabled. Multiplayer and online features are compiled out.

## Requirements

- A homebrew-capable PS5 with:
  - **kstuff**, to run fake-signed titles;
  - **ShadowMountPlus**, or another loader that registers folder titles;
  - a way to copy files to the console: ps5upload, FTP or PS5 Web File Manager.

  Tested on firmware **13.60** with the Relapse jailbreak and PLK's Payload Manager. Other firmware and loader combinations have not been tested.
- Your own copy of **RollerCoaster Tycoon 2**, for example the Steam *Triple Thrill Pack* (app 285330) or GOG. About 700 MB of it is copied to the console.
- About 1 GB of free console storage.
- Building from source isn't needed to play. It needs macOS or Linux with Docker; Apple Silicon with Colima is tested.

## Installation

1. Download `OpenRCT2-PS5-<version>.zip` from [Releases](https://github.com/icepuente/rct2-ps5/releases) and extract it. If no release is listed yet, [build from source](#building-and-layout).
2. Copy these folders from your own RollerCoaster Tycoon 2 installation into `PPSA99702/assets/rct2`:

   `Data`, `ObjData`, `Scenarios`, `Tracks`, `Landscapes` and `Saved Games`

   The folder is right when `PPSA99702/assets/rct2/Data/g1.dat` exists. The console's filesystem is case-sensitive, and OpenRCT2 looks objects up by uppercase name, so files in `ObjData` need uppercase names. Some copies ship `wallsign.dat`, `ssig4.dat` and `Cerberus.dat`; rename them to `WALLSIGN.DAT`, `SSIG4.DAT` and `CERBERUS.DAT`.
3. Upload the whole `PPSA99702` folder to your console's homebrew folder, e.g. `/data/homebrew/PPSA99702`. Use ps5upload, FTP or a file manager.
4. ShadowMountPlus registers the folder, and **OpenRCT2** appears on the home screen.

The first start shows the loading screen for about 20 seconds while OpenRCT2 indexes its objects. Later starts are faster.

**Updating:** close the game and replace everything in `PPSA99702` except `assets/rct2`. Saves and settings are kept.

**Getting the game files on a Mac or Linux PC:** Steam won't install the Windows-only Triple Thrill Pack there, but SteamCMD can download it:

```bash
steamcmd +login YOUR_STEAM_USERNAME +download_depot 285330 285331 +quit
```

## Controls

| Control | Action |
| --- | --- |
| Left stick | Move the cursor (speeds up with deflection) |
| D-pad | Jump the cursor to the nearest button in that direction. Steps through dropdown items and list rows; hold to repeat |
| Cross | Left click (hold to drag) |
| Circle | Right click (hold and move to drag the view) |
| Right stick | Scroll the map |
| L1 / R1 | Zoom out / in |
| Triangle | Rotate the view |
| Square | Close the top window |
| Options | Cancel / close (Escape) |
| Others | Passed to OpenRCT2. Bind them under *Options → Controls → Shortcut keys* |

## Validation and known limits

Verified on a PS5 (firmware 13.60):

- **Startup:** boots to the title screen with music, which renders at about 60 fps.
- **Indexing:** 2,518 objects, 204 track designs and 57 scenarios are indexed. Scenario names display correctly.
- **Every scenario loads.** The automated scenario tour loaded and ran all **57** RCT2 scenarios with no crashes or load failures. The simulation ran at 6.4–7.8× real time against an 8× target.
- **Gameplay.** The automated play test (see [Automated testing](#automated-testing)) passed in four scenarios: Over the Edge, Amity Airfield, Icy Adventures and Six Flags Holland. In each it:
  1. opened the park;
  2. hired staff, plus any loan, marketing and research the scenario allows;
  3. extended a path and built and opened a stall;
  4. played a full in-game year (eight months) at **8.0× real time**.
- **An established park.** Six Flags Holland starts with 1,331 guests and grew to 1,742 at full speed; its new Burger Bar served 190 customers. The other three parks start empty, so one stall drew almost no guests. They verify building and the simulation, not park growth.
- **Partial run.** In Great Wall of China every step passed except marketing, which the scenario forbids. That run was stopped after three months.
- **Saving and loading** through OpenRCT2's own windows works.
- **Controls:** the controller cursor and D-pad snapping work on hardware.
- **Releases:** the release workflow builds the release zip on GitHub from a clean checkout.

Known limits:

- **No networking.** Multiplayer, the server list and update checks are disabled.
- **Sprite fonts only.** TrueType fonts are disabled, so the Chinese, Japanese and Korean translations, which need TTF, won't render properly.
- **No custom home-screen backgrounds.** The tile uses the console's default; only the icon is custom.
- **On-screen keyboard is untested.** Naming parks and rides with the PS5 keyboard dialog hasn't been tried.
- **Development title ID.** `PPSA99702` is a development ID that isn't registered anywhere. Other homebrew could use the same one.

## How it works

OpenRCT2 and its libraries are cross-compiled with the [PS5 payload SDK](https://github.com/ps5-payload-dev/sdk) and [pacbrew](https://github.com/ps5-payload-dev/pacbrew-repo) packages. [`scripts/package-native.sh`](scripts/package-native.sh) then links them into a native title using [ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate)'s startup code, `libc.prx` runtime shim and FSELF signer.

Plain elfldr payloads run in the background and can't show video or take controller input. A native title can, but it gets a different runtime than payloads, which [`platform/`](platform) bridges:

| File | Purpose |
| --- | --- |
| `ps5_heap.c` | An allocator on up to 4 GiB of direct memory, falling back to flexible memory. The default libc heap fails at 8 MB, and flexible memory (about 448 MiB) is too small for large parks. |
| `ps5_native_shims.c` | Lazy loading of modules a title doesn't preload (keyboard, IME, random), and replacements for functions only `libScePosixForWebKit` exports. Console stdio goes to the kernel log, `exit()` asks the system to close the app, and `fork`/`popen` fail cleanly. |
| `ps5_libc_compat.c` | C-locale versions of the locale and libc functions that libc++ needs. |
| `ps5_dirent.c` | `opendir`/`readdir` on the kernel's `getdents`. The console libc lists nothing for a title, and `getdents` needs a 64 KiB buffer. |
| `ps5_emutls.c`, `ps5_thread_atexit.c` | Emulated TLS and `thread_local` destructors. |
| `sdl/ps5_virtual_mouse.c` | The controller-driven cursor, button mapping and D-pad repeat. |
| `sdl/ps5_sdl_render.c` | Makes SDL's software renderer offer only 32-bit texture formats, matching the screen. |

[`ps5/main.c`](ps5/main.c) is the launcher. It shows the loading screen, installs the TV-friendly default config, and points OpenRCT2 at the packaged data, the player's RCT2 files and the title's writable storage. [`ps5/ui_snap.cpp`](ps5/ui_snap.cpp) gives the D-pad its targets: it reads OpenRCT2's open windows for clickable widgets, or an open dropdown's rows, and is compiled with OpenRCT2's own flags.

[`scripts/check-imports.sh`](scripts/check-imports.sh) fails the build if any import would still resolve to a module the title doesn't load. Such an import would be a call to address 0 at run time.

Upstream fixes carried as patches:

- **PS5 SDL2 port** ([`deps/SDL2`](deps/SDL2)): falls back to a smaller video-memory reservation when the process budget can't fit two 4K framebuffers.
- **ICU** ([`deps/icu`](deps/icu)): takes `wchar_t`'s size from the compiler. It's 2 bytes on the PS5 target, but ICU assumes 4 on BSD.
- **OpenRCT2** ([`patches/openrct2`](patches/openrct2), applied at build time):
  - fixes the 2-byte `wchar_t` string conversions;
  - fixes a double free when a WAV, OGG or FLAC stream fails to load;
  - lets title sequences load scenario files: the scenario check was given the whole file name instead of its extension;
  - echoes plugin `console.log` output to stdout on PS5, for the autotest.

## Building and layout

Prerequisites: Docker (or Colima) and `git`. Build the toolchain image once (about 5–10 minutes), fetch OpenRCT2's data files, then build:

```bash
git clone --recursive https://github.com/icepuente/rct2-ps5
cd rct2-ps5
docker build -f docker/Dockerfile -t ps5-openrct2-env .
scripts/fetch-openrct2-data.sh
scripts/build.sh openrct2
```

The title folder is written to `build/dist/PPSA99702`; add your RCT2 files as in [Installation](#installation).

**Releases.** `scripts/make-release.sh v0.1.0` builds a clean title and writes `build/release/OpenRCT2-PS5-v0.1.0.zip` plus its SHA-256. The zip contains:

- the `PPSA99702` folder;
- an empty `assets/rct2` with instructions;
- licences and a short `README.txt`.

No game data is included. The [Release workflow](.github/workflows/release.yml) does the same on GitHub. Pushing a `v*` tag publishes a GitHub Release; running the workflow by hand produces a downloadable artifact.

### Development

These scripts shorten the edit-test loop against a console running PS5 Web File Manager (port 8888):

```bash
# Install or update the built title (only changed files need uploading).
PS5_HOST=192.168.0.134 scripts/upload.py build/dist/PPSA99702 /data/homebrew
# Upload RCT2 files from a local install; ObjData names are uppercased.
PS5_HOST=192.168.0.134 scripts/install-game-data.sh /path/to/rct2
```

The console locks a title's files while it runs, so close the game before uploading.

Build variants:

- `OPENRCT2_PS5_DEBUG=1 scripts/build.sh openrct2` adds OpenRCT2's verbose log and an SDL display trace. To read the console's log, launch **klogsrv** and run `nc <console IP> 3232`.
- `scripts/build.sh smoke-native` builds the SDL2 test title (`tools/sdl-smoke`, `PPSA99998`). It checks video, audio, the DualSense, the app heap and returning to the home screen on their own.
- `scripts/make-art.sh` redraws the icon and loading screen from `tools/make-icon.py` and `tools/make-loading-screen.py`.

### Automated testing

`OPENRCT2_PS5_AUTOTEST=play` or `tour` builds a title that installs the [`ps5/autotest/ps5-autotest.js`](ps5/autotest/ps5-autotest.js) plugin. Its results appear in the kernel log. Normal builds remove the plugin.

**Play:** each launch opens the next scenario in alphabetical order, or the one named in `assets/autotest/play.txt`. The plugin:

1. opens the park;
2. takes the maximum loan, hires a handyman, starts a marketing campaign and funds research (a scenario's rules may refuse some of these, which counts as skipped, not failed);
3. extends a footpath and builds and opens a food or drink stall on a path reachable from the park entrance;
4. fast-forwards a full in-game year (March to October), logging guests, rating, cash and stall customers each month.

It ends with a PASS or PARTIAL line.

**Tour:** on the title screen, the plugin loads every installed scenario in turn and runs each at the fastest speed for 20 seconds. It logs whether each one loaded, how fast its simulation ran, its guests and its rating. A full pass takes about 21 minutes and repeats, with a summary after each pass.

### Layout

```text
.github/     release workflow
docker/      toolchain image: payload SDK + pacbrew libraries
deps/        extra packages for the image: ICU (filtered), nlohmann/json, patched SDL2
native/      ps5-native-app-boilerplate (submodule)
openrct2/    OpenRCT2 v0.5.5 (submodule; openrct2.version pins its data release)
patches/     fixes applied to the OpenRCT2 source at build time
platform/    runtime shims linked into every native title
ps5/         launcher, D-pad navigation, icon and metadata, default config, loading screen, autotest plugin
scripts/     build, package, release, import check, upload, data install and artwork
tools/       sdl-smoke (SDL2 test title), dmem-probe (payload memory probe), artwork generators
```

## Credits and license

- [OpenRCT2](https://github.com/OpenRCT2/OpenRCT2) and its contributors (GPL-3.0). The PS5 build uses the official release's data files.
- [ps5-payload-dev](https://github.com/ps5-payload-dev) (John Törnblom and contributors): the payload SDK, SDL2 port, pacbrew packages and elfldr.
- [ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate) (BlackBearReloaded, GPL-3.0): native title tooling.
- The approach to libc++ compatibility and directory reading follows [morrowind-ps5](https://github.com/mshivam019/morrowind-ps5) and the Ship of Harkinian PS5 port.
- [ICU](https://icu.unicode.org) (Unicode License), [nlohmann/json](https://github.com/nlohmann/json) (MIT), libpng, zlib, libzip, zstd, FLAC, Ogg/Vorbis and libsamplerate, each under its own license.

This project is licensed under the GPL-3.0-or-later; see [LICENSE](LICENSE). Bundled components are listed in [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md). It is not affiliated with or endorsed by Sony Interactive Entertainment, Atari or the OpenRCT2 team. RollerCoaster Tycoon 2 is the property of its respective owners and is not included.
