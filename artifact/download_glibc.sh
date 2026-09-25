#!/usr/bin/env bash
set -euo pipefail

[[ $# -ne 1 ]] && { echo "Usage: $0 <glibc_version>"; exit 1; }
GLIBC_VERSION="$1"

PREFIX="$HOME/glibc-$GLIBC_VERSION"
BUILD_BASE_DIR="$HOME/glibc-build"
SRC_DIR="$BUILD_BASE_DIR/glibc-${GLIBC_VERSION}"
OBJ_DIR="$BUILD_BASE_DIR/build-${GLIBC_VERSION}"

mkdir -p "$BUILD_BASE_DIR"
cd "$BUILD_BASE_DIR"

wget -nc "https://ftp.gnu.org/gnu/glibc/glibc-${GLIBC_VERSION}.tar.xz" || true
tar -xf "glibc-${GLIBC_VERSION}.tar.xz"

mkdir -p "$OBJ_DIR"
cd "$OBJ_DIR"

"$SRC_DIR/configure" --prefix="$PREFIX" --disable-werror --enable-shared
make -j"$(nproc)"
make install