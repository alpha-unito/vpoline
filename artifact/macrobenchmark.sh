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

#!/bin/bash

RESULTS_DIR=~/mw26_artifact_evaluation/macrobenchmark
VPOLINE=~/vpoline
LIBSUD=$VPOLINE/benchmark/libsud_custom.so
MACROBENCHMARK_EXE=~/vpoline/build/bin/benchmark
DUMMY_EXE=~/vpoline/build/bin/dummy

################################################################################

echo "Compiling macrobenchmark executable..."
gcc -o $LIBSUD $VPOLINE/benchmark/libsud_custom.c -fpic -shared
gcc -o $MACROBENCHMARK_EXE $VPOLINE/benchmark/benchmark.c -lm -DDEBUG -DHOME_DIR="\"$HOME\""
gcc -o $DUMMY_EXE $VPOLINE/benchmark/dummy.c -O3

mkdir -p $RESULTS_DIR

echo "Measuring dd across different interception methods..."
$MACROBENCHMARK_EXE dd $RESULTS_DIR/dd_benchmark_results.json

echo "Measuring sha256sum across different interception methods..."
$MACROBENCHMARK_EXE sha256sum $RESULTS_DIR/sha256sum_benchmark_results.json
