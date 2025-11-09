#include "entity_inspector.h"
#include "ecs/components/name.h"
#include "ecs/components/transform.h"
#include "ecs/components/renderable.h"
#include "ecs/components/light.h"
#include "ecs/components/camera.h"
#include "ecs/components/rotator.h"
#include "resources/mesh_manager.h"
#include "resources/material_manager.h"

#include <imgui.h>
#include <cmath>

EntityInspector::EntityInspector(ECSCoordinator* ecs)
    : m_ECS(ecs) {
}

EntityInspector::~EntityInspector() = default;

void EntityInspector::Render(Entity entity) {
    if (!entity.IsValid()) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No entity selected");
        return;
    }

    // Render all components
    RenderNameComponent(entity);
    RenderTransformComponent(entity);
    RenderRenderableComponent(entity);
    RenderLightComponent(entity);
    RenderCameraComponent(entity);
    RenderRotatorComponent(entity);

    // Add component menu at bottom
    RenderAddComponentMenu(entity);
}

void EntityInspector::RenderNameComponent(Entity entity) {
    // Name component is optional, but always show name field
    bool hasName = m_ECS->HasComponent<Name>(entity);

    ImGui::PushID("NameComponent");

    ImGui::Text("Entity %d:%d", entity.index, entity.generation);
    ImGui::Separator();

    char nameBuf[Name::MaxLength];
    if (hasName) {
        Name& name = m_ECS->GetMutableComponent<Name>(entity);
        std::strncpy(nameBuf, name.GetName(), Name::MaxLength);
    } else {
        std::strcpy(nameBuf, "Entity");
    }

    ImGui::PushItemWidth(-1);
    if (ImGui::InputText("##Name", nameBuf, Name::MaxLength)) {
        if (hasName) {
            Name& name = m_ECS->GetMutableComponent<Name>(entity);
            name.SetName(nameBuf);
        } else {
            // Add name component if user starts typing
            m_ECS->AddComponent(entity, Name(nameBuf));
        }
    }
    ImGui::PopItemWidth();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PopID();
}

void EntityInspector::RenderTransformComponent(Entity entity) {
    if (!m_ECS->HasComponent<Transform>(entity)) {
        return;
    }

    Transform& transform = m_ECS->GetMutableComponent<Transform>(entity);

    bool removeRequested = false;
    if (BeginComponentHeader("Transform", false, &removeRequested)) {
        // Position
        if (ImGui::DragFloat3("Position", &transform.localPosition.x, 0.1f)) {
            transform.isDirty = true;
        }

        // Rotation - convert quaternion to Euler angles for editing
        Vec3 euler = QuatToEuler(transform.localRotation);
        euler = euler * (180.0f / 3.14159265359f);  // Convert to degrees

        if (ImGui::DragFloat3("Rotation", &euler.x, 1.0f)) {
            euler = euler * (3.14159265359f / 180.0f);  // Convert back to radians
            transform.localRotation = QuatFromEuler(euler);
            transform.isDirty = true;
        }

        // Scale
        if (ImGui::DragFloat3("Scale", &transform.localScale.x, 0.1f, 0.001f)) {
            transform.isDirty = true;
        }

        EndComponentHeader();
    }

    // Transform cannot be removed
    if (removeRequested) {
        ImGui::OpenPopup("Cannot Remove Transform");
    }

    if (ImGui::BeginPopupModal("Cannot Remove Transform", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Transform component is required and cannot be removed.");
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void EntityInspector::RenderRenderableComponent(Entity entity) {
    if (!m_ECS->HasComponent<Renderable>(entity)) {
        return;
    }

    Renderable& renderable = m_ECS->GetMutableComponent<Renderable>(entity);

    bool removeRequested = false;
    if (BeginComponentHeader("Renderable", true, &removeRequested)) {
        // Mesh display
        ImGui::Text("Mesh:");
        ImGui::SameLine();
        if (renderable.mesh.IsValid()) {
            std::string meshPath = MeshManager::Instance().GetPath(renderable.mesh);
            if (meshPath.empty()) {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Loaded (no path)");
            } else {
                // Extract filename from path
                size_t lastSlash = meshPath.find_last_of("/\\");
                std::string filename = (lastSlash != std::string::npos)
                    ? meshPath.substr(lastSlash + 1)
                    : meshPath;
                ImGui::TextColored(ImVec4(0.7f, 1.0f, 0.7f, 1.0f), "%s", filename.c_str());
            }
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "None");
        }

        // Material display
        ImGui::Text("Material:");
        ImGui::SameLine();
        if (renderable.material.IsValid()) {
            std::string materialPath = MaterialManager::Instance().GetPath(renderable.material);
            if (materialPath.empty()) {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Loaded (no path)");
            } else {
                size_t lastSlash = materialPath.find_last_of("/\\");
                std::string filename = (lastSlash != std::string::npos)
                    ? materialPath.substr(lastSlash + 1)
                    : materialPath;
                ImGui::TextColored(ImVec4(0.7f, 1.0f, 0.7f, 1.0f), "%s", filename.c_str());
            }
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "None");
        }

        ImGui::Spacing();

        // Flags
        ImGui::Checkbox("Visible", &renderable.visible);
        ImGui::Checkbox("Cast Shadows", &renderable.castsShadows);

        EndComponentHeader();
    }

    if (removeRequested) {
        m_ECS->RemoveComponent<Renderable>(entity);
    }
}

void EntityInspector::RenderLightComponent(Entity entity) {
    if (!m_ECS->HasComponent<Light>(entity)) {
        return;
    }

    Light& light = m_ECS->GetMutableComponent<Light>(entity);

    bool removeRequested = false;
    if (BeginComponentHeader("Light", true, &removeRequested)) {
        // Light type dropdown
        const char* lightTypes[] = {"Directional", "Point", "Spot", "Area", "Tube", "Hemisphere"};
        int typeIndex = static_cast<int>(light.type);
        if (ImGui::Combo("Type", &typeIndex, lightTypes, IM_ARRAYSIZE(lightTypes))) {
            light.type = static_cast<LightType>(typeIndex);
        }

        // Color picker
        ImGui::ColorEdit3("Color", &light.color.x);

        // Intensity
        ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 100.0f);

        // Type-specific parameters
        if (light.type == LightType::Point || light.type == LightType::Spot) {
            ImGui::DragFloat("Range", &light.range, 0.5f, 0.1f, 1000.0f);
            ImGui::DragFloat("Attenuation", &light.attenuation, 0.1f, 0.0f, 10.0f);
        }

        if (light.type == LightType::Spot) {
            ImGui::DragFloat("Inner Cone Angle", &light.innerConeAngle, 1.0f, 0.0f, 90.0f);
            ImGui::DragFloat("Outer Cone Angle", &light.outerConeAngle, 1.0f, 0.0f, 90.0f);

            // Clamp outer to be >= inner
            if (light.outerConeAngle < light.innerConeAngle) {
                light.outerConeAngle = light.innerConeAngle;
            }
        }

        if (light.type == LightType::Area) {
            ImGui::DragFloat("Width", &light.width, 0.1f, 0.1f, 100.0f);
            ImGui::DragFloat("Height", &light.height, 0.1f, 0.1f, 100.0f);
            ImGui::Checkbox("Two Sided", &light.twoSided);
        }

        if (light.type == LightType::Tube) {
            ImGui::DragFloat("Length", &light.tubeLength, 0.1f, 0.1f, 100.0f);
            ImGui::DragFloat("Radius", &light.tubeRadius, 0.01f, 0.01f, 10.0f);
        }

        if (light.type == LightType::Hemisphere) {
            ImGui::ColorEdit3("Sky Color", &light.skyColor.x);
            ImGui::ColorEdit3("Ground Color", &light.groundColor.x);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Shadow settings
        ImGui::Checkbox("Cast Shadows", &light.castsShadows);

        if (light.castsShadows) {
            const char* filterModes[] = {"PCF", "PCSS", "Contact Hardening", "EVSM"};
            int filterIndex = static_cast<int>(light.shadowFilterMode);
            if (ImGui::Combo("Shadow Filter", &filterIndex, filterModes, IM_ARRAYSIZE(filterModes))) {
                light.shadowFilterMode = static_cast<ShadowFilterMode>(filterIndex);
            }

            if (light.shadowFilterMode == ShadowFilterMode::PCSS ||
                light.shadowFilterMode == ShadowFilterMode::ContactHardening) {
                ImGui::DragFloat("Search Radius", &light.shadowSearchRadius, 0.5f, 0.1f, 50.0f);
            }

            if (light.shadowFilterMode == ShadowFilterMode::EVSM) {
                ImGui::DragFloat("Positive Exponent", &light.evsmPositiveExponent, 1.0f, 1.0f, 100.0f);
                ImGui::DragFloat("Negative Exponent", &light.evsmNegativeExponent, 1.0f, 1.0f, 100.0f);
                ImGui::DragFloat("Light Bleed Reduction", &light.evsmLightBleedReduction, 0.01f, 0.0f, 1.0f);
            }
        }

        EndComponentHeader();
    }

    if (removeRequested) {
        m_ECS->RemoveComponent<Light>(entity);
    }
}

void EntityInspector::RenderCameraComponent(Entity entity) {
    if (!m_ECS->HasComponent<Camera>(entity)) {
        return;
    }

    Camera& camera = m_ECS->GetMutableComponent<Camera>(entity);

    bool removeRequested = false;
    if (BeginComponentHeader("Camera", true, &removeRequested)) {
        // Projection type
        const char* projTypes[] = {"Perspective", "Orthographic"};
        int projIndex = static_cast<int>(camera.projection);
        if (ImGui::Combo("Projection", &projIndex, projTypes, IM_ARRAYSIZE(projTypes))) {
            camera.projection = static_cast<CameraProjection>(projIndex);
        }

        // Perspective parameters
        if (camera.projection == CameraProjection::Perspective) {
            ImGui::DragFloat("FOV", &camera.fov, 1.0f, 1.0f, 179.0f);
        }

        // Orthographic parameters
        if (camera.projection == CameraProjection::Orthographic) {
            ImGui::DragFloat("Ortho Size", &camera.orthoSize, 0.5f, 0.1f, 1000.0f);
        }

        // Common parameters
        ImGui::DragFloat("Near Plane", &camera.nearPlane, 0.01f, 0.001f, camera.farPlane - 0.1f);
        ImGui::DragFloat("Far Plane", &camera.farPlane, 1.0f, camera.nearPlane + 0.1f, 10000.0f);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Clear color
        ImGui::ColorEdit4("Clear Color", &camera.clearColor.r);

        // Active flag
        ImGui::Checkbox("Active", &camera.isActive);

        // Show if editor camera
        if (camera.isEditorCamera) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 1.0f, 1.0f), "(Editor Camera)");
        }

        EndComponentHeader();
    }

    if (removeRequested) {
        // Don't remove if it's the editor camera
        if (!camera.isEditorCamera) {
            m_ECS->RemoveComponent<Camera>(entity);
        } else {
            ImGui::OpenPopup("Cannot Remove Editor Camera");
        }
    }

    if (ImGui::BeginPopupModal("Cannot Remove Editor Camera", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Cannot remove the editor camera component.");
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void EntityInspector::RenderRotatorComponent(Entity entity) {
    if (!m_ECS->HasComponent<Rotator>(entity)) {
        return;
    }

    Rotator& rotator = m_ECS->GetMutableComponent<Rotator>(entity);

    bool removeRequested = false;
    if (BeginComponentHeader("Rotator", true, &removeRequested)) {
        ImGui::DragFloat3("Axis", &rotator.axis.x, 0.01f, -1.0f, 1.0f);
        ImGui::DragFloat("Speed", &rotator.speed, 0.1f, -360.0f, 360.0f);

        // Normalize axis button
        if (ImGui::Button("Normalize Axis")) {
            rotator.axis = Normalize(rotator.axis);
        }

        EndComponentHeader();
    }

    if (removeRequested) {
        m_ECS->RemoveComponent<Rotator>(entity);
    }
}

void EntityInspector::RenderAddComponentMenu(Entity entity) {
    ImGui::Spacing();
    ImGui::Spacing();

    if (ImGui::Button("Add Component", ImVec2(-1, 0))) {
        ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup")) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Select component to add:");
        ImGui::Separator();

        if (!m_ECS->HasComponent<Light>(entity) && ImGui::MenuItem("Light")) {
            m_ECS->AddComponent(entity, Light{});
        }

        if (!m_ECS->HasComponent<Camera>(entity) && ImGui::MenuItem("Camera")) {
            m_ECS->AddComponent(entity, Camera{});
        }

        if (!m_ECS->HasComponent<Renderable>(entity) && ImGui::MenuItem("Renderable")) {
            m_ECS->AddComponent(entity, Renderable{});
        }

        if (!m_ECS->HasComponent<Rotator>(entity) && ImGui::MenuItem("Rotator")) {
            m_ECS->AddComponent(entity, Rotator{});
        }

        ImGui::EndPopup();
    }
}

bool EntityInspector::BeginComponentHeader(const char* name, bool canRemove, bool* removeRequested) {
    ImGui::PushID(name);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen |
                               ImGuiTreeNodeFlags_Framed |
                               ImGuiTreeNodeFlags_AllowItemOverlap |
                               ImGuiTreeNodeFlags_SpanAvailWidth;

    bool open = ImGui::CollapsingHeader(name, flags);

    // Remove button (if allowed)
    if (canRemove) {
        ImGui::SameLine(ImGui::GetWindowWidth() - 35);
        if (ImGui::SmallButton("X")) {
            *removeRequested = true;
        }
    }

    if (open) {
        ImGui::Indent(10.0f);
        ImGui::Spacing();
    }

    return open;
}

void EntityInspector::EndComponentHeader() {
    ImGui::Spacing();
    ImGui::Unindent(10.0f);
    ImGui::Spacing();
    ImGui::PopID();
}
