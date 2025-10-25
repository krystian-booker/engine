#pragma once
#include "ecs/component_registry.h"
#include "ecs/components/transform.h"
#include "core/job_system.h"

class TransformSystem {
public:
    TransformSystem(ComponentRegistry* registry) : m_Registry(registry) {}

    // Update all transforms (called each frame)
    void Update(float deltaTime) {
        (void)deltaTime;  // Unused for now

        auto transforms = m_Registry->GetComponentArray<Transform>();

        // For now: simple linear update (no hierarchy)
        // Later: We'll add hierarchy traversal
        for (size_t i = 0; i < transforms->Size(); ++i) {
            Transform& t = transforms->Data()[i];

            if (t.isDirty) {
                if (t.parent.IsValid()) {
                    // TODO: Get parent's world matrix
                    // t.worldMatrix = parentWorld * t.GetLocalMatrix();
                    t.worldMatrix = t.GetLocalMatrix();
                } else {
                    // Root entity - local is world
                    t.worldMatrix = t.GetLocalMatrix();
                }

                t.isDirty = false;
            }
        }
    }

private:
    ComponentRegistry* m_Registry;
};
