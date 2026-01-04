#include <engine/debug-gui/debug_entity_inspector.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <engine/reflect/type_registry.hpp>
#include <engine/core/input.hpp>
#include <engine/core/math.hpp>

#include <imgui.h>
#include <cstring>
#include <cstdio>

namespace engine::debug_gui {

uint32_t DebugEntityInspector::get_shortcut_key() const {
    return static_cast<uint32_t>(core::Key::F4);
}

void DebugEntityInspector::draw() {
    ImGui::SetNextWindowSize(ImVec2(450, 600), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin(get_title(), &m_open)) {
        ImGui::End();
        return;
    }

    if (!m_world) {
        ImGui::Text("No world attached");
        ImGui::End();
        return;
    }

    // Split view: hierarchy on left, inspector on right
    ImGui::Columns(2, "inspector_columns", true);

    // Hierarchy panel
    ImGui::BeginChild("Hierarchy", ImVec2(0, 0), true);
    ImGui::InputTextWithHint("##Search", "Search entities...", m_search_filter, sizeof(m_search_filter));
    ImGui::Checkbox("Show hidden", &m_show_hidden);
    ImGui::Separator();
    draw_hierarchy();
    ImGui::EndChild();

    ImGui::NextColumn();

    // Inspector panel
    ImGui::BeginChild("Inspector", ImVec2(0, 0), true);
    draw_inspector();
    ImGui::EndChild();

    ImGui::Columns(1);

    ImGui::End();
}

void DebugEntityInspector::draw_hierarchy() {
    if (!m_world) return;

    // Get root entities
    auto roots = scene::get_root_entities(*m_world);

    for (auto entity : roots) {
        draw_entity_node(entity);
    }

    // Also show entities without hierarchy component
    auto view = m_world->view<scene::EntityInfo>();
    for (auto entity : view) {
        if (!m_world->has<scene::Hierarchy>(entity)) {
            draw_entity_node(entity);
        }
    }
}

void DebugEntityInspector::draw_entity_node(scene::Entity entity) {
    if (!m_world || !m_world->valid(entity)) return;

    // Get entity info
    auto* info = m_world->try_get<scene::EntityInfo>(entity);
    std::string name = info ? info->name : "";
    if (name.empty()) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Entity %u", static_cast<uint32_t>(entity));
        name = buf;
    }

    // Filter check
    if (m_search_filter[0] != '\0') {
        if (name.find(m_search_filter) == std::string::npos) {
            return;
        }
    }

    // Hidden entity check
    if (!m_show_hidden && info && !info->enabled) {
        return;
    }

    // Get children
    auto* hierarchy = m_world->try_get<scene::Hierarchy>(entity);
    bool has_children = hierarchy && hierarchy->first_child != scene::NullEntity;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (m_selected == entity) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    if (!has_children) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    // Dim disabled entities
    bool is_disabled = info && !info->enabled;
    if (is_disabled) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    }

    bool node_open = ImGui::TreeNodeEx(
        reinterpret_cast<void*>(static_cast<uintptr_t>(entity)),
        flags, "%s", name.c_str());

    if (is_disabled) {
        ImGui::PopStyleColor();
    }

    // Selection
    if (ImGui::IsItemClicked()) {
        m_selected = entity;
    }

    // Context menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Delete")) {
            m_world->destroy(entity);
            if (m_selected == entity) {
                m_selected = scene::NullEntity;
            }
        }
        ImGui::EndPopup();
    }

    // Draw children
    if (node_open) {
        if (hierarchy) {
            scene::Entity child = hierarchy->first_child;
            while (child != scene::NullEntity) {
                draw_entity_node(child);
                auto* child_hierarchy = m_world->try_get<scene::Hierarchy>(child);
                child = child_hierarchy ? child_hierarchy->next_sibling : scene::NullEntity;
            }
        }
        ImGui::TreePop();
    }
}

void DebugEntityInspector::draw_inspector() {
    if (m_selected == scene::NullEntity || !m_world || !m_world->valid(m_selected)) {
        ImGui::Text("No entity selected");
        return;
    }

    // Entity header
    auto* info = m_world->try_get<scene::EntityInfo>(m_selected);
    char name_buffer[256];
    if (info) {
        strncpy(name_buffer, info->name.c_str(), sizeof(name_buffer) - 1);
        name_buffer[sizeof(name_buffer) - 1] = '\0';
        if (ImGui::InputText("Name", name_buffer, sizeof(name_buffer))) {
            info->name = name_buffer;
        }
        ImGui::Checkbox("Enabled", &info->enabled);
    }

    ImGui::Text("Entity ID: %u", static_cast<uint32_t>(m_selected));
    ImGui::Separator();

    // Components
    auto& registry = reflect::TypeRegistry::instance();
    auto component_names = registry.get_all_component_names();

    for (const auto& comp_name : component_names) {
        // Check if entity has this component
        auto comp_any = registry.get_component_any(m_world->registry(), m_selected, comp_name);
        if (!comp_any) continue;

        // Get type info for display
        auto* type_info = registry.get_type_info(comp_name);
        const char* display_name = (type_info && !type_info->meta.display_name.empty())
            ? type_info->meta.display_name.c_str()
            : comp_name.c_str();

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed;
        if (ImGui::TreeNodeEx(comp_name.c_str(), flags, "%s", display_name)) {
            draw_component(comp_name);
            ImGui::TreePop();
        }

        // Component context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Remove Component")) {
                registry.remove_component_any(m_world->registry(), m_selected, comp_name);
            }
            ImGui::EndPopup();
        }
    }

    // Add component button
    ImGui::Separator();
    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup")) {
        for (const auto& comp_name : component_names) {
            // Skip if already has component
            auto existing = registry.get_component_any(m_world->registry(), m_selected, comp_name);
            if (existing) continue;

            if (ImGui::MenuItem(comp_name.c_str())) {
                registry.add_component_any(m_world->registry(), m_selected, comp_name);
            }
        }
        ImGui::EndPopup();
    }
}

void DebugEntityInspector::draw_component(const std::string& type_name) {
    auto& registry = reflect::TypeRegistry::instance();
    auto* type_info = registry.get_type_info(type_name);
    if (!type_info) return;

    // Get the component as meta_any
    auto comp_any = registry.get_component_any(m_world->registry(), m_selected, type_name);
    if (!comp_any) return;

    for (const auto& prop : type_info->properties) {
        if (prop.meta.hidden) continue;
        draw_property_editor(prop, comp_any);
    }
}

void DebugEntityInspector::draw_property_editor(const reflect::PropertyInfo& prop, entt::meta_any& comp_any) {
    const char* label = prop.meta.display_name.empty() ? prop.name.c_str() : prop.meta.display_name.c_str();

    // Get property value using the getter
    auto value = prop.getter(comp_any);
    if (!value) {
        ImGui::TextDisabled("%s: (no value)", label);
        return;
    }

    auto type_id = prop.type.id();

    // Create unique ID for this property to avoid ImGui ID conflicts
    ImGui::PushID(prop.name.c_str());

    // Edit based on type
    if (type_id == entt::type_hash<bool>::value()) {
        bool v = value.cast<bool>();
        if (ImGui::Checkbox(label, &v)) {
            prop.setter(comp_any, entt::meta_any{v});
        }
    }
    else if (type_id == entt::type_hash<float>::value()) {
        float v = value.cast<float>();
        if (prop.meta.is_angle) {
            float degrees = glm::degrees(v);
            if (ImGui::DragFloat(label, &degrees, 0.5f, -360.0f, 360.0f, "%.1f deg")) {
                prop.setter(comp_any, entt::meta_any{glm::radians(degrees)});
            }
        } else {
            if (ImGui::DragFloat(label, &v, 0.01f, 0.0f, 0.0f, "%.3f")) {
                prop.setter(comp_any, entt::meta_any{v});
            }
        }
    }
    else if (type_id == entt::type_hash<double>::value()) {
        double v = value.cast<double>();
        float fv = static_cast<float>(v);
        if (ImGui::DragFloat(label, &fv, 0.01f, 0.0f, 0.0f, "%.6f")) {
            prop.setter(comp_any, entt::meta_any{static_cast<double>(fv)});
        }
    }
    else if (type_id == entt::type_hash<int32_t>::value()) {
        int32_t v = value.cast<int32_t>();
        if (ImGui::DragInt(label, &v, 1.0f)) {
            prop.setter(comp_any, entt::meta_any{v});
        }
    }
    else if (type_id == entt::type_hash<uint32_t>::value()) {
        uint32_t v = value.cast<uint32_t>();
        if (ImGui::DragScalar(label, ImGuiDataType_U32, &v, 1.0f)) {
            prop.setter(comp_any, entt::meta_any{v});
        }
    }
    else if (type_id == entt::type_hash<int64_t>::value()) {
        int64_t v = value.cast<int64_t>();
        if (ImGui::DragScalar(label, ImGuiDataType_S64, &v, 1.0f)) {
            prop.setter(comp_any, entt::meta_any{v});
        }
    }
    else if (type_id == entt::type_hash<uint64_t>::value()) {
        uint64_t v = value.cast<uint64_t>();
        if (ImGui::DragScalar(label, ImGuiDataType_U64, &v, 1.0f)) {
            prop.setter(comp_any, entt::meta_any{v});
        }
    }
    else if (type_id == entt::type_hash<std::string>::value()) {
        std::string s = value.cast<std::string>();
        char buffer[256];
        strncpy(buffer, s.c_str(), sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
        if (ImGui::InputText(label, buffer, sizeof(buffer))) {
            prop.setter(comp_any, entt::meta_any{std::string(buffer)});
        }
    }
    else if (type_id == entt::type_hash<core::Vec2>::value()) {
        core::Vec2 v = value.cast<core::Vec2>();
        if (ImGui::DragFloat2(label, &v.x, 0.01f)) {
            prop.setter(comp_any, entt::meta_any{v});
        }
    }
    else if (type_id == entt::type_hash<core::Vec3>::value()) {
        core::Vec3 v = value.cast<core::Vec3>();
        if (prop.meta.is_color) {
            if (ImGui::ColorEdit3(label, &v.x, ImGuiColorEditFlags_Float)) {
                prop.setter(comp_any, entt::meta_any{v});
            }
        } else {
            if (ImGui::DragFloat3(label, &v.x, 0.01f)) {
                prop.setter(comp_any, entt::meta_any{v});
            }
        }
    }
    else if (type_id == entt::type_hash<core::Vec4>::value()) {
        core::Vec4 v = value.cast<core::Vec4>();
        if (prop.meta.is_color) {
            if (ImGui::ColorEdit4(label, &v.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar)) {
                prop.setter(comp_any, entt::meta_any{v});
            }
        } else {
            if (ImGui::DragFloat4(label, &v.x, 0.01f)) {
                prop.setter(comp_any, entt::meta_any{v});
            }
        }
    }
    else if (type_id == entt::type_hash<core::Quat>::value()) {
        core::Quat q = value.cast<core::Quat>();
        core::Vec3 euler = glm::degrees(glm::eulerAngles(q));
        if (ImGui::DragFloat3(label, &euler.x, 0.5f, -180.0f, 180.0f, "%.1f deg")) {
            core::Quat new_q = core::Quat(glm::radians(euler));
            prop.setter(comp_any, entt::meta_any{new_q});
        }
    }
    else {
        ImGui::TextDisabled("%s: (unsupported type)", label);
    }

    // Tooltip
    if (!prop.meta.tooltip.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", prop.meta.tooltip.c_str());
    }

    ImGui::PopID();
}

} // namespace engine::debug_gui
