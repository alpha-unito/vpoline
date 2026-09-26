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

VERSIONS=("2.37" "2.39" "2.41" "2.43")
RESULTS_DIR="$HOME/mw26_artifact_evaluation/coverage_results"
mkdir -p "$RESULTS_DIR"

WRAPPER_TEST_EXE="$HOME/vpoline/test/wrapper_test"
INTERCEPT_HOOK="$HOME/vpoline/test/intercept_sys_wrapper"
VPOLINE_HOOK="$HOME/vpoline/test/hook_wrapper_test"
LIBVPOLINE_BASE="$HOME/vpoline/build/libvpoline.so"
LIBSYSCALL_INTERCEPT_BASE="$HOME/syscall_intercept/build/libsyscall_intercept.so"

################################################################################

echo "Compiling tests and hooks..."
gcc -o "${WRAPPER_TEST_EXE}" "${WRAPPER_TEST_EXE}.c"
gcc -o "${INTERCEPT_HOOK}.so" "${INTERCEPT_HOOK}.c" -fpic -shared -I"$HOME/syscall_intercept/include" -L"$HOME/syscall_intercept/build" -lsyscall_intercept
gcc -o "${VPOLINE_HOOK}.so" "${VPOLINE_HOOK}.c" -fpic -shared -I"$HOME/vpoline/include"

echo "=== Start glibc patching coverage evaluation ==="

for VER in "${VERSIONS[@]}"; do
    echo "---------------------------------------------------"
    echo "Setting up environment for Glibc $VER"


    # 1. Create copies of the original binaries
    cp "${WRAPPER_TEST_EXE}" "${WRAPPER_TEST_EXE}_${VER}"
    cp "${INTERCEPT_HOOK}.so" "${INTERCEPT_HOOK}_${VER}.so"
    cp "${VPOLINE_HOOK}.so" "${VPOLINE_HOOK}_${VER}.so"

    cp "${LIBVPOLINE_BASE}" "$HOME/vpoline/build/libvpoline_${VER}.so"
    cp "${LIBSYSCALL_INTERCEPT_BASE}" "$HOME/syscall_intercept/build/libsyscall_intercept_${VER}.so"

    # 2. Patch executable to make sure it uses the current glibc version
    ./patch.sh "$VER" "${WRAPPER_TEST_EXE}_${VER}" > /dev/null

    # 3. Patch the shared hook libraries to remove dependencies on newer symbols.
    #    This prevents crashes if the host machine has glibc 2.41 but we are testing 2.37.
    ./so_patch.sh "$VER" "${INTERCEPT_HOOK}_${VER}.so" > /dev/null
    ./so_patch.sh "$VER" "${VPOLINE_HOOK}_${VER}.so" > /dev/null

    ./so_patch.sh "$VER" "$HOME/vpoline/build/libvpoline_${VER}.so" > /dev/null
    ./so_patch.sh "$VER" "$HOME/syscall_intercept/build/libsyscall_intercept_${VER}.so" > /dev/null

    echo "Run [vpoline] on Glibc $VER..."
    # Saving coverage report on logfile
    LD_PRELOAD="$HOME/vpoline/build/libvpoline_${VER}.so" LIBVPHOOK="${VPOLINE_HOOK}_${VER}.so" \
        "${WRAPPER_TEST_EXE}_${VER}" > "$RESULTS_DIR/vpoline_glibc_$VER.log" 2>&1 || true

    echo "Run [syscall_intercept] on Glibc $VER..."
    # Preload both the engine and the patched hook library
    LD_LIBRARY_PATH="$HOME/syscall_intercept/build:$HOME/vpoline/test"
    LD_PRELOAD="$HOME/syscall_intercept/build/libsyscall_intercept_${VER}.so:${INTERCEPT_HOOK}_${VER}.so" \
        "${WRAPPER_TEST_EXE}_${VER}" > "$RESULTS_DIR/syscall_intercept_glibc_$VER.log" 2>&1 || true

    echo "Runs for Glibc $VER completed."
done

echo "==================================================="
echo "Extracting coverage summary (Stats:):"
echo "---------------------------------------------------"
echo "VPOLINE:"
grep "Stats:" "$RESULTS_DIR"/vpoline_glibc_*.log || true
echo "---------------------------------------------------"
echo "SYSCALL_INTERCEPT:"
grep "Stats:" "$RESULTS_DIR"/syscall_intercept_glibc_*.log || true