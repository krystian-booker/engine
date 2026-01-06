#pragma once

#include <engine/dialogue/dialogue_node.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <memory>

namespace engine::dialogue {

// ============================================================================
// Dialogue Graph
// ============================================================================

class DialogueGraph {
public:
    DialogueGraph() = default;
    explicit DialogueGraph(const std::string& id);

    // ========================================================================
    // Properties
    // ========================================================================

    const std::string& get_id() const { return m_id; }
    void set_id(const std::string& id) { m_id = id; }

    const std::string& get_title() const { return m_title_key; }
    void set_title(const std::string& title_key) { m_title_key = title_key; }

    // ========================================================================
    // Speakers
    // ========================================================================

    void add_speaker(DialogueSpeaker speaker);
    DialogueSpeaker* get_speaker(const std::string& id);
    const DialogueSpeaker* get_speaker(const std::string& id) const;
    const std::vector<DialogueSpeaker>& get_speakers() const { return m_speakers; }

    // ========================================================================
    // Nodes
    // ========================================================================

    void add_node(DialogueNode node);
    DialogueNode* get_node(const std::string& id);
    const DialogueNode* get_node(const std::string& id) const;
    const std::vector<DialogueNode>& get_nodes() const { return m_nodes; }

    // ========================================================================
    // Entry Points
    // ========================================================================

    void set_default_entry(const std::string& node_id);
    const std::string& get_default_entry() const { return m_default_entry_id; }

    void add_conditional_entry(const std::string& node_id, std::vector<DialogueCondition> conditions);

    // Get the appropriate entry node based on conditions
    DialogueNode* get_entry_node();
    const DialogueNode* get_entry_node() const;

    // ========================================================================
    // Validation
    // ========================================================================

    bool validate() const;
    std::vector<std::string> get_validation_errors() const;

    // ========================================================================
    // State
    // ========================================================================

    void reset_shown_flags();

    // ========================================================================
    // Metadata
    // ========================================================================

    void set_metadata(const std::string& key, const std::string& value);
    std::string get_metadata(const std::string& key) const;

private:
    struct ConditionalEntry {
        std::string node_id;
        std::vector<DialogueCondition> conditions;
        int32_t priority = 0;
    };

    std::string m_id;
    std::string m_title_key;

    std::vector<DialogueSpeaker> m_speakers;
    std::vector<DialogueNode> m_nodes;
    std::unordered_map<std::string, size_t> m_node_index;
    std::unordered_map<std::string, size_t> m_speaker_index;

    std::string m_default_entry_id;
    std::vector<ConditionalEntry> m_conditional_entries;

    std::unordered_map<std::string, std::string> m_metadata;
};

// ============================================================================
// Dialogue Graph Builder
// ============================================================================

class DialogueGraphBuilder {
public:
    DialogueGraphBuilder(const std::string& id) {
        m_graph = std::make_unique<DialogueGraph>(id);
    }

    DialogueGraphBuilder& title(const std::string& title_key) {
        m_graph->set_title(title_key);
        return *this;
    }

    DialogueGraphBuilder& speaker(DialogueSpeaker s) {
        m_graph->add_speaker(std::move(s));
        return *this;
    }

    DialogueGraphBuilder& speaker(const std::string& id, const std::string& name_key,
                                   const std::string& portrait = "") {
        DialogueSpeaker s;
        s.id = id;
        s.display_name_key = name_key;
        s.portrait = portrait;
        m_graph->add_speaker(s);
        return *this;
    }

    DialogueGraphBuilder& node(DialogueNode n) {
        m_graph->add_node(std::move(n));
        return *this;
    }

    DialogueGraphBuilder& entry(const std::string& node_id) {
        m_graph->set_default_entry(node_id);
        return *this;
    }

    DialogueGraphBuilder& conditional_entry(const std::string& node_id,
                                             std::vector<DialogueCondition> conditions) {
        m_graph->add_conditional_entry(node_id, std::move(conditions));
        return *this;
    }

    DialogueGraphBuilder& metadata(const std::string& key, const std::string& value) {
        m_graph->set_metadata(key, value);
        return *this;
    }

    std::unique_ptr<DialogueGraph> build() {
        return std::move(m_graph);
    }

private:
    std::unique_ptr<DialogueGraph> m_graph;
};

inline DialogueGraphBuilder make_dialogue(const std::string& id) {
    return DialogueGraphBuilder(id);
}

// ============================================================================
// Dialogue Library
// ============================================================================

class DialogueLibrary {
public:
    static DialogueLibrary& instance();

    void register_graph(std::unique_ptr<DialogueGraph> graph);
    void unregister_graph(const std::string& id);

    DialogueGraph* get_graph(const std::string& id);
    const DialogueGraph* get_graph(const std::string& id) const;
    bool has_graph(const std::string& id) const;

    std::vector<std::string> get_all_graph_ids() const;

    // Loading from files (would be implemented with asset system)
    bool load_from_file(const std::string& path);
    bool save_to_file(const std::string& id, const std::string& path) const;

    void clear();

private:
    DialogueLibrary() = default;

    std::unordered_map<std::string, std::unique_ptr<DialogueGraph>> m_graphs;
};

inline DialogueLibrary& dialogues() { return DialogueLibrary::instance(); }

} // namespace engine::dialogue
