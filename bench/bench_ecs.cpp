#include "ecs/world.h"

#include <benchmark/benchmark.h>

#include <cstddef>
#include <vector>

namespace {

constexpr std::size_t kN = 1'000'000;
constexpr float kDt = 1.0f / 60.0f;

struct Position {
    float x = 0;
    float y = 0;
};

struct Velocity {
    float x = 1;
    float y = 1;
};

struct AoS {
    Position position;
    Velocity velocity;
};

void integrate(Position& pos, const Velocity& vel) {
    pos.x += vel.x * kDt;
    pos.y += vel.y * kDt;
}

} // namespace

static void BM_AoS(benchmark::State& state) {
    std::vector<AoS> rows(kN);
    for (auto _ : state) {
        for (AoS& row : rows) {
            integrate(row.position, row.velocity);
        }
        benchmark::DoNotOptimize(rows.data());
    }
}
BENCHMARK(BM_AoS)->Unit(benchmark::kMillisecond);

static void BM_SoA(benchmark::State& state) {
    std::vector<Position> positions(kN);
    std::vector<Velocity> velocities(kN);
    for (auto _ : state) {
        for (std::size_t i = 0; i < kN; ++i) {
            integrate(positions[i], velocities[i]);
        }
        benchmark::DoNotOptimize(positions.data());
        benchmark::DoNotOptimize(velocities.data());
    }
}
BENCHMARK(BM_SoA)->Unit(benchmark::kMillisecond);

static void BM_Archetype(benchmark::State& state) {
    hive::ecs::World world;
    for (std::size_t i = 0; i < kN; ++i) {
        const hive::ecs::Entity entity = world.create();
        world.add<Position>(entity, Position{});
        world.add<Velocity>(entity, Velocity{});
    }
    for (auto _ : state) {
        world.query<Position, const Velocity>().each([](Position& pos, const Velocity& vel) {
            integrate(pos, vel);
        });
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_Archetype)->Unit(benchmark::kMillisecond);
