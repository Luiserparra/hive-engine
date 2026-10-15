#pragma once

#include "core/vec2.h"
#include "demo/components.h"

#include <memory>
#include <span>
#include <vector>

namespace hive::demo {

struct Instance {
    hive::core::Vec2 pos{};
    Color color{};
};

class Renderer {
public:
    Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    ~Renderer();

    void upload(std::span<const Instance> instances);
    void draw(float radius);

    [[nodiscard]] bool instancing() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace hive::demo
