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

rm -rf "$DEST"
mkdir -p "$DEST/data" "$DEST/saves" "$DEST/savedata"

cp "$HERE/sonic3air.3dsx" "$DEST/"

# Engine data first, then game data on top (the 3DS build uses one merged "data" folder)
cp -r "$ROOT/Oxygen/oxygenengine/data/." "$DEST/data/"
cp -r "$ROOT/Oxygen/sonic3air/data/."    "$DEST/data/"

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

# Optional: the installable .cia (built with 'make cia'), put next to the "3ds" folder for easy installing (e.g. with FBI)
if [ -f "$HERE/sonic3air.cia" ]; then
	cp "$HERE/sonic3air.cia" "$OUT/"
	echo "Included sonic3air.cia (install it, it doesn't need to stay on the SD card)"
fi

echo
echo "Done: $DEST"
echo "Now copy the contents of '$OUT' to the root of your SD card, and put your own"
echo "'Sonic_Knuckles_wSonic3.bin' (the 4 MB Sonic 3 & Knuckles ROM that the PC version of Sonic 3 A.I.R. asks for) into /3ds/sonic3air/ on the SD card."
