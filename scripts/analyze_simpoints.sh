#!/bin/bash

module restore gem5

LOG_DIR=${HOME}/gem5-runahead/spec2017/logs
SIMPOINT=${HOME}/gem5-runahead/SimPoint.3.2/bin/simpoint

for BENCH_DIR in $LOG_DIR/*; do
    BENCHMARK=$(basename $BENCH_DIR)
    M5OUT="$BENCH_DIR/m5out-spec2017-simpoint"
    SIMPOINT_BBV="$M5OUT/simpoint.bb.gz"

    if [[ ! -d "$M5OUT" ]]; then
        echo "No simpoint m5out found for $BENCHMARK, skipping"
        echo
        continue
    fi

    echo "Doing simpoint analysis for: $BENCHMARK"
    echo "======================================="
    echo "    Simpoint file: $SIMPOINT_BBV"
    echo "    Creating simpoint dir in m5out..."

    mkdir $M5OUT/simpoint > /dev/null 2>&1

    echo "    Performing simpoint analysis..."
    $SIMPOINT -inputVectorsGzipped -maxK 10 \
              -loadFVFile $SIMPOINT_BBV \
              -saveSimpoints $M5OUT/simpoint/simpoint.txt \
              -saveSimpointWeights $M5OUT/simpoint/weights.txt \
              > /dev/null 2>&1

    echo
done
