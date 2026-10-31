# Sonic 3 A.I.R. for Nintendo 3DS (unofficial, experimental)

A proof-of-concept port of [Sonic 3 A.I.R.](https://github.com/Eukaryot/sonic3air) ("Sonic 3 - Angel Island Revisited", built on the Oxygen Engine by Eukaryot) to **New Nintendo 3DS** homebrew. It is a personal "what if" experiment, not an official or supported port.

This is a fork of the upstream project with the 3DS changes on top. All credit for the game, engine and scripts goes to Eukaryot and the Sonic 3 A.I.R. contributors. The original readme is kept as [README_UPSTREAM.md](README_UPSTREAM.md).

## Status

It boots, reaches the title screen and menus, and is playable on a New 3DS (installed as a .cia or run as a .3dsx).

Known problems:
- **Frame rate** is not stable yet (around 28-30 fps in testing, before building with the nativized scripts, which should be faster).
- The **sound** can be a bit crunchy, probably tied to the frame rate.
- **NTR screen streaming** shows wrong colours (the console screen itself is fine).
- The **HOME menu banner sound** may not play yet.
- Original **Genesis soundtrack only**: the 126 MB remastered OGG music is not included.
- Online features, update checks and mods are not adapted for the 3DS. The bottom screen is not used.
- Old 3DS / 2DS models are not supported.

## What is different from upstream

- A `PLATFORM_3DS` code path: software renderer only, a 400x224 game screen on the top screen, 32 kHz audio, SD card paths under `sdmc:/3ds/sonic3air`.
- SDL2 2.32 (bundled in `framework/external/sdl`) built with its native 3DS backend.
- Fixes for ARM11 quirks: unaligned multi-register and 64-bit accesses, small default stacks (the game runs on its own large-stack thread), memory mode settings.
- Build files, a makerom spec for the .cia, banner and icon, and staging scripts in [`Oxygen/sonic3air/build/_3ds`](Oxygen/sonic3air/build/_3ds).
- A precompiled `scripts.bin` for this exact version, so the 3DS does not have to compile the scripts itself.

## Building and installing

Needs [devkitPro](https://devkitpro.org/wiki/Getting_Started) with the 3DS tools on Windows, Linux or macOS. Full steps, the .cia flow, and what to put on the SD card are in [`Oxygen/sonic3air/build/_3ds/README.md`](Oxygen/sonic3air/build/_3ds/README.md). In short:

```
cd Oxygen/sonic3air/build/_3ds
./build_sdl2.sh
make cia          # add NATIVIZED=0 for quicker test builds
./stage_sdcard.sh
```

Copy the generated `sdcard` folder to the root of your SD card.

## You need your own ROM

The game needs a **Sonic 3 & Knuckles (with Sonic 3 locked on)** ROM that you own, named `Sonic_Knuckles_wSonic3.bin`, placed in `/3ds/sonic3air/` on the SD card. No ROM or other SEGA game files are in this repository or may be distributed with it.

## License and disclaimer

Sonic 3 A.I.R. and this port are published under the GNU GPLv3 (see [LICENSE](LICENSE)).

Sonic 3 A.I.R. is a non-profit fan game project. It is not affiliated in any way with SEGA or Sonic Team. Sonic the Hedgehog is a trademark of SEGA. All copyrights regarding Sonic the Hedgehog, including characters, names, terms, art, and music belong to SEGA. The developers have no intent to infringe said copyrights and registered trademarks. No financial gain is made from this project. Any commercial use without SEGA's explicit consent is strictly prohibited.
