#pragma once
#include "entity.h"
#include "core/types.h"
#include "core/config.h"
#if ECS_USE_SMALL_VECTOR
#include "core/small_vector.h"
#endif
#include <vector>
#include <cassert>

// Base interface for component arrays (for polymorphic entity removal)
class IComponentArray {
public:
    virtual ~IComponentArray() = default;
    virtual void EntityRemoved(Entity entity) = 0;
};

// Sparse set for component storage
// Sparse array: entity.index -> dense index
// Dense array: actual component data (packed)
template<typename T>
class ComponentArray : public IComponentArray {
public:
    ComponentArray(u32 maxEntities = 1024)
        : m_VersionCounter(0) {
        m_Sparse.resize(maxEntities, INVALID_INDEX);
        m_Dense.reserve(maxEntities);
        m_Entities.reserve(maxEntities);
        m_Versions.reserve(maxEntities);
    }

    // Add component to entity
    void Add(Entity entity, const T& component) {
        assert(!Has(entity) && "Component already exists");

        // Grow sparse array if needed
        if (entity.index >= m_Sparse.size()) {
            m_Sparse.resize(entity.index + 1, INVALID_INDEX);
        }

        u32 denseIndex = static_cast<u32>(m_Dense.size());
        m_Sparse[entity.index] = denseIndex;
        m_Dense.push_back(component);
        m_Entities.push_back(entity);
        m_Versions.push_back(NextVersion());
    }

    // Remove component from entity
    void Remove(Entity entity) {
        assert(Has(entity) && "Component doesn't exist");

        u32 denseIndex = m_Sparse[entity.index];
        u32 lastIndex = static_cast<u32>(m_Dense.size()) - 1;

        // If removing last element, no swap needed
        if (denseIndex == lastIndex) {
            m_Sparse[entity.index] = INVALID_INDEX;
            m_Dense.pop_back();
            m_Entities.pop_back();
            m_Versions.pop_back();
            return;
        }

        // Swap with last element
        m_Dense[denseIndex] = m_Dense[lastIndex];
        m_Entities[denseIndex] = m_Entities[lastIndex];
        m_Versions[denseIndex] = m_Versions[lastIndex];

        // Update sparse array for swapped entity
        Entity lastEntity = m_Entities[denseIndex];
        m_Sparse[lastEntity.index] = denseIndex;

        // Clear removed slot
        m_Sparse[entity.index] = INVALID_INDEX;
        m_Dense.pop_back();
        m_Entities.pop_back();
        m_Versions.pop_back();
    }

    // Get component (const and non-const)
    T& Get(Entity entity) {
        assert(Has(entity) && "Component doesn't exist");
        return m_Dense[m_Sparse[entity.index]];
    }

    const T& Get(Entity entity) const {
        assert(Has(entity) && "Component doesn't exist");
        return m_Dense[m_Sparse[entity.index]];
    }

    T& GetMutable(Entity entity) {
        MarkDirty(entity);
        return Get(entity);
    }

    u32 GetVersion(Entity entity) const {
        assert(Has(entity) && "Component doesn't exist");
        return m_Versions[m_Sparse[entity.index]];
    }

    void MarkDirty(Entity entity) {
        assert(Has(entity) && "Component doesn't exist");
        m_Versions[m_Sparse[entity.index]] = NextVersion();
    }

    // Check if entity has component
    bool Has(Entity entity) const {
        return entity.index < m_Sparse.size() &&
               m_Sparse[entity.index] != INVALID_INDEX;
    }

    // Get component count
    size_t Size() const { return m_Dense.size(); }

    // Iteration (cache-friendly dense array)
    T* Data() { return m_Dense.data(); }
    const T* Data() const { return m_Dense.data(); }

    Entity GetEntity(size_t index) const { return m_Entities[index]; }

    // For range-based for loops
    auto begin() { return m_Dense.begin(); }
    auto end() { return m_Dense.end(); }
    auto begin() const { return m_Dense.begin(); }
    auto end() const { return m_Dense.end(); }

    // IComponentArray interface implementation
    void EntityRemoved(Entity entity) override {
        if (Has(entity)) {
            Remove(entity);
        }
    }

private:
    u32 NextVersion() {
        ++m_VersionCounter;
        if (m_VersionCounter == 0) {
            ++m_VersionCounter;
        }
        return m_VersionCounter;
    }

    static constexpr u32 INVALID_INDEX = 0xFFFFFFFF;

    std::vector<u32> m_Sparse;      // entity.index -> dense index

#if ECS_USE_SMALL_VECTOR
    using DenseStorage = SmallVector<T, ECS_SMALL_VECTOR_INLINE_CAPACITY>;
    using EntityStorage = SmallVector<Entity, ECS_SMALL_VECTOR_INLINE_CAPACITY>;
    using VersionStorage = SmallVector<u32, ECS_SMALL_VECTOR_INLINE_CAPACITY>;
#else
    using DenseStorage = std::vector<T>;
    using EntityStorage = std::vector<Entity>;
    using VersionStorage = std::vector<u32>;
#endif

    DenseStorage m_Dense;         // Packed component data
    EntityStorage m_Entities;     // entity for each component (parallel to dense)
    VersionStorage m_Versions;    // version per component slot
    u32 m_VersionCounter;
};
