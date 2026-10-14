#pragma once

#include <cmath>

namespace hive::core {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

[[nodiscard]] inline Vec2 operator+(Vec2 a, Vec2 b) noexcept {
    return {a.x + b.x, a.y + b.y};
}

[[nodiscard]] inline Vec2 operator-(Vec2 a, Vec2 b) noexcept {
    return {a.x - b.x, a.y - b.y};
}

[[nodiscard]] inline Vec2 operator-(Vec2 v) noexcept {
    return {-v.x, -v.y};
}

[[nodiscard]] inline Vec2 operator*(Vec2 v, float s) noexcept {
    return {v.x * s, v.y * s};
}

[[nodiscard]] inline Vec2 operator*(float s, Vec2 v) noexcept {
    return v * s;
}

inline Vec2& operator+=(Vec2& a, Vec2 b) noexcept {
    a = a + b;
    return a;
}

inline Vec2& operator-=(Vec2& a, Vec2 b) noexcept {
    a = a - b;
    return a;
}

inline Vec2& operator*=(Vec2& v, float s) noexcept {
    v = v * s;
    return v;
}

[[nodiscard]] inline float dot(Vec2 a, Vec2 b) noexcept {
    return a.x * b.x + a.y * b.y;
}

[[nodiscard]] inline float length_sq(Vec2 v) noexcept {
    return dot(v, v);
}

[[nodiscard]] inline float length(Vec2 v) noexcept {
    return std::sqrt(length_sq(v));
}

[[nodiscard]] inline Vec2 normalize(Vec2 v) noexcept {
    const float len = length(v);
    if (len <= 1.0e-6f) {
        return {};
    }
    return v * (1.0f / len);
}

} // namespace hive::core
