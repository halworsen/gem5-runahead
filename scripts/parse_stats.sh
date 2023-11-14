#!/bin/bash

module restore gem5

LOG_DIR=${HOME}/gem5-runahead/spec2017/logs
STATPARSE=${HOME}/gem5-runahead/scripts/stats/statdump.py

for BENCH_DIR in $LOG_DIR/*; do
    BENCHMARK=$(basename $BENCH_DIR)
    M5OUT="$BENCH_DIR/m5out-spec2017-o3-baseline"
    STATS="$M5OUT/stats.txt"

    if [[ ! -d "$M5OUT" ]]; then
        echo "No m5out found for $BENCHMARK, skipping"
        echo
        continue
    fi

    echo "Parsing stats for: $BENCHMARK"
    echo "======================================="
    echo "    Stats file: $STATS"
    echo "    Parsing and dumping stats as JSON in m5out..."

    STATOUT="$M5OUT/gem5stats.json"
    python $STATPARSE \
        --format json --out $STATOUT \
        $STATS \
        > /dev/null 2>&1

    echo "    Wrote stats to $STATOUT.json"

    echo
done
