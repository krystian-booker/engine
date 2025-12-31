#pragma once

namespace engine::core {

// Fixed timestep accumulator for deterministic game simulation
struct GameClock {
    double fixed_dt;           // Fixed timestep duration (from ProjectSettings)
    double max_accumulator;    // Maximum accumulator value to prevent spiral of death
    double accumulator;        // Accumulated time for fixed updates
    double alpha;              // Interpolation factor for rendering (0 to 1)

    explicit GameClock(double timestep = 1.0 / 60.0)
        : fixed_dt(timestep)
        , max_accumulator(0.25)
        , accumulator(0.0)
        , alpha(0.0)
    {}

    // Update the accumulator with delta time
    void update(double dt) {
        accumulator += dt;
        // Clamp to prevent spiral of death
        if (accumulator > max_accumulator) {
            accumulator = max_accumulator;
        }
    }

    // Returns true if a fixed update tick should run
    // Call in a while loop until it returns false
    bool consume_tick() {
        if (accumulator >= fixed_dt) {
            accumulator -= fixed_dt;
            return true;
        }
        // Calculate interpolation factor for rendering
        alpha = accumulator / fixed_dt;
        return false;
    }

    // Get interpolation alpha for smooth rendering between fixed updates
    double get_alpha() const {
        return alpha;
    }

    // Reset the accumulator
    void reset() {
        accumulator = 0.0;
        alpha = 0.0;
    }
};

} // namespace engine::core
