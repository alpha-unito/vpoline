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

if [[ $# -lt 1 || $# -gt 3 ]]; then
    echo "Usage: $0 <output_filename.csv> [target_ip] [target_port]"
    echo "Example (Tunnel SSH / Default): $0 vpoline_redis.csv"
    echo "Example (QEMU Local): $0 vpoline_redis.csv 127.0.0.1 6380"
    echo "Example (LAN Board): $0 vpoline_redis.csv 192.168.1.50 6379"
    exit 1
fi

OUTPUT_FILE="$1"
TARGET_IP="${2:-127.0.0.1}"
TARGET_PORT="${3:-6379}"
ITERATIONS=4

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
