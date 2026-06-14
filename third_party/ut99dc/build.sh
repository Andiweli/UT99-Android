#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(pwd)"
BUILD_DIR="${REPO_ROOT}/build"
CONFIG="Debug"
OUTPUT_DIR="${BUILD_DIR}/${CONFIG}"

# Use all available CPU cores for faster builds.
NUM_JOBS="$(nproc || echo 1)"
#NUM_JOBS="1"
cmake -S "${REPO_ROOT}/Source" -B "${BUILD_DIR}" \
  -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE="${CONFIG}" \
  -DCMAKE_C_FLAGS=-m32 \
  -DCMAKE_CXX_FLAGS=-m32

cmake --build "${BUILD_DIR}" --config "${CONFIG}" --target install -- -j"${NUM_JOBS}"


