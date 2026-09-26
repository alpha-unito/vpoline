#
# Copyright 2026 University of Turin
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#!/usr/bin/env bash
set -euo pipefail

# Usage: ./patch_glibc.sh <glibc_version> <binary>
# Example: ./patch_glibc.sh 2.41 wrapper_test

usage() {
    echo "Usage: $0 <glibc_version> <binary>"
    echo "  glibc_version  e.g. 2.41"
    echo "  binary         path to the binary to patch"
    exit 1
}

[[ $# -ne 2 ]] && usage

GLIBC_VERSION="$1"
BINARY="$2"
GLIBC_LIB="$HOME/glibc-${GLIBC_VERSION}/lib"
INTERPRETER="${GLIBC_LIB}/ld-linux-riscv64-lp64d.so.1"

# Sanity checks
[[ -f "$BINARY" ]]      || { echo "Error: binary '$BINARY' not found"; exit 1; }
[[ -d "$GLIBC_LIB" ]]   || { echo "Error: glibc lib dir '$GLIBC_LIB' not found"; exit 1; }
[[ -f "$INTERPRETER" ]] || { echo "Error: interpreter '$INTERPRETER' not found"; exit 1; }

echo "==> Patching '$BINARY' with glibc ${GLIBC_VERSION}"
echo "    Interpreter : $INTERPRETER"
echo "    rpath       : $GLIBC_LIB"
echo ""

# 1. Set interpreter
echo "[1/3] Setting interpreter..."
patchelf --set-interpreter "$INTERPRETER" "$BINARY"

# 2. Set rpath
echo "[2/3] Setting rpath..."
patchelf --set-rpath "$GLIBC_LIB" "$BINARY"

# 3. Verify
echo "[3/3] Verifying with ldd..."
ldd "$BINARY"

echo ""
echo "Done."