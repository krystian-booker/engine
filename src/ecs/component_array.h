#pragma once
#include "entity.h"
#include "core/types.h"
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
    ComponentArray(u32 maxEntities = 1024) {
        m_Sparse.resize(maxEntities, INVALID_INDEX);
        m_Dense.reserve(maxEntities);
        m_Entities.reserve(maxEntities);
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
    }

    // Remove component from entity
    void Remove(Entity entity) {
        assert(Has(entity) && "Component doesn't exist");

        u32 denseIndex = m_Sparse[entity.index];
        u32 lastIndex = static_cast<u32>(m_Dense.size()) - 1;

        // Swap with last element
        m_Dense[denseIndex] = m_Dense[lastIndex];
        m_Entities[denseIndex] = m_Entities[lastIndex];

        // Update sparse array for swapped entity
        Entity lastEntity = m_Entities[denseIndex];
        m_Sparse[lastEntity.index] = denseIndex;

        // Clear removed slot
        m_Sparse[entity.index] = INVALID_INDEX;
        m_Dense.pop_back();
        m_Entities.pop_back();
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
    static constexpr u32 INVALID_INDEX = 0xFFFFFFFF;

    std::vector<u32> m_Sparse;      // entity.index -> dense index
    std::vector<T> m_Dense;         // Packed component data
    std::vector<Entity> m_Entities; // entity for each component (parallel to dense)
};
