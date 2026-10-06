# OpenRCT2: PS5 native port

A native PS5 port of [OpenRCT2](https://github.com/OpenRCT2/OpenRCT2) v0.5.5, the open-source re-implementation of RollerCoaster Tycoon 2. It launches from the home screen of a jailbroken console. Bring your own game files.

> **Status: work in progress.** The toolchain, native-title packaging and an SDL2 test title (video, audio, DualSense, clean exit) are verified on hardware. OpenRCT2 builds, links and installs, but its first launch on the console has not been validated yet.

[Requirements](#requirements) · [Installation](#installation) · [Controls](#controls) · [Build from source](#building-and-layout)

## Features

- Native folder title (`PPSA99702`), registered by ShadowMountPlus and started from the home screen. No PKG is needed.
- OpenRCT2 v0.5.5 is built from unmodified upstream source as a pinned submodule. Every PS5 difference is handled at link time in [`platform/`](platform).
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

  Developed with the Relapse jailbreak (firmware 7.00–13.60) and PLK's Payload Manager. Other firmware and loader combinations have not been tested.
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

Only the game's `Data`, `ObjData`, `Scenarios`, `Tracks`, `Landscapes` and `Saved Games` folders are uploaded. Nothing from the game is ever added to this repository.

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
| D-pad, others | Passed to OpenRCT2. Bind them under *Options → Controls → Shortcut keys* |

## Validation and known limits

Verified on hardware with the SDL2 test title (`tools/sdl-smoke`, `PPSA99998`):

- 1080p video;
- 48 kHz stereo audio;
- DualSense buttons and sticks;
- the 376 MiB app heap;
- returning to the home screen without a crash.

Not yet verified: OpenRCT2 itself on the console. Known limits:

- **No networking.** Multiplayer, the server list and update checks are disabled.
- **Sprite fonts only.** TrueType fonts are disabled, so the Chinese, Japanese and Korean translations, which need TTF, won't render properly.
- **Placeholder art.** The icon and backgrounds are the boilerplate's. `PPSA99702` is a development title ID.
- **No on-screen keyboard yet.** Naming parks and rides with the PS5 keyboard dialog is untested.

To debug, launch **klogsrv** and read the console log; the title writes its stdout there:

```bash
nc 192.168.0.134 3232
```

## How it works

OpenRCT2 and its libraries are cross-compiled with the [PS5 payload SDK](https://github.com/ps5-payload-dev/sdk) and [pacbrew](https://github.com/ps5-payload-dev/pacbrew-repo) packages. [`scripts/package-native.sh`](scripts/package-native.sh) then links them into a native title using [ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate)'s startup code, runtime shim and FSELF signer.

Plain elfldr payloads run in the background and can't show video or take controller input. A native title can, but it gets a different runtime than payloads, which [`platform/`](platform) bridges:

| File | Purpose |
| --- | --- |
| `ps5_heap.c` | An allocator on a large flexible-memory mapping. The default libc heap fails at 8 MB. |
| `ps5_native_shims.c` | Lazy loading of modules a title doesn't preload (keyboard, IME, random). Replacements for functions only `libScePosixForWebKit` exports. stdout goes to the kernel log, and `exit()` asks the system to close the app. |
| `ps5_libc_compat.c` | C-locale versions of the locale and libc functions that libc++ needs. |
| `ps5_emutls.c`, `ps5_thread_atexit.c` | Emulated TLS and `thread_local` destructors. |
| `sdl/ps5_virtual_mouse.c` | The controller-driven cursor. |

[`scripts/check-imports.sh`](scripts/check-imports.sh) fails the build if any import would still resolve to a module the title doesn't load. Such an import would be a call to address 0 at run time.

The PS5 SDL2 port gets one patch ([`deps/SDL2`](deps/SDL2)). It falls back to a smaller video-memory reservation when the process budget can't fit two 4K framebuffers.

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
openrct2/    OpenRCT2 v0.5.5 (submodule, unmodified)
platform/    runtime shims linked into every native title
ps5/         OpenRCT2 launcher, title metadata, default config
scripts/     build, package, import check, upload and data install
tools/       sdl-smoke (SDL2 test title), dmem-probe (payload memory probe)
```

## Credits and license

- [OpenRCT2](https://github.com/OpenRCT2/OpenRCT2) and its contributors (GPL-3.0). The PS5 build uses the official release's data files.
- [ps5-payload-dev](https://github.com/ps5-payload-dev) (John Törnblom and contributors): the payload SDK, SDL2 port, pacbrew packages and elfldr.
- [ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate) (BlackBearReloaded, GPL-3.0): native title tooling.
- The approach to libc++ compatibility follows [morrowind-ps5](https://github.com/mshivam019/morrowind-ps5) and the Ship of Harkinian PS5 port.
- [ICU](https://icu.unicode.org) (Unicode License), [nlohmann/json](https://github.com/nlohmann/json) (MIT), libpng, zlib, libzip, zstd, FLAC, Ogg/Vorbis and libsamplerate, each under its own license.

This project is licensed under the GPL-3.0-or-later; see [LICENSE](LICENSE). It is not affiliated with or endorsed by Sony Interactive Entertainment, Atari or the OpenRCT2 team. RollerCoaster Tycoon 2 is the property of its respective owners and is not included.
