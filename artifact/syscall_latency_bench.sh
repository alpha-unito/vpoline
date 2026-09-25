#!/bin/bash

RESULTS_DIR=~/mw26_artifact_evaluation/syscall_latency
VPOLINE=~/vpoline
LIBVPOLINE=$VPOLINE/build/libvpoline.so
SYSCALL_LATENCY_EXE=$VPOLINE/benchmark/syscall_latency
LIBSYSCALL_INTERCEPT=~/syscall_intercept/build/libsyscall_intercept.so
LIBSUD=$VPOLINE/benchmark/libsud_custom.so
KO=$VPOLINE/benchmark/kprobes/build/example.ko

################################################################################

echo "Compiling syscall_latency benchmark executable..."
gcc -o $SYSCALL_LATENCY_EXE $SYSCALL_LATENCY_EXE.c -lm
gcc -o $LIBSUD $VPOLINE/benchmark/libsud_custom.c -fpic -shared
make -C $VPOLINE/benchmark/kprobes
mkdir -p $RESULTS_DIR

################################################################################

echo "Measuring baseline syscall latency..."
$SYSCALL_LATENCY_EXE $RESULTS_DIR/baseline_syscall_latency.json

################################################################################

echo "Measuring vpoline syscall latency..."
LD_PRELOAD=$LIBVPOLINE $SYSCALL_LATENCY_EXE $RESULTS_DIR/vpoline_syscall_latency.json

################################################################################

echo "Measuring syscall_intercept syscall latency..."
LD_PRELOAD=$LIBSYSCALL_INTERCEPT $SYSCALL_LATENCY_EXE $RESULTS_DIR/gekko_syscall_latency.json

################################################################################

echo "Measuring SUD syscall latency..."
LD_PRELOAD=$LIBSUD $SYSCALL_LATENCY_EXE $RESULTS_DIR/sud_syscall_latency.json

################################################################################

echo "Measuring strace syscall latency..."
strace -o /dev/null $SYSCALL_LATENCY_EXE $RESULTS_DIR/strace_syscall_latency.json

################################################################################

#echo "Measuring kprobes syscall latency..."
#
#sudo rmmod example 2>/dev/null || true
#rm -f /tmp/kprobe_pipe
#mkfifo /tmp/kprobe_pipe
#exec 3<> /tmp/kprobe_pipe
#
#$SYSCALL_LATENCY_EXE $RESULTS_DIR/kprobes_syscall_latency.json 1 <&3 &
#BENCH_PID=$!
#sleep 0.5
#sudo insmod $KO target_pid=$BENCH_PID
#echo "" >&3
#
#wait $BENCH_PID
#
#sudo rmmod example
#exec 3>&-
#rm -f /tmp/kprobe_pipe