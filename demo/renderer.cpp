#include "demo/renderer.h"

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <cstddef>
#include <cstdint>

namespace hive::demo {
namespace {

struct QuadVert {
    float x, y, u, v;
};

struct GpuInstance {
    float x = 0.0f;
    float y = 0.0f;
    std::uint32_t rgba = 0xffffffffu;
};

constexpr QuadVert kQuad[6] = {
    {-1.0f, -1.0f, 0.0f, 0.0f}, {1.0f, -1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f},
    {-1.0f, -1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f},  {-1.0f, 1.0f, 0.0f, 1.0f},
};

constexpr const char* kVs = R"(#version 330
layout (location = 0) in vec2 aCorner;
layout (location = 1) in vec2 aUV;
layout (location = 2) in vec2 aPos;
layout (location = 3) in vec4 aColor;
uniform mat4 mvp;
uniform float radius;
out vec2 vUV;
out vec4 vColor;
void main() {
    vUV = aUV;
    vColor = aColor;
    vec2 world = aPos + aCorner * radius;
    gl_Position = mvp * vec4(world, 0.0, 1.0);
}
)";

constexpr const char* kFs = R"(#version 330
in vec2 vUV;
in vec4 vColor;
out vec4 finalColor;
void main() {
    vec2 p = vUV * 2.0 - 1.0;
    if (dot(p, p) > 1.0) discard;
    finalColor = vColor;
}
)";

std::uint32_t pack_color(Color color) noexcept {
    return static_cast<std::uint32_t>(color.r) | (static_cast<std::uint32_t>(color.g) << 8) |
           (static_cast<std::uint32_t>(color.b) << 16) |
           (static_cast<std::uint32_t>(color.a) << 24);
}

} // namespace

struct Renderer::Impl {
    bool instancing = false;
    Shader shader{};
    int loc_mvp = -1;
    int loc_radius = -1;
    unsigned int vao = 0;
    unsigned int quad_vbo = 0;
    unsigned int instance_vbo = 0;
    int instance_capacity = 0;
    std::vector<GpuInstance> gpu;
    std::vector<Instance> fallback;

    void grow_instances(int count) {
        if (count <= instance_capacity && instance_vbo != 0) {
            return;
        }
        if (instance_vbo != 0) {
            rlUnloadVertexBuffer(instance_vbo);
            instance_vbo = 0;
        }
        instance_capacity = count < 1024 ? 1024 : count;
        instance_vbo = rlLoadVertexBuffer(
            nullptr, instance_capacity * static_cast<int>(sizeof(GpuInstance)), true);
        if (vao != 0) {
            rlEnableVertexArray(vao);
            rlEnableVertexBuffer(instance_vbo);
            rlSetVertexAttribute(2, 2, RL_FLOAT, false, static_cast<int>(sizeof(GpuInstance)), 0);
            rlSetVertexAttribute(3, 4, RL_UNSIGNED_BYTE, true,
                                 static_cast<int>(sizeof(GpuInstance)),
                                 static_cast<int>(offsetof(GpuInstance, rgba)));
            rlEnableVertexAttribute(2);
            rlEnableVertexAttribute(3);
            rlSetVertexAttributeDivisor(2, 1);
            rlSetVertexAttributeDivisor(3, 1);
            rlDisableVertexArray();
        }
    }
};

Renderer::Renderer() : m_impl(std::make_unique<Impl>()) {
    Impl& impl = *m_impl;
    impl.shader = LoadShaderFromMemory(kVs, kFs);
    impl.loc_mvp = GetShaderLocation(impl.shader, "mvp");
    impl.loc_radius = GetShaderLocation(impl.shader, "radius");
    if (impl.shader.id == 0 || impl.loc_mvp < 0) {
        impl.instancing = false;
        return;
    }

    impl.vao = rlLoadVertexArray();
    if (impl.vao == 0 || !rlEnableVertexArray(impl.vao)) {
        impl.instancing = false;
        return;
    }

    impl.quad_vbo = rlLoadVertexBuffer(kQuad, static_cast<int>(sizeof(kQuad)), false);
    rlEnableVertexBuffer(impl.quad_vbo);
    rlSetVertexAttribute(0, 2, RL_FLOAT, false, static_cast<int>(sizeof(QuadVert)), 0);
    rlSetVertexAttribute(1, 2, RL_FLOAT, false, static_cast<int>(sizeof(QuadVert)),
                         static_cast<int>(offsetof(QuadVert, u)));
    rlEnableVertexAttribute(0);
    rlEnableVertexAttribute(1);
    impl.grow_instances(1024);
    rlDisableVertexArray();
    impl.instancing = impl.instance_vbo != 0;
}

Renderer::~Renderer() {
    if (m_impl == nullptr) {
        return;
    }
    Impl& impl = *m_impl;
    if (impl.instance_vbo != 0) {
        rlUnloadVertexBuffer(impl.instance_vbo);
    }
    if (impl.quad_vbo != 0) {
        rlUnloadVertexBuffer(impl.quad_vbo);
    }
    if (impl.vao != 0) {
        rlUnloadVertexArray(impl.vao);
    }
    if (impl.shader.id != 0) {
        UnloadShader(impl.shader);
    }
}

bool Renderer::instancing() const noexcept {
    return m_impl != nullptr && m_impl->instancing;
}

void Renderer::upload(std::span<const Instance> instances) {
    Impl& impl = *m_impl;
    impl.gpu.resize(instances.size());
    for (std::size_t i = 0; i < instances.size(); ++i) {
        impl.gpu[i].x = instances[i].pos.x;
        impl.gpu[i].y = instances[i].pos.y;
        impl.gpu[i].rgba = pack_color(instances[i].color);
    }
    impl.fallback.assign(instances.begin(), instances.end());
    if (!impl.instancing || impl.gpu.empty()) {
        return;
    }
    impl.grow_instances(static_cast<int>(impl.gpu.size()));
    rlUpdateVertexBuffer(impl.instance_vbo, impl.gpu.data(),
                         static_cast<int>(impl.gpu.size() * sizeof(GpuInstance)), 0);
}

void Renderer::draw(float radius) {
    Impl& impl = *m_impl;
    if (impl.gpu.empty() && impl.fallback.empty()) {
        return;
    }
    if (!impl.instancing) {
        for (const Instance& instance : impl.fallback) {
            const ::Color color{instance.color.r, instance.color.g, instance.color.b,
                                instance.color.a};
            DrawCircleV(::Vector2{instance.pos.x, instance.pos.y}, radius, color);
        }
        return;
    }

    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    rlDisableDepthTest();

    const Matrix mvp = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
    rlEnableShader(impl.shader.id);
    rlSetUniformMatrix(impl.loc_mvp, mvp);
    rlSetUniform(impl.loc_radius, &radius, RL_SHADER_UNIFORM_FLOAT, 1);
    rlEnableVertexArray(impl.vao);
    rlDrawVertexArrayInstanced(0, 6, static_cast<int>(impl.gpu.size()));
    rlDisableVertexArray();
    rlEnableShader(rlGetShaderIdDefault());
    rlSetTexture(0);
    rlSetBlendMode(BLEND_ALPHA);
    rlEnableColorBlend();
}

} // namespace hive::demo
