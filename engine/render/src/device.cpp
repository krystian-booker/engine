#include <engine/render/device.hpp>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

namespace engine::render {

static uint32_t s_width = 0;
static uint32_t s_height = 0;

bool Device::init(const DeviceInit& config) {
    s_width = config.width;
    s_height = config.height;

    bgfx::PlatformData pd{};
    pd.nwh = config.native_window_handle;
    bgfx::setPlatformData(pd);

    bgfx::Init init;
    init.type = bgfx::RendererType::Count; // auto-select
    init.resolution.width = config.width;
    init.resolution.height = config.height;
    init.resolution.reset = config.vsync ? BGFX_RESET_VSYNC : BGFX_RESET_NONE;

    if (!bgfx::init(init)) {
        return false;
    }

    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
    bgfx::setViewRect(0, 0, 0, uint16_t(config.width), uint16_t(config.height));

    return true;
}

void Device::shutdown() {
    bgfx::shutdown();
}

void Device::begin_frame() {
    bgfx::touch(0);
}

void Device::end_frame() {
    bgfx::frame();
}

void Device::resize(uint32_t width, uint32_t height) {
    s_width = width;
    s_height = height;
    bgfx::reset(width, height, BGFX_RESET_VSYNC);
    bgfx::setViewRect(0, 0, 0, uint16_t(width), uint16_t(height));
}

} // namespace engine::render
