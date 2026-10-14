#include "ecs/command_buffer.h"

namespace hive::ecs {

void CommandBuffer::playback() {
    for (const Op& op : m_ops) {
        switch (op.kind) {
        case OpKind::Create:
            m_world->spawn(op.entity);
            break;
        case OpKind::Destroy:
            m_world->destroy(op.entity);
            break;
        case OpKind::Add:
            m_world->add_erased(op.entity, op.type_id, m_payload.data() + op.payload_offset);
            break;
        case OpKind::Remove:
            m_world->remove_erased(op.entity, op.type_id);
            break;
        }
    }
    m_ops.clear();
    m_payload.clear();
}

} // namespace hive::ecs
