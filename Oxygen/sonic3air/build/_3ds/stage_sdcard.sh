#!/usr/bin/env bash
#
# Collects everything the game needs into one folder ("sdcard/3ds/sonic3air"), which you then copy to the root of your 3DS SD card.
# Run after a successful 'make'.   Usage:  ./stage_sdcard.sh [output folder]
#
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../../.." && pwd)"
OUT="${1:-$HERE/sdcard}"
DEST="$OUT/3ds/sonic3air"

if [ ! -f "$HERE/sonic3air.3dsx" ]; then
	echo "sonic3air.3dsx not found - run 'make' first."
	exit 1
fi

rm -rf "$DEST" "$OUT/3ds/sonic3air.3dsx" "$OUT/cias/sonic3air.cia"
mkdir -p "$DEST/data" "$DEST/saves" "$DEST/savedata"

# The .3dsx goes directly into the "3ds" folder (where the Homebrew Launcher looks), the game data stays in "3ds/sonic3air"
cp "$HERE/sonic3air.3dsx" "$OUT/3ds/sonic3air.3dsx"

# Engine data first, then game data on top (the 3DS build uses one merged "data" folder)
cp -r "$ROOT/Oxygen/oxygenengine/data/." "$DEST/data/"
cp -r "$ROOT/Oxygen/sonic3air/data/."    "$DEST/data/"

# A disclaimer image made for the 400x224 screen (the original is a 1920x1080 image with text that is unreadable when scaled down)
cp "$HERE/disclaimer.png" "$DEST/data/images/menu/disclaimer.png"

# The remastered soundtrack (126 MB of OGG) is far too heavy for the 3DS, the game falls back to the original (emulated) soundtrack
rm -rf "$DEST/data/audio/remastered"

# Scripts and project description
cp -r "$ROOT/Oxygen/sonic3air/scripts" "$DEST/scripts"
cp "$ROOT/Oxygen/sonic3air/oxygenproject.json" "$DEST/"
cp "$HERE/config_3ds.json" "$DEST/config.json"

# Optional: A precompiled script file makes the first start much faster, see README.md
if [ -f "$HERE/scripts.bin" ]; then
	cp "$HERE/scripts.bin" "$DEST/data/scripts.bin"
	echo "Included precompiled scripts.bin"
fi

# Optional: your own ROM. Put "Sonic_Knuckles_wSonic3.bin" next to this script and it gets copied to /3ds/sonic3air/ (never commit it)
if [ -f "$HERE/Sonic_Knuckles_wSonic3.bin" ]; then
	cp "$HERE/Sonic_Knuckles_wSonic3.bin" "$DEST/"
	echo "Included Sonic_Knuckles_wSonic3.bin"
else
	echo "NOTE: Sonic_Knuckles_wSonic3.bin not found next to this script - copy your ROM to /3ds/sonic3air/ on the SD card yourself"
fi

# Optional: the installable .cia (built with 'make cia'), goes into a "cias" folder
if [ -f "$HERE/sonic3air.cia" ]; then
	mkdir -p "$OUT/cias"
	cp "$HERE/sonic3air.cia" "$OUT/cias/"
	echo "Included cias/sonic3air.cia (install it with e.g. FBI, it doesn't need to stay on the SD card)"
fi

echo
echo "Done: $DEST"
echo "Now copy the contents of '$OUT' to the root of your SD card, and put your own"
echo "'Sonic_Knuckles_wSonic3.bin' (the 4 MB Sonic 3 & Knuckles ROM that the PC version of Sonic 3 A.I.R. asks for) into /3ds/sonic3air/ on the SD card."
