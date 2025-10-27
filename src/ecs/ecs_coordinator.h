#pragma once
#include "core/config.h"

#include "component_registry.h"
#include "hierarchy_manager.h"
#include "entity_view.h"
#include "core/job_system.h"
#include <memory>
#include <functional>
#include <vector>
#include <tuple>
#include <utility>
#include <type_traits>
#include <algorithm>
#include <typeindex>

class TransformSystem;
class CameraSystem;
class CameraController;

// Central hub for all ECS operations
// Provides unified API for entity, component, and system management
class ECSCoordinator {
public:
    ECSCoordinator() = default;
    ~ECSCoordinator();

    struct SystemIntent {
        std::vector<std::type_index> reads;
        std::vector<std::type_index> writes;

        template<typename T>
        void Read() {
            reads.emplace_back(typeid(T));
        }

        template<typename T>
        void Write() {
            writes.emplace_back(typeid(T));
        }

        bool ConflictsWith(const SystemIntent& other) const {
            for (const auto& writeType : writes) {
                if (Contains(other.writes, writeType) || Contains(other.reads, writeType)) {
                    return true;
                }
            }
            for (const auto& writeType : other.writes) {
                if (Contains(reads, writeType)) {
                    return true;
                }
            }
            return false;
        }

    private:
        static bool Contains(const std::vector<std::type_index>& list, const std::type_index& value) {
            return std::find(list.begin(), list.end(), value) != list.end();
        }
    };

    struct CameraSystemDeleter {
        void operator()(CameraSystem* system) const;
    };

    struct CameraControllerDeleter {
        void operator()(CameraController* controller) const;
    };

    // ========================================================================
    // Lifecycle Management
    // ========================================================================

    void Init();

    void Shutdown();

    // Initialize camera controller (call after Init, requires Window*)
    void SetupCameraController(class Window* window);

    // ========================================================================
    // Entity API
    // ========================================================================

    Entity CreateEntity() {
        return m_EntityManager->CreateEntity();
    }

    CameraSystem* GetCameraSystem() {
        return m_CameraSystem.get();
    }

    const CameraSystem* GetCameraSystem() const {
        return m_CameraSystem.get();
    }

    CameraController* GetCameraController() {
        return m_CameraController.get();
    }

    const CameraController* GetCameraController() const {
        return m_CameraController.get();
    }

    void DestroyEntity(Entity entity) {
        if (!StructuralChangesAllowed()) {
            ENGINE_ASSERT(false && "DestroyEntity not allowed during SafeForEach");
            return;
        }

        if (IsIterating()) {
            EnqueueDestroyEntity(entity);
            return;
        }

        DestroyEntityImmediate(entity);
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
        if (!StructuralChangesAllowed()) {
            ENGINE_ASSERT(false && "AddComponent not allowed during SafeForEach");
            return;
        }

        if (IsIterating()) {
            ENGINE_ASSERT(!HasComponent<T>(entity) && "Component already exists");
            EnqueueAddComponent<T>(entity, component);
            return;
        }

        AddComponentImmediate<T>(entity, component);
    }

    template<typename T>
    void RemoveComponent(Entity entity) {
        if (!StructuralChangesAllowed()) {
            ENGINE_ASSERT(false && "RemoveComponent not allowed during SafeForEach");
            return;
        }

        if (IsIterating()) {
            ENGINE_ASSERT(HasComponent<T>(entity) && "Component doesn't exist");
            EnqueueRemoveComponent<T>(entity);
            return;
        }

        RemoveComponentImmediate<T>(entity);
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
    T& GetMutableComponent(Entity entity) {
        auto array = m_ComponentRegistry->GetComponentArray<T>();
        return array->GetMutable(entity);
    }

    template<typename T>
    void MarkComponentDirty(Entity entity) {
        auto array = m_ComponentRegistry->GetComponentArray<T>();
        array->MarkDirty(entity);
    }

    template<typename T>
    u32 GetComponentVersion(Entity entity) const {
        auto array = m_ComponentRegistry->GetComponentArray<T>();
        return array->GetVersion(entity);
    }

    template<typename T>
    bool HasComponent(Entity entity) const {
        auto array = m_ComponentRegistry->GetComponentArray<T>();
        return array->Has(entity);
    }

    // ========================================================================
    // Query API
    // ========================================================================

    // Query entities that have all specified components
    // Returns a vector of entities with all required components
    // Optimized: iterates the first component array and checks for others
    template<typename... Components>
    std::vector<Entity> QueryEntities() {
        static_assert(sizeof...(Components) > 0, "Must specify at least one component type");

        std::vector<Entity> result;

        EntityView<Components...> view(m_ComponentRegistry.get(), m_EntityManager.get());
        result.reserve(view.Size());
        for (auto entry : view) {
            result.push_back(std::get<0>(entry));
        }

        return result;
    }

    // Iterate entities with callback, providing component references
    // Callback signature: void(Entity, ComponentA&, ComponentB&, ...)
    // Components are passed by reference and can be modified
    template<typename... Components, typename Func>
    void ForEach(Func&& callback) {
        static_assert(sizeof...(Components) > 0, "Must specify at least one component type");

        IterationScope scope(this, true);
        EntityView<Components...> view(m_ComponentRegistry.get(), m_EntityManager.get());
        auto&& cb = std::forward<Func>(callback);
        view.ForRange(
            0,
            view.Size(),
            [&](auto entry) {
                std::apply(
                    [&](Entity entity, Components&... comps) {
                        std::invoke(cb, entity, comps...);
                    },
                    entry);
            });
    }

    template<typename... Components, typename Func>
    void SafeForEach(Func&& callback) {
        static_assert(sizeof...(Components) > 0, "Must specify at least one component type");

        IterationScope scope(this, false);
        EntityView<Components...> view(m_ComponentRegistry.get(), m_EntityManager.get());
        auto&& cb = std::forward<Func>(callback);
        view.ForRange(
            0,
            view.Size(),
            [&](auto entry) {
                std::apply(
                    [&](Entity entity, Components&... comps) {
                        std::invoke(cb, entity, comps...);
                    },
                    entry);
            });
    }

    template<typename... Components, typename Func>
    void ForEachParallel(u32 chunkSize, Func&& callback) {
        static_assert(sizeof...(Components) > 0, "Must specify at least one component type");

        IterationScope scope(this, true);
        EntityView<Components...> view(m_ComponentRegistry.get(), m_EntityManager.get());

        const u32 total = static_cast<u32>(view.Size());
        if (total == 0) {
            return;
        }

        if (chunkSize == 0) {
            chunkSize = 64;
        }

        using CallbackType = std::decay_t<Func>;
        auto callbackHolder = std::make_shared<CallbackType>(std::forward<Func>(callback));

        auto invokeCallback = [&](auto entry) {
            std::apply(
                [&](Entity entity, Components&... comps) {
                    std::invoke(*callbackHolder, entity, comps...);
                },
                entry);
        };

        if (total <= chunkSize) {
            view.ForRange(0, total, invokeCallback);
            return;
        }

        chunkSize = std::max<u32>(1, chunkSize);
        const u32 chunkCount = (total + chunkSize - 1) / chunkSize;

        struct ChunkTask {
            EntityView<Components...>* view;
            std::shared_ptr<CallbackType> callback;
            size_t begin;
            size_t end;
        };

        std::vector<ChunkTask> tasks;
        tasks.reserve(chunkCount);

        JobSystem::TaskGroup group;
        JobSystem::InitTaskGroup(group);

        auto jobFunc = [](void* ptr) {
            auto* task = static_cast<ChunkTask*>(ptr);
            task->view->ForRange(
                task->begin,
                task->end,
                [&](auto entry) {
                    std::apply(
                        [&](Entity entity, Components&... comps) {
                            std::invoke(*task->callback, entity, comps...);
                        },
                        entry);
                });
        };

        bool hasAsyncWork = false;

        for (u32 chunk = 0; chunk < chunkCount; ++chunk) {
            size_t beginIndex = static_cast<size_t>(chunk) * chunkSize;
            size_t endIndex = std::min<size_t>(beginIndex + chunkSize, total);

            tasks.push_back(ChunkTask{&view, callbackHolder, beginIndex, endIndex});
            ChunkTask* taskData = &tasks.back();

            if (Job* job = JobSystem::CreateJob(jobFunc, taskData)) {
                JobSystem::AttachToTaskGroup(group, job);
                JobSystem::Run(job);
                hasAsyncWork = true;
            } else {
                jobFunc(taskData);
            }
        }

        if (hasAsyncWork) {
            JobSystem::Wait(group);
        }
    }

    bool CanRunInParallel(const SystemIntent& a, const SystemIntent& b) const {
        return !a.ConflictsWith(b);
    }

    void FlushDeferredOperations() {
        ENGINE_ASSERT(!IsIterating() && "Cannot flush deferred operations while iterating");
        FlushDeferredOps();
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

    void Update(float deltaTime);

    // ========================================================================
    // Internal Access (for serialization, etc.)
    // ========================================================================

    ComponentRegistry* GetComponentRegistry() {
        return m_ComponentRegistry.get();
    }

    const ComponentRegistry* GetComponentRegistry() const {
        return m_ComponentRegistry.get();
    }

    EntityManager* GetEntityManager() {
        return m_EntityManager.get();
    }

    const EntityManager* GetEntityManager() const {
        return m_EntityManager.get();
    }

    HierarchyManager* GetHierarchyManager() {
        return m_HierarchyManager.get();
    }

    const HierarchyManager* GetHierarchyManager() const {
        return m_HierarchyManager.get();
    }

private:
    // ========================================================================
    // Query Helpers
    // ========================================================================

    // Check if entity has all specified components
    // Uses fold expressions for compile-time expansion
    template<typename... Components>
    bool HasAllComponents(Entity entity) const {
        return (HasComponent<Components>(entity) && ...);
    }

    struct DeferredOp {
        enum class Type {
            AddComponent,
            RemoveComponent,
            DestroyEntity
        };

        Type type;
        Entity entity;
        std::function<void()> apply;
    };

    struct TransformSystemDeleter {
        void operator()(TransformSystem* system) const;
    };

    struct IterationScope {
        IterationScope(ECSCoordinator* coordinator, bool allowStructuralChanges)
            : m_Coordinator(coordinator)
            , m_AllowStructuralChanges(allowStructuralChanges) {
            m_Coordinator->BeginIteration(m_AllowStructuralChanges);
        }

        ~IterationScope() {
            m_Coordinator->EndIteration(m_AllowStructuralChanges);
        }

    private:
        ECSCoordinator* m_Coordinator;
        bool m_AllowStructuralChanges;
    };

    void BeginIteration(bool allowStructuralChanges) {
        ++m_DeferDepth;
        if (!allowStructuralChanges) {
            ++m_SafeIterationDepth;
        }
    }

    void EndIteration(bool allowStructuralChanges) {
        ENGINE_ASSERT(m_DeferDepth > 0);
        if (!allowStructuralChanges) {
            ENGINE_ASSERT(m_SafeIterationDepth > 0);
            --m_SafeIterationDepth;
        }

        --m_DeferDepth;
        if (m_DeferDepth == 0) {
            FlushDeferredOps();
        }
    }

    bool IsIterating() const {
        return m_DeferDepth > 0;
    }

    bool StructuralChangesAllowed() const {
        return m_SafeIterationDepth == 0;
    }

    void FlushDeferredOps() {
        ENGINE_ASSERT(!IsIterating() && "Cannot flush while iterating");

        while (!m_DeferredOps.empty()) {
            auto ops = std::move(m_DeferredOps);
            m_DeferredOps.clear();

            for (auto& op : ops) {
                if (op.apply) {
                    op.apply();
                }
            }
        }
    }

    template<typename T>
    void AddComponentImmediate(Entity entity, const T& component) {
        auto array = m_ComponentRegistry->GetComponentArray<T>();
        array->Add(entity, component);
        OnComponentAdded<T>(entity);
    }

    template<typename T>
    void RemoveComponentImmediate(Entity entity, bool strict = true) {
        auto array = m_ComponentRegistry->GetComponentArray<T>();
        if (!array->Has(entity)) {
            ENGINE_ASSERT(!strict && "Component doesn't exist");
            return;
        }
        array->Remove(entity);
        OnComponentRemoved<T>(entity);
    }

    template<typename T>
    void EnqueueAddComponent(Entity entity, const T& component) {
        m_DeferredOps.push_back(DeferredOp{
            DeferredOp::Type::AddComponent,
            entity,
            [this, entity, componentCopy = component]() {
                AddComponentImmediate<T>(entity, componentCopy);
            }});
    }

    template<typename T>
    void EnqueueRemoveComponent(Entity entity) {
        m_DeferredOps.push_back(DeferredOp{
            DeferredOp::Type::RemoveComponent,
            entity,
            [this, entity]() {
                RemoveComponentImmediate<T>(entity, false);
            }});
    }

    void EnqueueDestroyEntity(Entity entity) {
        m_DeferredOps.push_back(DeferredOp{
            DeferredOp::Type::DestroyEntity,
            entity,
            [this, entity]() {
                DestroyEntityImmediate(entity);
            }});
    }

    void DestroyEntityImmediate(Entity entity) {
        if (!m_EntityManager->IsAlive(entity)) {
            return;
        }

        m_EntityManager->DestroyEntity(entity);
        m_ComponentRegistry->OnEntityDestroyed(entity);
        m_HierarchyManager->OnEntityDestroyed(entity);
    }

#if ECS_ENABLE_SIGNATURES
    template<typename T>
    void OnComponentAdded(Entity entity) {
        auto bit = m_ComponentRegistry->GetComponentTypeId<T>();
        m_EntityManager->SetSignatureBit(entity, bit);
    }

    template<typename T>
    void OnComponentRemoved(Entity entity) {
        auto bit = m_ComponentRegistry->GetComponentTypeId<T>();
        m_EntityManager->ClearSignatureBit(entity, bit);
    }
#else
    template<typename T>
    void OnComponentAdded(Entity entity) {
        (void)entity;
    }

    template<typename T>
    void OnComponentRemoved(Entity entity) {
        (void)entity;
    }
#endif

    std::unique_ptr<EntityManager> m_EntityManager;
    std::unique_ptr<ComponentRegistry> m_ComponentRegistry;
    std::unique_ptr<HierarchyManager> m_HierarchyManager;
    std::unique_ptr<TransformSystem, TransformSystemDeleter> m_TransformSystem;
    std::unique_ptr<CameraSystem, CameraSystemDeleter> m_CameraSystem;
    std::unique_ptr<CameraController, CameraControllerDeleter> m_CameraController;
    std::vector<DeferredOp> m_DeferredOps;
    u32 m_DeferDepth = 0;
    u32 m_SafeIterationDepth = 0;
};

