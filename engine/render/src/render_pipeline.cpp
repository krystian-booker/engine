#include <engine/render/render_pipeline.hpp>
#include <engine/render/renderer.hpp>
#include <engine/core/log.hpp>
#include <algorithm>
#include <array>
#include <cmath>

namespace engine::render {

using namespace engine::core;

RenderPipeline::~RenderPipeline() {
    if (m_initialized) {
        shutdown();
    }
}

void RenderPipeline::init(IRenderer* renderer, const RenderPipelineConfig& config) {
    m_renderer = renderer;
    m_config = config;
    m_width = renderer->get_width();
    m_height = renderer->get_height();

    // Calculate internal resolution based on render scale
    m_internal_width = static_cast<uint32_t>(m_width * config.render_scale);
    m_internal_height = static_cast<uint32_t>(m_height * config.render_scale);

    // Create render targets
    create_render_targets();

    // Initialize subsystems
    if (has_flag(config.enabled_passes, RenderPassFlags::Shadows)) {
        m_shadow_system.init(renderer, config.shadow_config);
    }

    if (has_flag(config.enabled_passes, RenderPassFlags::SSAO)) {
        m_ssao_system.init(renderer, config.ssao_config);
    }

    if (has_flag(config.enabled_passes, RenderPassFlags::PostProcess)) {
        PostProcessConfig pp_config;
        pp_config.bloom = config.bloom_config;
        pp_config.tonemapping = config.tonemap_config;
        m_post_process_system.init(renderer, pp_config);
    }

    if (has_flag(config.enabled_passes, RenderPassFlags::TAA)) {
        m_taa_system.init(renderer, config.taa_config);
    }

    if (has_flag(config.enabled_passes, RenderPassFlags::Volumetric)) {
        m_volumetric_system.init(renderer, config.volumetric_config);
    }

    m_initialized = true;
    log(LogLevel::Info, "Render pipeline initialized ({}x{} @ {}x internal)",
        m_width, m_height, config.render_scale);
}

void RenderPipeline::shutdown() {
    if (!m_initialized) return;

    // Shutdown subsystems
    m_volumetric_system.shutdown();
    m_taa_system.shutdown();
    m_post_process_system.shutdown();
    m_ssao_system.shutdown();
    m_shadow_system.shutdown();

    // Destroy render targets
    destroy_render_targets();

    m_initialized = false;
    m_renderer = nullptr;

    log(LogLevel::Info, "Render pipeline shutdown");
}

void RenderPipeline::set_config(const RenderPipelineConfig& config) {
    bool needs_resize = (config.render_scale != m_config.render_scale);
    m_config = config;

    // Update subsystem configs
    if (has_flag(config.enabled_passes, RenderPassFlags::Shadows)) {
        m_shadow_system.set_config(config.shadow_config);
    }

    if (has_flag(config.enabled_passes, RenderPassFlags::SSAO)) {
        m_ssao_system.set_config(config.ssao_config);
    }

    if (has_flag(config.enabled_passes, RenderPassFlags::PostProcess)) {
        PostProcessConfig pp_config;
        pp_config.bloom = config.bloom_config;
        pp_config.tonemapping = config.tonemap_config;
        m_post_process_system.set_config(pp_config);
    }

    if (has_flag(config.enabled_passes, RenderPassFlags::TAA)) {
        m_taa_system.set_config(config.taa_config);
    }

    if (has_flag(config.enabled_passes, RenderPassFlags::Volumetric)) {
        m_volumetric_system.set_config(config.volumetric_config);
    }

    if (needs_resize) {
        resize(m_width, m_height);
    }
}

void RenderPipeline::apply_quality_preset(RenderQuality quality) {
    RenderPipelineConfig config = m_config;
    config.quality = quality;

    switch (quality) {
        case RenderQuality::Low:
            config.render_scale = 0.75f;
            config.shadow_config.cascade_resolution = 1024;
            config.shadow_config.cascade_count = 2;
            config.ssao_config.sample_count = 8;
            config.ssao_config.half_resolution = true;
            config.bloom_config.enabled = false;
            config.taa_config.enabled = false;
            config.volumetric_config.froxel_depth = 32;
            config.enabled_passes = RenderPassFlags::Shadows |
                                    RenderPassFlags::MainOpaque |
                                    RenderPassFlags::Transparent |
                                    RenderPassFlags::Final;
            break;

        case RenderQuality::Medium:
            config.render_scale = 1.0f;
            config.shadow_config.cascade_resolution = 2048;
            config.shadow_config.cascade_count = 3;
            config.ssao_config.sample_count = 16;
            config.ssao_config.half_resolution = true;
            config.bloom_config.enabled = true;
            config.bloom_config.mip_count = 4;
            config.taa_config.enabled = true;
            config.volumetric_config.froxel_depth = 64;
            config.enabled_passes = RenderPassFlags::Shadows |
                                    RenderPassFlags::SSAO |
                                    RenderPassFlags::MainOpaque |
                                    RenderPassFlags::Transparent |
                                    RenderPassFlags::PostProcess |
                                    RenderPassFlags::TAA |
                                    RenderPassFlags::Final;
            break;

        case RenderQuality::High:
            config.render_scale = 1.0f;
            config.shadow_config.cascade_resolution = 2048;
            config.shadow_config.cascade_count = 4;
            config.ssao_config.sample_count = 32;
            config.ssao_config.half_resolution = false;
            config.bloom_config.enabled = true;
            config.bloom_config.mip_count = 5;
            config.taa_config.enabled = true;
            config.volumetric_config.froxel_depth = 128;
            config.enabled_passes = RenderPassFlags::All;
            break;

        case RenderQuality::Ultra:
            config.render_scale = 1.0f;
            config.shadow_config.cascade_resolution = 4096;
            config.shadow_config.cascade_count = 4;
            config.shadow_config.pcf_samples = 49;  // 7x7
            config.ssao_config.sample_count = 64;
            config.ssao_config.half_resolution = false;
            config.bloom_config.enabled = true;
            config.bloom_config.mip_count = 6;
            config.taa_config.enabled = true;
            config.volumetric_config.froxel_depth = 128;
            config.volumetric_config.temporal_reprojection = true;
            config.enabled_passes = RenderPassFlags::All;
            break;

        case RenderQuality::Custom:
            // Keep current settings
            break;
    }

    set_config(config);
}

void RenderPipeline::begin_frame() {
    // Reset stats
    m_stats = RenderStats{};

    // Clear visible object lists
    m_visible_opaque.clear();
    m_visible_transparent.clear();
    m_shadow_casters.clear();

    m_frame_count++;
}

void RenderPipeline::render(const CameraData& camera,
                            const std::vector<RenderObject>& objects,
                            const std::vector<LightData>& lights) {
    if (!m_initialized) return;

    // Update camera uniforms
    update_camera_uniforms(camera);

    // Cull objects
    cull_objects(camera, objects, m_visible_opaque);

    // Separate opaque and transparent based on blend mode
    m_visible_transparent.clear();
    auto it = std::partition(m_visible_opaque.begin(), m_visible_opaque.end(),
        [](const RenderObject* obj) {
            // Check blend mode - BlendMode enum: 0=Opaque, 1=AlphaTest, 2+=Transparent
            // Opaque and AlphaTest render with the opaque batch
            return obj->blend_mode <= 1;
        });
    m_visible_transparent.assign(it, m_visible_opaque.end());
    m_visible_opaque.erase(it, m_visible_opaque.end());

    // Sort opaque front-to-back for early-z
    sort_objects_front_to_back(camera, m_visible_opaque);

    // Sort transparent back-to-front
    sort_objects_back_to_front(camera, m_visible_transparent);

    // Find shadow casters
    for (const auto* obj : m_visible_opaque) {
        if (obj->casts_shadows) {
            m_shadow_casters.push_back(obj);
        }
    }

    // Execute render passes in order
    if (has_flag(m_config.enabled_passes, RenderPassFlags::Shadows)) {
        shadow_pass(camera, objects, lights);
    }

    if (has_flag(m_config.enabled_passes, RenderPassFlags::DepthPrepass)) {
        depth_prepass(camera, objects);
    }

    // GBuffer pass for normals (needed by SSAO)
    if (has_flag(m_config.enabled_passes, RenderPassFlags::GBuffer)) {
        gbuffer_pass(camera, objects);
    }

    // Motion vectors pass (needed by TAA)
    if (has_flag(m_config.enabled_passes, RenderPassFlags::TAA) && m_config.taa_config.enabled) {
        motion_vector_pass(camera, objects);
    }

    if (has_flag(m_config.enabled_passes, RenderPassFlags::SSAO)) {
        ssao_pass(camera);
    }

    if (has_flag(m_config.enabled_passes, RenderPassFlags::MainOpaque)) {
        main_pass(camera, objects, lights);
    }

    if (has_flag(m_config.enabled_passes, RenderPassFlags::Volumetric)) {
        volumetric_pass(camera, lights);
    }

    if (has_flag(m_config.enabled_passes, RenderPassFlags::Transparent)) {
        transparent_pass(camera, objects, lights);
    }

    if (has_flag(m_config.enabled_passes, RenderPassFlags::PostProcess)) {
        post_process_pass(camera);
    }

    if (has_flag(m_config.enabled_passes, RenderPassFlags::Debug)) {
        debug_pass(camera);
    }

    if (has_flag(m_config.enabled_passes, RenderPassFlags::Final)) {
        final_pass();
    }

    // Update stats
    m_stats.objects_rendered = static_cast<uint32_t>(m_visible_opaque.size() + m_visible_transparent.size());
    m_stats.shadow_casters = static_cast<uint32_t>(m_shadow_casters.size());
    m_stats.lights = static_cast<uint32_t>(lights.size());
}

void RenderPipeline::end_frame() {
    // Nothing to do for now
}

void RenderPipeline::submit_object(const RenderObject& object) {
    // For streaming submission (not used in current implementation)
}

void RenderPipeline::submit_light(const LightData& light) {
    // For streaming submission (not used in current implementation)
}

void RenderPipeline::resize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;
    m_internal_width = static_cast<uint32_t>(width * m_config.render_scale);
    m_internal_height = static_cast<uint32_t>(height * m_config.render_scale);

    // Recreate render targets
    destroy_render_targets();
    create_render_targets();

    // Resize subsystems
    m_ssao_system.resize(m_internal_width, m_internal_height);
    m_post_process_system.resize(m_internal_width, m_internal_height);
    m_taa_system.resize(m_internal_width, m_internal_height);
    m_volumetric_system.resize(m_internal_width, m_internal_height);

    log(LogLevel::Info, "Render pipeline resized to {}x{} (internal: {}x{})",
        width, height, m_internal_width, m_internal_height);
}

TextureHandle RenderPipeline::get_final_texture() const {
    if (m_ldr_target.valid()) {
        return m_renderer->get_render_target_texture(m_ldr_target, 0);
    }
    return TextureHandle{};
}

TextureHandle RenderPipeline::get_depth_texture() const {
    if (m_depth_target.valid()) {
        return m_renderer->get_render_target_texture(m_depth_target, 0);
    }
    return TextureHandle{};
}

TextureHandle RenderPipeline::get_shadow_debug_texture() const {
    RenderTargetHandle rt = m_shadow_system.get_cascade_render_target(0);
    if (rt.valid()) {
        return m_renderer->get_render_target_texture(rt, 0);
    }
    return TextureHandle{};
}

TextureHandle RenderPipeline::get_ssao_debug_texture() const {
    return m_ssao_system.get_ao_texture();
}

TextureHandle RenderPipeline::get_volumetric_debug_texture() const {
    return m_volumetric_system.get_volumetric_texture();
}

void RenderPipeline::add_custom_pass(RenderView after_view, CustomRenderCallback callback) {
    m_custom_passes.emplace_back(after_view, std::move(callback));
}

void RenderPipeline::create_render_targets() {
    // Depth-only target
    {
        RenderTargetDesc desc;
        desc.width = m_internal_width;
        desc.height = m_internal_height;
        desc.color_attachment_count = 0;
        desc.has_depth = true;
        desc.depth_format = TextureFormat::Depth32F;
        desc.samplable = true;
        desc.debug_name = "Pipeline_Depth";

        m_depth_target = m_renderer->create_render_target(desc);
    }

    // GBuffer with normals (for SSAO)
    {
        RenderTargetDesc desc;
        desc.width = m_internal_width;
        desc.height = m_internal_height;
        desc.color_attachment_count = 1;
        desc.color_format = TextureFormat::RGBA16F;  // World-space normals
        desc.has_depth = true;
        desc.depth_format = TextureFormat::Depth32F;
        desc.samplable = true;
        desc.debug_name = "Pipeline_GBuffer";

        m_gbuffer = m_renderer->create_render_target(desc);

        // Configure GBuffer view
        ViewConfig view_config;
        view_config.render_target = m_gbuffer;
        view_config.clear_color_enabled = true;
        view_config.clear_color = 0x808080FF;  // Neutral normal (0.5, 0.5, 0.5)
        view_config.clear_depth_enabled = true;
        view_config.clear_depth = 1.0f;
        m_renderer->configure_view(RenderView::GBuffer, view_config);
    }

    // Motion vectors (for TAA)
    {
        RenderTargetDesc desc;
        desc.width = m_internal_width;
        desc.height = m_internal_height;
        desc.color_attachment_count = 1;
        desc.color_format = TextureFormat::RGBA16F;  // RG = motion, BA = unused
        desc.has_depth = false;
        desc.samplable = true;
        desc.debug_name = "Pipeline_MotionVectors";

        m_motion_vectors = m_renderer->create_render_target(desc);

        // Configure motion vector view
        ViewConfig view_config;
        view_config.render_target = m_motion_vectors;
        view_config.clear_color_enabled = true;
        view_config.clear_color = 0x00000000;  // Zero motion by default
        view_config.clear_depth_enabled = false;
        m_renderer->configure_view(RenderView::MotionVectors, view_config);
    }

    // HDR render target
    {
        RenderTargetDesc desc;
        desc.width = m_internal_width;
        desc.height = m_internal_height;
        desc.color_attachment_count = 1;
        desc.color_format = TextureFormat::RGBA16F;
        desc.has_depth = true;
        desc.depth_format = TextureFormat::Depth32F;
        desc.samplable = true;
        desc.debug_name = "Pipeline_HDR";

        m_hdr_target = m_renderer->create_render_target(desc);
    }

    // LDR output target
    {
        RenderTargetDesc desc;
        desc.width = m_width;
        desc.height = m_height;
        desc.color_attachment_count = 1;
        desc.color_format = TextureFormat::RGBA8;
        desc.has_depth = false;
        desc.samplable = true;
        desc.debug_name = "Pipeline_LDR";

        m_ldr_target = m_renderer->create_render_target(desc);
    }

    // Configure views
    {
        ViewConfig view_config;
        view_config.render_target = m_depth_target;
        view_config.clear_depth_enabled = true;
        view_config.clear_depth = 1.0f;
        view_config.clear_color_enabled = false;
        m_renderer->configure_view(RenderView::DepthPrepass, view_config);
    }

    {
        ViewConfig view_config;
        view_config.render_target = m_hdr_target;
        view_config.clear_color_enabled = true;
        view_config.clear_color = 0x000000FF;  // Black
        view_config.clear_depth_enabled = true;
        view_config.clear_depth = 1.0f;
        m_renderer->configure_view(RenderView::MainOpaque, view_config);
    }

    {
        ViewConfig view_config;
        view_config.render_target = m_hdr_target;
        view_config.clear_color_enabled = false;
        view_config.clear_depth_enabled = false;
        m_renderer->configure_view(RenderView::MainTransparent, view_config);
    }

    {
        ViewConfig view_config;
        view_config.render_target = m_ldr_target;
        view_config.clear_color_enabled = true;
        view_config.clear_color = 0x000000FF;
        view_config.clear_depth_enabled = false;
        m_renderer->configure_view(RenderView::Final, view_config);
    }
}

void RenderPipeline::destroy_render_targets() {
    if (m_depth_target.valid()) {
        m_renderer->destroy_render_target(m_depth_target);
        m_depth_target = RenderTargetHandle{};
    }

    if (m_gbuffer.valid()) {
        m_renderer->destroy_render_target(m_gbuffer);
        m_gbuffer = RenderTargetHandle{};
    }

    if (m_motion_vectors.valid()) {
        m_renderer->destroy_render_target(m_motion_vectors);
        m_motion_vectors = RenderTargetHandle{};
    }

    if (m_hdr_target.valid()) {
        m_renderer->destroy_render_target(m_hdr_target);
        m_hdr_target = RenderTargetHandle{};
    }

    if (m_ldr_target.valid()) {
        m_renderer->destroy_render_target(m_ldr_target);
        m_ldr_target = RenderTargetHandle{};
    }
}

void RenderPipeline::update_camera_uniforms(const CameraData& camera) {
    m_renderer->set_view_transform(RenderView::MainOpaque, camera.view_matrix, camera.projection_matrix);
    m_renderer->set_view_transform(RenderView::MainTransparent, camera.view_matrix, camera.projection_matrix);
}

void RenderPipeline::update_light_uniforms(const std::vector<LightData>& lights) {
    // Pack lights into uniform buffer
    // Max 8 lights currently supported
    constexpr int MAX_LIGHTS = 8;
    int light_count = std::min(static_cast<int>(lights.size()), MAX_LIGHTS);

    for (int i = 0; i < light_count; ++i) {
        m_renderer->set_light(static_cast<uint32_t>(i), lights[i]);
    }
}

// Extract frustum planes from view-projection matrix
// Planes are stored as Vec4 (a, b, c, d) where ax + by + cz + d = 0
static void extract_frustum_planes(const Mat4& vp, std::array<Vec4, 6>& planes) {
    // Left plane
    planes[0] = Vec4(
        vp[0][3] + vp[0][0],
        vp[1][3] + vp[1][0],
        vp[2][3] + vp[2][0],
        vp[3][3] + vp[3][0]
    );

    // Right plane
    planes[1] = Vec4(
        vp[0][3] - vp[0][0],
        vp[1][3] - vp[1][0],
        vp[2][3] - vp[2][0],
        vp[3][3] - vp[3][0]
    );

    // Bottom plane
    planes[2] = Vec4(
        vp[0][3] + vp[0][1],
        vp[1][3] + vp[1][1],
        vp[2][3] + vp[2][1],
        vp[3][3] + vp[3][1]
    );

    // Top plane
    planes[3] = Vec4(
        vp[0][3] - vp[0][1],
        vp[1][3] - vp[1][1],
        vp[2][3] - vp[2][1],
        vp[3][3] - vp[3][1]
    );

    // Near plane
    planes[4] = Vec4(
        vp[0][3] + vp[0][2],
        vp[1][3] + vp[1][2],
        vp[2][3] + vp[2][2],
        vp[3][3] + vp[3][2]
    );

    // Far plane
    planes[5] = Vec4(
        vp[0][3] - vp[0][2],
        vp[1][3] - vp[1][2],
        vp[2][3] - vp[2][2],
        vp[3][3] - vp[3][2]
    );

    // Normalize all planes
    for (auto& plane : planes) {
        float len = std::sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
        if (len > 0.0001f) {
            plane /= len;
        }
    }
}

// Test if AABB is outside frustum (returns true if completely outside)
static bool aabb_outside_frustum(const Vec3& min, const Vec3& max, const std::array<Vec4, 6>& planes) {
    for (const auto& plane : planes) {
        // Find the corner of AABB most in the direction of plane normal
        Vec3 positive_corner(
            plane.x >= 0 ? max.x : min.x,
            plane.y >= 0 ? max.y : min.y,
            plane.z >= 0 ? max.z : min.z
        );

        // If the positive corner is outside the plane, AABB is fully outside
        float dist = plane.x * positive_corner.x + plane.y * positive_corner.y +
                     plane.z * positive_corner.z + plane.w;
        if (dist < 0.0f) {
            return true;  // Completely outside this plane
        }
    }
    return false;  // Inside or intersecting all planes
}

void RenderPipeline::cull_objects(const CameraData& camera,
                                   const std::vector<RenderObject>& objects,
                                   std::vector<const RenderObject*>& visible_objects) {
    visible_objects.clear();
    visible_objects.reserve(objects.size());

    // Extract frustum planes from view-projection matrix
    std::array<Vec4, 6> frustum_planes;
    extract_frustum_planes(camera.view_projection, frustum_planes);

    for (const auto& obj : objects) {
        if (!obj.visible) {
            m_stats.objects_culled++;
            continue;
        }

        // If object has valid bounds, perform frustum culling
        if (obj.bounds.min != Vec3(0) || obj.bounds.max != Vec3(0)) {
            // Transform AABB to world space
            // For simplicity, we transform the center and half-extents
            Vec3 center = (obj.bounds.min + obj.bounds.max) * 0.5f;
            Vec3 half_extent = (obj.bounds.max - obj.bounds.min) * 0.5f;

            // Transform center to world space
            Vec4 world_center = obj.transform * Vec4(center, 1.0f);

            // Compute world-space AABB (conservative approximation)
            // We use the absolute values of the rotation matrix to compute max extent
            Vec3 world_half_extent(
                std::abs(obj.transform[0][0]) * half_extent.x +
                std::abs(obj.transform[1][0]) * half_extent.y +
                std::abs(obj.transform[2][0]) * half_extent.z,
                std::abs(obj.transform[0][1]) * half_extent.x +
                std::abs(obj.transform[1][1]) * half_extent.y +
                std::abs(obj.transform[2][1]) * half_extent.z,
                std::abs(obj.transform[0][2]) * half_extent.x +
                std::abs(obj.transform[1][2]) * half_extent.y +
                std::abs(obj.transform[2][2]) * half_extent.z
            );

            Vec3 world_min = Vec3(world_center) - world_half_extent;
            Vec3 world_max = Vec3(world_center) + world_half_extent;

            if (aabb_outside_frustum(world_min, world_max, frustum_planes)) {
                m_stats.objects_culled++;
                continue;
            }
        }

        visible_objects.push_back(&obj);
    }
}

void RenderPipeline::sort_objects_front_to_back(const CameraData& camera,
                                                 std::vector<const RenderObject*>& objects) {
    const Vec3& cam_pos = camera.position;

    std::sort(objects.begin(), objects.end(),
        [&cam_pos](const RenderObject* a, const RenderObject* b) {
            Vec3 pos_a = Vec3(a->transform[3]);
            Vec3 pos_b = Vec3(b->transform[3]);
            float dist_a = length(pos_a - cam_pos);
            float dist_b = length(pos_b - cam_pos);
            return dist_a < dist_b;
        });
}

void RenderPipeline::sort_objects_back_to_front(const CameraData& camera,
                                                 std::vector<const RenderObject*>& objects) {
    const Vec3& cam_pos = camera.position;

    std::sort(objects.begin(), objects.end(),
        [&cam_pos](const RenderObject* a, const RenderObject* b) {
            Vec3 pos_a = Vec3(a->transform[3]);
            Vec3 pos_b = Vec3(b->transform[3]);
            float dist_a = length(pos_a - cam_pos);
            float dist_b = length(pos_b - cam_pos);
            return dist_a > dist_b;
        });
}

void RenderPipeline::shadow_pass(const CameraData& camera,
                                  const std::vector<RenderObject>& objects,
                                  const std::vector<LightData>& lights) {
    // Find directional light for CSM
    const LightData* sun_light = nullptr;
    for (const auto& light : lights) {
        if (light.type == 0 && light.cast_shadows) {
            sun_light = &light;
            break;
        }
    }

    if (!sun_light) return;

    // Update shadow system
    m_shadow_system.update_cascades(camera.view_matrix, camera.projection_matrix,
                                     sun_light->direction, camera.near_plane, camera.far_plane);

    // Render shadow casters to each cascade
    for (uint32_t cascade = 0; cascade < m_config.shadow_config.cascade_count; ++cascade) {
        RenderView shadow_view = static_cast<RenderView>(
            static_cast<int>(RenderView::ShadowCascade0) + cascade);

        for (const auto* obj : m_shadow_casters) {
            if (obj->skinned && obj->bone_matrices) {
                m_renderer->submit_skinned_mesh(shadow_view, obj->mesh, obj->material,
                                                 obj->transform, obj->bone_matrices, obj->bone_count);
            } else {
                m_renderer->submit_mesh(shadow_view, obj->mesh, obj->material, obj->transform);
            }
            m_stats.draw_calls++;
        }
    }
}

void RenderPipeline::depth_prepass(const CameraData& camera,
                                    const std::vector<RenderObject>& objects) {
    for (const auto* obj : m_visible_opaque) {
        // Submit with depth-only material
        m_renderer->submit_mesh(RenderView::DepthPrepass, obj->mesh, obj->material, obj->transform);
        m_stats.draw_calls++;
    }
}

void RenderPipeline::gbuffer_pass(const CameraData& camera,
                                   const std::vector<RenderObject>& objects) {
    // Set view transform for GBuffer pass
    m_renderer->set_view_transform(RenderView::GBuffer, camera.view_matrix, camera.projection_matrix);

    // Render all visible opaque objects to GBuffer
    // This outputs world-space normals to the GBuffer color attachment
    for (const auto* obj : m_visible_opaque) {
        if (obj->skinned && obj->bone_matrices) {
            m_renderer->submit_skinned_mesh(RenderView::GBuffer, obj->mesh, obj->material,
                                             obj->transform, obj->bone_matrices, obj->bone_count);
        } else {
            m_renderer->submit_mesh(RenderView::GBuffer, obj->mesh, obj->material, obj->transform);
        }
        m_stats.draw_calls++;
    }
}

void RenderPipeline::motion_vector_pass(const CameraData& camera,
                                         const std::vector<RenderObject>& objects) {
    // Set view transform for motion vector pass
    m_renderer->set_view_transform(RenderView::MotionVectors, camera.view_matrix, camera.projection_matrix);

    // Render all visible opaque objects to calculate motion vectors
    // Motion is calculated in the shader using current and previous view-projection matrices
    // Note: For now we only support camera motion. Per-object motion would require
    // extending the renderer interface to support previous transforms per-draw.
    for (const auto* obj : m_visible_opaque) {
        if (obj->skinned && obj->bone_matrices) {
            m_renderer->submit_skinned_mesh(RenderView::MotionVectors, obj->mesh, obj->material,
                                             obj->transform, obj->bone_matrices, obj->bone_count);
        } else {
            m_renderer->submit_mesh(RenderView::MotionVectors, obj->mesh, obj->material, obj->transform);
        }
        m_stats.draw_calls++;
    }
}

void RenderPipeline::ssao_pass(const CameraData& camera) {
    TextureHandle depth_tex = get_depth_texture();
    if (!depth_tex.valid()) return;

    // Get normal texture from GBuffer
    TextureHandle normal_tex;
    if (m_gbuffer.valid()) {
        normal_tex = m_renderer->get_render_target_texture(m_gbuffer, 0);
    }

    m_ssao_system.render(depth_tex, normal_tex, camera.projection_matrix, camera.view_matrix);
}

void RenderPipeline::main_pass(const CameraData& camera,
                                const std::vector<RenderObject>& objects,
                                const std::vector<LightData>& lights) {
    // Update light uniforms
    update_light_uniforms(lights);

    // Bind shadow maps
    if (has_flag(m_config.enabled_passes, RenderPassFlags::Shadows)) {
        auto shadow_matrices = m_shadow_system.get_cascade_matrices();
        auto cascade_splits = m_shadow_system.get_cascade_splits();

        m_renderer->set_shadow_data(shadow_matrices, cascade_splits,
                                     Vec4(m_config.shadow_config.shadow_bias,
                                          m_config.shadow_config.normal_bias,
                                          static_cast<float>(m_config.shadow_config.pcf_samples),
                                          1.0f));

        for (uint32_t i = 0; i < m_config.shadow_config.cascade_count; ++i) {
            auto rt = m_shadow_system.get_cascade_render_target(i);
            TextureHandle shadow_tex = m_renderer->get_render_target_texture(rt, UINT32_MAX);
            m_renderer->set_shadow_texture(i, shadow_tex);
        }

        m_renderer->enable_shadows(true);
    }

    // Bind SSAO texture if available
    if (has_flag(m_config.enabled_passes, RenderPassFlags::SSAO)) {
        TextureHandle ao_tex = m_ssao_system.get_ao_texture();
        if (ao_tex.valid()) {
            m_renderer->set_ao_texture(ao_tex);
        }
    }

    // Render opaque objects
    for (const auto* obj : m_visible_opaque) {
        if (obj->skinned && obj->bone_matrices) {
            m_renderer->submit_skinned_mesh(RenderView::MainOpaque, obj->mesh, obj->material,
                                             obj->transform, obj->bone_matrices, obj->bone_count);
        } else {
            m_renderer->submit_mesh(RenderView::MainOpaque, obj->mesh, obj->material, obj->transform);
        }
        m_stats.draw_calls++;
    }
}

void RenderPipeline::volumetric_pass(const CameraData& camera,
                                      const std::vector<LightData>& lights) {
    // Gather shadow data
    auto shadow_matrices = m_shadow_system.get_cascade_matrices();
    std::array<TextureHandle, 4> shadow_maps;
    for (uint32_t i = 0; i < 4; ++i) {
        auto rt = m_shadow_system.get_cascade_render_target(i);
        shadow_maps[i] = m_renderer->get_render_target_texture(rt, UINT32_MAX);
    }

    TextureHandle depth_tex = get_depth_texture();
    if (!depth_tex.valid()) return;

    m_volumetric_system.update(camera.view_matrix, camera.projection_matrix,
                                camera.prev_view_projection, depth_tex,
                                shadow_maps, shadow_matrices);

    // Convert lights to volumetric format
    std::vector<VolumetricLightData> vol_lights;
    for (const auto& light : lights) {
        VolumetricLightData vl;
        vl.position = light.position;
        vl.direction = light.direction;
        vl.color = light.color;
        vl.intensity = light.intensity;
        vl.range = light.range;
        vl.spot_angle_cos = std::cos(light.outer_angle * 3.14159f / 180.0f);
        vl.type = light.type;
        vl.shadow_cascade = light.shadow_map_index;
        vol_lights.push_back(vl);
    }
    m_volumetric_system.set_lights(vol_lights);
}

void RenderPipeline::transparent_pass(const CameraData& camera,
                                       const std::vector<RenderObject>& objects,
                                       const std::vector<LightData>& lights) {
    for (const auto* obj : m_visible_transparent) {
        if (obj->skinned && obj->bone_matrices) {
            m_renderer->submit_skinned_mesh(RenderView::MainTransparent, obj->mesh, obj->material,
                                             obj->transform, obj->bone_matrices, obj->bone_count);
        } else {
            m_renderer->submit_mesh(RenderView::MainTransparent, obj->mesh, obj->material, obj->transform);
        }
        m_stats.draw_calls++;
    }
}

void RenderPipeline::post_process_pass(const CameraData& camera) {
    TextureHandle hdr_tex = m_renderer->get_render_target_texture(m_hdr_target, 0);
    if (!hdr_tex.valid()) return;

    // Apply volumetric fog compositing
    // The volumetric texture contains: RGB = in-scattered light, A = transmission
    // Compositing formula: final = scene * transmission + in_scatter
    if (has_flag(m_config.enabled_passes, RenderPassFlags::Volumetric)) {
        TextureHandle vol_tex = m_volumetric_system.get_volumetric_texture();
        if (vol_tex.valid()) {
            // Set the volumetric texture for the post-process system to composite
            // The tone mapping shader will blend: hdr * vol.a + vol.rgb
            m_post_process_system.set_volumetric_texture(vol_tex);
        }
    }

    // Apply TAA
    if (has_flag(m_config.enabled_passes, RenderPassFlags::TAA) && m_config.taa_config.enabled) {
        TextureHandle depth_tex = get_depth_texture();
        TextureHandle motion_tex;
        if (m_motion_vectors.valid()) {
            motion_tex = m_renderer->get_render_target_texture(m_motion_vectors, 0);
        }
        TextureHandle resolved = m_taa_system.resolve(hdr_tex, depth_tex, motion_tex);
        if (resolved.valid()) {
            hdr_tex = resolved;
        }
    }

    // Render bloom and tonemapping to LDR target
    m_post_process_system.process(hdr_tex, m_ldr_target);
}

void RenderPipeline::debug_pass(const CameraData& camera) {
    if (!m_config.show_debug_overlay) return;

    // Debug rendering is handled by DebugDraw system
    m_renderer->flush_debug_draw(RenderView::Debug);
}

void RenderPipeline::final_pass() {
    // Copy LDR target to backbuffer
    TextureHandle ldr_tex = m_renderer->get_render_target_texture(m_ldr_target, 0);
    if (ldr_tex.valid()) {
        m_renderer->blit_to_screen(RenderView::Final, ldr_tex);
    }
}

// Helper functions

CameraData make_camera_data(
    const Vec3& position,
    const Vec3& target,
    const Vec3& up,
    float fov_y,
    float aspect_ratio,
    float near_plane,
    float far_plane
) {
    CameraData camera;

    camera.position = position;
    camera.forward = normalize(target - position);
    camera.up = normalize(up);
    camera.right = normalize(cross(camera.forward, camera.up));
    camera.up = cross(camera.right, camera.forward);  // Orthogonalize

    camera.near_plane = near_plane;
    camera.far_plane = far_plane;
    camera.fov_y = fov_y;
    camera.aspect_ratio = aspect_ratio;

    // Calculate view matrix
    camera.view_matrix = lookAt(position, target, up);
    camera.inverse_view = inverse(camera.view_matrix);

    // Calculate projection matrix
    camera.projection_matrix = glm::perspective(
        fov_y * 3.14159f / 180.0f,
        aspect_ratio,
        near_plane,
        far_plane
    );
    camera.inverse_projection = inverse(camera.projection_matrix);

    // Combined matrices
    camera.view_projection = camera.projection_matrix * camera.view_matrix;
    camera.inverse_view_projection = inverse(camera.view_projection);

    return camera;
}

LightData make_directional_light(
    const Vec3& direction,
    const Vec3& color,
    float intensity,
    bool casts_shadows
) {
    LightData light;
    light.type = 0;
    light.direction = normalize(direction);
    light.color = color;
    light.intensity = intensity;
    light.cast_shadows = casts_shadows;
    light.range = 0.0f;  // Infinite
    return light;
}

LightData make_point_light(
    const Vec3& position,
    const Vec3& color,
    float intensity,
    float range,
    bool casts_shadows
) {
    LightData light;
    light.type = 1;
    light.position = position;
    light.color = color;
    light.intensity = intensity;
    light.range = range;
    light.cast_shadows = casts_shadows;
    return light;
}

LightData make_spot_light(
    const Vec3& position,
    const Vec3& direction,
    const Vec3& color,
    float intensity,
    float range,
    float inner_angle,
    float outer_angle,
    bool casts_shadows
) {
    LightData light;
    light.type = 2;
    light.position = position;
    light.direction = normalize(direction);
    light.color = color;
    light.intensity = intensity;
    light.range = range;
    light.inner_angle = inner_angle;
    light.outer_angle = outer_angle;
    light.cast_shadows = casts_shadows;
    return light;
}

} // namespace engine::render
