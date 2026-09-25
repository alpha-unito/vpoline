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
