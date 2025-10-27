#include "core/types.h"
#include "ecs/systems/render_system.h"
#include "ecs/ecs_coordinator.h"
#include "ecs/components/transform.h"
#include "ecs/components/renderable.h"
#include "resources/mesh_manager.h"

#include <iostream>
#include <vector>

static int testsRun = 0;
static int testsPassed = 0;
static int testsFailed = 0;

#define TEST(name) \
    static void name(); \
    static void name##_runner() { \
        testsRun++; \
        std::cout << "Running " << #name << "... "; \
        try { \
            name(); \
            testsPassed++; \
            std::cout << "PASSED" << std::endl; \
        } catch (...) { \
            testsFailed++; \
            std::cout << "FAILED (exception)" << std::endl; \
        } \
    } \
    static void name()

#define ASSERT(expr) \
    if (!(expr)) { \
        std::cout << "FAILED at line " << __LINE__ << ": " << #expr << std::endl; \
        testsFailed++; \
        return; \
    }

class StubRenderSystem : public RenderSystem {
public:
    explicit StubRenderSystem(ECSCoordinator* ecs)
        : RenderSystem(ecs, nullptr) {
    }

    const std::vector<MeshHandle>& GetLoadOrder() const { return m_LoadOrder; }

protected:
    std::unique_ptr<VulkanMesh> CreateVulkanMesh(MeshHandle handle, const MeshData* meshData) override {
        (void)meshData;
        m_LoadOrder.push_back(handle);
        return std::make_unique<VulkanMesh>();
    }

private:
    std::vector<MeshHandle> m_LoadOrder;
};

static MeshHandle CreateTestMesh() {
    return MeshManager::Instance().CreateCube();
}

static void DestroyTestMesh(MeshHandle handle) {
    MeshManager::Instance().Destroy(handle);
}

TEST(RenderSystem_UpdateCollectsRenderableEntities) {
    ECSCoordinator ecs;
    ecs.Init();

    StubRenderSystem renderSystem(&ecs);

    Entity entity = ecs.CreateEntity();

    Transform transform;
    transform.localPosition = Vec3(1.0f, 2.0f, 3.0f);
    ecs.AddComponent(entity, transform);

    MeshHandle meshHandle = CreateTestMesh();

    Renderable renderable;
    renderable.mesh = meshHandle;
    ecs.AddComponent(entity, renderable);

    ecs.Update(0.0f);
    renderSystem.Update();

    const auto& renderData = renderSystem.GetRenderData();
    ASSERT(renderData.size() == 1);
    ASSERT(renderData[0].meshHandle == meshHandle);

    Transform& storedTransform = ecs.GetComponent<Transform>(entity);
    ASSERT(renderData[0].modelMatrix == storedTransform.worldMatrix);

    renderSystem.Update();
    ASSERT(renderSystem.GetLoadOrder().size() == 1);

    renderSystem.Shutdown();
    ecs.Shutdown();
    DestroyTestMesh(meshHandle);
}

TEST(RenderSystem_SkipsInvisibleOrInvalid) {
    ECSCoordinator ecs;
    ecs.Init();

    StubRenderSystem renderSystem(&ecs);

    MeshHandle meshHandle = CreateTestMesh();

    Entity visibleEntity = ecs.CreateEntity();
    ecs.AddComponent(visibleEntity, Transform{});
    Renderable visibleRenderable;
    visibleRenderable.mesh = meshHandle;
    ecs.AddComponent(visibleEntity, visibleRenderable);

    Entity invisibleEntity = ecs.CreateEntity();
    ecs.AddComponent(invisibleEntity, Transform{});
    Renderable invisibleRenderable;
    invisibleRenderable.mesh = meshHandle;
    invisibleRenderable.visible = false;
    ecs.AddComponent(invisibleEntity, invisibleRenderable);

    Entity invalidEntity = ecs.CreateEntity();
    ecs.AddComponent(invalidEntity, Transform{});
    Renderable invalidRenderable;
    invalidRenderable.mesh = MeshHandle::Invalid;
    ecs.AddComponent(invalidEntity, invalidRenderable);

    ecs.Update(0.0f);
    renderSystem.Update();

    const auto& renderData = renderSystem.GetRenderData();
    ASSERT(renderData.size() == 1);
    ASSERT(renderData[0].meshHandle == meshHandle);

    renderSystem.Shutdown();
    ecs.Shutdown();
    DestroyTestMesh(meshHandle);
}

TEST(RenderSystem_LoadsEachMeshOnce) {
    ECSCoordinator ecs;
    ecs.Init();

    StubRenderSystem renderSystem(&ecs);

    MeshHandle meshHandleA = CreateTestMesh();
    MeshHandle meshHandleB = CreateTestMesh();

    Entity entityA = ecs.CreateEntity();
    ecs.AddComponent(entityA, Transform{});
    Renderable renderableA;
    renderableA.mesh = meshHandleA;
    ecs.AddComponent(entityA, renderableA);

    Entity entityB = ecs.CreateEntity();
    ecs.AddComponent(entityB, Transform{});
    Renderable renderableB;
    renderableB.mesh = meshHandleB;
    ecs.AddComponent(entityB, renderableB);

    ecs.Update(0.0f);
    renderSystem.Update();
    renderSystem.Update();

    ASSERT(renderSystem.GetLoadOrder().size() == 2);
    ASSERT(renderSystem.GetLoadOrder()[0] == meshHandleA);
    ASSERT(renderSystem.GetLoadOrder()[1] == meshHandleB);

    renderSystem.Shutdown();
    ecs.Shutdown();
    DestroyTestMesh(meshHandleA);
    DestroyTestMesh(meshHandleB);
}

TEST(RenderSystem_CollectsMultipleTransformsUnique) {
    ECSCoordinator ecs;
    ecs.Init();

    StubRenderSystem renderSystem(&ecs);

    MeshHandle meshHandle = CreateTestMesh();

    const Vec3 positions[] = {
        Vec3(0.0f, 0.0f, 0.0f),
        Vec3(5.0f, 0.0f, 0.0f),
        Vec3(0.0f, 0.0f, 5.0f)
    };

    for (const Vec3& pos : positions) {
        Entity entity = ecs.CreateEntity();
        Transform transform;
        transform.localPosition = pos;
        transform.MarkDirty();
        ecs.AddComponent(entity, transform);

        Renderable renderable;
        renderable.mesh = meshHandle;
        ecs.AddComponent(entity, renderable);
    }

    ecs.Update(0.0f);
    renderSystem.Update();

    const auto& renderData = renderSystem.GetRenderData();
    ASSERT(renderData.size() == ARRAY_COUNT(positions));

    bool translationsDiffer = false;
    for (size_t i = 0; i < renderData.size() && !translationsDiffer; ++i) {
        for (size_t j = i + 1; j < renderData.size() && !translationsDiffer; ++j) {
            const Mat4& a = renderData[i].modelMatrix;
            const Mat4& b = renderData[j].modelMatrix;
            if (a[3][0] != b[3][0] || a[3][1] != b[3][1] || a[3][2] != b[3][2]) {
                translationsDiffer = true;
            }
        }
    }
    ASSERT(translationsDiffer);

    renderSystem.Shutdown();
    ecs.Shutdown();
    DestroyTestMesh(meshHandle);
}

int main() {
    std::cout << "=== RenderSystem Tests ===" << std::endl << std::endl;

    RenderSystem_UpdateCollectsRenderableEntities_runner();
    RenderSystem_SkipsInvisibleOrInvalid_runner();
    RenderSystem_LoadsEachMeshOnce_runner();
    RenderSystem_CollectsMultipleTransformsUnique_runner();

    std::cout << std::endl;
    std::cout << "================================" << std::endl;
    std::cout << "Tests run: " << testsRun << std::endl;
    std::cout << "Tests passed: " << testsPassed << std::endl;
    std::cout << "Tests failed: " << testsFailed << std::endl;
    std::cout << "================================" << std::endl;

    return testsFailed == 0 ? 0 : 1;
}
