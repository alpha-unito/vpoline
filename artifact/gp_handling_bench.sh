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

RESULTS_DIR=~/mw26_artifact_evaluation/gp_fault_handling_overhead
VPOLINE=~/vpoline
LIBVPOLINE=$VPOLINE/build/libvpoline.so
GP_FAULT_BENCH_EXE=~/vpoline/build/bin/gp_benchmark_bulk_master

################################################################################

mkdir -p $RESULTS_DIR

echo "Measuring vpoline gp_fault handling overhead..."
$GP_FAULT_BENCH_EXE $RESULTS_DIR/baseline.csv $RESULTS_DIR/vpoline.csv