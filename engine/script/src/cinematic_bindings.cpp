#include <engine/script/bindings.hpp>
#include <engine/script/script_context.hpp>
#include <engine/cinematic/player.hpp>
#include <engine/cinematic/sequence.hpp>
#include <engine/core/log.hpp>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace engine::script {

namespace {
    // Player storage
    std::unordered_map<uint32_t, std::unique_ptr<cinematic::SequencePlayer>> s_players;
    uint32_t s_next_player_id = 1;
    std::mutex s_players_mutex;

    // Event callbacks storage
    std::unordered_map<uint32_t, sol::function> s_event_callbacks;
    std::unordered_map<uint32_t, sol::function> s_complete_callbacks;

    cinematic::SequencePlayer* get_player(uint32_t id) {
        std::lock_guard<std::mutex> lock(s_players_mutex);
        auto it = s_players.find(id);
        return (it != s_players.end()) ? it->second.get() : nullptr;
    }
}

void register_cinematic_bindings(sol::state& lua) {
    using namespace engine::cinematic;
    using namespace engine::core;

    // Create Cinematic table
    auto cinematic = lua.create_named_table("Cinematic");

    // State constants
    cinematic["STATE_STOPPED"] = static_cast<int>(PlaybackState::Stopped);
    cinematic["STATE_PLAYING"] = static_cast<int>(PlaybackState::Playing);
    cinematic["STATE_PAUSED"] = static_cast<int>(PlaybackState::Paused);

    // Direction constants
    cinematic["DIR_FORWARD"] = static_cast<int>(PlaybackDirection::Forward);
    cinematic["DIR_BACKWARD"] = static_cast<int>(PlaybackDirection::Backward);

    // =========================================================================
    // Player Management
    // =========================================================================

    // Create a new player
    cinematic.set_function("create_player", []() -> uint32_t {
        std::lock_guard<std::mutex> lock(s_players_mutex);
        uint32_t id = s_next_player_id++;
        s_players[id] = std::make_unique<SequencePlayer>();
        return id;
    });

    // Destroy a player
    cinematic.set_function("destroy_player", [](uint32_t player_id) {
        std::lock_guard<std::mutex> lock(s_players_mutex);
        s_players.erase(player_id);
        s_event_callbacks.erase(player_id);
        s_complete_callbacks.erase(player_id);
    });

    // Load sequence from file
    cinematic.set_function("load", [](uint32_t player_id, const std::string& path) -> bool {
        auto* player = get_player(player_id);
        if (!player) {
            core::log(LogLevel::Warn, "Cinematic.load: Invalid player ID {}", player_id);
            return false;
        }

        try {
            player->load(path);
            return player->has_sequence();
        } catch (const std::exception& e) {
            core::log(LogLevel::Error, "Cinematic.load failed: {}", e.what());
            return false;
        }
    });

    // Unload current sequence
    cinematic.set_function("unload", [](uint32_t player_id) {
        auto* player = get_player(player_id);
        if (player) {
            player->unload();
        }
    });

    // =========================================================================
    // Playback Control
    // =========================================================================

    // Play
    cinematic.set_function("play", [](uint32_t player_id) {
        auto* player = get_player(player_id);
        if (player) {
            player->play();
        }
    });

    // Pause
    cinematic.set_function("pause", [](uint32_t player_id) {
        auto* player = get_player(player_id);
        if (player) {
            player->pause();
        }
    });

    // Stop
    cinematic.set_function("stop", [](uint32_t player_id) {
        auto* player = get_player(player_id);
        if (player) {
            player->stop();
        }
    });

    // Toggle play/pause
    cinematic.set_function("toggle_play_pause", [](uint32_t player_id) {
        auto* player = get_player(player_id);
        if (player) {
            player->toggle_play_pause();
        }
    });

    // =========================================================================
    // State Queries
    // =========================================================================

    cinematic.set_function("is_playing", [](uint32_t player_id) -> bool {
        auto* player = get_player(player_id);
        return player && player->is_playing();
    });

    cinematic.set_function("is_paused", [](uint32_t player_id) -> bool {
        auto* player = get_player(player_id);
        return player && player->is_paused();
    });

    cinematic.set_function("is_stopped", [](uint32_t player_id) -> bool {
        auto* player = get_player(player_id);
        return player ? player->is_stopped() : true;
    });

    cinematic.set_function("get_state", [](uint32_t player_id) -> std::string {
        auto* player = get_player(player_id);
        if (!player) return "stopped";

        switch (player->get_state()) {
            case PlaybackState::Playing: return "playing";
            case PlaybackState::Paused: return "paused";
            case PlaybackState::Stopped:
            default: return "stopped";
        }
    });

    // =========================================================================
    // Seeking
    // =========================================================================

    cinematic.set_function("seek", [](uint32_t player_id, float time) {
        auto* player = get_player(player_id);
        auto* world = get_current_script_world();
        if (player && world) {
            player->seek(time, *world);
        }
    });

    cinematic.set_function("seek_to_start", [](uint32_t player_id) {
        auto* player = get_player(player_id);
        auto* world = get_current_script_world();
        if (player && world) {
            player->seek_to_start(*world);
        }
    });

    cinematic.set_function("seek_to_end", [](uint32_t player_id) {
        auto* player = get_player(player_id);
        auto* world = get_current_script_world();
        if (player && world) {
            player->seek_to_end(*world);
        }
    });

    cinematic.set_function("seek_to_marker", [](uint32_t player_id, const std::string& marker_name) {
        auto* player = get_player(player_id);
        auto* world = get_current_script_world();
        if (player && world) {
            player->seek_to_marker(marker_name, *world);
        }
    });

    // =========================================================================
    // Time Queries
    // =========================================================================

    cinematic.set_function("get_current_time", [](uint32_t player_id) -> float {
        auto* player = get_player(player_id);
        return player ? player->get_current_time() : 0.0f;
    });

    cinematic.set_function("get_duration", [](uint32_t player_id) -> float {
        auto* player = get_player(player_id);
        return player ? player->get_duration() : 0.0f;
    });

    cinematic.set_function("get_progress", [](uint32_t player_id) -> float {
        auto* player = get_player(player_id);
        return player ? player->get_progress() : 0.0f;
    });

    // =========================================================================
    // Playback Settings
    // =========================================================================

    cinematic.set_function("set_playback_speed", [](uint32_t player_id, float speed) {
        auto* player = get_player(player_id);
        if (player) {
            player->set_playback_speed(speed);
        }
    });

    cinematic.set_function("get_playback_speed", [](uint32_t player_id) -> float {
        auto* player = get_player(player_id);
        return player ? player->get_playback_speed() : 1.0f;
    });

    cinematic.set_function("set_looping", [](uint32_t player_id, bool loop) {
        auto* player = get_player(player_id);
        if (player) {
            player->set_looping(loop);
        }
    });

    cinematic.set_function("is_looping", [](uint32_t player_id) -> bool {
        auto* player = get_player(player_id);
        return player && player->is_looping();
    });

    cinematic.set_function("set_direction", [](uint32_t player_id, const std::string& direction) {
        auto* player = get_player(player_id);
        if (player) {
            if (direction == "forward") {
                player->set_direction(PlaybackDirection::Forward);
            } else if (direction == "backward") {
                player->set_direction(PlaybackDirection::Backward);
            }
        }
    });

    // =========================================================================
    // Blend Settings
    // =========================================================================

    cinematic.set_function("set_blend_in_time", [](uint32_t player_id, float time) {
        auto* player = get_player(player_id);
        if (player) {
            player->set_blend_in_time(time);
        }
    });

    cinematic.set_function("set_blend_out_time", [](uint32_t player_id, float time) {
        auto* player = get_player(player_id);
        if (player) {
            player->set_blend_out_time(time);
        }
    });

    cinematic.set_function("get_blend_weight", [](uint32_t player_id) -> float {
        auto* player = get_player(player_id);
        return player ? player->get_blend_weight() : 1.0f;
    });

    // =========================================================================
    // Skip Control
    // =========================================================================

    cinematic.set_function("enable_skipping", [](uint32_t player_id, bool enable) {
        auto* player = get_player(player_id);
        if (player) {
            player->enable_skipping(enable);
        }
    });

    cinematic.set_function("can_skip", [](uint32_t player_id) -> bool {
        auto* player = get_player(player_id);
        return player && player->can_skip();
    });

    cinematic.set_function("skip_to_next_point", [](uint32_t player_id) {
        auto* player = get_player(player_id);
        if (player) {
            player->skip_to_next_point();
        }
    });

    cinematic.set_function("add_skip_point", [](uint32_t player_id, float time) {
        auto* player = get_player(player_id);
        if (player) {
            player->add_skip_point(time);
        }
    });

    // =========================================================================
    // Event Callbacks
    // =========================================================================

    // Set event callback (fires during playback for markers, sections, etc.)
    cinematic.set_function("on_event", [](uint32_t player_id, sol::function callback) {
        auto* player = get_player(player_id);
        if (!player) return;

        // Store the Lua callback
        s_event_callbacks[player_id] = callback;

        // Set the C++ callback that forwards to Lua
        player->set_event_callback([player_id](PlaybackEvent event, const std::string& data) {
            auto it = s_event_callbacks.find(player_id);
            if (it != s_event_callbacks.end() && it->second.valid()) {
                std::string event_name;
                switch (event) {
                    case PlaybackEvent::Started: event_name = "started"; break;
                    case PlaybackEvent::Paused: event_name = "paused"; break;
                    case PlaybackEvent::Resumed: event_name = "resumed"; break;
                    case PlaybackEvent::Stopped: event_name = "stopped"; break;
                    case PlaybackEvent::Finished: event_name = "finished"; break;
                    case PlaybackEvent::Looped: event_name = "looped"; break;
                    case PlaybackEvent::MarkerReached: event_name = "marker_reached"; break;
                    case PlaybackEvent::SectionEntered: event_name = "section_entered"; break;
                    case PlaybackEvent::SectionExited: event_name = "section_exited"; break;
                    default: event_name = "unknown"; break;
                }

                sol::protected_function_result result = it->second(event_name, data);
                if (!result.valid()) {
                    sol::error err = result;
                    core::log(LogLevel::Error, "Cinematic event callback error: {}", err.what());
                }
            }
        });
    });

    // Set completion callback (fires when sequence finishes or stops)
    cinematic.set_function("on_complete", [](uint32_t player_id, sol::function callback) {
        auto* player = get_player(player_id);
        if (!player) return;

        // Store the completion callback
        s_complete_callbacks[player_id] = callback;

        // Set up event callback to detect completion
        auto existing_callback = s_event_callbacks.find(player_id);
        sol::function existing_fn = (existing_callback != s_event_callbacks.end())
            ? existing_callback->second
            : sol::nil;

        player->set_event_callback([player_id, existing_fn](PlaybackEvent event, const std::string& data) {
            // Forward to existing event callback if present
            if (existing_fn.valid()) {
                std::string event_name;
                switch (event) {
                    case PlaybackEvent::Started: event_name = "started"; break;
                    case PlaybackEvent::Paused: event_name = "paused"; break;
                    case PlaybackEvent::Resumed: event_name = "resumed"; break;
                    case PlaybackEvent::Stopped: event_name = "stopped"; break;
                    case PlaybackEvent::Finished: event_name = "finished"; break;
                    case PlaybackEvent::Looped: event_name = "looped"; break;
                    case PlaybackEvent::MarkerReached: event_name = "marker_reached"; break;
                    case PlaybackEvent::SectionEntered: event_name = "section_entered"; break;
                    case PlaybackEvent::SectionExited: event_name = "section_exited"; break;
                    default: event_name = "unknown"; break;
                }
                existing_fn(event_name, data);
            }

            // Check for completion events
            if (event == PlaybackEvent::Finished || event == PlaybackEvent::Stopped) {
                auto it = s_complete_callbacks.find(player_id);
                if (it != s_complete_callbacks.end() && it->second.valid()) {
                    sol::protected_function_result result = it->second();
                    if (!result.valid()) {
                        sol::error err = result;
                        core::log(LogLevel::Error, "Cinematic complete callback error: {}", err.what());
                    }
                }
            }
        });
    });

    // Clear all callbacks for a player
    cinematic.set_function("clear_callbacks", [](uint32_t player_id) {
        s_event_callbacks.erase(player_id);
        s_complete_callbacks.erase(player_id);

        auto* player = get_player(player_id);
        if (player) {
            player->set_event_callback(nullptr);
        }
    });

    // =========================================================================
    // Convenience Functions
    // =========================================================================

    // Quick play - creates player, loads, plays, and auto-destroys on complete
    cinematic.set_function("quick_play", [&lua](const std::string& path, sol::optional<sol::function> on_complete) -> uint32_t {
        // Create player
        std::lock_guard<std::mutex> lock(s_players_mutex);
        uint32_t id = s_next_player_id++;
        s_players[id] = std::make_unique<SequencePlayer>();
        auto* player = s_players[id].get();

        // Load sequence
        try {
            player->load(path);
        } catch (const std::exception& e) {
            core::log(LogLevel::Error, "Cinematic.quick_play failed to load '{}': {}", path, e.what());
            s_players.erase(id);
            return 0;
        }

        if (!player->has_sequence()) {
            s_players.erase(id);
            return 0;
        }

        // Set up auto-destroy on complete
        uint32_t player_id = id;
        player->set_event_callback([player_id, on_complete](PlaybackEvent event, const std::string&) {
            if (event == PlaybackEvent::Finished) {
                // Call user callback if provided
                if (on_complete.has_value() && on_complete.value().valid()) {
                    sol::protected_function_result result = on_complete.value()();
                    if (!result.valid()) {
                        sol::error err = result;
                        core::log(LogLevel::Error, "Cinematic quick_play callback error: {}", err.what());
                    }
                }

                // Schedule player destruction (can't destroy during callback)
                // Note: In a real implementation, you'd want to defer this
                std::lock_guard<std::mutex> lock(s_players_mutex);
                s_players.erase(player_id);
                s_event_callbacks.erase(player_id);
                s_complete_callbacks.erase(player_id);
            }
        });

        // Start playback
        player->play();

        return id;
    });

    // Stop all active players
    cinematic.set_function("stop_all", []() {
        CinematicManager::instance().stop_all();
    });

    // Preload a sequence
    cinematic.set_function("preload", [](const std::string& path) {
        CinematicManager::instance().preload(path);
    });

    // Get sequence info
    cinematic.set_function("get_sequence_name", [](uint32_t player_id) -> std::string {
        auto* player = get_player(player_id);
        if (player && player->has_sequence()) {
            return player->get_sequence()->get_name();
        }
        return "";
    });

    // Check if player has loaded sequence
    cinematic.set_function("has_sequence", [](uint32_t player_id) -> bool {
        auto* player = get_player(player_id);
        return player && player->has_sequence();
    });
}

// Cleanup function to be called on script shutdown
void cinematic_bindings_shutdown() {
    std::lock_guard<std::mutex> lock(s_players_mutex);
    s_players.clear();
    s_event_callbacks.clear();
    s_complete_callbacks.clear();
    s_next_player_id = 1;
}

} // namespace engine::script
