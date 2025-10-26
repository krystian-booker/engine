#pragma once
#include "component_array.h"
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
};
