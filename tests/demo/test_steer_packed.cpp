#include "core/vec2.h"
#include "demo/steering.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cstdint>
#include <vector>

namespace {

struct PackedScene {
    std::vector<hive::core::Vec2> pos;
    std::vector<hive::core::Vec2> vel;
    std::vector<hive::demo::Team> team;
    std::vector<std::uint32_t> indices;
};

constexpr hive::core::Vec2 kSelfPos{0.0f, 0.0f};
constexpr hive::demo::Team kSelfTeam{0};
constexpr std::uint32_t kSelfId = 42;

void push_slot(PackedScene& scene, hive::core::Vec2 pos, hive::core::Vec2 vel,
               hive::demo::Team team, std::uint32_t id) {
    scene.pos.push_back(pos);
    scene.vel.push_back(vel);
    scene.team.push_back(team);
    scene.indices.push_back(id);
}

// Nine slots covering the SIMD width, a tail, self, both teams, and the three
// radius bands (sep / perception / outside).
PackedScene mixed_scene(std::uint32_t n) {
    PackedScene scene;
    const struct {
        hive::core::Vec2 pos;
        hive::core::Vec2 vel;
        hive::demo::Team team;
        std::uint32_t id;
    } slots[] = {
        {{8.0f, 0.0f}, {1.0f, 0.0f}, {0}, 1},       // 0 inside sep, same team
        {{30.0f, 0.0f}, {0.0f, 2.0f}, {0}, 2},      // 1 inside perc, same team
        {{50.0f, 0.0f}, {3.0f, 0.0f}, {0}, 3},      // 2 outside perc
        {{0.0f, 0.0f}, {9.0f, 9.0f}, {0}, kSelfId}, // 3 self
        {{4.0f, 0.0f}, {0.0f, 4.0f}, {1}, 5},       // 4 inside sep, other team
        {{20.0f, 0.0f}, {5.0f, 0.0f}, {1}, 6},      // 5 inside perc, other team
        {{12.0f, 16.0f}, {0.0f, 6.0f}, {0}, 7},     // 6 inside perc, same team
        {{0.0f, 0.0f}, {7.0f, 0.0f}, {0}, 8},       // 7 coinciding other id (eps)
        {{6.0f, 0.0f}, {0.0f, 8.0f}, {0}, 9},       // 8 inside sep, same team (tail)
    };
    for (std::uint32_t i = 0; i < n; ++i) {
        push_slot(scene, slots[i].pos, slots[i].vel, slots[i].team, slots[i].id);
    }
    return scene;
}

PackedScene uniform_scene(std::uint32_t n, hive::core::Vec2 pos, hive::core::Vec2 vel,
                          hive::demo::Team team) {
    PackedScene scene;
    for (std::uint32_t i = 0; i < n; ++i) {
        push_slot(scene, pos, vel, team, i + 1);
    }
    return scene;
}

void scalar_range(hive::demo::SteerAccum& acc, const PackedScene& scene, std::uint32_t begin,
                  std::uint32_t end, const hive::demo::BoidParams& params) {
    for (std::uint32_t k = begin; k < end; ++k) {
        if (scene.indices[k] == kSelfId) {
            continue;
        }
        const hive::core::Vec2 away = kSelfPos - scene.pos[k];
        const float dist_sq = hive::core::length_sq(away);
        hive::demo::accumulate_neighbor_dist_sq(acc, kSelfPos, {}, kSelfTeam, scene.pos[k],
                                                scene.vel[k], scene.team[k], away, dist_sq, params);
    }
}

void require_match(const hive::demo::SteerAccum& simd, const hive::demo::SteerAccum& scalar) {
    REQUIRE(simd.n_sep == scalar.n_sep);
    REQUIRE(simd.n_flock == scalar.n_flock);
    REQUIRE_THAT(simd.sep.x, Catch::Matchers::WithinAbs(scalar.sep.x, 1.0e-3f));
    REQUIRE_THAT(simd.sep.y, Catch::Matchers::WithinAbs(scalar.sep.y, 1.0e-3f));
    REQUIRE_THAT(simd.align.x, Catch::Matchers::WithinAbs(scalar.align.x, 1.0e-3f));
    REQUIRE_THAT(simd.align.y, Catch::Matchers::WithinAbs(scalar.align.y, 1.0e-3f));
    REQUIRE_THAT(simd.coh.x, Catch::Matchers::WithinAbs(scalar.coh.x, 1.0e-3f));
    REQUIRE_THAT(simd.coh.y, Catch::Matchers::WithinAbs(scalar.coh.y, 1.0e-3f));
}

void check_scene(const PackedScene& scene) {
    const hive::demo::BoidParams params{};
    const float perc_sq = params.perception * params.perception;
    const float sep_sq = params.separation_radius * params.separation_radius;
    const auto n = static_cast<std::uint32_t>(scene.pos.size());

    hive::demo::SteerAccum scalar{};
    scalar_range(scalar, scene, 0, n, params);

    hive::demo::SteerAccum simd{};
    hive::demo::accumulate_packed_range(simd, kSelfPos, kSelfTeam, kSelfId, scene.pos, scene.vel,
                                        scene.team, scene.indices, 0, n, perc_sq, sep_sq);
    require_match(simd, scalar);
}

} // namespace

TEST_CASE("packed range accum matches scalar (length 1, 3, 5, 8, 9)") {
    const std::uint32_t lengths[] = {1, 3, 5, 8, 9};
    for (const std::uint32_t n : lengths) {
        CAPTURE(n);
        check_scene(mixed_scene(n));
    }
}

TEST_CASE("packed range skips self in the middle of a vector") {
    const PackedScene scene = mixed_scene(8);
    REQUIRE(scene.indices[3] == kSelfId);

    const hive::demo::BoidParams params{};
    hive::demo::SteerAccum scalar{};
    scalar_range(scalar, scene, 0, 8, params);

    hive::demo::SteerAccum simd{};
    hive::demo::accumulate_packed_range(
        simd, kSelfPos, kSelfTeam, kSelfId, scene.pos, scene.vel, scene.team, scene.indices, 0, 8,
        params.perception * params.perception, params.separation_radius * params.separation_radius);
    require_match(simd, scalar);
    REQUIRE(simd.n_sep + simd.n_flock > 0);
}

TEST_CASE("packed range other team does not flock") {
    const PackedScene scene = uniform_scene(8, {8.0f, 0.0f}, {0.0f, 3.0f}, hive::demo::Team{1});
    check_scene(scene);

    hive::demo::SteerAccum simd{};
    const hive::demo::BoidParams params{};
    hive::demo::accumulate_packed_range(
        simd, kSelfPos, kSelfTeam, kSelfId, scene.pos, scene.vel, scene.team, scene.indices, 0, 8,
        params.perception * params.perception, params.separation_radius * params.separation_radius);
    REQUIRE(simd.n_flock == 0);
    REQUIRE(simd.n_sep == 8);
}

TEST_CASE("packed range inside sep") {
    const PackedScene scene = uniform_scene(8, {8.0f, 0.0f}, {1.0f, 0.0f}, hive::demo::Team{0});
    check_scene(scene);

    hive::demo::SteerAccum simd{};
    const hive::demo::BoidParams params{};
    hive::demo::accumulate_packed_range(
        simd, kSelfPos, kSelfTeam, kSelfId, scene.pos, scene.vel, scene.team, scene.indices, 0, 8,
        params.perception * params.perception, params.separation_radius * params.separation_radius);
    REQUIRE(simd.n_sep == 8);
    REQUIRE(simd.n_flock == 8);
}

TEST_CASE("packed range inside perception") {
    const PackedScene scene = uniform_scene(8, {30.0f, 0.0f}, {0.0f, 2.0f}, hive::demo::Team{0});
    check_scene(scene);

    hive::demo::SteerAccum simd{};
    const hive::demo::BoidParams params{};
    hive::demo::accumulate_packed_range(
        simd, kSelfPos, kSelfTeam, kSelfId, scene.pos, scene.vel, scene.team, scene.indices, 0, 8,
        params.perception * params.perception, params.separation_radius * params.separation_radius);
    REQUIRE(simd.n_sep == 0);
    REQUIRE(simd.n_flock == 8);
}

TEST_CASE("packed range outside perception") {
    const PackedScene scene = uniform_scene(8, {50.0f, 0.0f}, {3.0f, 0.0f}, hive::demo::Team{0});
    check_scene(scene);

    hive::demo::SteerAccum simd{};
    const hive::demo::BoidParams params{};
    hive::demo::accumulate_packed_range(
        simd, kSelfPos, kSelfTeam, kSelfId, scene.pos, scene.vel, scene.team, scene.indices, 0, 8,
        params.perception * params.perception, params.separation_radius * params.separation_radius);
    REQUIRE(simd.n_sep == 0);
    REQUIRE(simd.n_flock == 0);
    REQUIRE_THAT(simd.sep.x, Catch::Matchers::WithinAbs(0.0f, 1.0e-5f));
    REQUIRE_THAT(simd.align.x, Catch::Matchers::WithinAbs(0.0f, 1.0e-5f));
    REQUIRE_THAT(simd.coh.x, Catch::Matchers::WithinAbs(0.0f, 1.0e-5f));
}
