# hive-engine

Work-stealing job system + data-oriented ECS in C++20, with a 100k-entity real-time Swarm demo.

## Build and run the demo

```text
cmake --preset debug
cmake --build --preset debug
./build/debug/hive_engine
```

Release (the 60 FPS measurement):

```text
cmake --preset release
cmake --build --preset release
./build/release/hive_engine
```

ImGui panel: entity count, thread count, spatial grid on/off, SoA vs AoS, per-pass timings, Dump `trace.json`.

Tests: `ctest --preset debug --output-on-failure`.
