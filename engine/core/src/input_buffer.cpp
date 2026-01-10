#include <engine/core/input_buffer.hpp>
#include <algorithm>
#include <memory>

namespace engine::core {

InputBuffer::InputBuffer(Config config)
    : m_config(config) {
    m_buffer.reserve(m_config.max_buffered_inputs);
}

void InputBuffer::buffer(const std::string& action, float duration) {
    // Use default duration if not specified
    float actual_duration = (duration < 0.0f) ? m_config.default_buffer_duration : duration;

    // Check for duplicates if not allowed
    if (!m_config.allow_duplicates) {
        auto it = std::find_if(m_buffer.begin(), m_buffer.end(),
            [&action](const BufferedInput& input) {
                return input.action == action;
            });

        if (it != m_buffer.end()) {
            // Refresh the existing entry's duration
            it->time_remaining = actual_duration;
            return;
        }
    }

    // Remove oldest if at capacity
    if (m_buffer.size() >= m_config.max_buffered_inputs) {
        m_buffer.erase(m_buffer.begin());
    }

    m_buffer.push_back({action, actual_duration});
}

bool InputBuffer::has(const std::string& action) const {
    return std::any_of(m_buffer.begin(), m_buffer.end(),
        [&action](const BufferedInput& input) {
            return input.action == action;
        });
}

std::string InputBuffer::peek() const {
    if (m_buffer.empty()) {
        return {};
    }
    return m_buffer.front().action;
}

bool InputBuffer::consume(const std::string& action) {
    auto it = std::find_if(m_buffer.begin(), m_buffer.end(),
        [&action](const BufferedInput& input) {
            return input.action == action;
        });

    if (it != m_buffer.end()) {
        m_buffer.erase(it);
        return true;
    }

    return false;
}

std::string InputBuffer::consume_oldest() {
    if (m_buffer.empty()) {
        return {};
    }

    std::string action = std::move(m_buffer.front().action);
    m_buffer.erase(m_buffer.begin());
    return action;
}

void InputBuffer::clear(const std::string& action) {
    m_buffer.erase(
        std::remove_if(m_buffer.begin(), m_buffer.end(),
            [&action](const BufferedInput& input) {
                return input.action == action;
            }),
        m_buffer.end());
}

void InputBuffer::clear_all() {
    m_buffer.clear();
}

void InputBuffer::update(float dt) {
    // Update timers and remove expired inputs
    m_buffer.erase(
        std::remove_if(m_buffer.begin(), m_buffer.end(),
            [dt](BufferedInput& input) {
                input.time_remaining -= dt;
                return input.time_remaining <= 0.0f;
            }),
        m_buffer.end());
}

size_t InputBuffer::count() const {
    return m_buffer.size();
}

bool InputBuffer::empty() const {
    return m_buffer.empty();
}

std::vector<std::string> InputBuffer::get_all() const {
    std::vector<std::string> result;
    result.reserve(m_buffer.size());
    for (const auto& input : m_buffer) {
        result.push_back(input.action);
    }
    return result;
}

// Global instance management
namespace {
    std::unique_ptr<InputBuffer> g_input_buffer;
}

InputBuffer& input_buffer() {
    if (!g_input_buffer) {
        g_input_buffer = std::make_unique<InputBuffer>();
    }
    return *g_input_buffer;
}

void init_input_buffer(InputBuffer::Config config) {
    g_input_buffer = std::make_unique<InputBuffer>(config);
}

void shutdown_input_buffer() {
    g_input_buffer.reset();
}

} // namespace engine::core
