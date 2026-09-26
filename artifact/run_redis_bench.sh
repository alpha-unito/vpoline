#!/usr/bin/env bash
set -euo pipefail

# Require an output filename as an argument
if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <output_filename.csv>"
    echo "Example: $0 vpoline_redis.csv"
    exit 1
fi

OUTPUT_FILE="$1"
TARGET_IP="127.0.0.1"
TARGET_PORT="6380"
ITERATIONS=4

# Added xadd to match your CSV example
COMMANDS="ping_inline,set,get,incr,lpush,lpop,xadd"

echo "Running Redis Benchmark on $TARGET_IP:$TARGET_PORT"
echo "Output will be saved to: $OUTPUT_FILE"

TMP_CSV=$(mktemp)

for i in $(seq 1 $ITERATIONS); do
    echo "[Run $i/$ITERATIONS] Generating load..."
    # -c 50 clients, -n 100000 requests are typical defaults for this paper
    redis-benchmark -h "$TARGET_IP" -p "$TARGET_PORT" -t "$COMMANDS" --csv >> "$TMP_CSV"

    redis-benchmark -h "$TARGET_IP" -p "$TARGET_PORT" --csv XADD mystream \* field1 value1 >> "$TMP_CSV"

    sleep 2
done

# Parse the accumulated CSV, compute statistics, and write directly to the target file
awk -F'","' '
    BEGIN {
        print "Command,Average,StdDev"
        # Define exact print order
        split("PING_INLINE SET GET INCR LPUSH LPOP XADD", order, " ");
    }
    {
        # Clean quotes
        gsub(/"/, "", $1);
        gsub(/"/, "", $2);
        
        # Ensure command is uppercase
        cmd = toupper($1);
        rps = $2;
        
        count[cmd]++;
        sum[cmd] += rps;
        sum_sq[cmd] += rps * rps;
    }
    END {
        for (i = 1; i <= length(order); i++) {
            cmd = order[i];
            if (count[cmd] > 0) {
                n = count[cmd];
                mean = sum[cmd] / n;
                if (n > 1) {
                    variance = (sum_sq[cmd] - (sum[cmd] * sum[cmd] / n)) / n;
                    stddev = sqrt(variance);
                } else {
                    stddev = 0;
                }
                printf "%s,%.2f,%.2f\n", cmd, mean, stddev;
            }
        }
    }
' "$TMP_CSV" > "$OUTPUT_FILE"

rm -f "$TMP_CSV"
echo "Done. Results saved to $OUTPUT_FILE"
