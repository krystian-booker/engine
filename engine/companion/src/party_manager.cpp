#include <engine/companion/party_manager.hpp>
#include <engine/companion/companion.hpp>
#include <engine/scene/transform.hpp>
#include <engine/core/game_events.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <sstream>

namespace engine::companion {

PartyManager& PartyManager::instance() {
    static PartyManager instance;
    return instance;
}

PartyManager::PartyManager()
    : m_formation(Formation::wedge(4, 2.0f)) {
}

// ============================================================================
// Leader Management
// ============================================================================

void PartyManager::set_leader(scene::Entity leader) {
    m_leader = leader;
}

// ============================================================================
// Companion Management
// ============================================================================

bool PartyManager::add_companion(scene::Entity companion) {
    // Check if already in party
    if (is_companion(companion)) {
        return false;
    }

    // Check party size
    if (m_companions.size() >= m_max_party_size) {
        return false;
    }

    // Verify entity has CompanionComponent
    if (m_world) {
        auto* comp = m_world->try_get<CompanionComponent>(companion);
        if (!comp) {
            return false;
        }

        // Set owner
        comp->owner = m_leader;
        comp->set_state(CompanionState::Following);
    }

    m_companions.push_back(companion);

    // Assign formation slot
    assign_formation_slot(companion);

    // Fire event
    if (m_world) {
        core::game_events().broadcast(CompanionJoinedEvent{companion, m_leader});
    }

    // Callback
    if (m_on_joined) {
        m_on_joined(companion);
    }

    return true;
}

bool PartyManager::remove_companion(scene::Entity companion) {
    auto it = std::find(m_companions.begin(), m_companions.end(), companion);
    if (it == m_companions.end()) {
        return false;
    }

    // Release formation slot
    release_formation_slot(companion);

    m_companions.erase(it);

    // Clear owner
    if (m_world) {
        auto* comp = m_world->try_get<CompanionComponent>(companion);
        if (comp) {
            comp->owner = scene::NullEntity;
        }

        core::game_events().broadcast(CompanionLeftEvent{companion, m_leader, true});
    }

    // Callback
    if (m_on_left) {
        m_on_left(companion);
    }

    return true;
}

void PartyManager::dismiss_all() {
    // Copy vector as remove_companion modifies it
    auto companions = m_companions;
    for (auto companion : companions) {
        remove_companion(companion);
    }
}

bool PartyManager::is_companion(scene::Entity entity) const {
    return std::find(m_companions.begin(), m_companions.end(), entity) != m_companions.end();
}

// ============================================================================
// Commands
// ============================================================================

void PartyManager::issue_command(CompanionCommand cmd) {
    for (auto companion : m_companions) {
        issue_command(companion, cmd);
    }
}

void PartyManager::issue_command(scene::Entity companion, CompanionCommand cmd) {
    if (!m_world) return;

    auto* comp = m_world->try_get<CompanionComponent>(companion);
    if (!comp || !comp->can_be_commanded) return;

    switch (cmd) {
        case CompanionCommand::Follow:
            comp->set_state(CompanionState::Following);
            break;

        case CompanionCommand::Wait:
            if (auto* transform = m_world->try_get<scene::WorldTransform>(companion)) {
                comp->wait_position = transform->position();
            }
            comp->set_state(CompanionState::Waiting);
            break;

        case CompanionCommand::Dismiss:
            remove_companion(companion);
            return;  // Don't fire event - remove_companion does

        case CompanionCommand::Revive:
            if (comp->state == CompanionState::Dead) {
                comp->set_state(CompanionState::Following);
                core::game_events().broadcast(CompanionRevivedEvent{companion, m_leader});
            }
            break;

        default:
            break;
    }

    core::game_events().broadcast(CompanionCommandedEvent{
        companion, cmd, Vec3(0.0f), scene::NullEntity
    });
}

void PartyManager::issue_command(CompanionCommand cmd, const Vec3& target_position) {
    for (auto companion : m_companions) {
        issue_command(companion, cmd, target_position);
    }
}

void PartyManager::issue_command(scene::Entity companion, CompanionCommand cmd, const Vec3& target_position) {
    if (!m_world) return;

    auto* comp = m_world->try_get<CompanionComponent>(companion);
    if (!comp || !comp->can_be_commanded) return;

    comp->command_position = target_position;

    switch (cmd) {
        case CompanionCommand::Move:
            comp->set_state(CompanionState::Moving);
            break;

        case CompanionCommand::Defend:
            comp->set_state(CompanionState::Defending);
            break;

        default:
            issue_command(companion, cmd);
            return;
    }

    core::game_events().broadcast(CompanionCommandedEvent{
        companion, cmd, target_position, scene::NullEntity
    });
}

void PartyManager::issue_command(CompanionCommand cmd, scene::Entity target_entity) {
    for (auto companion : m_companions) {
        issue_command(companion, cmd, target_entity);
    }
}

void PartyManager::issue_command(scene::Entity companion, CompanionCommand cmd, scene::Entity target_entity) {
    if (!m_world) return;

    auto* comp = m_world->try_get<CompanionComponent>(companion);
    if (!comp || !comp->can_be_commanded) return;

    comp->command_target = target_entity;

    switch (cmd) {
        case CompanionCommand::Attack:
            comp->combat_target = target_entity;
            comp->set_state(CompanionState::Attacking);
            break;

        case CompanionCommand::Interact:
            comp->set_state(CompanionState::Interacting);
            break;

        case CompanionCommand::Defend:
            comp->set_state(CompanionState::Defending);
            break;

        default:
            issue_command(companion, cmd);
            return;
    }

    core::game_events().broadcast(CompanionCommandedEvent{
        companion, cmd, Vec3(0.0f), target_entity
    });
}

// ============================================================================
// Formation
// ============================================================================

void PartyManager::set_formation(FormationType type) {
    m_formation = [&]() {
        switch (type) {
            case FormationType::Line:   return Formation::line(static_cast<int>(m_max_party_size));
            case FormationType::Wedge:  return Formation::wedge(static_cast<int>(m_max_party_size));
            case FormationType::Circle: return Formation::circle(static_cast<int>(m_max_party_size));
            case FormationType::Column: return Formation::column(static_cast<int>(m_max_party_size));
            case FormationType::Spread: return Formation::spread(static_cast<int>(m_max_party_size));
            default:                    return Formation::wedge(static_cast<int>(m_max_party_size));
        }
    }();

    update_formation();
}

void PartyManager::set_custom_formation(const Formation& formation) {
    m_formation = formation;
    update_formation();
}

void PartyManager::update_formation() {
    if (!m_world) return;

    // Reassign all companions to formation slots
    m_formation.clear_occupancy();

    for (auto companion : m_companions) {
        assign_formation_slot(companion);
    }
}

void PartyManager::assign_formation_slot(scene::Entity companion) {
    if (!m_world) return;

    auto* comp = m_world->try_get<CompanionComponent>(companion);
    if (!comp) return;

    // Find best slot based on current position
    Vec3 companion_pos{0.0f};
    Vec3 leader_pos{0.0f};
    Vec3 leader_forward{0.0f, 0.0f, 1.0f};

    if (auto* transform = m_world->try_get<scene::WorldTransform>(companion)) {
        companion_pos = transform->position();
    }

    if (m_leader != scene::NullEntity) {
        if (auto* transform = m_world->try_get<scene::WorldTransform>(m_leader)) {
            leader_pos = transform->position();
            // Get forward from rotation
            Quat rot = transform->rotation();
            leader_forward = rot * Vec3(0.0f, 0.0f, 1.0f);
        }
    }

    int slot = find_best_slot(m_formation, companion_pos, leader_pos, leader_forward);
    if (slot >= 0) {
        m_formation.set_slot_occupied(slot, true);
        comp->formation_slot = slot;
    }
}

void PartyManager::release_formation_slot(scene::Entity companion) {
    if (!m_world) return;

    auto* comp = m_world->try_get<CompanionComponent>(companion);
    if (!comp) return;

    if (comp->formation_slot >= 0) {
        m_formation.set_slot_occupied(comp->formation_slot, false);
        comp->formation_slot = -1;
    }
}

// ============================================================================
// Queries
// ============================================================================

std::vector<scene::Entity> PartyManager::get_companions_in_state(CompanionState state) const {
    std::vector<scene::Entity> result;
    if (!m_world) return result;

    for (auto companion : m_companions) {
        auto* comp = m_world->try_get<CompanionComponent>(companion);
        if (comp && comp->state == state) {
            result.push_back(companion);
        }
    }

    return result;
}

std::vector<scene::Entity> PartyManager::get_companions_in_combat() const {
    std::vector<scene::Entity> result;
    if (!m_world) return result;

    for (auto companion : m_companions) {
        auto* comp = m_world->try_get<CompanionComponent>(companion);
        if (comp && comp->is_in_combat()) {
            result.push_back(companion);
        }
    }

    return result;
}

std::vector<scene::Entity> PartyManager::get_idle_companions() const {
    std::vector<scene::Entity> result;
    if (!m_world) return result;

    for (auto companion : m_companions) {
        auto* comp = m_world->try_get<CompanionComponent>(companion);
        if (comp && comp->is_idle()) {
            result.push_back(companion);
        }
    }

    return result;
}

std::vector<scene::Entity> PartyManager::get_downed_companions() const {
    return get_companions_in_state(CompanionState::Dead);
}

scene::Entity PartyManager::find_companion(const std::string& companion_id) const {
    if (!m_world) return scene::NullEntity;

    for (auto companion : m_companions) {
        auto* comp = m_world->try_get<CompanionComponent>(companion);
        if (comp && comp->companion_id == companion_id) {
            return companion;
        }
    }

    return scene::NullEntity;
}

// ============================================================================
// Serialization
// ============================================================================

void PartyManager::serialize(nlohmann::json& out) const {
    out["formation_type"] = static_cast<int>(m_formation.type);
    out["max_party_size"] = m_max_party_size;

    // Serialize companion IDs (not entities - those are runtime)
    out["companions"] = nlohmann::json::array();
    if (m_world) {
        for (auto companion : m_companions) {
            auto* comp = m_world->try_get<CompanionComponent>(companion);
            if (comp) {
                nlohmann::json comp_data;
                comp_data["id"] = comp->companion_id;
                comp_data["state"] = static_cast<int>(comp->state);
                comp_data["formation_slot"] = comp->formation_slot;
                out["companions"].push_back(comp_data);
            }
        }
    }
}

void PartyManager::deserialize(const nlohmann::json& in, scene::World& world) {
    m_world = &world;

    if (in.contains("formation_type")) {
        set_formation(static_cast<FormationType>(in["formation_type"].get<int>()));
    }

    if (in.contains("max_party_size")) {
        m_max_party_size = in["max_party_size"].get<size_t>();
    }

    // Note: Actual companion entities must be recreated by the game
    // This just stores the IDs and states for restoration
}

// ============================================================================
// Debug
// ============================================================================

std::string PartyManager::get_debug_info() const {
    std::ostringstream ss;
    ss << "Party Manager Debug Info\n";
    ss << "========================\n";
    ss << "Leader: " << (m_leader != scene::NullEntity ? "Set" : "None") << "\n";
    ss << "Companions: " << m_companions.size() << "/" << m_max_party_size << "\n";
    ss << "Formation: " << formation_type_to_string(m_formation.type) << "\n";
    ss << "Occupied Slots: " << m_formation.get_occupied_count() << "/" << m_formation.get_capacity() << "\n";

    if (m_world) {
        ss << "\nCompanion Details:\n";
        for (auto companion : m_companions) {
            auto* comp = m_world->try_get<CompanionComponent>(companion);
            if (comp) {
                ss << "  - " << comp->display_name
                   << " [" << companion_state_to_string(comp->state) << "]"
                   << " Slot: " << comp->formation_slot << "\n";
            }
        }
    }

    return ss.str();
}

} // namespace engine::companion
