// Packed neighbor accumulate. Compiled with /arch:AVX2 (MSVC) or -mavx2
// (GCC/Clang) on this file only. The public API is Vec2/span, not __m256.

#include "demo/steering.h"

#include "core/vec2.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstring>
#include <immintrin.h>

namespace hive::demo {
namespace {

static_assert(sizeof(Team) == 1, "team ids are packed as consecutive bytes for an 8-wide load");

void accumulate_one(SteerAccum& acc, hive::core::Vec2 self_pos, Team self_team,
                    std::uint32_t self_id, hive::core::Vec2 npos, hive::core::Vec2 nvel, Team nteam,
                    std::uint32_t idx, float perc_sq, float sep_sq) noexcept {
    if (idx == self_id) {
        return;
    }
    const hive::core::Vec2 away = self_pos - npos;
    const float dist_sq = hive::core::length_sq(away);
    if (dist_sq > perc_sq || dist_sq < 1.0e-8f) {
        return;
    }
    if (dist_sq < sep_sq) {
        acc.sep += away * (1.0f / (dist_sq + 1.0e-4f));
        acc.n_sep += 1;
    }
    if (self_team.id == nteam.id) {
        acc.align += nvel;
        acc.coh += npos;
        acc.n_flock += 1;
    }
}

hive::core::Vec2 hsum_xy(__m256 v) noexcept {
    const __m128 lo = _mm256_castps256_ps128(v);
    const __m128 hi = _mm256_extractf128_ps(v, 1);
    const __m128 s = _mm_add_ps(lo, hi);
    const __m128 pair = _mm_add_ps(s, _mm_movehl_ps(s, s));
    return {_mm_cvtss_f32(pair), _mm_cvtss_f32(_mm_shuffle_ps(pair, pair, 1))};
}

int count_pairs(__m256 mask) noexcept {
    const unsigned bits = static_cast<unsigned>(_mm256_movemask_ps(mask));
    return static_cast<int>(std::popcount(bits & 0x55u));
}

__m256 dup_dwords_to_xy(const __m256i dwords, int half) noexcept {
    const __m128i four =
        (half == 0) ? _mm256_castsi256_si128(dwords) : _mm256_extracti128_si256(dwords, 1);
    return _mm256_castsi256_ps(_mm256_cvtepi32_epi64(four));
}

} // namespace

void accumulate_packed_range(SteerAccum& acc, hive::core::Vec2 self_pos, Team self_team,
                             std::uint32_t self_id, std::span<const hive::core::Vec2> packed_pos,
                             std::span<const hive::core::Vec2> packed_vel,
                             std::span<const Team> packed_team,
                             std::span<const std::uint32_t> indices, std::uint32_t begin,
                             std::uint32_t end, float perc_sq, float sep_sq) noexcept {
    std::uint32_t limit = static_cast<std::uint32_t>(packed_pos.size());
    limit = std::min(limit, static_cast<std::uint32_t>(packed_vel.size()));
    limit = std::min(limit, static_cast<std::uint32_t>(packed_team.size()));
    limit = std::min(limit, static_cast<std::uint32_t>(indices.size()));
    if (begin >= end || begin >= limit) {
        return;
    }
    end = std::min(end, limit);

    const __m256 self_xyxy = _mm256_set_ps(self_pos.y, self_pos.x, self_pos.y, self_pos.x,
                                           self_pos.y, self_pos.x, self_pos.y, self_pos.x);
    const __m256 perc_v = _mm256_set1_ps(perc_sq);
    const __m256 sep_v = _mm256_set1_ps(sep_sq);
    const __m256 eps_v = _mm256_set1_ps(1.0e-8f);
    const __m256 tiny_v = _mm256_set1_ps(1.0e-4f);
    const __m256 one_v = _mm256_set1_ps(1.0f);
    const __m256i self_id_v = _mm256_set1_epi32(static_cast<int>(self_id));
    const __m256i all_ones = _mm256_set1_epi32(-1);
    const __m128i self_team_v = _mm_set1_epi8(static_cast<char>(self_team.id));

    __m256 sep_acc = _mm256_setzero_ps();
    __m256 align_acc = _mm256_setzero_ps();
    __m256 coh_acc = _mm256_setzero_ps();
    int n_sep = 0;
    int n_flock = 0;

    std::uint32_t k = begin;
    while (end - k >= 8) {
        const float* pos_ptr = reinterpret_cast<const float*>(packed_pos.data() + k);
        const float* vel_ptr = reinterpret_cast<const float*>(packed_vel.data() + k);
        const __m256 npos_lo = _mm256_loadu_ps(pos_ptr);
        const __m256 npos_hi = _mm256_loadu_ps(pos_ptr + 8);
        const __m256 nvel_lo = _mm256_loadu_ps(vel_ptr);
        const __m256 nvel_hi = _mm256_loadu_ps(vel_ptr + 8);

        const __m256 away_lo = _mm256_sub_ps(self_xyxy, npos_lo);
        const __m256 away_hi = _mm256_sub_ps(self_xyxy, npos_hi);
        const __m256 sq_lo = _mm256_mul_ps(away_lo, away_lo);
        const __m256 sq_hi = _mm256_mul_ps(away_hi, away_hi);
        const __m256 dist_lo = _mm256_add_ps(sq_lo, _mm256_shuffle_ps(sq_lo, sq_lo, 0xB1));
        const __m256 dist_hi = _mm256_add_ps(sq_hi, _mm256_shuffle_ps(sq_hi, sq_hi, 0xB1));

        const __m256 in_perc_lo = _mm256_and_ps(_mm256_cmp_ps(dist_lo, perc_v, _CMP_LE_OQ),
                                                _mm256_cmp_ps(dist_lo, eps_v, _CMP_GE_OQ));
        const __m256 in_perc_hi = _mm256_and_ps(_mm256_cmp_ps(dist_hi, perc_v, _CMP_LE_OQ),
                                                _mm256_cmp_ps(dist_hi, eps_v, _CMP_GE_OQ));

        const __m256i ids =
            _mm256_loadu_si256(reinterpret_cast<const __m256i*>(indices.data() + k));
        const __m256i not_self = _mm256_xor_si256(_mm256_cmpeq_epi32(ids, self_id_v), all_ones);
        const __m256 not_self_lo = dup_dwords_to_xy(not_self, 0);
        const __m256 not_self_hi = dup_dwords_to_xy(not_self, 1);

        std::uint64_t team_bits = 0;
        std::memcpy(&team_bits, packed_team.data() + k, 8);
        const __m128i team_bytes = _mm_cvtsi64_si128(static_cast<long long>(team_bits));
        const __m256i team_eq32 = _mm256_cvtepi8_epi32(_mm_cmpeq_epi8(team_bytes, self_team_v));
        const __m256 team_lo = dup_dwords_to_xy(team_eq32, 0);
        const __m256 team_hi = dup_dwords_to_xy(team_eq32, 1);

        const __m256 valid_lo = _mm256_and_ps(in_perc_lo, not_self_lo);
        const __m256 valid_hi = _mm256_and_ps(in_perc_hi, not_self_hi);
        const __m256 in_sep_lo = _mm256_and_ps(valid_lo, _mm256_cmp_ps(dist_lo, sep_v, _CMP_LT_OQ));
        const __m256 in_sep_hi = _mm256_and_ps(valid_hi, _mm256_cmp_ps(dist_hi, sep_v, _CMP_LT_OQ));

        const __m256 inv_lo = _mm256_div_ps(one_v, _mm256_add_ps(dist_lo, tiny_v));
        const __m256 inv_hi = _mm256_div_ps(one_v, _mm256_add_ps(dist_hi, tiny_v));
        sep_acc = _mm256_add_ps(sep_acc, _mm256_and_ps(_mm256_mul_ps(away_lo, inv_lo), in_sep_lo));
        sep_acc = _mm256_add_ps(sep_acc, _mm256_and_ps(_mm256_mul_ps(away_hi, inv_hi), in_sep_hi));

        const __m256 flock_lo = _mm256_and_ps(valid_lo, team_lo);
        const __m256 flock_hi = _mm256_and_ps(valid_hi, team_hi);
        align_acc = _mm256_add_ps(align_acc, _mm256_and_ps(nvel_lo, flock_lo));
        align_acc = _mm256_add_ps(align_acc, _mm256_and_ps(nvel_hi, flock_hi));
        coh_acc = _mm256_add_ps(coh_acc, _mm256_and_ps(npos_lo, flock_lo));
        coh_acc = _mm256_add_ps(coh_acc, _mm256_and_ps(npos_hi, flock_hi));

        n_sep += count_pairs(in_sep_lo) + count_pairs(in_sep_hi);
        n_flock += count_pairs(flock_lo) + count_pairs(flock_hi);
        k += 8;
    }

    if (k != begin) {
        acc.sep += hsum_xy(sep_acc);
        acc.align += hsum_xy(align_acc);
        acc.coh += hsum_xy(coh_acc);
        acc.n_sep += n_sep;
        acc.n_flock += n_flock;
    }

    for (; k < end; ++k) {
        accumulate_one(acc, self_pos, self_team, self_id, packed_pos[k], packed_vel[k],
                       packed_team[k], indices[k], perc_sq, sep_sq);
    }
}

} // namespace hive::demo
