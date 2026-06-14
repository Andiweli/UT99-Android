#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build_host_dcutil"
CONFIG="RelWithDebInfo"
OUTPUT_DIR="${BUILD_DIR}/${CONFIG}"
DEST_SYSTEM_DIR="${REPO_ROOT}/ut99pc/System"

MESH_PATTERN="${1:-*.u}"
TEX_PATTERN="${2:-../Textures/*.utx}"
MAP_PATTERN="${3:-../Maps/Dig.unr}"

cmake -S "${REPO_ROOT}/Source" -B "${BUILD_DIR}" \
  -G "Unix Makefiles" \
  -DBUILD_DCUTIL=ON \
  -DBUILD_EDITOR=ON \
  -DBUILD_UNREAL=OFF \
  -DBUILD_WINDRV=OFF \
  -DBUILD_NOPENALDRV=OFF \
  -DBUILD_NULLSOUNDDRV=ON \
  -DUSE_SDL=ON \
  -DCMAKE_C_FLAGS=-m32 \
  -DCMAKE_CXX_FLAGS=-m32

cmake --build "${BUILD_DIR}" --target install

mkdir -p "${DEST_SYSTEM_DIR}"

shopt -s nullglob
for so_file in "${OUTPUT_DIR}"/*.so; do
  cp "${so_file}" "${DEST_SYSTEM_DIR}/"
done
shopt -u nullglob

if [[ ! -f "${OUTPUT_DIR}/DCUtil" ]]; then
  echo "DCUtil was not produced in ${OUTPUT_DIR}" >&2
  exit 1
fi

cp "${OUTPUT_DIR}/DCUtil" "${DEST_SYSTEM_DIR}/"

pushd "${DEST_SYSTEM_DIR}" >/dev/null
export LD_LIBRARY_PATH="${DEST_SYSTEM_DIR}:${LD_LIBRARY_PATH:-}"
./DCUtil -LOG "CVTUTX=${TEX_PATTERN}" 
#./DCUtil.bin "CVTUAX=../Sounds/*.uax"
#./DCUtil.bin "CVTUMH=${MESH_PATTERN}"
popd >/dev/null

