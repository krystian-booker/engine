#include <engine/render/renderer.hpp>
#include <engine/core/log.hpp>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <algorithm>
#include <unordered_map>
#include <cmath>

namespace engine::render {

using namespace engine::core;

// Vertex layout for our Vertex struct
static bgfx::VertexLayout s_vertex_layout;

class BGFXRenderer : public IRenderer {
public:
    ~BGFXRenderer() override {
        if (m_initialized) {
            shutdown();
        }
    }

    bool init(void* native_window_handle, uint32_t width, uint32_t height) override {
        m_width = width;
        m_height = height;

        bgfx::PlatformData pd{};
        pd.nwh = native_window_handle;
        bgfx::setPlatformData(pd);

        bgfx::Init init;
        init.type = bgfx::RendererType::Count;  // Auto-select
        init.resolution.width = width;
        init.resolution.height = height;
        init.resolution.reset = m_vsync ? BGFX_RESET_VSYNC : BGFX_RESET_NONE;

        if (!bgfx::init(init)) {
            log(LogLevel::Error, "Failed to initialize BGFX");
            return false;
        }

        // Initialize vertex layout
        s_vertex_layout
            .begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Tangent, 3, bgfx::AttribType::Float)
            .end();

        // Set view 0 as default
        bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
        bgfx::setViewRect(0, 0, 0, uint16_t(width), uint16_t(height));

        m_initialized = true;
        return true;
    }

    void shutdown() override {
        // Destroy all resources
        for (auto& [id, handle] : m_meshes) {
            bgfx::destroy(handle.vbh);
            if (bgfx::isValid(handle.ibh)) {
                bgfx::destroy(handle.ibh);
            }
        }
        m_meshes.clear();

        for (auto& [id, handle] : m_textures) {
            bgfx::destroy(handle);
        }
        m_textures.clear();

        for (auto& [id, handle] : m_shaders) {
            bgfx::destroy(handle);
        }
        m_shaders.clear();

        bgfx::shutdown();
        m_initialized = false;
    }

    void begin_frame() override {
        bgfx::touch(0);
    }

    void end_frame() override {
        bgfx::frame();
    }

    void resize(uint32_t width, uint32_t height) override {
        m_width = width;
        m_height = height;
        uint32_t flags = m_vsync ? BGFX_RESET_VSYNC : BGFX_RESET_NONE;
        bgfx::reset(width, height, flags);
        bgfx::setViewRect(0, 0, 0, uint16_t(width), uint16_t(height));
    }

    MeshHandle create_mesh(const MeshData& data) override {
        if (data.vertices.empty()) {
            return MeshHandle{};
        }

        MeshHandle handle{m_next_mesh_id++};

        BGFXMesh mesh;
        mesh.vertex_count = static_cast<uint32_t>(data.vertices.size());
        mesh.index_count = static_cast<uint32_t>(data.indices.size());
        mesh.bounds = data.bounds;

        // Create vertex buffer
        const bgfx::Memory* vb_mem = bgfx::copy(
            data.vertices.data(),
            static_cast<uint32_t>(data.vertices.size() * sizeof(Vertex))
        );
        mesh.vbh = bgfx::createVertexBuffer(vb_mem, s_vertex_layout);

        // Create index buffer if we have indices
        if (!data.indices.empty()) {
            const bgfx::Memory* ib_mem = bgfx::copy(
                data.indices.data(),
                static_cast<uint32_t>(data.indices.size() * sizeof(uint32_t))
            );
            mesh.ibh = bgfx::createIndexBuffer(ib_mem, BGFX_BUFFER_INDEX32);
        }

        m_meshes[handle.id] = mesh;
        return handle;
    }

    TextureHandle create_texture(const TextureData& data) override {
        if (data.pixels.empty()) {
            return TextureHandle{};
        }

        TextureHandle handle{m_next_texture_id++};

        bgfx::TextureFormat::Enum format = bgfx::TextureFormat::RGBA8;
        switch (data.format) {
            case TextureFormat::RGBA8: format = bgfx::TextureFormat::RGBA8; break;
            case TextureFormat::RGBA16F: format = bgfx::TextureFormat::RGBA16F; break;
            case TextureFormat::RGBA32F: format = bgfx::TextureFormat::RGBA32F; break;
            case TextureFormat::R8: format = bgfx::TextureFormat::R8; break;
            case TextureFormat::RG8: format = bgfx::TextureFormat::RG8; break;
            case TextureFormat::Depth24: format = bgfx::TextureFormat::D24; break;
            case TextureFormat::Depth32F: format = bgfx::TextureFormat::D32F; break;
            case TextureFormat::BC1: format = bgfx::TextureFormat::BC1; break;
            case TextureFormat::BC3: format = bgfx::TextureFormat::BC3; break;
            case TextureFormat::BC7: format = bgfx::TextureFormat::BC7; break;
        }

        const bgfx::Memory* mem = bgfx::copy(data.pixels.data(), static_cast<uint32_t>(data.pixels.size()));

        bgfx::TextureHandle th;
        if (data.is_cubemap) {
            th = bgfx::createTextureCube(
                uint16_t(data.width),
                data.mip_levels > 1,
                1, format,
                BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE,
                mem
            );
        } else {
            th = bgfx::createTexture2D(
                uint16_t(data.width),
                uint16_t(data.height),
                data.mip_levels > 1,
                1, format,
                BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE,
                mem
            );
        }

        m_textures[handle.id] = th;
        return handle;
    }

    ShaderHandle create_shader(const ShaderData& data) override {
        if (data.vertex_binary.empty() || data.fragment_binary.empty()) {
            return ShaderHandle{};
        }

        ShaderHandle handle{m_next_shader_id++};

        const bgfx::Memory* vs_mem = bgfx::copy(data.vertex_binary.data(), static_cast<uint32_t>(data.vertex_binary.size()));
        const bgfx::Memory* fs_mem = bgfx::copy(data.fragment_binary.data(), static_cast<uint32_t>(data.fragment_binary.size()));

        bgfx::ShaderHandle vsh = bgfx::createShader(vs_mem);
        bgfx::ShaderHandle fsh = bgfx::createShader(fs_mem);

        bgfx::ProgramHandle program = bgfx::createProgram(vsh, fsh, true);
        m_shaders[handle.id] = program;

        return handle;
    }

    MaterialHandle create_material(const MaterialData& /*data*/) override {
        // Simplified for now - just return a valid handle
        MaterialHandle handle{m_next_material_id++};
        m_materials[handle.id] = MaterialData{};
        return handle;
    }

    MeshHandle create_primitive(PrimitiveMesh type, float size) override {
        MeshData data;

        switch (type) {
            case PrimitiveMesh::Cube:
                data = create_cube_mesh(size);
                break;
            case PrimitiveMesh::Sphere:
                data = create_sphere_mesh(size, 32, 16);
                break;
            case PrimitiveMesh::Plane:
                data = create_plane_mesh(size);
                break;
            case PrimitiveMesh::Quad:
                data = create_quad_mesh(size);
                break;
            default:
                data = create_cube_mesh(size);
                break;
        }

        return create_mesh(data);
    }

    void destroy_mesh(MeshHandle h) override {
        auto it = m_meshes.find(h.id);
        if (it != m_meshes.end()) {
            bgfx::destroy(it->second.vbh);
            if (bgfx::isValid(it->second.ibh)) {
                bgfx::destroy(it->second.ibh);
            }
            m_meshes.erase(it);
        }
    }

    void destroy_texture(TextureHandle h) override {
        auto it = m_textures.find(h.id);
        if (it != m_textures.end()) {
            bgfx::destroy(it->second);
            m_textures.erase(it);
        }
    }

    void destroy_shader(ShaderHandle h) override {
        auto it = m_shaders.find(h.id);
        if (it != m_shaders.end()) {
            bgfx::destroy(it->second);
            m_shaders.erase(it);
        }
    }

    void destroy_material(MaterialHandle h) override {
        m_materials.erase(h.id);
    }

    void queue_draw(const DrawCall& call) override {
        m_draw_queue.push_back(call);
    }

    void set_camera(const Mat4& view, const Mat4& proj) override {
        m_view_matrix = view;
        m_proj_matrix = proj;
        bgfx::setViewTransform(0, glm::value_ptr(view), glm::value_ptr(proj));
    }

    void set_light(uint32_t index, const LightData& light) override {
        if (index < 8) {
            m_lights[index] = light;
        }
    }

    void clear_lights() override {
        for (auto& light : m_lights) {
            light = LightData{};
        }
    }

    void flush() override {
        // Sort draw calls by material then mesh for batching
        std::sort(m_draw_queue.begin(), m_draw_queue.end(),
            [](const DrawCall& a, const DrawCall& b) {
                if (a.material.id != b.material.id) return a.material.id < b.material.id;
                return a.mesh.id < b.mesh.id;
            });

        // Submit draw calls
        for (const auto& call : m_draw_queue) {
            auto mesh_it = m_meshes.find(call.mesh.id);
            if (mesh_it == m_meshes.end()) continue;

            const auto& mesh = mesh_it->second;

            // Set transform
            bgfx::setTransform(glm::value_ptr(call.transform));

            // Set vertex buffer
            bgfx::setVertexBuffer(0, mesh.vbh);

            // Set index buffer if available
            if (bgfx::isValid(mesh.ibh)) {
                bgfx::setIndexBuffer(mesh.ibh);
            }

            // Set state
            uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                             BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS |
                             BGFX_STATE_CULL_CW | BGFX_STATE_MSAA;

            bgfx::setState(state);

            // Submit (using view 0, no program for now - debug rendering)
            bgfx::submit(0, BGFX_INVALID_HANDLE);
        }

        m_draw_queue.clear();
    }

    void clear(uint32_t color, float depth) override {
        bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, color, depth, 0);
    }

    uint32_t get_width() const override { return m_width; }
    uint32_t get_height() const override { return m_height; }

    void set_vsync(bool enabled) override {
        m_vsync = enabled;
        uint32_t flags = enabled ? BGFX_RESET_VSYNC : BGFX_RESET_NONE;
        bgfx::reset(m_width, m_height, flags);
    }

    bool get_vsync() const override { return m_vsync; }

private:
    // Internal mesh structure
    struct BGFXMesh {
        bgfx::VertexBufferHandle vbh = BGFX_INVALID_HANDLE;
        bgfx::IndexBufferHandle ibh = BGFX_INVALID_HANDLE;
        uint32_t vertex_count = 0;
        uint32_t index_count = 0;
        AABB bounds;
    };

    // Primitive mesh generators
    MeshData create_cube_mesh(float size) {
        MeshData data;
        float h = size * 0.5f;

        // 24 vertices (4 per face for proper normals)
        data.vertices = {
            // Front face
            {{-h, -h,  h}, {0, 0, 1}, {0, 0}, {1, 1, 1, 1}, {1, 0, 0}},
            {{ h, -h,  h}, {0, 0, 1}, {1, 0}, {1, 1, 1, 1}, {1, 0, 0}},
            {{ h,  h,  h}, {0, 0, 1}, {1, 1}, {1, 1, 1, 1}, {1, 0, 0}},
            {{-h,  h,  h}, {0, 0, 1}, {0, 1}, {1, 1, 1, 1}, {1, 0, 0}},
            // Back face
            {{ h, -h, -h}, {0, 0, -1}, {0, 0}, {1, 1, 1, 1}, {-1, 0, 0}},
            {{-h, -h, -h}, {0, 0, -1}, {1, 0}, {1, 1, 1, 1}, {-1, 0, 0}},
            {{-h,  h, -h}, {0, 0, -1}, {1, 1}, {1, 1, 1, 1}, {-1, 0, 0}},
            {{ h,  h, -h}, {0, 0, -1}, {0, 1}, {1, 1, 1, 1}, {-1, 0, 0}},
            // Top face
            {{-h,  h,  h}, {0, 1, 0}, {0, 0}, {1, 1, 1, 1}, {1, 0, 0}},
            {{ h,  h,  h}, {0, 1, 0}, {1, 0}, {1, 1, 1, 1}, {1, 0, 0}},
            {{ h,  h, -h}, {0, 1, 0}, {1, 1}, {1, 1, 1, 1}, {1, 0, 0}},
            {{-h,  h, -h}, {0, 1, 0}, {0, 1}, {1, 1, 1, 1}, {1, 0, 0}},
            // Bottom face
            {{-h, -h, -h}, {0, -1, 0}, {0, 0}, {1, 1, 1, 1}, {1, 0, 0}},
            {{ h, -h, -h}, {0, -1, 0}, {1, 0}, {1, 1, 1, 1}, {1, 0, 0}},
            {{ h, -h,  h}, {0, -1, 0}, {1, 1}, {1, 1, 1, 1}, {1, 0, 0}},
            {{-h, -h,  h}, {0, -1, 0}, {0, 1}, {1, 1, 1, 1}, {1, 0, 0}},
            // Right face
            {{ h, -h,  h}, {1, 0, 0}, {0, 0}, {1, 1, 1, 1}, {0, 0, -1}},
            {{ h, -h, -h}, {1, 0, 0}, {1, 0}, {1, 1, 1, 1}, {0, 0, -1}},
            {{ h,  h, -h}, {1, 0, 0}, {1, 1}, {1, 1, 1, 1}, {0, 0, -1}},
            {{ h,  h,  h}, {1, 0, 0}, {0, 1}, {1, 1, 1, 1}, {0, 0, -1}},
            // Left face
            {{-h, -h, -h}, {-1, 0, 0}, {0, 0}, {1, 1, 1, 1}, {0, 0, 1}},
            {{-h, -h,  h}, {-1, 0, 0}, {1, 0}, {1, 1, 1, 1}, {0, 0, 1}},
            {{-h,  h,  h}, {-1, 0, 0}, {1, 1}, {1, 1, 1, 1}, {0, 0, 1}},
            {{-h,  h, -h}, {-1, 0, 0}, {0, 1}, {1, 1, 1, 1}, {0, 0, 1}},
        };

        // 36 indices (6 per face)
        data.indices = {
            0, 1, 2, 0, 2, 3,       // Front
            4, 5, 6, 4, 6, 7,       // Back
            8, 9, 10, 8, 10, 11,    // Top
            12, 13, 14, 12, 14, 15, // Bottom
            16, 17, 18, 16, 18, 19, // Right
            20, 21, 22, 20, 22, 23  // Left
        };

        data.bounds = AABB{{-h, -h, -h}, {h, h, h}};
        return data;
    }

    MeshData create_sphere_mesh(float radius, int segments, int rings) {
        MeshData data;

        for (int ring = 0; ring <= rings; ++ring) {
            float theta = static_cast<float>(ring) * 3.14159265f / static_cast<float>(rings);
            float sin_theta = std::sin(theta);
            float cos_theta = std::cos(theta);

            for (int seg = 0; seg <= segments; ++seg) {
                float phi = static_cast<float>(seg) * 2.0f * 3.14159265f / static_cast<float>(segments);
                float sin_phi = std::sin(phi);
                float cos_phi = std::cos(phi);

                Vec3 normal{sin_theta * cos_phi, cos_theta, sin_theta * sin_phi};
                Vec3 pos = normal * radius;
                Vec2 uv{static_cast<float>(seg) / segments, static_cast<float>(ring) / rings};

                data.vertices.push_back({pos, normal, uv, {1, 1, 1, 1}, {-sin_phi, 0, cos_phi}});
            }
        }

        for (int ring = 0; ring < rings; ++ring) {
            for (int seg = 0; seg < segments; ++seg) {
                int a = ring * (segments + 1) + seg;
                int b = a + segments + 1;

                data.indices.push_back(a);
                data.indices.push_back(b);
                data.indices.push_back(a + 1);

                data.indices.push_back(b);
                data.indices.push_back(b + 1);
                data.indices.push_back(a + 1);
            }
        }

        data.bounds = AABB{{-radius, -radius, -radius}, {radius, radius, radius}};
        return data;
    }

    MeshData create_plane_mesh(float size) {
        MeshData data;
        float h = size * 0.5f;

        data.vertices = {
            {{-h, 0, -h}, {0, 1, 0}, {0, 0}, {1, 1, 1, 1}, {1, 0, 0}},
            {{ h, 0, -h}, {0, 1, 0}, {1, 0}, {1, 1, 1, 1}, {1, 0, 0}},
            {{ h, 0,  h}, {0, 1, 0}, {1, 1}, {1, 1, 1, 1}, {1, 0, 0}},
            {{-h, 0,  h}, {0, 1, 0}, {0, 1}, {1, 1, 1, 1}, {1, 0, 0}},
        };

        data.indices = {0, 1, 2, 0, 2, 3};
        data.bounds = AABB{{-h, 0, -h}, {h, 0, h}};
        return data;
    }

    MeshData create_quad_mesh(float size) {
        MeshData data;
        float h = size * 0.5f;

        data.vertices = {
            {{-h, -h, 0}, {0, 0, 1}, {0, 0}, {1, 1, 1, 1}, {1, 0, 0}},
            {{ h, -h, 0}, {0, 0, 1}, {1, 0}, {1, 1, 1, 1}, {1, 0, 0}},
            {{ h,  h, 0}, {0, 0, 1}, {1, 1}, {1, 1, 1, 1}, {1, 0, 0}},
            {{-h,  h, 0}, {0, 0, 1}, {0, 1}, {1, 1, 1, 1}, {1, 0, 0}},
        };

        data.indices = {0, 1, 2, 0, 2, 3};
        data.bounds = AABB{{-h, -h, 0}, {h, h, 0}};
        return data;
    }

    // State
    bool m_initialized = false;
    bool m_vsync = true;
    uint32_t m_width = 0;
    uint32_t m_height = 0;

    // Resources
    uint32_t m_next_mesh_id = 1;
    uint32_t m_next_texture_id = 1;
    uint32_t m_next_shader_id = 1;
    uint32_t m_next_material_id = 1;

    std::unordered_map<uint32_t, BGFXMesh> m_meshes;
    std::unordered_map<uint32_t, bgfx::TextureHandle> m_textures;
    std::unordered_map<uint32_t, bgfx::ProgramHandle> m_shaders;
    std::unordered_map<uint32_t, MaterialData> m_materials;

    // Camera
    Mat4 m_view_matrix{1.0f};
    Mat4 m_proj_matrix{1.0f};

    // Lights
    LightData m_lights[8]{};

    // Draw queue
    std::vector<DrawCall> m_draw_queue;
};

std::unique_ptr<IRenderer> create_bgfx_renderer() {
    return std::make_unique<BGFXRenderer>();
}

} // namespace engine::render
