#pragma once
#include "component_array.h"
#include "core/config.h"
#include <memory>
#include <unordered_map>
#include <typeindex>

// Central registry for all component types
// Manages component arrays and handles entity destruction cleanup
class ComponentRegistry {
public:
    ComponentRegistry() = default;
    ~ComponentRegistry() = default;

    // Register a component type (call once per type)
    // Creates a ComponentArray<T> for the given type
    template<typename T>
    void RegisterComponent() {
        std::type_index typeIndex = std::type_index(typeid(T));
        assert(m_ComponentArrays.find(typeIndex) == m_ComponentArrays.end()
               && "Component already registered");

        m_ComponentArrays[typeIndex] = std::make_shared<ComponentArray<T>>();

#if ECS_ENABLE_SIGNATURES
        assert(m_ComponentTypeIds.find(typeIndex) == m_ComponentTypeIds.end()
               && "Component type id already registered");
        assert(m_NextComponentTypeId < ECS_SIGNATURE_BITS && "Exceeded signature bit capacity");
        m_ComponentTypeIds[typeIndex] = m_NextComponentTypeId++;
#endif
    }

    // Get component array for a registered type
    // Returns nullptr if type not registered (in release), asserts in debug
    template<typename T>
    std::shared_ptr<ComponentArray<T>> GetComponentArray() {
        std::type_index typeIndex = std::type_index(typeid(T));
        assert(m_ComponentArrays.find(typeIndex) != m_ComponentArrays.end()
               && "Component not registered");

        return std::static_pointer_cast<ComponentArray<T>>(m_ComponentArrays[typeIndex]);
    }

    // Const overload for GetComponentArray
    template<typename T>
    std::shared_ptr<ComponentArray<T>> GetComponentArray() const {
        std::type_index typeIndex = std::type_index(typeid(T));
        assert(m_ComponentArrays.find(typeIndex) != m_ComponentArrays.end()
               && "Component not registered");

        return std::static_pointer_cast<ComponentArray<T>>(m_ComponentArrays.at(typeIndex));
    }

#if ECS_ENABLE_SIGNATURES
    template<typename T>
    u32 GetComponentTypeId() const {
        std::type_index typeIndex = std::type_index(typeid(T));
        auto it = m_ComponentTypeIds.find(typeIndex);
        assert(it != m_ComponentTypeIds.end() && "Component not registered");
        return it->second;
    }
#endif

    // Entity destroyed - remove all its components from all arrays
    // Safe to call even if entity doesn't have components in some arrays
    void OnEntityDestroyed(Entity entity) {
        // Iterate all arrays and remove entity if it has a component
        for (auto& [typeIndex, componentArray] : m_ComponentArrays) {
            auto array = std::static_pointer_cast<IComponentArray>(componentArray);
            array->EntityRemoved(entity);
        }
    }

private:
    // Maps type_index to component arrays (stored as void* for type erasure)
    std::unordered_map<std::type_index, std::shared_ptr<void>> m_ComponentArrays;
#if ECS_ENABLE_SIGNATURES
    std::unordered_map<std::type_index, u32> m_ComponentTypeIds;
    u32 m_NextComponentTypeId = 0;
#endif
};
