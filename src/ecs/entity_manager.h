#pragma once
#include "entity.h"
#include "core/config.h"
#include <vector>
#include <queue>

class EntityManager {
public:
    EntityManager();
    ~EntityManager();

    // Entity lifecycle
    Entity CreateEntity();
    void DestroyEntity(Entity entity);
    bool IsAlive(Entity entity) const;

#if ECS_ENABLE_SIGNATURES
    EntitySignature GetSignature(Entity entity) const;
    void SetSignatureBit(Entity entity, u32 bitIndex);
    void ClearSignatureBit(Entity entity, u32 bitIndex);
    void ResetSignature(Entity entity);
#endif

    // Stats
    u32 GetEntityCount() const { return m_AliveCount; }
    u32 GetCapacity() const { return static_cast<u32>(m_Generations.size()); }

private:
    std::vector<u32> m_Generations;  // Generation per slot
    std::queue<u32> m_FreeList;      // Recycled indices
    u32 m_AliveCount;
#if ECS_ENABLE_SIGNATURES
    std::vector<EntitySignature> m_Signatures;
#endif
};
