#!/usr/bin/env bash
set -euo pipefail

: "${KOS_BASE:?Please source your KOS environ.sh to set KOS_BASE}"

SRC_DIR="./Source"
BUILD_DIR="./build_dc"

EXTRA_FLAGS=( -DDREAMCAST_BUILD_CDI=ON )

# Now build Dreamcast target
cmake -S "$SRC_DIR" -B "$BUILD_DIR" \
  -G "Unix Makefiles" \
  -DCMAKE_TOOLCHAIN_FILE="$KOS_CMAKE_TOOLCHAIN" \
  -DPLATFORM_DREAMCAST=ON ${EXTRA_FLAGS[@]:-} ${EXTRA_CMAKE_FLAGS:-}

cmake --build "$BUILD_DIR" -- -j$(nproc)
cmake --install "$BUILD_DIR"
cmake --build "$BUILD_DIR" --target cdi -- -j$(nproc)

# Create ISO target
echo "Creating ISO image..."

# Find the ELF file (check common locations)
ELF_FILE=""
if [[ -f "$BUILD_DIR/RelWithDebInfo/UnrealTournament.elf" ]]; then
  ELF_FILE="$BUILD_DIR/RelWithDebInfo/UnrealTournament.elf"
elif [[ -f "$BUILD_DIR/UnrealTournament/UnrealTournament.elf" ]]; then
  ELF_FILE="$BUILD_DIR/UnrealTournament/UnrealTournament.elf"
elif [[ -f "$BUILD_DIR/Release/UnrealTournament.elf" ]]; then
  ELF_FILE="$BUILD_DIR/Release/UnrealTournament.elf"
elif [[ -f "$BUILD_DIR/Debug/UnrealTournament.elf" ]]; then
  ELF_FILE="$BUILD_DIR/Debug/UnrealTournament.elf"
else
  # Try to find it
  ELF_FILE=$(find "$BUILD_DIR" -name "UnrealTournament.elf" -type f | head -1)
fi

if [[ -z "$ELF_FILE" || ! -f "$ELF_FILE" ]]; then
  echo "Error: Could not find UnrealTournament.elf in build directory"
  exit 1
fi

echo "Found ELF: $ELF_FILE"

# Create 1ST_READ.BIN from ELF
GAMEDATA_DIR="./UT99"
mkdir -p "$GAMEDATA_DIR"

echo "Creating 1ST_READ.BIN from $ELF_FILE..."
kos-objcopy -R .stack -O binary "$ELF_FILE" "$GAMEDATA_DIR/1ST_READ.BIN"

if [[ ! -f "$GAMEDATA_DIR/1ST_READ.BIN" ]]; then
  echo "Error: Failed to create 1ST_READ.BIN"
  exit 1
fi

echo "Created 1ST_READ.BIN ($(du -h "$GAMEDATA_DIR/1ST_READ.BIN" | cut -f1))"


EMU_PATH="./flycast-x86_64.AppImage"
CDI_PATH="./UnrealTournament.cdi"

if [[ -x "$EMU_PATH" ]]; then
  "$EMU_PATH" "$CDI_PATH"
else
  echo "Flycast AppImage not found or not executable at $EMU_PATH (skipping launch)"
fi