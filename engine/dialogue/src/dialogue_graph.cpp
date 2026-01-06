#include <engine/dialogue/dialogue_graph.hpp>
#include <engine/core/log.hpp>
#include <algorithm>

namespace engine::dialogue {

// ============================================================================
// Dialogue Condition Evaluation
// ============================================================================

bool DialogueCondition::evaluate() const {
    // Default evaluation - would be overridden by condition checkers
    if (type == Type::Custom && custom_check) {
        bool result = custom_check();
        return negate ? !result : result;
    }
    return !negate;  // Default to true (conditions checked by DialoguePlayer)
}

// ============================================================================
// Dialogue Graph Implementation
// ============================================================================

DialogueGraph::DialogueGraph(const std::string& id)
    : m_id(id) {
}

void DialogueGraph::add_speaker(DialogueSpeaker speaker) {
    std::string id = speaker.id;
    m_speaker_index[id] = m_speakers.size();
    m_speakers.push_back(std::move(speaker));
}

DialogueSpeaker* DialogueGraph::get_speaker(const std::string& id) {
    auto it = m_speaker_index.find(id);
    if (it == m_speaker_index.end()) return nullptr;
    return &m_speakers[it->second];
}

const DialogueSpeaker* DialogueGraph::get_speaker(const std::string& id) const {
    auto it = m_speaker_index.find(id);
    if (it == m_speaker_index.end()) return nullptr;
    return &m_speakers[it->second];
}

void DialogueGraph::add_node(DialogueNode node) {
    std::string id = node.id;

    // Track entry point
    if (node.is_entry_point && m_default_entry_id.empty()) {
        m_default_entry_id = id;
    }

    m_node_index[id] = m_nodes.size();
    m_nodes.push_back(std::move(node));
}

DialogueNode* DialogueGraph::get_node(const std::string& id) {
    auto it = m_node_index.find(id);
    if (it == m_node_index.end()) return nullptr;
    return &m_nodes[it->second];
}

const DialogueNode* DialogueGraph::get_node(const std::string& id) const {
    auto it = m_node_index.find(id);
    if (it == m_node_index.end()) return nullptr;
    return &m_nodes[it->second];
}

void DialogueGraph::set_default_entry(const std::string& node_id) {
    m_default_entry_id = node_id;
}

void DialogueGraph::add_conditional_entry(const std::string& node_id,
                                           std::vector<DialogueCondition> conditions) {
    ConditionalEntry entry;
    entry.node_id = node_id;
    entry.conditions = std::move(conditions);
    entry.priority = static_cast<int32_t>(m_conditional_entries.size());
    m_conditional_entries.push_back(std::move(entry));
}

DialogueNode* DialogueGraph::get_entry_node() {
    // Check conditional entries first (highest priority first)
    for (auto it = m_conditional_entries.rbegin(); it != m_conditional_entries.rend(); ++it) {
        bool all_pass = true;
        for (const auto& cond : it->conditions) {
            if (!cond.evaluate()) {
                all_pass = false;
                break;
            }
        }
        if (all_pass) {
            return get_node(it->node_id);
        }
    }

    // Fall back to default entry
    return get_node(m_default_entry_id);
}

const DialogueNode* DialogueGraph::get_entry_node() const {
    return const_cast<DialogueGraph*>(this)->get_entry_node();
}

bool DialogueGraph::validate() const {
    return get_validation_errors().empty();
}

std::vector<std::string> DialogueGraph::get_validation_errors() const {
    std::vector<std::string> errors;

    // Check for entry point
    if (m_default_entry_id.empty() && m_conditional_entries.empty()) {
        errors.push_back("No entry point defined");
    }

    // Check that default entry exists
    if (!m_default_entry_id.empty() && !get_node(m_default_entry_id)) {
        errors.push_back("Default entry node '" + m_default_entry_id + "' not found");
    }

    // Check all nodes
    for (const auto& node : m_nodes) {
        // Check speaker exists
        if (!node.speaker_id.empty() && !get_speaker(node.speaker_id)) {
            errors.push_back("Node '" + node.id + "' references unknown speaker '" + node.speaker_id + "'");
        }

        // Check next node exists
        if (!node.next_node_id.empty() && !get_node(node.next_node_id)) {
            errors.push_back("Node '" + node.id + "' references unknown next node '" + node.next_node_id + "'");
        }

        // Check choice targets exist
        for (const auto& choice : node.choices) {
            if (!choice.is_exit && !choice.target_node_id.empty() && !get_node(choice.target_node_id)) {
                errors.push_back("Choice '" + choice.id + "' in node '" + node.id +
                                 "' references unknown target '" + choice.target_node_id + "'");
            }
        }

        // Check for dead-end nodes (no exit, no next, no choices)
        if (!node.is_exit_point && node.next_node_id.empty() && node.choices.empty()) {
            // This could be intentional for terminal nodes
            // errors.push_back("Node '" + node.id + "' has no exit path");
        }
    }

    // Check conditional entries
    for (const auto& entry : m_conditional_entries) {
        if (!get_node(entry.node_id)) {
            errors.push_back("Conditional entry references unknown node '" + entry.node_id + "'");
        }
    }

    return errors;
}

void DialogueGraph::reset_shown_flags() {
    for (auto& node : m_nodes) {
        node.shown = false;
    }
}

void DialogueGraph::set_metadata(const std::string& key, const std::string& value) {
    m_metadata[key] = value;
}

std::string DialogueGraph::get_metadata(const std::string& key) const {
    auto it = m_metadata.find(key);
    return it != m_metadata.end() ? it->second : "";
}

// ============================================================================
// Dialogue Library Implementation
// ============================================================================

DialogueLibrary& DialogueLibrary::instance() {
    static DialogueLibrary s_instance;
    return s_instance;
}

void DialogueLibrary::register_graph(std::unique_ptr<DialogueGraph> graph) {
    if (!graph) return;

    std::string id = graph->get_id();

    // Validate
    auto errors = graph->get_validation_errors();
    if (!errors.empty()) {
        core::log(core::LogLevel::Warning, "Dialogue graph '{}' has validation errors:", id);
        for (const auto& error : errors) {
            core::log(core::LogLevel::Warning, "  - {}", error);
        }
    }

    m_graphs[id] = std::move(graph);
    core::log(core::LogLevel::Info, "Dialogue graph registered: {}", id);
}

void DialogueLibrary::unregister_graph(const std::string& id) {
    m_graphs.erase(id);
}

DialogueGraph* DialogueLibrary::get_graph(const std::string& id) {
    auto it = m_graphs.find(id);
    return it != m_graphs.end() ? it->second.get() : nullptr;
}

const DialogueGraph* DialogueLibrary::get_graph(const std::string& id) const {
    auto it = m_graphs.find(id);
    return it != m_graphs.end() ? it->second.get() : nullptr;
}

bool DialogueLibrary::has_graph(const std::string& id) const {
    return m_graphs.count(id) > 0;
}

std::vector<std::string> DialogueLibrary::get_all_graph_ids() const {
    std::vector<std::string> ids;
    ids.reserve(m_graphs.size());
    for (const auto& [id, _] : m_graphs) {
        ids.push_back(id);
    }
    return ids;
}

bool DialogueLibrary::load_from_file(const std::string& path) {
    // Would integrate with asset system for loading JSON/custom format
    core::log(core::LogLevel::Warning, "Dialogue loading from file not yet implemented: {}", path);
    return false;
}

bool DialogueLibrary::save_to_file(const std::string& id, const std::string& path) const {
    // Would integrate with asset system for saving
    core::log(core::LogLevel::Warning, "Dialogue saving to file not yet implemented: {}", path);
    return false;
}

void DialogueLibrary::clear() {
    m_graphs.clear();
}

} // namespace engine::dialogue
