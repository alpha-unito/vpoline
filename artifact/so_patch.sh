#!/usr/bin/env bash
set -euo pipefail

# Usage: ./patch_glibc_so.sh <glibc_version> <library.so>
# Example: ./patch_glibc_so.sh 2.37 libfoo.so

usage() {
    echo "Usage: $0 <glibc_version> <library.so>"
    echo "  glibc_version  e.g. 2.37"
    echo "  library.so     path to the shared library to patch"
    exit 1
}

[[ $# -ne 2 ]] && usage

GLIBC_VERSION="$1"
LIBRARY="$2"

GLIBC_ROOT="$HOME/glibc-${GLIBC_VERSION}"

# Detect lib vs lib64
if [[ -d "${GLIBC_ROOT}/lib64" ]]; then
    GLIBC_LIB="${GLIBC_ROOT}/lib64"
elif [[ -d "${GLIBC_ROOT}/lib" ]]; then
    GLIBC_LIB="${GLIBC_ROOT}/lib"
else
    echo "Error: no lib or lib64 directory found under ${GLIBC_ROOT}/"; exit 1
fi

# Sanity checks
[[ -f "$LIBRARY" ]]   || { echo "Error: library '$LIBRARY' not found"; exit 1; }
[[ -d "$GLIBC_LIB" ]] || { echo "Error: glibc lib dir '$GLIBC_LIB' not found"; exit 1; }

echo "==> Patching '$LIBRARY' with glibc ${GLIBC_VERSION}"
echo "    rpath : $GLIBC_LIB"
echo ""

# 1. Show current state
echo "[1/4] Current NEEDED libraries..."
patchelf --print-needed "$LIBRARY"
echo ""

# 2. Set rpath so the .so finds the right libc.so.6 at runtime
echo "[2/4] Setting rpath..."
patchelf --set-rpath "$GLIBC_LIB" "$LIBRARY"

# 3. Clear symbol version requirements for any GLIBC symbols newer than target
echo "[3/4] Clearing symbol versions newer than ${GLIBC_VERSION}..."
NEWER_SYMS=$(readelf -V "$LIBRARY" 2>/dev/null \
    | grep -oP 'GLIBC_\K[0-9]+\.[0-9]+' \
    | sort -Vu \
    | awk -F. -v target="$GLIBC_VERSION" '
        BEGIN { split(target, t, ".") }
        {
            split($0, v, ".")
            if (v[1] > t[1] || (v[1] == t[1] && v[2] > t[2]))
                print "GLIBC_" $0
        }
    ')

if [[ -z "$NEWER_SYMS" ]]; then
    echo "    No symbol versions newer than ${GLIBC_VERSION} found — nothing to clear."
else
    echo "    Newer version tags found: $NEWER_SYMS"
    readelf -sW "$LIBRARY" 2>/dev/null \
        | awk '/@@GLIBC_/{
            match($0, /([a-zA-Z0-9_]+)@@(GLIBC_[0-9.]+)/, m)
            if (m[1] != "" && m[2] != "") print m[1] " " m[2]
          }' \
        | while read -r SYM VER; do
            for NEWER in $NEWER_SYMS; do
                if [[ "$VER" == "$NEWER" ]]; then
                    echo "    Clearing ${SYM}@${VER}"
                    patchelf --clear-symbol-version "$SYM" "$LIBRARY" || \
                        echo "    Warning: could not clear ${SYM}@${VER} (skipping)"
                fi
            done
          done
fi

# 4. Verify
echo ""
echo "[4/4] Verifying with ldd..."
ldd "$LIBRARY" || echo "Warning: ldd reported issues (may be normal for some .so files)"
echo ""
echo "==> Done. Verify runtime behaviour before deploying."