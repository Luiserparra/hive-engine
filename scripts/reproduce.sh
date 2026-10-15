#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure

mkdir -p build/release/bench-results

if [[ -f build/release/bench/hive_bench.exe ]]; then
    BENCH=build/release/bench/hive_bench.exe
elif [[ -f build/release/bench/hive_bench ]]; then
    BENCH=build/release/bench/hive_bench
else
    echo "hive_bench not found under build/release/bench/" >&2
    exit 1
fi

"$BENCH" --benchmark_out=build/release/bench-results/hive_bench.json --benchmark_out_format=json

if [[ -f build/release/hive_engine.exe ]]; then
    DEMO=build/release/hive_engine.exe
else
    DEMO=build/release/hive_engine
fi

echo "Demo: $DEMO"
echo "Benchmark JSON: build/release/bench-results/hive_bench.json"
