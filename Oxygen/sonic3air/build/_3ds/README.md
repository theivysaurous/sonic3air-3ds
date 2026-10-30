# Sonic 3 A.I.R. on Nintendo 3DS (experimental proof of concept)

Local, unofficial experiment: **New 3DS only**, software renderer, original (emulated) soundtrack.
Nothing here has been built or run on a 3DS yet, so expect to hit problems on the first build. Report back exactly where it stops.

## What you need

- A Windows PC with [devkitPro](https://devkitpro.org/wiki/Getting_Started) installed (use its installer, select the **3DS** components).
- A New 3DS with custom firmware and the Homebrew Launcher.
- Your own **Sonic 3 & Knuckles (with Sonic 3 locked on) ROM**, the 4 MB file the PC version asks for. Expected name: `Sonic_Knuckles_wSonic3.bin`.

The game does not ship with the ROM and this port can't pick one with a file dialog, so it has to be on the SD card before the first start.

## Build steps (Windows)

Open the **devkitPro MSYS2 shell** (not a normal command prompt). It sets `DEVKITPRO` and `DEVKITARM` for you.

1. Install the packages once:

   ```
   pacman -S --needed 3ds-dev 3ds-cmake cmake
   ```

2. Go to this folder (adjust the drive path to wherever the repo is, e.g. `/c/...`):

   ```
   cd /c/path/to/sonic3air/Oxygen/sonic3air/build/_3ds
   ```

3. Build SDL2 for the 3DS once. It uses the copy bundled in `framework/external/sdl` (2.32, which has a native 3DS backend) and installs it into the devkitPro portlibs:

   ```
   ./build_sdl2.sh
   ```

4. Build the game. **Do the first build without the nativized scripts**, it compiles much faster and is easier to debug:

   ```
   make NATIVIZED=0
   ```

   This produces `sonic3air.3dsx`. Once that works, run `make clean` and then `make` (default `NATIVIZED=1`) for the faster script code. That compiles a very large generated file, so give it time and RAM.

5. Collect everything for the SD card:

   ```
   ./stage_sdcard.sh
   ```

   This creates `sdcard/3ds/sonic3air/` with the `.3dsx`, data, scripts and a 3DS `config.json`. It leaves out the 126 MB remastered soundtrack.

## Put it on the SD card

1. Copy the contents of the `sdcard` folder to the **root** of your SD card, so you end up with `/3ds/sonic3air/sonic3air.3dsx`.
2. Copy your ROM to `/3ds/sonic3air/Sonic_Knuckles_wSonic3.bin`.
3. Start it from the Homebrew Launcher.

Save data, config and any log files are written inside `/3ds/sonic3air/`.

## What to expect and known risks

- **First start may be very slow.** The game scripts get compiled on launch unless a precompiled `scripts.bin` exists. If you produce one on the PC build, drop it next to `stage_sdcard.sh` as `scripts.bin` and re-run the staging step.
- **Speed is unknown.** It may not hold 60 FPS. Tell me what you see and I'll look at what costs the most.
- **Memory.** The amount of RAM a `.3dsx` gets can depend on how it's launched. If it crashes right at startup or while loading, tell me; the likely fix is building a `.cia` with a larger memory mode.
- **Audio.** It uses the emulated Genesis soundtrack at a 32 kHz sample rate. If there is no sound at all, the console may be missing its DSP firmware dump (`/3ds/dspfirm.cdc`), a common requirement for homebrew audio.
- **Controls and UI.** The game screen is 400x224 on the top screen. The bottom screen is not used yet, and desktop-style menus (mods, online features) are not adapted for the 3DS.
- **No network features.** Update checks and online features are not expected to work.

## When something fails, send me

- The last lines of the build output (for build problems).
- For runtime problems: what you see on screen, when it happens (boot, ROM load, title screen, in game), and any log file from `/3ds/sonic3air/`.
