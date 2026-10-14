#pragma once

#include "ecs/entity.h"
#include "ecs/type.h"
#include "ecs/world.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace hive::ecs {

// Records structural changes and applies them later via World's immediate API.
// Not thread-safe. Create+add+add may migrate twice on playback — correct, not hot.
class CommandBuffer {
public:
    explicit CommandBuffer(World& world) noexcept : m_world(&world) {}

    CommandBuffer(const CommandBuffer&) = delete;
    CommandBuffer& operator=(const CommandBuffer&) = delete;
    CommandBuffer(CommandBuffer&&) = delete;
    CommandBuffer& operator=(CommandBuffer&&) = delete;
    ~CommandBuffer() = default;

    Entity create();
    void destroy(Entity entity);

    template <Component C> void add(Entity entity, const C& value);

    template <Component C> void remove(Entity entity);

    void playback();

private:
    enum class OpKind : std::uint8_t {
        Create,
        Destroy,
        Add,
        Remove,
    };

    struct Op {
        OpKind kind = OpKind::Create;
        Entity entity;
        std::uint32_t type_id = 0;
        std::size_t payload_offset = 0;
    };

    World* m_world;
    std::vector<Op> m_ops;
    std::vector<std::byte> m_payload;
};

inline Entity CommandBuffer::create() {
    const Entity entity = m_world->reserve();
    Op op;
    op.kind = OpKind::Create;
    op.entity = entity;
    m_ops.push_back(op);
    return entity;
}

inline void CommandBuffer::destroy(Entity entity) {
    Op op;
    op.kind = OpKind::Destroy;
    op.entity = entity;
    m_ops.push_back(op);
}

template <Component C> void CommandBuffer::add(Entity entity, const C& value) {
    const std::size_t offset = align_up(m_payload.size(), alignof(C));
    m_payload.resize(offset + sizeof(C));
    std::memcpy(m_payload.data() + offset, &value, sizeof(C));
    Op op;
    op.kind = OpKind::Add;
    op.entity = entity;
    op.type_id = TypeId::of<C>();
    op.payload_offset = offset;
    m_ops.push_back(op);
}

template <Component C> void CommandBuffer::remove(Entity entity) {
    Op op;
    op.kind = OpKind::Remove;
    op.entity = entity;
    op.type_id = TypeId::of<C>();
    m_ops.push_back(op);
}

} // namespace hive::ecs
