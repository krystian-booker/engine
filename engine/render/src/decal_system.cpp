#include <engine/render/decal_system.hpp>
#include <algorithm>
#include <random>
#include <cmath>

namespace engine::render {

// Global instance
static DecalSystem* s_decal_system = nullptr;

DecalSystem& get_decal_system() {
    if (!s_decal_system) {
        static DecalSystem instance;
        s_decal_system = &instance;
    }
    return *s_decal_system;
}

DecalSystem::~DecalSystem() {
    if (m_initialized) {
        shutdown();
    }
}

void DecalSystem::init(const DecalSystemConfig& config) {
    if (m_initialized) return;

    m_config = config;

    // Allocate storage
    m_definitions.resize(m_config.max_definitions);
    m_definition_used.resize(m_config.max_definitions, false);

    m_instances.resize(m_config.max_decals);
    m_instance_used.resize(m_config.max_decals, false);

    // Create unit cube for decal volumes
    create_unit_cube();

    // Create uniforms
    u_decal_params = bgfx::createUniform("u_decalParams", bgfx::UniformType::Vec4);
    u_decal_color = bgfx::createUniform("u_decalColor", bgfx::UniformType::Vec4);
    u_decal_size = bgfx::createUniform("u_decalSize", bgfx::UniformType::Vec4);
    u_inv_view_proj = bgfx::createUniform("u_customInvViewProj", bgfx::UniformType::Mat4);
    s_depth = bgfx::createUniform("s_depth", bgfx::UniformType::Sampler);
    s_gbuffer_normal = bgfx::createUniform("s_gbufferNormal", bgfx::UniformType::Sampler);
    s_decal_albedo = bgfx::createUniform("s_decalAlbedo", bgfx::UniformType::Sampler);
    s_decal_normal = bgfx::createUniform("s_decalNormal", bgfx::UniformType::Sampler);

    m_initialized = true;
}

void DecalSystem::shutdown() {
    if (!m_initialized) return;

    destroy_gpu_resources();

    m_definitions.clear();
    m_definition_used.clear();
    m_instances.clear();
    m_instance_used.clear();

    m_initialized = false;
}

void DecalSystem::create_unit_cube() {
    // Unit cube centered at origin, size 1x1x1
    struct Vertex {
        float x, y, z;
    };

    static Vertex vertices[] = {
        // Front face
        {-0.5f, -0.5f,  0.5f},
        { 0.5f, -0.5f,  0.5f},
        { 0.5f,  0.5f,  0.5f},
        {-0.5f,  0.5f,  0.5f},
        // Back face
        {-0.5f, -0.5f, -0.5f},
        { 0.5f, -0.5f, -0.5f},
        { 0.5f,  0.5f, -0.5f},
        {-0.5f,  0.5f, -0.5f},
    };

    static uint16_t indices[] = {
        // Front
        0, 1, 2, 2, 3, 0,
        // Back
        5, 4, 7, 7, 6, 5,
        // Left
        4, 0, 3, 3, 7, 4,
        // Right
        1, 5, 6, 6, 2, 1,
        // Top
        3, 2, 6, 6, 7, 3,
        // Bottom
        4, 5, 1, 1, 0, 4,
    };

    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .end();

    m_cube_vb = bgfx::createVertexBuffer(
        bgfx::makeRef(vertices, sizeof(vertices)),
        layout
    );

    m_cube_ib = bgfx::createIndexBuffer(
        bgfx::makeRef(indices, sizeof(indices))
    );
}

void DecalSystem::destroy_gpu_resources() {
    if (bgfx::isValid(m_cube_vb)) {
        bgfx::destroy(m_cube_vb);
        m_cube_vb = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_cube_ib)) {
        bgfx::destroy(m_cube_ib);
        m_cube_ib = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_decal_program)) {
        bgfx::destroy(m_decal_program);
        m_decal_program = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(u_decal_params)) {
        bgfx::destroy(u_decal_params);
        u_decal_params = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(u_decal_color)) {
        bgfx::destroy(u_decal_color);
        u_decal_color = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(u_decal_size)) {
        bgfx::destroy(u_decal_size);
        u_decal_size = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(u_inv_view_proj)) {
        bgfx::destroy(u_inv_view_proj);
        u_inv_view_proj = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(s_depth)) {
        bgfx::destroy(s_depth);
        s_depth = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(s_gbuffer_normal)) {
        bgfx::destroy(s_gbuffer_normal);
        s_gbuffer_normal = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(s_decal_albedo)) {
        bgfx::destroy(s_decal_albedo);
        s_decal_albedo = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(s_decal_normal)) {
        bgfx::destroy(s_decal_normal);
        s_decal_normal = BGFX_INVALID_HANDLE;
    }
}

DecalDefHandle DecalSystem::create_definition(const DecalDefinition& def) {
    // Find free slot
    for (uint32_t i = 0; i < m_config.max_definitions; ++i) {
        if (!m_definition_used[i]) {
            m_definitions[i] = def;
            m_definition_used[i] = true;
            m_definition_count++;
            return i;
        }
    }
    return INVALID_DECAL_DEF;
}

void DecalSystem::destroy_definition(DecalDefHandle handle) {
    if (handle >= m_config.max_definitions || !m_definition_used[handle]) {
        return;
    }

    // Destroy any instances using this definition
    for (uint32_t i = 0; i < m_config.max_decals; ++i) {
        if (m_instance_used[i] && m_instances[i].definition == handle) {
            m_instance_used[i] = false;
            m_active_count--;
        }
    }

    m_definition_used[handle] = false;
    m_definitions[handle] = DecalDefinition{};
    m_definition_count--;
}

DecalDefinition* DecalSystem::get_definition(DecalDefHandle handle) {
    if (handle >= m_config.max_definitions || !m_definition_used[handle]) {
        return nullptr;
    }
    return &m_definitions[handle];
}

const DecalDefinition* DecalSystem::get_definition(DecalDefHandle handle) const {
    if (handle >= m_config.max_definitions || !m_definition_used[handle]) {
        return nullptr;
    }
    return &m_definitions[handle];
}

Quat DecalSystem::calculate_rotation(const Vec3& direction, const Vec3& up, bool random_rotation) {
    // Normalize direction
    Vec3 forward = normalize(direction);

    // Calculate right vector
    Vec3 right = normalize(cross(up, forward));

    // Recalculate up to ensure orthogonal
    Vec3 actual_up = cross(forward, right);

    // Build rotation matrix
    Mat4 rot_mat(1.0f);
    rot_mat[0][0] = right.x;
    rot_mat[0][1] = right.y;
    rot_mat[0][2] = right.z;
    rot_mat[1][0] = actual_up.x;
    rot_mat[1][1] = actual_up.y;
    rot_mat[1][2] = actual_up.z;
    rot_mat[2][0] = forward.x;
    rot_mat[2][1] = forward.y;
    rot_mat[2][2] = forward.z;

    Quat result = glm::quat_cast(rot_mat);

    if (random_rotation) {
        // Add random rotation around forward axis
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<float> dist(0.0f, 2.0f * 3.14159265f);

        float angle = dist(gen);
        Quat random_rot = glm::angleAxis(angle, forward);
        result = random_rot * result;
    }

    return result;
}

DecalHandle DecalSystem::spawn(const DecalSpawnParams& params) {
    if (params.definition == INVALID_DECAL_DEF) {
        return INVALID_DECAL;
    }

    // Find free slot
    for (uint32_t i = 0; i < m_config.max_decals; ++i) {
        if (!m_instance_used[i]) {
            DecalInstance& instance = m_instances[i];
            instance.position = params.position;
            instance.rotation = calculate_rotation(params.direction, params.up, params.random_rotation);
            instance.scale = params.scale;
            instance.definition = params.definition;
            instance.color_tint = params.color_tint;
            instance.opacity = params.opacity;
            instance.lifetime = params.lifetime;
            instance.fade_in_time = params.fade_in_time;
            instance.fade_out_time = params.fade_out_time;
            instance.age = 0.0f;
            instance.active = true;
            instance.instance_id = m_next_instance_id++;

            m_instance_used[i] = true;
            m_active_count++;

            return i;
        }
    }

    return INVALID_DECAL;
}

DecalHandle DecalSystem::spawn(DecalDefHandle def, const Vec3& position, const Vec3& direction) {
    DecalSpawnParams params;
    params.definition = def;
    params.position = position;
    params.direction = direction;
    return spawn(params);
}

void DecalSystem::destroy(DecalHandle handle) {
    if (handle >= m_config.max_decals || !m_instance_used[handle]) {
        return;
    }

    m_instance_used[handle] = false;
    m_instances[handle] = DecalInstance{};
    m_active_count--;
}

void DecalSystem::destroy_all() {
    for (uint32_t i = 0; i < m_config.max_decals; ++i) {
        if (m_instance_used[i]) {
            m_instance_used[i] = false;
            m_instances[i] = DecalInstance{};
        }
    }
    m_active_count = 0;
}

DecalInstance* DecalSystem::get_instance(DecalHandle handle) {
    if (handle >= m_config.max_decals || !m_instance_used[handle]) {
        return nullptr;
    }
    return &m_instances[handle];
}

const DecalInstance* DecalSystem::get_instance(DecalHandle handle) const {
    if (handle >= m_config.max_decals || !m_instance_used[handle]) {
        return nullptr;
    }
    return &m_instances[handle];
}

void DecalSystem::spawn_batch(const std::vector<DecalSpawnParams>& params, std::vector<DecalHandle>& out_handles) {
    out_handles.clear();
    out_handles.reserve(params.size());

    for (const auto& p : params) {
        DecalHandle h = spawn(p);
        out_handles.push_back(h);
    }
}

void DecalSystem::destroy_expired() {
    for (uint32_t i = 0; i < m_config.max_decals; ++i) {
        if (m_instance_used[i] && m_instances[i].is_expired()) {
            destroy(i);
        }
    }
}

void DecalSystem::update(float dt) {
    if (!m_initialized) return;

    m_stats.draws_this_frame = 0;
    m_stats.culled_this_frame = 0;

    // Update instance ages
    for (uint32_t i = 0; i < m_config.max_decals; ++i) {
        if (m_instance_used[i]) {
            m_instances[i].age += dt;
        }
    }

    // Periodic cleanup of expired decals
    m_update_accumulator += dt;
    float update_interval = 1.0f / m_config.update_frequency;
    if (m_update_accumulator >= update_interval) {
        m_update_accumulator = 0.0f;
        destroy_expired();
    }

    // Update stats
    m_stats.active_decals = m_active_count;
    m_stats.definitions = m_definition_count;
}

void DecalSystem::render(bgfx::ViewId view_id,
                         bgfx::TextureHandle depth_texture,
                         bgfx::TextureHandle normal_texture,
                         const Mat4& view_matrix,
                         const Mat4& proj_matrix,
                         const Mat4& inv_view_proj) {
    if (!m_initialized || !bgfx::isValid(m_decal_program)) {
        return;
    }

    // Sort decals by priority (could be optimized with caching)
    std::vector<uint32_t> sorted_indices;
    sorted_indices.reserve(m_active_count);

    for (uint32_t i = 0; i < m_config.max_decals; ++i) {
        if (!m_instance_used[i]) continue;

        const auto& instance = m_instances[i];
        if (!instance.active) continue;

        // Distance culling
        if (m_config.enable_distance_culling) {
            float dist = length(instance.position - m_camera_position);
            if (dist > m_config.cull_distance) {
                m_stats.culled_this_frame++;
                continue;
            }
        }

        sorted_indices.push_back(i);
    }

    // Sort by definition priority
    std::sort(sorted_indices.begin(), sorted_indices.end(),
        [this](uint32_t a, uint32_t b) {
            const auto* def_a = get_definition(m_instances[a].definition);
            const auto* def_b = get_definition(m_instances[b].definition);
            int prio_a = def_a ? def_a->sort_priority : 0;
            int prio_b = def_b ? def_b->sort_priority : 0;
            return prio_a < prio_b;
        }
    );

    // Set inverse view-projection matrix
    bgfx::setUniform(u_inv_view_proj, glm::value_ptr(inv_view_proj));

    // Bind G-buffer textures
    bgfx::setTexture(0, s_depth, depth_texture);
    bgfx::setTexture(1, s_gbuffer_normal, normal_texture);

    // Render each decal
    for (uint32_t idx : sorted_indices) {
        const auto& instance = m_instances[idx];
        const auto* def = get_definition(instance.definition);
        if (!def) continue;

        // Calculate world transform
        Mat4 world = instance.get_transform();

        // Apply definition size
        Mat4 scale_mat = glm::scale(Mat4(1.0f), def->size);
        world = world * scale_mat;

        // Set transform
        bgfx::setTransform(glm::value_ptr(world));

        // Set decal parameters
        float current_opacity = instance.get_current_opacity();
        Vec4 color = def->base_color * instance.color_tint;
        color.w *= current_opacity;
        bgfx::setUniform(u_decal_color, &color.x);

        // Decal params: angle_fade_start, angle_fade_end, channels, blend_mode
        float params[4] = {
            def->angle_fade_start,
            def->angle_fade_end,
            static_cast<float>(static_cast<uint8_t>(def->channels)),
            static_cast<float>(static_cast<uint8_t>(def->blend_mode))
        };
        bgfx::setUniform(u_decal_params, params);

        // Size for projection
        float size[4] = { def->size.x, def->size.y, def->size.z, 0.0f };
        bgfx::setUniform(u_decal_size, size);

        // Bind decal textures
        if (bgfx::isValid(def->albedo_texture)) {
            bgfx::setTexture(2, s_decal_albedo, def->albedo_texture);
        }
        if (bgfx::isValid(def->normal_texture)) {
            bgfx::setTexture(3, s_decal_normal, def->normal_texture);
        }

        // Set geometry
        bgfx::setVertexBuffer(0, m_cube_vb);
        bgfx::setIndexBuffer(m_cube_ib);

        // Set render state based on blend mode
        uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_GEQUAL;

        switch (def->blend_mode) {
            case DecalBlendMode::Normal:
                state |= BGFX_STATE_BLEND_ALPHA;
                break;
            case DecalBlendMode::Additive:
                state |= BGFX_STATE_BLEND_ADD;
                break;
            case DecalBlendMode::Multiply:
                state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_DST_COLOR, BGFX_STATE_BLEND_ZERO);
                break;
            case DecalBlendMode::Overlay:
                state |= BGFX_STATE_BLEND_ALPHA;
                break;
        }

        bgfx::setState(state);

        // Submit
        bgfx::submit(view_id, m_decal_program);
        m_stats.draws_this_frame++;
    }
}

std::vector<DecalHandle> DecalSystem::get_visible_decals(const Vec3& camera_pos, float max_distance) const {
    std::vector<DecalHandle> visible;
    visible.reserve(m_active_count);

    float max_dist_sq = max_distance * max_distance;

    for (uint32_t i = 0; i < m_config.max_decals; ++i) {
        if (!m_instance_used[i]) continue;

        const auto& instance = m_instances[i];
        if (!instance.active) continue;

        Vec3 diff = instance.position - camera_pos;
        float dist_sq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;

        if (dist_sq <= max_dist_sq) {
            visible.push_back(i);
        }
    }

    return visible;
}

} // namespace engine::render
