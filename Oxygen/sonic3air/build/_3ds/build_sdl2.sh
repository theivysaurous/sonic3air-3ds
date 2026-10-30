#!/usr/bin/env bash
#
# Builds the SDL2 copy from "framework/external/sdl/SDL2" for the Nintendo 3DS (SDL 2.32 has a native 3DS backend)
# and installs it into the devkitPro 3DS portlibs, so that the Makefile can link against it.
#
# Run this once, from a devkitPro MSYS2 shell (Windows) or any shell with DEVKITPRO set (Linux / macOS).
#
set -euo pipefail

: "${DEVKITPRO:?DEVKITPRO is not set. Install devkitPro and run this from the devkitPro MSYS2 shell.}"

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SDL_SRC="$(cd "$HERE/../../../../framework/external/sdl/SDL2" && pwd)"
BUILD_DIR="$HERE/sdl2_build"
PREFIX="$DEVKITPRO/portlibs/3ds"
TOOLCHAIN="$DEVKITPRO/cmake/3DS.cmake"

if [ ! -f "$TOOLCHAIN" ]; then
	echo "Could not find $TOOLCHAIN"
	echo "Install the devkitPro CMake toolchain files first:   pacman -S 3ds-cmake"
	exit 1
fi
if ! command -v cmake >/dev/null 2>&1; then
	echo "cmake not found. Install it first, e.g.:   pacman -S cmake   (or install CMake for your OS)"
	exit 1
fi

echo "Configuring SDL2 for 3DS..."
cmake -S "$SDL_SRC" -B "$BUILD_DIR" \
	-DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX="$PREFIX" \
	-DSDL_SHARED=OFF -DSDL_STATIC=ON \
	-DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF -DSDL_INSTALL_TESTS=OFF

echo "Building SDL2..."
cmake --build "$BUILD_DIR" -j "$(nproc 2>/dev/null || echo 4)"

echo "Installing SDL2 into $PREFIX ..."
cmake --install "$BUILD_DIR"

echo "Done. You can now run 'make' in this folder."
