#!/usr/bin/env bash
set -euo pipefail

# ---------------------------------------------------------------------------
# Configuration — edit these to taste
# ---------------------------------------------------------------------------
BINARY="$HOME/vpoline/artifact/data_rw"
ITERATIONS=3

CAPIO_CL_CONFIG_PATH=$HOME/bench/capio_cl.json

# File sizes to benchmark (in MB)
SIZES=(1 2 4 8 16 32 64 128 256 512 1024 2048)

VPOLINE_BASEDIR=$HOME/capio-vpoline-main/build_vpoline
VPOLIME_LIBCAPIO_PATH=$VPOLINE_BASEDIR/capio/posix/libcapio_posix.so.1.0.0
VPOLINE_PATH=$VPOLINE_BASEDIR/_deps/vpoline-build/libvpoline.so
VPOLINE_CAPIO_SERVER_PATH=$VPOLINE_BASEDIR/capio/server/capio_server


SYSCALL_BASEDIR=$HOME/capio-vpoline-main/build_syscall
SYSCALL_LIBCAPIO_PATH=$SYSCALL_BASEDIR/capio/posix/libcapio_posix.so.1.0.0
SYSCALL_CAPIO_SERVER_PATH=$SYSCALL_BASEDIR/capio/server/capio_server

RESULTS_DIR=$HOME/mw26_artifact_evaluation/capio

export CAPIO_DIR=.
export CAPIO_WORKFLOW_NAME=benchmark


# ---------------------------------------------------------------------------
# Sanity checks
# ---------------------------------------------------------------------------
if [[ ! -x "$BINARY" ]]; then
    echo "ERROR: binary '$BINARY' not found or not executable." >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Helper: parse "WRITE : xxxx us / READ : yyyy us" and return sum in seconds
# ---------------------------------------------------------------------------
parse_and_sum_us() {
    local write_output="$1"
    local read_output="$2"

    local write_us read_us
    write_us=$(echo "$write_output" | grep -oP 'WRITE\s*:\s*\K[0-9]+')
    read_us=$(echo "$read_output"   | grep -oP 'READ\s*:\s*\K[0-9]+')

    if [[ -z "$write_us" || -z "$read_us" ]]; then
        echo "ERROR: could not parse timing output." >&2
        echo "  write output: $write_output" >&2
        echo "  read  output: $read_output"  >&2
        exit 1
    fi

    # Sum microseconds, convert to seconds with 6 decimal places
    echo "scale=6; ($write_us + $read_us) / 1000000" | bc
}

# ---------------------------------------------------------------------------
# Helper: compute standard deviation from a bash array of values
# Uses population std dev (divided by N). Requires bc.
# ---------------------------------------------------------------------------
compute_stddev() {
    local values=("$@")
    local n=${#values[@]}

    if (( n < 2 )); then
        echo "0.000000"
        return
    fi

    # Build a bc script that computes mean then sum of squared deviations
    local bc_script="scale=10; "

    # Sum
    local sum_expr
    sum_expr=$(IFS=+; echo "${values[*]}")
    bc_script+="mean = ($sum_expr) / $n; "

    # Sum of squared deviations
    local sq_devs=()
    for v in "${values[@]}"; do
        sq_devs+=("(${v} - mean)^2")
    done
    local sq_sum_expr
    sq_sum_expr=$(IFS=+; echo "${sq_devs[*]}")
    bc_script+="variance = ($sq_sum_expr) / $n; "
    bc_script+="sqrt(variance)"

    echo "scale=6; $(echo "$bc_script" | bc -l)" | bc -l
}

# ---------------------------------------------------------------------------
# CSV header (create file if it doesn't exist)
# ---------------------------------------------------------------------------
if [[ ! -f "$RESULTS_DIR/baseline.csv" ]]; then
    echo "size_mb,iterations,avg_total_s,stddev_s" > "$RESULTS_DIR/baseline.csv"
fi

if [[ ! -f "$RESULTS_DIR/vpoline_mem.csv" ]]; then
    echo "size_mb,iterations,avg_total_s,stddev_s" > "$RESULTS_DIR/vpoline_mem.csv"
fi

if [[ ! -f "$RESULTS_DIR/syscall_mem.csv" ]]; then
    echo "size_mb,iterations,avg_total_s,stddev_s" > "$RESULTS_DIR/syscall_mem.csv"
fi

# ---------------------------------------------------------------------------
# Helper: benchmark a single size
# ---------------------------------------------------------------------------
run_baseline() {
    local size_mb="$1"

    echo ""
    echo ""
    echo ""
    echo "======================================================"
    echo " BASELINE Size: ${size_mb} MB  |  runs: $ITERATIONS"
    echo "======================================================"
    printf "%-6s  %-12s\n" "Run" "Total (s)"
    echo "------------------------------------------------------"

    local total=0
    local samples=()

    for i in $(seq 1 "$ITERATIONS"); do

        write_out=$("$BINARY" write "$size_mb")
        read_out=$("$BINARY" read)

        elapsed=$(parse_and_sum_us "$write_out" "$read_out")
        printf "%-6s  %-12s\n" "$i" "$elapsed"

        samples+=("$elapsed")
        total=$(echo "scale=6; $total + $elapsed" | bc)
    done

    avg_total=$(echo "scale=6; $total / $ITERATIONS" | bc)
    stddev=$(compute_stddev "${samples[@]}")

    echo "------------------------------------------------------"
    printf "%-6s  %-12s  %-12s\n" "AVG" "$avg_total" "±$stddev"

    echo "${size_mb},${ITERATIONS},${avg_total},${stddev}" >> "$RESULTS_DIR/baseline.csv"

    rm -f data.dat
}


run_vpoline_mem() {
    local size_mb="$1"

    echo ""
    echo ""
    echo ""
    echo "======================================================"
    echo "CAPIO MEM VP: Size: ${size_mb} MB  |  runs: $ITERATIONS"
    echo "======================================================"
    printf "%-6s  %-12s\n" "Run" "Total (s)"
    echo "------------------------------------------------------"
#####################################

    local total=0
    local samples=()

    rm -rf /dev/shm/* files_location*
    $VPOLINE_CAPIO_SERVER_PATH -c $CAPIO_CL_CONFIG_PATH -b none > server.log &
    SERVER_PID=$!

    #warmup server
    CAPIO_APP_NAME=all LD_PRELOAD=$VPOLINE_PATH LIBVPHOOK=$VPOLIME_LIBCAPIO_PATH "$BINARY" write "$size_mb"

    for i in $(seq 1 "$ITERATIONS"); do

        write_out=$(CAPIO_APP_NAME=all LD_PRELOAD=$VPOLINE_PATH LIBVPHOOK=$VPOLIME_LIBCAPIO_PATH "$BINARY" write "$size_mb")
        read_out=$(CAPIO_APP_NAME=all  LD_PRELOAD=$VPOLINE_PATH LIBVPHOOK=$VPOLIME_LIBCAPIO_PATH "$BINARY" read)

        elapsed=$(parse_and_sum_us "$write_out" "$read_out")
        printf "%-6s  %-12s\n" "$i" "$elapsed"

        samples+=("$elapsed")
        total=$(echo "scale=6; $total + $elapsed" | bc)
    done

    avg_total=$(echo "scale=6; $total / $ITERATIONS" | bc)
    stddev=$(compute_stddev "${samples[@]}")

    kill $SERVER_PID

    echo "------------------------------------------------------"
    printf "%-6s  %-12s  %-12s\n" "AVG" "$avg_total" "±$stddev"

    echo "${size_mb},${ITERATIONS},${avg_total},${stddev}" >> "$RESULTS_DIR/vpoline_mem.csv"

    rm -f data.dat
}


run_syscall_mem() {
    local size_mb="$1"

    echo ""
    echo ""
    echo ""
    echo "======================================================"
    echo "CAPIO MEM SC: Size: ${size_mb} MB  |  runs: $ITERATIONS"
    echo "======================================================"
    printf "%-6s  %-12s\n" "Run" "Total (s)"
    echo "------------------------------------------------------"

    local total=0
    local samples=()

    rm -rf /dev/shm/* files_location*
    $SYSCALL_CAPIO_SERVER_PATH -c $CAPIO_CL_CONFIG_PATH -b none > server.log &
    SERVER_PID=$!

    #warmup server
    CAPIO_APP_NAME=all LD_LIBRARY_PATH=$HOME/syscall_intercept/build:$SYSCALL_LIBCAPIO_PATH LD_PRELOAD=$SYSCALL_LIBCAPIO_PATH "$BINARY" write "$size_mb"

    for i in $(seq 1 "$ITERATIONS"); do

        write_out=$(CAPIO_APP_NAME=all LD_LIBRARY_PATH=$HOME/syscall_intercept/build:$SYSCALL_LIBCAPIO_PATH LD_PRELOAD=$SYSCALL_LIBCAPIO_PATH "$BINARY" write "$size_mb")
        read_out=$(CAPIO_APP_NAME=all  LD_LIBRARY_PATH=$HOME/syscall_intercept/build:$SYSCALL_LIBCAPIO_PATH LD_PRELOAD=$SYSCALL_LIBCAPIO_PATH "$BINARY" read)

        elapsed=$(parse_and_sum_us "$write_out" "$read_out")
        printf "%-6s  %-12s\n" "$i" "$elapsed"

        samples+=("$elapsed")
        total=$(echo "scale=6; $total + $elapsed" | bc)
    done

    avg_total=$(echo "scale=6; $total / $ITERATIONS" | bc)
    stddev=$(compute_stddev "${samples[@]}")

    kill $SERVER_PID

    echo "------------------------------------------------------"
    printf "%-6s  %-12s  %-12s\n" "AVG" "$avg_total" "±$stddev"

    echo "${size_mb},${ITERATIONS},${avg_total},${stddev}" >> "$RESULTS_DIR/syscall_mem.csv"

    rm -f data.dat
}

# ---------------------------------------------------------------------------
# Main — iterate over all sizes
# ---------------------------------------------------------------------------
echo "Benchmark started: $(date)"
echo "Binary : $BINARY"
echo "Sizes  : ${SIZES[*]} MB"

for size in "${SIZES[@]}"; do
    run_baseline "$size"
    run_vpoline_mem "$size" 
    #run_syscall_mem "$size"
done

echo ""
echo "======================================================"
echo "All done."
echo "======================================================"

