#pragma once

#include <string>
#include <vector>
#include <cstddef>

namespace engine::core {

// Generic input buffer for action buffering
// Allows buffering any action (string-based) for a configurable duration
// Useful for: attack combos, interaction queuing, ability casting
class InputBuffer {
public:
    struct Config {
        float default_buffer_duration = 0.15f;  // How long inputs stay buffered
        size_t max_buffered_inputs = 8;         // Maximum buffered actions
        bool allow_duplicates = false;          // Allow same action multiple times
    };

    explicit InputBuffer(Config config = {});
    ~InputBuffer() = default;

    // Non-copyable but movable
    InputBuffer(const InputBuffer&) = delete;
    InputBuffer& operator=(const InputBuffer&) = delete;
    InputBuffer(InputBuffer&&) = default;
    InputBuffer& operator=(InputBuffer&&) = default;

    // Buffer an action for later consumption
    // duration: how long to buffer (-1 uses default_buffer_duration)
    void buffer(const std::string& action, float duration = -1.0f);

    // Check if an action is currently buffered (without consuming)
    bool has(const std::string& action) const;

    // Peek at the oldest buffered action (without consuming)
    // Returns empty string if buffer is empty
    std::string peek() const;

    // Consume (remove and return true if found) a buffered action
    bool consume(const std::string& action);

    // Consume the oldest buffered action
    // Returns empty string if buffer is empty
    std::string consume_oldest();

    // Clear a specific action from buffer
    void clear(const std::string& action);

    // Clear all buffered actions
    void clear_all();

    // Update timers (call each frame with delta time)
    void update(float dt);

    // Query state
    size_t count() const;
    bool empty() const;
    const Config& get_config() const { return m_config; }

    // Get all currently buffered actions (for debugging/UI)
    std::vector<std::string> get_all() const;

private:
    struct BufferedInput {
        std::string action;
        float time_remaining;
    };

    Config m_config;
    std::vector<BufferedInput> m_buffer;
};

// Global input buffer instance
InputBuffer& input_buffer();

// Initialize global input buffer with custom config
void init_input_buffer(InputBuffer::Config config = {});

// Shutdown global input buffer
void shutdown_input_buffer();

} // namespace engine::core
