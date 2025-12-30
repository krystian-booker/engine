#pragma once

#include <cstdint>

namespace engine::render {

struct DeviceInit {
    void* native_window_handle = nullptr;
    uint32_t width = 1280;
    uint32_t height = 720;
    bool vsync = true;
};

struct Device {
    static bool init(const DeviceInit& config);
    static void shutdown();
    static void begin_frame();
    static void end_frame();
    static void resize(uint32_t width, uint32_t height);
};

} // namespace engine::render
