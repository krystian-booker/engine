#include <engine/render/material_instance.hpp>
#include <engine/render/renderer.hpp>
#include <engine/core/log.hpp>
#include <bgfx/bgfx.h>
#include <algorithm>

namespace engine::render {

using namespace engine::core;

// Global instance
static MaterialInstanceManager s_material_instance_manager;

MaterialInstanceManager& get_material_instance_manager() {
    return s_material_instance_manager;
}

// ============================================================================
// MaterialInstance implementation
// ============================================================================

void MaterialInstance::set_float(const std::string& name, float value) {
    for (auto& override : overrides) {
        if (override.name == name) {
            override.value = value;
            return;
        }
    }
    overrides.push_back({name, value});
}

void MaterialInstance::set_vec2(const std::string& name, const Vec2& value) {
    for (auto& override : overrides) {
        if (override.name == name) {
            override.value = value;
            return;
        }
    }
    overrides.push_back({name, value});
}

void MaterialInstance::set_vec3(const std::string& name, const Vec3& value) {
    for (auto& override : overrides) {
        if (override.name == name) {
            override.value = value;
            return;
        }
    }
    overrides.push_back({name, value});
}

void MaterialInstance::set_vec4(const std::string& name, const Vec4& value) {
    for (auto& override : overrides) {
        if (override.name == name) {
            override.value = value;
            return;
        }
    }
    overrides.push_back({name, value});
}

void MaterialInstance::set_texture(const std::string& name, TextureHandle texture) {
    for (auto& override : overrides) {
        if (override.name == name) {
            override.value = texture;
            return;
        }
    }
    overrides.push_back({name, texture});
}

const MaterialParamValue* MaterialInstance::get_parameter(const std::string& name) const {
    for (const auto& override : overrides) {
        if (override.name == name) {
            return &override.value;
        }
    }
    return nullptr;
}

void MaterialInstance::clear_overrides() {
    overrides.clear();
}

// ============================================================================
// MaterialInstanceManager implementation
// ============================================================================

MaterialInstanceManager::~MaterialInstanceManager() {
    if (m_initialized) {
        shutdown();
    }
}

void MaterialInstanceManager::init(IRenderer* renderer) {
    m_renderer = renderer;
    m_initialized = true;

    log(LogLevel::Info, "Material instance manager initialized");
}

void MaterialInstanceManager::shutdown() {
    if (!m_initialized) return;

    // Destroy cached uniform handles
    for (const auto& [name, info] : m_uniform_cache) {
        bgfx::UniformHandle handle = { static_cast<uint16_t>(info.handle) };
        if (bgfx::isValid(handle)) {
            bgfx::destroy(handle);
        }
    }
    m_uniform_cache.clear();

    // Destroy cached sampler handles
    for (const auto& [name, info] : m_sampler_cache) {
        bgfx::UniformHandle handle = { static_cast<uint16_t>(info.sampler_handle) };
        if (bgfx::isValid(handle)) {
            bgfx::destroy(handle);
        }
    }
    m_sampler_cache.clear();
    m_next_texture_slot = 4;

    m_instances.clear();
    m_instance_count = 0;
    m_initialized = false;
    m_renderer = nullptr;

    log(LogLevel::Info, "Material instance manager shutdown");
}

MaterialInstanceHandle MaterialInstanceManager::create(MaterialHandle base_material) {
    if (!base_material.valid()) {
        return MaterialInstanceHandle{};
    }

    uint32_t id = m_next_id++;
    m_instances[id] = MaterialInstance{base_material, {}};
    m_instance_count++;

    log(LogLevel::Debug, "Created material instance {} from base material {}",
        id, base_material.id);

    return MaterialInstanceHandle{id};
}

MaterialInstanceHandle MaterialInstanceManager::clone(MaterialInstanceHandle source) {
    auto* src_instance = get(source);
    if (!src_instance) {
        return MaterialInstanceHandle{};
    }

    uint32_t id = m_next_id++;
    m_instances[id] = *src_instance;  // Copy
    m_instance_count++;

    log(LogLevel::Debug, "Cloned material instance {} from {}", id, source.id);

    return MaterialInstanceHandle{id};
}

void MaterialInstanceManager::destroy(MaterialInstanceHandle handle) {
    auto it = m_instances.find(handle.id);
    if (it != m_instances.end()) {
        m_instances.erase(it);
        m_instance_count--;

        log(LogLevel::Debug, "Destroyed material instance {}", handle.id);
    }
}

MaterialInstance* MaterialInstanceManager::get(MaterialInstanceHandle handle) {
    auto it = m_instances.find(handle.id);
    if (it != m_instances.end()) {
        return &it->second;
    }
    return nullptr;
}

const MaterialInstance* MaterialInstanceManager::get(MaterialInstanceHandle handle) const {
    auto it = m_instances.find(handle.id);
    if (it != m_instances.end()) {
        return &it->second;
    }
    return nullptr;
}

void MaterialInstanceManager::set_float(MaterialInstanceHandle handle, const std::string& name, float value) {
    if (auto* instance = get(handle)) {
        instance->set_float(name, value);
    }
}

void MaterialInstanceManager::set_vec2(MaterialInstanceHandle handle, const std::string& name, const Vec2& value) {
    if (auto* instance = get(handle)) {
        instance->set_vec2(name, value);
    }
}

void MaterialInstanceManager::set_vec3(MaterialInstanceHandle handle, const std::string& name, const Vec3& value) {
    if (auto* instance = get(handle)) {
        instance->set_vec3(name, value);
    }
}

void MaterialInstanceManager::set_vec4(MaterialInstanceHandle handle, const std::string& name, const Vec4& value) {
    if (auto* instance = get(handle)) {
        instance->set_vec4(name, value);
    }
}

void MaterialInstanceManager::set_texture(MaterialInstanceHandle handle, const std::string& name, TextureHandle texture) {
    if (auto* instance = get(handle)) {
        instance->set_texture(name, texture);
    }
}

MaterialHandle MaterialInstanceManager::get_base_material(MaterialInstanceHandle handle) const {
    if (const auto* instance = get(handle)) {
        return instance->base_material;
    }
    return MaterialHandle{};
}

MaterialInstanceManager::UniformInfo MaterialInstanceManager::get_or_create_uniform(
    const std::string& name, uint8_t type) {

    auto it = m_uniform_cache.find(name);
    if (it != m_uniform_cache.end()) {
        return it->second;
    }

    // Create new uniform handle
    bgfx::UniformHandle handle = bgfx::createUniform(
        name.c_str(),
        static_cast<bgfx::UniformType::Enum>(type)
    );

    UniformInfo info;
    info.handle = handle.idx;
    info.type = type;

    m_uniform_cache[name] = info;

    log(LogLevel::Debug, "Created uniform '{}' with handle {}", name, handle.idx);

    return info;
}

MaterialInstanceManager::SamplerInfo MaterialInstanceManager::get_or_create_sampler(
    const std::string& name) {

    auto it = m_sampler_cache.find(name);
    if (it != m_sampler_cache.end()) {
        return it->second;
    }

    // Create new sampler uniform handle
    bgfx::UniformHandle handle = bgfx::createUniform(
        name.c_str(),
        bgfx::UniformType::Sampler
    );

    SamplerInfo info;
    info.sampler_handle = handle.idx;
    info.slot = m_next_texture_slot++;

    // Wrap texture slots to avoid overflow (8-15 are typically for material textures)
    if (m_next_texture_slot > 15) {
        m_next_texture_slot = 8;
    }

    m_sampler_cache[name] = info;

    log(LogLevel::Debug, "Created sampler '{}' with handle {} at slot {}",
        name, handle.idx, info.slot);

    return info;
}

void MaterialInstanceManager::bind(MaterialInstanceHandle handle) {
    auto* instance = get(handle);
    if (!instance || !m_renderer) {
        return;
    }

    // Apply parameter overrides
    // Note: Base material is bound separately via submit_mesh which handles the material handle
    for (const auto& override : instance->overrides) {
        std::visit([this, &override](auto&& value) {
            using T = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<T, float>) {
                // bgfx requires Vec4 for uniforms, pack float into x component
                auto uniform_info = get_or_create_uniform(
                    override.name,
                    static_cast<uint8_t>(bgfx::UniformType::Vec4)
                );
                bgfx::UniformHandle uniform_handle = { static_cast<uint16_t>(uniform_info.handle) };
                float vec4_value[4] = { value, 0.0f, 0.0f, 0.0f };
                bgfx::setUniform(uniform_handle, vec4_value);

            } else if constexpr (std::is_same_v<T, Vec2>) {
                auto uniform_info = get_or_create_uniform(
                    override.name,
                    static_cast<uint8_t>(bgfx::UniformType::Vec4)
                );
                bgfx::UniformHandle uniform_handle = { static_cast<uint16_t>(uniform_info.handle) };
                float vec4_value[4] = { value.x, value.y, 0.0f, 0.0f };
                bgfx::setUniform(uniform_handle, vec4_value);

            } else if constexpr (std::is_same_v<T, Vec3>) {
                auto uniform_info = get_or_create_uniform(
                    override.name,
                    static_cast<uint8_t>(bgfx::UniformType::Vec4)
                );
                bgfx::UniformHandle uniform_handle = { static_cast<uint16_t>(uniform_info.handle) };
                float vec4_value[4] = { value.x, value.y, value.z, 0.0f };
                bgfx::setUniform(uniform_handle, vec4_value);

            } else if constexpr (std::is_same_v<T, Vec4>) {
                auto uniform_info = get_or_create_uniform(
                    override.name,
                    static_cast<uint8_t>(bgfx::UniformType::Vec4)
                );
                bgfx::UniformHandle uniform_handle = { static_cast<uint16_t>(uniform_info.handle) };
                bgfx::setUniform(uniform_handle, &value);

            } else if constexpr (std::is_same_v<T, TextureHandle>) {
                if (value.valid()) {
                    auto sampler_info = get_or_create_sampler(override.name);
                    bgfx::UniformHandle sampler_handle = { static_cast<uint16_t>(sampler_info.sampler_handle) };
                    uint16_t native_handle = m_renderer->get_native_texture_handle(value);
                    if (native_handle != bgfx::kInvalidHandle) {
                        bgfx::TextureHandle tex_handle = { native_handle };
                        bgfx::setTexture(sampler_info.slot, sampler_handle, tex_handle);
                    }
                }
            }
        }, override.value);
    }
}

} // namespace engine::render
