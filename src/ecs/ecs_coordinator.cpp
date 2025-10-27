#include "ecs/ecs_coordinator.h"
#include "ecs/components/transform.h"
#include "ecs/components/mesh_renderer.h"
#include "ecs/components/renderable.h"
#include "ecs/components/rotator.h"
#include "ecs/components/camera.h"
#include "ecs/components/light.h"
#include "ecs/systems/transform_system.h"
#include <iostream>

void ECSCoordinator::TransformSystemDeleter::operator()(TransformSystem* system) const {
    delete system;
}

void ECSCoordinator::Init() {
    m_EntityManager = std::make_unique<EntityManager>();
    m_ComponentRegistry = std::make_unique<ComponentRegistry>();
    m_HierarchyManager = std::make_unique<HierarchyManager>();

    RegisterComponent<Transform>();
    RegisterComponent<MeshRenderer>();
    RegisterComponent<Renderable>();
    RegisterComponent<Rotator>();
    RegisterComponent<Camera>();
    RegisterComponent<Light>();

    std::cout << "Renderable component registered" << std::endl;

    m_TransformSystem = std::unique_ptr<TransformSystem, TransformSystemDeleter>(
        new TransformSystem(m_ComponentRegistry.get(), m_HierarchyManager.get()));
}

void ECSCoordinator::Shutdown() {
    m_TransformSystem.reset();
    m_HierarchyManager.reset();
    m_ComponentRegistry.reset();
    m_EntityManager.reset();
}

void ECSCoordinator::Update(float deltaTime) {
    if (m_TransformSystem) {
        m_TransformSystem->Update(deltaTime);
    }
}

ECSCoordinator::~ECSCoordinator() = default;
