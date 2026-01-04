#pragma once

#include <engine/render/types.hpp>
#include <engine/core/math.hpp>
#include <string>
#include <vector>
#include <variant>
#include <unordered_map>

namespace engine::render {

using namespace engine::core;

class IRenderer;

// Handle for material instances
struct MaterialInstanceHandle {
    uint32_t id = UINT32_MAX;
    bool valid() const { return id != UINT32_MAX; }
};

// Material parameter value types
using MaterialParamValue = std::variant<
    float,
    Vec2,
    Vec3,
    Vec4,
    TextureHandle
>;

// Parameter override in a material instance
struct MaterialParameterOverride {
    std::string name;
    MaterialParamValue value;
};

// Material instance - references a base material with parameter overrides
struct MaterialInstance {
    MaterialHandle base_material;
    std::vector<MaterialParameterOverride> overrides;

    // Quick setters for common parameters
    void set_float(const std::string& name, float value);
    void set_vec2(const std::string& name, const Vec2& value);
    void set_vec3(const std::string& name, const Vec3& value);
    void set_vec4(const std::string& name, const Vec4& value);
    void set_texture(const std::string& name, TextureHandle texture);

    // Get parameter by name (returns nullptr if not overridden)
    const MaterialParamValue* get_parameter(const std::string& name) const;

    // Clear all overrides
    void clear_overrides();
};

// Manager for material instances
class MaterialInstanceManager {
public:
    MaterialInstanceManager() = default;
    ~MaterialInstanceManager();

    void init(IRenderer* renderer);
    void shutdown();

    // Create a new material instance from a base material
    MaterialInstanceHandle create(MaterialHandle base_material);

    // Create a copy of an existing instance
    MaterialInstanceHandle clone(MaterialInstanceHandle source);

    // Destroy a material instance
    void destroy(MaterialInstanceHandle handle);

    // Get the instance data for modification
    MaterialInstance* get(MaterialInstanceHandle handle);
    const MaterialInstance* get(MaterialInstanceHandle handle) const;

    // Set parameters on an instance
    void set_float(MaterialInstanceHandle handle, const std::string& name, float value);
    void set_vec2(MaterialInstanceHandle handle, const std::string& name, const Vec2& value);
    void set_vec3(MaterialInstanceHandle handle, const std::string& name, const Vec3& value);
    void set_vec4(MaterialInstanceHandle handle, const std::string& name, const Vec4& value);
    void set_texture(MaterialInstanceHandle handle, const std::string& name, TextureHandle texture);

    // Get the base material for an instance
    MaterialHandle get_base_material(MaterialInstanceHandle handle) const;

    // Apply instance overrides to the renderer (call before drawing with this instance)
    void bind(MaterialInstanceHandle handle);

    // Get instance count
    uint32_t get_instance_count() const { return m_instance_count; }

private:
    // Helper to get or create a uniform handle for a parameter name
    struct UniformInfo {
        uint32_t handle;  // bgfx uniform handle (stored as uint32_t for forward decl)
        uint8_t type;     // UniformType
    };
    UniformInfo get_or_create_uniform(const std::string& name, uint8_t type);

    // Helper to get texture sampler and slot
    struct SamplerInfo {
        uint32_t sampler_handle;  // bgfx uniform handle for sampler
        uint8_t slot;             // Texture slot
    };
    SamplerInfo get_or_create_sampler(const std::string& name);

    IRenderer* m_renderer = nullptr;
    bool m_initialized = false;

    std::unordered_map<uint32_t, MaterialInstance> m_instances;
    uint32_t m_next_id = 1;
    uint32_t m_instance_count = 0;

    // Cached uniform handles (name -> UniformInfo)
    std::unordered_map<std::string, UniformInfo> m_uniform_cache;

    // Cached sampler handles (name -> SamplerInfo)
    std::unordered_map<std::string, SamplerInfo> m_sampler_cache;
    uint8_t m_next_texture_slot = 4;  // Start at slot 4 to avoid conflicts with built-in slots
};

// Global material instance manager
MaterialInstanceManager& get_material_instance_manager();

} // namespace engine::render
