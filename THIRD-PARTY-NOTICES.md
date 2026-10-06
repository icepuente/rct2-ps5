# Third-party notices

The OpenRCT2 PS5 release bundles the software and data below. The port's own code is GPL-3.0-or-later; see [LICENSE](LICENSE). Source code for every release is the tagged commit of https://github.com/icepuente/rct2-ps5, with its submodules and the dependency versions pinned in `docker/` and `deps/`.

RollerCoaster Tycoon 2 data is **not** included. Players supply their own copy.

| Component | Use | License |
| --- | --- | --- |
| [OpenRCT2](https://github.com/OpenRCT2/OpenRCT2) v0.5.5 | The game engine, built from source with the patches in `patches/openrct2` | GPL-3.0-or-later |
| OpenRCT2 data (from the official v0.5.5 release) | `assets/openrct2`: graphics, languages, title sequences, sound packs | GPL-3.0-or-later; [OpenRCT2 objects](https://github.com/OpenRCT2/objects) are CC BY 4.0 |
| QuickJS-ng | Plugin scripting, bundled with OpenRCT2 | MIT |
| [SDL2 PS5 port](https://github.com/ps5-payload-dev/SDL) | Video, audio and controller input, with the patch in `deps/SDL2` | zlib |
| [PS5 payload SDK](https://github.com/ps5-payload-dev/sdk) | Toolchain, libc++, libc++abi and libunwind | GPL-3.0-or-later (SDK); Apache-2.0 WITH LLVM-exception (LLVM runtimes) |
| [ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate) | Startup code, `sce_module/libc.prx` runtime shim, FSELF tooling | GPL-3.0-or-later |
| [ICU](https://icu.unicode.org) 77.1 | Text conversion | Unicode-3.0 |
| [nlohmann/json](https://github.com/nlohmann/json) 3.12.0 | JSON parsing | MIT |
| libpng | PNG images | libpng-2.0 |
| zlib | Compression | Zlib |
| libzip | Zip archives | BSD-3-Clause |
| zstd | Compression | BSD-3-Clause |
| bzip2 | Compression | bzip2-1.0.6 |
| xz (liblzma) | Compression | 0BSD |
| FLAC | Audio decoding | BSD-3-Clause |
| libogg, libvorbis | Audio decoding | BSD-3-Clause |
| libsamplerate | Audio resampling | BSD-2-Clause |
| openlibm | Math library | MIT and BSD-style |
| DejaVu Sans | Text in the loading screen image (rendered into `loading.bmp`) | Bitstream Vera / DejaVu license |

The release zip carries this file, the GPL text and OpenRCT2's own licence under `PPSA99702/licenses/`.

This project is not affiliated with or endorsed by Sony Interactive Entertainment, Atari, or the OpenRCT2 team.
