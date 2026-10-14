# Hive Engine: Job System + Data-Oriented ECS (with visual demo)

## 1. Overview
A minimal C++20 engine made of:
- A multithreaded **job system** with *work-stealing* (lock-free Chase-Lev deques).
- An **ECS** (Entity Component System) with **archetype-based SoA** storage.
- A custom instrumented **profiler** (per-thread scopes, ImGui viewer).
- A **visual demo**, "Swarm": 100k+ entities simulated and rendered in real time.
- A **documented performance investigation** with reproducible benchmarks.

Goal: demonstrate mastery of memory, threading, data-oriented design, the cost of abstractions, and end-to-end ownership (profile → fix → document).

## 2. Tech stack
| Area | Choice |
|---|---|
| Language | C++20 (concepts, `std::span`, `std::atomic`, `constexpr`) |
| Build | CMake + presets, vcpkg or FetchContent |
| Demo window/rendering | raylib (or SDL2 + OpenGL) |
| Debug UI | Dear ImGui |
| External profiling | Tracy, `perf` / VTune, cachegrind |
| Tests | Catch2 or GoogleTest |
| Benchmarks | Google Benchmark |
| Quality | ASan, UBSan, TSan, clang-tidy, clang-format |
| CI | GitHub Actions (Linux + Windows, Debug/Release/Sanitizers) |

## 3. Architecture
```
/core        allocators, containers, math (optional SIMD)
/jobs        scheduler, work-stealing deques, counters/dependencies
/ecs         entities, archetypes, queries, systems, command buffers
/profiler    scope macros, per-thread buffers, export, ImGui viewer
/demo        "Swarm" app (raylib + ImGui)
/bench       benchmarks (Google Benchmark)
/tests       unit and stress tests
/docs        performance investigation, design notes, charts
```

## 4. Module: Job System
### Requirements
- One worker thread per physical core (optional affinity).
- One **Chase-Lev** deque per thread: lock-free `push`/`pop` by the owner, `steal` from other threads.
- Small jobs with no dynamic allocations: fixed-size job pool (≤ 64 bytes, cache-line aligned).
- **Dependencies** via atomic counters (`wait(counter)` helps by running other jobs instead of blocking).
- `parallel_for(range, grain_size, fn)` with recursive splitting.
- Clean shutdown with no data races (TSan green).

### Expected API
```cpp
jobs::Scheduler sched{std::thread::hardware_concurrency()};
jobs::Counter done;
sched.parallel_for(0, N, 1024, [&](size_t i){ positions[i] += velocities[i] * dt; }, &done);
sched.wait(done);
```

### What to demonstrate
- Correct memory orders (`acquire/release/seq_cst`), justified in comments.
- Avoiding *false sharing* (`alignas(std::hardware_destructive_interference_size)`).
- Comparison: `std::async` vs. mutex-based thread pool vs. work-stealing.

## 5. Module: ECS
### Requirements
- `Entity` = index + generation (stale handle detection).
- **Archetypes**: each component combination lives in 16 KB *chunks* of SoA arrays.
- Typed queries using variadic templates + concepts:
  ```cpp
  world.query<Position, const Velocity>().each([](Position& p, const Velocity& v){ p.v += v.v; });
  ```
- Parallel query execution per chunk on top of the job system.
- **Command buffers** to create/destroy entities during iteration without invalidating data.
- System scheduling with read/write conflict detection (non-conflicting systems run in parallel).

### What to demonstrate
- Measured AoS vs. SoA vs. archetype layouts (time and cache misses).
- Cost of abstractions: `virtual` vs. `std::variant` vs. templates; `std::function` vs. typed lambdas.
- Custom allocators: per-frame arena, per-chunk-type pool.

## 6. Module: Profiler
- `PROFILE_SCOPE("name")` macro with < 50 ns overhead per scope.
- Lock-free per-thread ring buffers, timestamps via `rdtsc` or `steady_clock`.
- ImGui *flame graph* / timeline viewer per thread and per frame.
- Export to Chrome Trace format (`chrome://tracing` / Perfetto).
- Optional Tracy integration to validate the custom measurements.

## 7. Visual demo: "Swarm"
### Description
A 2D swarm (boids) simulation with 10k–200k entities:
- Components: `Position`, `Velocity`, `Color`, `Lifetime`, `Team`.
- Systems: movement integration, *separation/alignment/cohesion* using a **spatial grid**, edge bouncing, continuous spawn/despawn, color updates based on speed.
- Instanced rendering of quads/circles (one draw call per batch).

### ImGui panel
- Sliders for entity count and thread count (1…N).
- Toggles: AoS / SoA layout, single-thread / multi-thread, spatial grid on/off.
- Live per-system timings and a frame time graph.
- Embedded profiler viewer.

### Success criteria
- 100k entities at ≥ 60 FPS on an 8-core CPU (Release).
- Near-linear scaling up to 4–6 threads, documented.
- Flipping toggles live shows the performance difference instantly.

## 8. Benchmarks and the documented investigation
`/bench` includes:
1. AoS vs. SoA vs. archetypes (iterating 1M components).
2. Job system scaling 1→N threads (`parallel_for`, dependency tree).
3. Per-job overhead (mutex queue vs. Chase-Lev).
4. Boids: brute force O(n²) vs. spatial grid.

`/docs/performance-investigation.md` tells the full story of a real problem:
> *Symptom → measurement (profiler, `perf stat` cache misses) → hypothesis → fix → before/after numbers → lessons learned.*

Include charts (PNG) generated from Google Benchmark JSON results.

## 9. Milestone roadmap
Each milestone ends with something verifiable; don't move on until its exit criteria are met.

### M1 · Foundations — done
- CMake with presets, CI (Linux + Windows), ASan/UBSan/TSan, clang-tidy and clang-format.
- Base allocators (per-frame arena, pool) and a minimal profiler with `PROFILE_SCOPE`.
- **Exit (met):** CI green with sanitizers; allocator tests; Chrome Trace from `hive_engine` (`build/debug/trace.json`).

### M2 · Job System — done
- Thread pool, Chase-Lev deques, dependency counters, `parallel_for`.
- Stress tests (millions of jobs, dependency trees) under TSan.
- **Exit (met):** unit + 1e6-job stress tests green; Release mutex vs work-stealing JSON at `docs/bench/mutex_vs_work_stealing.json`. TSan is Linux CI (`linux-tsan`; MSVC has no TSan).

### M3 · ECS core — done
- Generational entities, archetypes, SoA chunks, typed queries, command buffers.
- **Exit (met):** Catch2 tests for create/destroy/migration/query/command buffer; Release AoS vs SoA vs archetypes JSON at `docs/bench/aos_soa_archetype.json`.

### M4 · Parallel ECS — done
- Per-chunk query execution on the job system; system scheduling with read/write conflict detection.
- **Exit (met):** Catch2 parallel query + wave tests; Release 1→N JSON at `docs/bench/parallel_query_scaling.json`. Independent systems overlap in `trace.json` (Perfetto / `chrome://tracing`). TSan is Linux CI (`linux-tsan`; MSVC has no TSan).

### M5 · "Swarm" visual demo — done
- raylib + ImGui, boids with a spatial grid, instanced rendering, continuous spawn/despawn.
- Panel with live toggles (AoS/SoA, threads, grid on/off) and a timings panel (not a flame graph).
- **Exit (met):** 100k entities at ≥ 60 FPS in Release, with toggles showing the difference instantly. Measure locally (`cmake --preset release` then `build/release/hive_engine`).

### M6 · Performance investigation
- Find a real bottleneck with the profiler and `perf`, fix it, and measure.
- Write `docs/performance-investigation.md` with symptom, measurement, hypothesis, fix, and before/after numbers.
- **Exit:** the document, with charts generated from the benchmark JSON files.

### M7 · Polish and presentation
- Final README, architecture diagram, demo GIF/video, one-command reproducible build and benchmarks.
- **Exit:** someone else can clone the repo, build it, and run the demo without help.

## 10. Final README (what recruiters will see)
- A GIF of the 100k-entity demo within the first 5 seconds of reading.
- A table of key results (before/after, thread scaling).
- An architecture diagram.
- Design decisions and trade-offs (why archetypes, why Chase-Lev).
- How to build and reproduce the benchmarks with one command.
