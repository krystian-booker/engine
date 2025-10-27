#include "entity_manager.h"

// Define the static Invalid entity
const Entity Entity::Invalid = {0xFFFFFFFF, 0xFFFFFFFF};

EntityManager::EntityManager()
    : m_AliveCount(0) {
}

EntityManager::~EntityManager() {
}

Entity EntityManager::CreateEntity() {
    u32 index;
    u32 generation;

    if (!m_FreeList.empty()) {
        // Reuse a freed slot
        index = m_FreeList.front();
        m_FreeList.pop();
        generation = m_Generations[index];
#if ECS_ENABLE_SIGNATURES
        m_Signatures[index] = 0;
#endif
    } else {
        // Allocate a new slot
        index = static_cast<u32>(m_Generations.size());
        generation = 0;
        m_Generations.push_back(generation);
#if ECS_ENABLE_SIGNATURES
        m_Signatures.push_back(0);
#endif
    }

    m_AliveCount++;

    return Entity{index, generation};
}

void EntityManager::DestroyEntity(Entity entity) {
    ENGINE_ASSERT(entity.IsValid());
    ENGINE_ASSERT(entity.index < m_Generations.size());
    ENGINE_ASSERT(IsAlive(entity));

    // Increment generation counter to invalidate old handles
    m_Generations[entity.index]++;

#if ECS_ENABLE_SIGNATURES
    m_Signatures[entity.index] = 0;
#endif

    // Add to free list for reuse
    m_FreeList.push(entity.index);

    m_AliveCount--;
}

bool EntityManager::IsAlive(Entity entity) const {
    if (!entity.IsValid()) {
        return false;
    }

    if (entity.index >= m_Generations.size()) {
        return false;
    }

    // Check if generation matches
    return m_Generations[entity.index] == entity.generation;
}

#if ECS_ENABLE_SIGNATURES
EntitySignature EntityManager::GetSignature(Entity entity) const {
    if (!entity.IsValid() || entity.index >= m_Signatures.size()) {
        return 0;
    }
    return m_Signatures[entity.index];
}

void EntityManager::SetSignatureBit(Entity entity, u32 bitIndex) {
    if (!entity.IsValid() || entity.index >= m_Signatures.size()) {
        return;
    }

    ENGINE_ASSERT(bitIndex < ECS_SIGNATURE_BITS);
    m_Signatures[entity.index] |= (EntitySignature{1} << bitIndex);
}

void EntityManager::ClearSignatureBit(Entity entity, u32 bitIndex) {
    if (!entity.IsValid() || entity.index >= m_Signatures.size()) {
        return;
    }

    ENGINE_ASSERT(bitIndex < ECS_SIGNATURE_BITS);
    m_Signatures[entity.index] &= ~(EntitySignature{1} << bitIndex);
}

void EntityManager::ResetSignature(Entity entity) {
    if (!entity.IsValid() || entity.index >= m_Signatures.size()) {
        return;
    }

    m_Signatures[entity.index] = 0;
}
#endif
