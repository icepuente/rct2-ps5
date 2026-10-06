# OpenRCT2: PS5 native port

A native PS5 port of [OpenRCT2](https://github.com/OpenRCT2/OpenRCT2) v0.5.5, the open-source re-implementation of RollerCoaster Tycoon 2. It launches from the home screen of a jailbroken console. Bring your own game files.

> **Status: early but playable.** On hardware, OpenRCT2 boots to the title screen with music and loads RCT2 scenarios into a playable park. Saving and loading games works. Long play sessions and every menu have not been tested yet.

[Requirements](#requirements) · [Installation](#installation) · [Controls](#controls) · [Build from source](#building-and-layout)

## Features

- Native folder title (`PPSA99702`), registered by ShadowMountPlus and started from the home screen. No PKG is needed.
- An original loading screen covers startup. The first launch indexes 2,518 objects, which takes about 20 seconds.
- OpenRCT2 v0.5.5 is built from upstream source as a pinned submodule. PS5 differences are handled at link time in [`platform/`](platform). Two small upstream bug fixes are applied from [`patches/openrct2`](patches/openrct2).
- Software rendering at 1920×1080 through the PS5 SDL2 port, with the interface scaled 2× for the TV.
- A DualSense-driven mouse cursor. OpenRCT2 is mouse-driven, so the controller moves a drawn pointer (see [Controls](#controls)).
- Sound effects and music through SDL2 audio.
- Saves and settings are stored in the title's writable `/download0` storage.
- Plugin scripting (QuickJS) is enabled. Multiplayer and HTTP features are compiled out.

## Requirements

- A homebrew-capable PS5 with:
  - an ELF loader (elfldr);
  - **kstuff**, to run fake-signed titles;
  - **ShadowMountPlus**, to register folder titles;
  - **PS5 Web File Manager** (port 8888), which the upload scripts use.

  Tested on firmware **13.60** with the Relapse jailbreak and PLK's Payload Manager. Other firmware and loader combinations have not been tested.
- Your own copy of **RollerCoaster Tycoon 2**, for example the Steam *Triple Thrill Pack* (app 285330) or GOG. About 700 MB of it is uploaded.
- About 1 GB of free console storage.
- To build: macOS or Linux with Docker. Apple Silicon with Colima is tested.

## Installation

There are no prebuilt releases yet; [build from source](#building-and-layout) first. Then, with the console's IP address:

1. Upload the title:

   ```bash
   PS5_HOST=192.168.0.134 scripts/upload.py build/dist/PPSA99702 /data/homebrew
   ```

2. Upload your RCT2 files into the title. The folder is the game's install directory, the one containing `Data/g1.dat`:

   ```bash
   PS5_HOST=192.168.0.134 scripts/install-game-data.sh /path/to/rct2
   ```

   Steam won't download this Windows-only game on macOS, but SteamCMD will download the depot:

   ```bash
   steamcmd +login YOUR_STEAM_USERNAME +download_depot 285330 285331 +quit
   ```

3. ShadowMountPlus adds **OpenRCT2** to the home screen. Launch it from there.

Everything except the game's `Install` folder is uploaded. `ObjData` file names are uppercased, because OpenRCT2 looks objects up by uppercase name and the console's filesystem is case-sensitive. Nothing from the game is ever added to this repository.

## Controls

| Control | Action |
| --- | --- |
| Left stick | Move the cursor (speeds up with deflection) |
| Right stick | Scroll the map |
| Cross | Left click (hold to drag) |
| Circle | Right click (hold and move to drag the view) |
| L1 / R1 | Zoom out / in |
| Triangle | Rotate the view (Return) |
| Square | Close the top window (Backspace) |
| Options | Cancel / close (Escape) |
| D-pad | Jump the cursor to the nearest button in that direction. Steps through dropdown items and list rows; hold to repeat |
| Others | Passed to OpenRCT2. Bind them under *Options → Controls → Shortcut keys* |

## Validation and known limits

Verified on hardware:

- The title screen renders at about 60 fps with title music.
- 2,518 objects, 204 track designs and 57 scenarios are indexed.
- The automated scenario tour loaded and ran all 57 RCT2 scenarios with no crashes or load failures. Each ran at 6.4–7.8× real time against an 8× target. The busiest park was Six Flags Holland, with 2,781 guests at 7.3×. A full pass takes 21 minutes.
- Scenario names are readable, and a scenario loads into a playable park.
- Saving a game, and loading it again through OpenRCT2's own load window, works. Saves go to the title's `/download0` storage.
- The controller-driven cursor works for clicking and scrolling.
- The SDL2 test title (`tools/sdl-smoke`, `PPSA99998`) checks video, audio, the DualSense, the app heap and returning to the home screen on their own.

Known limits:

- **No networking.** Multiplayer, the server list and update checks are disabled.
- **Sprite fonts only.** TrueType fonts are disabled, so the Chinese, Japanese and Korean translations, which need TTF, won't render properly.
- **Placeholder icon and backgrounds.** They're the boilerplate's. `PPSA99702` is a development title ID.
- **No on-screen keyboard yet.** Naming parks and rides with the PS5 keyboard dialog is untested.
- **The "What's new" window is empty** on first launch, because OpenRCT2's `doc/` files aren't packaged.

### Automated testing

There are two modes.

**Play** (`OPENRCT2_PS5_AUTOTEST=play`): each launch opens the next scenario, and the plugin plays it. It opens the park, takes the maximum loan, hires a handyman, runs a marketing campaign and funds research. It then extends a footpath and builds and opens a stall on a path reachable from the park entrance. Finally it fast-forwards a year, logging guests, rating, cash and stall customers each month, and ends with a PASS or PARTIAL line.

**Tour** (`OPENRCT2_PS5_AUTOTEST=tour`): `scripts/build.sh openrct2` with that setting builds a title that installs [`ps5/autotest/ps5-autotest.js`](ps5/autotest/ps5-autotest.js). On the title screen, the plugin tours every installed scenario. It loads each one, runs it at the fastest speed for 20 seconds, and logs a line per scenario: whether it loaded, how fast the simulation ran, guests and rating. The tour repeats, with a summary after each pass. Leave the game on the title screen and read the results from the kernel log. Normal builds remove the plugin.

To debug, build with `OPENRCT2_PS5_DEBUG=1 scripts/build.sh openrct2`. That adds OpenRCT2's verbose log and an SDL display trace. Launch **klogsrv** and read the log:

```bash
nc 192.168.0.134 3232
```

## How it works

OpenRCT2 and its libraries are cross-compiled with the [PS5 payload SDK](https://github.com/ps5-payload-dev/sdk) and [pacbrew](https://github.com/ps5-payload-dev/pacbrew-repo) packages. [`scripts/package-native.sh`](scripts/package-native.sh) then links them into a native title using [ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate)'s startup code, runtime shim and FSELF signer.

Plain elfldr payloads run in the background and can't show video or take controller input. A native title can, but it gets a different runtime than payloads, which [`platform/`](platform) bridges:

| File | Purpose |
| --- | --- |
| `ps5_heap.c` | An allocator on up to 4 GiB of direct memory, falling back to flexible memory. The default libc heap fails at 8 MB, and flexible memory (about 448 MiB) is too small for large parks. |
| `ps5_native_shims.c` | Lazy loading of modules a title doesn't preload (keyboard, IME, random). Replacements for functions only `libScePosixForWebKit` exports. stdout goes to the kernel log, and `exit()` asks the system to close the app. |
| `ps5_libc_compat.c` | C-locale versions of the locale and libc functions that libc++ needs. |
| `ps5_dirent.c` | `opendir`/`readdir` on the kernel's `getdents`. The console libc lists nothing for a title, and `getdents` needs a 64 KiB buffer. |
| `ps5_emutls.c`, `ps5_thread_atexit.c` | Emulated TLS and `thread_local` destructors. |
| `sdl/ps5_virtual_mouse.c` | The controller-driven cursor. |
| `sdl/ps5_sdl_render.c` | Makes SDL's software renderer offer only 32-bit texture formats, matching the screen. |

[`ps5/ui_snap.cpp`](ps5/ui_snap.cpp) gives the D-pad its targets. It reads OpenRCT2's open windows for clickable widgets, or an open dropdown's rows, and is compiled with OpenRCT2's own flags.

[`scripts/check-imports.sh`](scripts/check-imports.sh) fails the build if any import would still resolve to a module the title doesn't load. Such an import would be a call to address 0 at run time.

A few upstream fixes are carried as patches:

- **PS5 SDL2 port** ([`deps/SDL2`](deps/SDL2)): falls back to a smaller video-memory reservation when the process budget can't fit two 4K framebuffers.
- **ICU** ([`deps/icu`](deps/icu)): takes `wchar_t`'s size from the compiler. It's 2 bytes on the PS5 target, but ICU assumes 4 on BSD.
- **OpenRCT2** ([`patches/openrct2`](patches/openrct2), applied at build time):
  - fixes the 2-byte `wchar_t` string conversions;
  - fixes a double free when a WAV, OGG or FLAC stream fails to load;
  - lets title sequences load scenario files: the scenario check was given the whole file name instead of its extension;
  - echoes plugin `console.log` output to stdout on PS5, for the autotest.

## Building and layout

Prerequisites: Docker (or Colima) and `git`. Build the toolchain image once (about 5 minutes), fetch OpenRCT2's data files, then build:

```bash
git clone --recursive https://github.com/icepuente/rct2-ps5
cd rct2-ps5
docker build -f docker/Dockerfile -t ps5-openrct2-env .
scripts/fetch-openrct2-data.sh
scripts/build.sh openrct2
```

The title folder is written to `build/dist/PPSA99702`. `scripts/build.sh smoke-native` builds the SDL2 test title.

```text
docker/      toolchain image: payload SDK + pacbrew libraries
deps/        extra packages for the image: ICU (filtered), nlohmann/json, patched SDL2
native/      ps5-native-app-boilerplate (submodule)
openrct2/    OpenRCT2 v0.5.5 (submodule)
patches/     fixes applied to the OpenRCT2 source at build time
platform/    runtime shims linked into every native title
ps5/         OpenRCT2 launcher, title metadata, default config, autotest plugin
scripts/     build, package, import check, upload and data install
tools/       sdl-smoke (SDL2 test title), dmem-probe (payload memory probe),
             make-loading-screen.py (draws ps5/assets/loading.bmp)
```

## Credits and license

- [OpenRCT2](https://github.com/OpenRCT2/OpenRCT2) and its contributors (GPL-3.0). The PS5 build uses the official release's data files.
- [ps5-payload-dev](https://github.com/ps5-payload-dev) (John Törnblom and contributors): the payload SDK, SDL2 port, pacbrew packages and elfldr.
- [ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate) (BlackBearReloaded, GPL-3.0): native title tooling.
- The approach to libc++ compatibility follows [morrowind-ps5](https://github.com/mshivam019/morrowind-ps5) and the Ship of Harkinian PS5 port.
- [ICU](https://icu.unicode.org) (Unicode License), [nlohmann/json](https://github.com/nlohmann/json) (MIT), libpng, zlib, libzip, zstd, FLAC, Ogg/Vorbis and libsamplerate, each under its own license.

This project is licensed under the GPL-3.0-or-later; see [LICENSE](LICENSE). It is not affiliated with or endorsed by Sony Interactive Entertainment, Atari or the OpenRCT2 team. RollerCoaster Tycoon 2 is the property of its respective owners and is not included.
