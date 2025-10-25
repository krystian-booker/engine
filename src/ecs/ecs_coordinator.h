#pragma once
#include "entity_manager.h"
#include "component_registry.h"
#include "hierarchy_manager.h"
#include "systems/transform_system.h"
#include <memory>

// Central hub for all ECS operations
// Provides unified API for entity, component, and system management
class ECSCoordinator {
public:
    ECSCoordinator() = default;
    ~ECSCoordinator() = default;

    // ========================================================================
    // Lifecycle Management
    // ========================================================================

    void Init() {
        m_EntityManager = std::make_unique<EntityManager>();
        m_ComponentRegistry = std::make_unique<ComponentRegistry>();
        m_HierarchyManager = std::make_unique<HierarchyManager>();

        // Register core components
        RegisterComponent<Transform>();

        // Initialize systems
        m_TransformSystem = std::make_unique<TransformSystem>(m_ComponentRegistry.get(), m_HierarchyManager.get());
    }

    void Shutdown() {
        m_TransformSystem.reset();
        m_HierarchyManager.reset();
        m_ComponentRegistry.reset();
        m_EntityManager.reset();
    }

    // ========================================================================
    // Entity API
    // ========================================================================

    Entity CreateEntity() {
        return m_EntityManager->CreateEntity();
    }

    void DestroyEntity(Entity entity) {
        m_EntityManager->DestroyEntity(entity);
        m_ComponentRegistry->OnEntityDestroyed(entity);
        m_HierarchyManager->OnEntityDestroyed(entity);
    }

    bool IsEntityAlive(Entity entity) const {
        return m_EntityManager->IsAlive(entity);
    }

    u32 GetEntityCount() const {
        return m_EntityManager->GetEntityCount();
    }

    // ========================================================================
    // Component API
    // ========================================================================

    template<typename T>
    void RegisterComponent() {
        m_ComponentRegistry->RegisterComponent<T>();
    }

    template<typename T>
    void AddComponent(Entity entity, const T& component) {
        auto array = m_ComponentRegistry->GetComponentArray<T>();
        array->Add(entity, component);
    }

    template<typename T>
    void RemoveComponent(Entity entity) {
        auto array = m_ComponentRegistry->GetComponentArray<T>();
        array->Remove(entity);
    }

    template<typename T>
    T& GetComponent(Entity entity) {
        auto array = m_ComponentRegistry->GetComponentArray<T>();
        return array->Get(entity);
    }

    template<typename T>
    const T& GetComponent(Entity entity) const {
        auto array = m_ComponentRegistry->GetComponentArray<T>();
        return array->Get(entity);
    }

    template<typename T>
    bool HasComponent(Entity entity) const {
        auto array = m_ComponentRegistry->GetComponentArray<T>();
        return array->Has(entity);
    }

    // ========================================================================
    // Hierarchy API
    // ========================================================================

    void SetParent(Entity child, Entity parent) {
        m_HierarchyManager->SetParent(child, parent);
    }

    void RemoveParent(Entity child) {
        m_HierarchyManager->RemoveParent(child);
    }

    Entity GetParent(Entity child) const {
        return m_HierarchyManager->GetParent(child);
    }

    const std::vector<Entity>& GetChildren(Entity parent) const {
        return m_HierarchyManager->GetChildren(parent);
    }

    bool HasChildren(Entity entity) const {
        return m_HierarchyManager->HasChildren(entity);
    }

    std::vector<Entity> GetRootEntities() const {
        return m_HierarchyManager->GetRootEntities();
    }

    void TraverseDepthFirst(Entity root, std::function<void(Entity)> callback) const {
        m_HierarchyManager->TraverseDepthFirst(root, callback);
    }

    // ========================================================================
    // System API
    // ========================================================================

    void Update(float deltaTime) {
        // Update all systems
        m_TransformSystem->Update(deltaTime);
        // Add more systems here as we build them
    }

private:
    std::unique_ptr<EntityManager> m_EntityManager;
    std::unique_ptr<ComponentRegistry> m_ComponentRegistry;
    std::unique_ptr<HierarchyManager> m_HierarchyManager;
    std::unique_ptr<TransformSystem> m_TransformSystem;
};
