#pragma once

#include <engine/companion/companion.hpp>
#include <engine/companion/formation.hpp>
#include <engine/scene/entity.hpp>
#include <engine/scene/world.hpp>
#include <nlohmann/json_fwd.hpp>
#include <vector>
#include <functional>

namespace engine::companion {

// ============================================================================
// Party Manager
// ============================================================================

class PartyManager {
public:
    static PartyManager& instance();

    // Delete copy/move
    PartyManager(const PartyManager&) = delete;
    PartyManager& operator=(const PartyManager&) = delete;

    // ========================================================================
    // Initialization
    // ========================================================================

    // Set the world reference (call once at startup)
    void set_world(scene::World* world) { m_world = world; }
    scene::World* get_world() const { return m_world; }

    // ========================================================================
    // Leader Management
    // ========================================================================

    // Set the party leader (usually the player)
    void set_leader(scene::Entity leader);
    scene::Entity get_leader() const { return m_leader; }
    bool has_leader() const { return m_leader != scene::NullEntity; }

    // ========================================================================
    // Companion Management
    // ========================================================================

    // Add a companion to the party
    // Returns true if successful, false if party is full or already in party
    bool add_companion(scene::Entity companion);

    // Remove a companion from the party
    // Returns true if successful
    bool remove_companion(scene::Entity companion);

    // Dismiss all companions
    void dismiss_all();

    // Check if entity is a companion in this party
    bool is_companion(scene::Entity entity) const;

    // Get all companions
    const std::vector<scene::Entity>& get_companions() const { return m_companions; }

    // Get companion count
    size_t get_companion_count() const { return m_companions.size(); }

    // Get maximum party size
    size_t get_max_party_size() const { return m_max_party_size; }
    void set_max_party_size(size_t max_size) { m_max_party_size = max_size; }

    // ========================================================================
    // Commands
    // ========================================================================

    // Issue command to all companions
    void issue_command(CompanionCommand cmd);

    // Issue command to specific companion
    void issue_command(scene::Entity companion, CompanionCommand cmd);

    // Issue command with position target (Move, Defend)
    void issue_command(CompanionCommand cmd, const Vec3& target_position);
    void issue_command(scene::Entity companion, CompanionCommand cmd, const Vec3& target_position);

    // Issue command with entity target (Attack, Interact, Defend)
    void issue_command(CompanionCommand cmd, scene::Entity target_entity);
    void issue_command(scene::Entity companion, CompanionCommand cmd, scene::Entity target_entity);

    // ========================================================================
    // Formation
    // ========================================================================

    // Set formation type (generates new formation)
    void set_formation(FormationType type);

    // Set custom formation
    void set_custom_formation(const Formation& formation);

    // Get current formation
    const Formation& get_formation() const { return m_formation; }
    Formation& get_formation() { return m_formation; }

    // Update formation (reassigns slots based on current positions)
    void update_formation();

    // ========================================================================
    // Queries
    // ========================================================================

    // Get companions in a specific state
    std::vector<scene::Entity> get_companions_in_state(CompanionState state) const;

    // Get companions in combat
    std::vector<scene::Entity> get_companions_in_combat() const;

    // Get idle companions (following or waiting)
    std::vector<scene::Entity> get_idle_companions() const;

    // Get downed/dead companions
    std::vector<scene::Entity> get_downed_companions() const;

    // Find companion by ID
    scene::Entity find_companion(const std::string& companion_id) const;

    // ========================================================================
    // Callbacks
    // ========================================================================

    using CompanionCallback = std::function<void(scene::Entity)>;

    void set_on_companion_joined(CompanionCallback callback) { m_on_joined = callback; }
    void set_on_companion_left(CompanionCallback callback) { m_on_left = callback; }
    void set_on_companion_downed(CompanionCallback callback) { m_on_downed = callback; }

    // ========================================================================
    // Serialization
    // ========================================================================

    // Serialize party state to JSON
    void serialize(nlohmann::json& out) const;

    // Deserialize party state from JSON
    void deserialize(const nlohmann::json& in, scene::World& world);

    // ========================================================================
    // Debug
    // ========================================================================

    // Get debug info string
    std::string get_debug_info() const;

private:
    PartyManager();
    ~PartyManager() = default;

    void assign_formation_slot(scene::Entity companion);
    void release_formation_slot(scene::Entity companion);

    scene::World* m_world = nullptr;
    scene::Entity m_leader = scene::NullEntity;
    std::vector<scene::Entity> m_companions;
    Formation m_formation;
    size_t m_max_party_size = 4;

    // Callbacks
    CompanionCallback m_on_joined;
    CompanionCallback m_on_left;
    CompanionCallback m_on_downed;
};

// Convenience accessor
inline PartyManager& party_manager() { return PartyManager::instance(); }

} // namespace engine::companion
