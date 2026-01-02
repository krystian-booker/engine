#include <engine/debug-gui/imgui_layer.hpp>
#include <engine/core/log.hpp>

#include <bgfx/bgfx.h>
#include <bx/math.h>

// ImGui headers from bgfx's 3rdparty
#include <imgui.h>

#include <fstream>
#include <string>

namespace engine::debug_gui {

namespace {

// Helper function to load shader binary from file
bgfx::ShaderHandle load_shader_from_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return BGFX_INVALID_HANDLE;
    }

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    const bgfx::Memory* mem = bgfx::alloc(static_cast<uint32_t>(size) + 1);
    file.read(reinterpret_cast<char*>(mem->data), size);
    mem->data[size] = '\0';

    return bgfx::createShader(mem);
}

// Get shader path based on renderer type
std::string get_shader_path() {
    bgfx::RendererType::Enum renderer = bgfx::getRendererType();
    std::string shader_dir = "shaders/";

    switch (renderer) {
        case bgfx::RendererType::Direct3D11:
        case bgfx::RendererType::Direct3D12:
            shader_dir += "dx11/";
            break;
        case bgfx::RendererType::OpenGL:
            shader_dir += "glsl/";
            break;
        case bgfx::RendererType::Vulkan:
            shader_dir += "spirv/";
            break;
        case bgfx::RendererType::Metal:
            shader_dir += "metal/";
            break;
        default:
            shader_dir += "dx11/";
            break;
    }

    return shader_dir;
}

} // anonymous namespace

ImGuiLayer::ImGuiLayer() = default;

ImGuiLayer::~ImGuiLayer() {
    shutdown();
}

bool ImGuiLayer::init(void* /*native_window_handle*/, uint32_t width, uint32_t height) {
    if (m_initialized) {
        return true;
    }

    m_width = width;
    m_height = height;

    // Create ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    // Create vertex layout for ImGui
    m_vertex_layout
        .begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();

    // Create uniform for texture sampler
    m_u_texture = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);

    // Load shader program
    std::string shader_path = get_shader_path();
    bgfx::ShaderHandle vsh = load_shader_from_file(shader_path + "vs_imgui.sc.bin");
    bgfx::ShaderHandle fsh = load_shader_from_file(shader_path + "fs_imgui.sc.bin");

    if (bgfx::isValid(vsh) && bgfx::isValid(fsh)) {
        m_program = bgfx::createProgram(vsh, fsh, true);
        core::log(core::LogLevel::Info, "ImGui shader program loaded");
    } else {
        core::log(core::LogLevel::Warn, "Failed to load ImGui shaders - debug GUI will not render");
        if (bgfx::isValid(vsh)) bgfx::destroy(vsh);
        if (bgfx::isValid(fsh)) bgfx::destroy(fsh);
    }

    // Setup style
    setup_style();

    // Build fonts
    build_fonts();

    m_initialized = true;
    core::log(core::LogLevel::Info, "ImGuiLayer initialized");

    return true;
}

void ImGuiLayer::shutdown() {
    if (!m_initialized) {
        return;
    }

    destroy_font_texture();

    if (bgfx::isValid(m_u_texture)) {
        bgfx::destroy(m_u_texture);
        m_u_texture = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(m_program)) {
        bgfx::destroy(m_program);
        m_program = BGFX_INVALID_HANDLE;
    }

    ImGui::DestroyContext();

    m_initialized = false;
    core::log(core::LogLevel::Info, "ImGuiLayer shutdown");
}

void ImGuiLayer::resize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;

    if (m_initialized) {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
    }
}

void ImGuiLayer::begin_frame(float dt) {
    if (!m_initialized) return;

    ImGuiIO& io = ImGui::GetIO();
    io.DeltaTime = dt > 0.0f ? dt : 1.0f / 60.0f;
    ImGui::NewFrame();
}

void ImGuiLayer::end_frame() {
    if (!m_initialized) return;

    ImGui::EndFrame();
    ImGui::Render();
}

void ImGuiLayer::render(render::RenderView view) {
    if (!m_initialized) return;

    ImDrawData* draw_data = ImGui::GetDrawData();
    if (!draw_data || draw_data->TotalVtxCount == 0) {
        return;
    }

    render_draw_data(draw_data, view);
}

void ImGuiLayer::render_draw_data(ImDrawData* draw_data, render::RenderView view) {
    const bgfx::ViewId view_id = static_cast<bgfx::ViewId>(view);

    // Avoid rendering when minimized
    int fb_width = static_cast<int>(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    int fb_height = static_cast<int>(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0) {
        return;
    }

    // Setup orthographic projection matrix
    const float L = draw_data->DisplayPos.x;
    const float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    const float T = draw_data->DisplayPos.y;
    const float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;

    float ortho[16];
    bx::mtxOrtho(ortho, L, R, B, T, 0.0f, 1000.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);

    bgfx::setViewTransform(view_id, nullptr, ortho);
    bgfx::setViewRect(view_id, 0, 0, static_cast<uint16_t>(fb_width), static_cast<uint16_t>(fb_height));

    const ImVec2 clip_off = draw_data->DisplayPos;
    const ImVec2 clip_scale = draw_data->FramebufferScale;

    // Render command lists
    for (int n = 0; n < draw_data->CmdListsCount; ++n) {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];

        uint32_t num_vertices = static_cast<uint32_t>(cmd_list->VtxBuffer.Size);
        uint32_t num_indices = static_cast<uint32_t>(cmd_list->IdxBuffer.Size);

        // Check if we have enough transient buffer space
        if (!bgfx::getAvailTransientVertexBuffer(num_vertices, m_vertex_layout)) {
            break;
        }
        if (!bgfx::getAvailTransientIndexBuffer(num_indices, sizeof(ImDrawIdx) == 4)) {
            break;
        }

        bgfx::TransientVertexBuffer tvb;
        bgfx::TransientIndexBuffer tib;

        bgfx::allocTransientVertexBuffer(&tvb, num_vertices, m_vertex_layout);
        bgfx::allocTransientIndexBuffer(&tib, num_indices, sizeof(ImDrawIdx) == 4);

        // Copy vertex and index data
        ImDrawVert* verts = reinterpret_cast<ImDrawVert*>(tvb.data);
        memcpy(verts, cmd_list->VtxBuffer.Data, num_vertices * sizeof(ImDrawVert));

        ImDrawIdx* indices = reinterpret_cast<ImDrawIdx*>(tib.data);
        memcpy(indices, cmd_list->IdxBuffer.Data, num_indices * sizeof(ImDrawIdx));

        uint32_t offset = 0;
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; ++cmd_i) {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];

            if (pcmd->UserCallback) {
                // User callback (used for custom drawing)
                pcmd->UserCallback(cmd_list, pcmd);
            } else {
                // Calculate scissor rect
                ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x,
                               (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x,
                               (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

                // Clamp to viewport
                if (clip_min.x < 0.0f) clip_min.x = 0.0f;
                if (clip_min.y < 0.0f) clip_min.y = 0.0f;
                if (clip_max.x > static_cast<float>(fb_width)) clip_max.x = static_cast<float>(fb_width);
                if (clip_max.y > static_cast<float>(fb_height)) clip_max.y = static_cast<float>(fb_height);

                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y) {
                    offset += pcmd->ElemCount;
                    continue;
                }

                // Set scissor
                bgfx::setScissor(
                    static_cast<uint16_t>(clip_min.x),
                    static_cast<uint16_t>(clip_min.y),
                    static_cast<uint16_t>(clip_max.x - clip_min.x),
                    static_cast<uint16_t>(clip_max.y - clip_min.y)
                );

                // Bind texture
                bgfx::TextureHandle tex = m_font_texture;
                if (pcmd->GetTexID()) {
                    tex.idx = static_cast<uint16_t>((uintptr_t)pcmd->GetTexID());
                }

                if (bgfx::isValid(tex)) {
                    bgfx::setTexture(0, m_u_texture, tex);
                }

                // Set buffers
                bgfx::setVertexBuffer(0, &tvb, pcmd->VtxOffset, num_vertices);
                bgfx::setIndexBuffer(&tib, pcmd->IdxOffset, pcmd->ElemCount);

                // Set render state
                uint64_t state = BGFX_STATE_WRITE_RGB
                    | BGFX_STATE_WRITE_A
                    | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);

                bgfx::setState(state);

                // Submit
                if (bgfx::isValid(m_program)) {
                    bgfx::submit(view_id, m_program);
                } else {
                    // Without a valid program, we can't render
                    // This is expected until shaders are properly loaded
                }
            }

            offset += pcmd->ElemCount;
        }
    }
}

void ImGuiLayer::process_input(const ImGuiInputEvent& event) {
    if (!m_initialized) return;

    ImGuiIO& io = ImGui::GetIO();

    switch (event.type) {
        case ImGuiInputEvent::Type::MouseMove:
            io.AddMousePosEvent(event.mouse_x, event.mouse_y);
            break;

        case ImGuiInputEvent::Type::MouseButton:
            io.AddMouseButtonEvent(event.button, event.button_down);
            break;

        case ImGuiInputEvent::Type::MouseScroll:
            io.AddMouseWheelEvent(event.scroll_x, event.scroll_y);
            break;

        case ImGuiInputEvent::Type::Key:
            // Map engine keys to ImGui keys
            // This is a simplified mapping - extend as needed
            io.KeyCtrl = event.ctrl;
            io.KeyShift = event.shift;
            io.KeyAlt = event.alt;
            break;

        case ImGuiInputEvent::Type::Char:
            io.AddInputCharacter(event.character);
            break;
    }
}

bool ImGuiLayer::wants_capture_mouse() const {
    if (!m_initialized) return false;
    return ImGui::GetIO().WantCaptureMouse;
}

bool ImGuiLayer::wants_capture_keyboard() const {
    if (!m_initialized) return false;
    return ImGui::GetIO().WantCaptureKeyboard;
}

void ImGuiLayer::add_font(const char* path, float size_pixels) {
    if (!m_initialized) return;

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF(path, size_pixels);
}

void ImGuiLayer::build_fonts() {
    ImGuiIO& io = ImGui::GetIO();

    // Add default font if none added
    if (io.Fonts->Fonts.empty()) {
        io.Fonts->AddFontDefault();
    }

    // io.Fonts->Build(); // Not needed in 1.92+
    create_font_texture();
}

void ImGuiLayer::setup_style() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Dark theme with slightly rounded corners
    style.WindowRounding = 4.0f;
    style.FrameRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.ScrollbarRounding = 4.0f;
    style.TabRounding = 4.0f;

    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;

    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.FramePadding = ImVec2(6.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 4.0f);
    style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);

    // Colors - dark theme
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.12f, 0.94f);
    colors[ImGuiCol_Border] = ImVec4(0.30f, 0.30f, 0.35f, 0.50f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.24f, 0.28f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.40f, 0.60f, 0.55f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.52f, 0.78f, 0.80f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.52f, 0.78f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.40f, 0.60f, 0.62f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.52f, 0.78f, 0.79f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.26f, 0.52f, 0.78f, 1.00f);
    // Tab colors (commented out as they might not be available in minimized ImGui params)
    // colors[ImGuiCol_Tab] = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);
    // colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.52f, 0.78f, 0.80f);
    // colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.40f, 0.60f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.12f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.45f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.55f, 1.00f);
}

void ImGuiLayer::create_font_texture() {
    destroy_font_texture();

    ImGuiIO& io = ImGui::GetIO();
    
    // Use ImTextureData directly as GetTexDataAsRGBA32 is obsolete/disabled
    ImTextureData* tex_data = io.Fonts->TexData;
    IM_ASSERT(tex_data != nullptr);
    
    unsigned char* pixels = tex_data->Pixels;
    int width = tex_data->Width;
    int height = tex_data->Height;

    m_font_texture = bgfx::createTexture2D(
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height),
        false,  // no mipmaps
        1,      // single layer
        bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT,
        bgfx::copy(pixels, width * height * 4)
    );

    tex_data->SetTexID((ImTextureID)(uintptr_t)m_font_texture.idx);
}

void ImGuiLayer::destroy_font_texture() {
    if (bgfx::isValid(m_font_texture)) {
        bgfx::destroy(m_font_texture);
        m_font_texture = BGFX_INVALID_HANDLE;
    }
}

} // namespace engine::debug_gui
