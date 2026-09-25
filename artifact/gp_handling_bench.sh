#!/bin/bash

RESULTS_DIR=~/mw26_artifact_evaluation/gp_fault_handling_overhead
VPOLINE=~/vpoline
LIBVPOLINE=$VPOLINE/build/libvpoline.so
GP_FAULT_BENCH_EXE=~/vpoline/build/bin/gp_benchmark_bulk_master

################################################################################

mkdir -p $RESULTS_DIR

echo "Measuring vpoline gp_fault handling overhead..."
$GP_FAULT_BENCH_EXE $RESULTS_DIR/baseline.csv $RESULTS_DIR/vpoline.csv