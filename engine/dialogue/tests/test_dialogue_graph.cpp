#include <catch2/catch_test_macros.hpp>
#include <engine/dialogue/dialogue_graph.hpp>

using namespace engine::dialogue;

// ============================================================================
// DialogueGraph Tests
// ============================================================================

TEST_CASE("DialogueGraph default constructor", "[dialogue][graph]") {
    DialogueGraph graph;

    REQUIRE(graph.get_id().empty());
    REQUIRE(graph.get_title().empty());
    REQUIRE(graph.get_speakers().empty());
    REQUIRE(graph.get_nodes().empty());
    REQUIRE(graph.get_default_entry().empty());
}

TEST_CASE("DialogueGraph constructor with id", "[dialogue][graph]") {
    DialogueGraph graph("merchant_dialogue");

    REQUIRE(graph.get_id() == "merchant_dialogue");
}

TEST_CASE("DialogueGraph set properties", "[dialogue][graph]") {
    DialogueGraph graph;
    graph.set_id("test_dialogue");
    graph.set_title("TEST_DIALOGUE_TITLE");

    REQUIRE(graph.get_id() == "test_dialogue");
    REQUIRE(graph.get_title() == "TEST_DIALOGUE_TITLE");
}

TEST_CASE("DialogueGraph add_speaker", "[dialogue][graph]") {
    DialogueGraph graph("test");

    DialogueSpeaker speaker;
    speaker.id = "npc_merchant";
    speaker.display_name_key = "NPC_MERCHANT";
    speaker.portrait = "portrait_merchant";

    graph.add_speaker(speaker);

    REQUIRE(graph.get_speakers().size() == 1);

    auto* found = graph.get_speaker("npc_merchant");
    REQUIRE(found != nullptr);
    REQUIRE(found->id == "npc_merchant");
    REQUIRE(found->display_name_key == "NPC_MERCHANT");
}

TEST_CASE("DialogueGraph get_speaker not found", "[dialogue][graph]") {
    DialogueGraph graph("test");

    auto* found = graph.get_speaker("nonexistent");
    REQUIRE(found == nullptr);
}

TEST_CASE("DialogueGraph add_node", "[dialogue][graph]") {
    DialogueGraph graph("test");

    DialogueNode node;
    node.id = "node_1";
    node.speaker_id = "merchant";
    node.text_key = "TEXT_1";

    graph.add_node(node);

    REQUIRE(graph.get_nodes().size() == 1);

    auto* found = graph.get_node("node_1");
    REQUIRE(found != nullptr);
    REQUIRE(found->id == "node_1");
    REQUIRE(found->speaker_id == "merchant");
}

TEST_CASE("DialogueGraph get_node not found", "[dialogue][graph]") {
    DialogueGraph graph("test");

    auto* found = graph.get_node("nonexistent");
    REQUIRE(found == nullptr);
}

TEST_CASE("DialogueGraph set_default_entry", "[dialogue][graph]") {
    DialogueGraph graph("test");

    DialogueNode node;
    node.id = "start_node";
    graph.add_node(node);

    graph.set_default_entry("start_node");

    REQUIRE(graph.get_default_entry() == "start_node");
}

TEST_CASE("DialogueGraph get_entry_node", "[dialogue][graph]") {
    DialogueGraph graph("test");

    DialogueNode entry;
    entry.id = "entry_node";
    entry.is_entry_point = true;
    graph.add_node(entry);

    graph.set_default_entry("entry_node");

    auto* entry_node = graph.get_entry_node();
    REQUIRE(entry_node != nullptr);
    REQUIRE(entry_node->id == "entry_node");
}

TEST_CASE("DialogueGraph metadata", "[dialogue][graph]") {
    DialogueGraph graph("test");

    graph.set_metadata("author", "Game Designer");
    graph.set_metadata("version", "1.0");
    graph.set_metadata("category", "shop");

    REQUIRE(graph.get_metadata("author") == "Game Designer");
    REQUIRE(graph.get_metadata("version") == "1.0");
    REQUIRE(graph.get_metadata("category") == "shop");
    REQUIRE(graph.get_metadata("nonexistent").empty());
}

TEST_CASE("DialogueGraph multiple speakers", "[dialogue][graph]") {
    DialogueGraph graph("conversation");

    DialogueSpeaker player;
    player.id = "player";
    player.display_name_key = "PLAYER_NAME";

    DialogueSpeaker npc1;
    npc1.id = "merchant";
    npc1.display_name_key = "MERCHANT_NAME";

    DialogueSpeaker npc2;
    npc2.id = "guard";
    npc2.display_name_key = "GUARD_NAME";

    graph.add_speaker(player);
    graph.add_speaker(npc1);
    graph.add_speaker(npc2);

    REQUIRE(graph.get_speakers().size() == 3);
    REQUIRE(graph.get_speaker("player") != nullptr);
    REQUIRE(graph.get_speaker("merchant") != nullptr);
    REQUIRE(graph.get_speaker("guard") != nullptr);
}

TEST_CASE("DialogueGraph multiple nodes", "[dialogue][graph]") {
    DialogueGraph graph("test");

    DialogueNode node1;
    node1.id = "node_1";
    node1.next_node_id = "node_2";

    DialogueNode node2;
    node2.id = "node_2";
    node2.next_node_id = "node_3";

    DialogueNode node3;
    node3.id = "node_3";
    node3.is_exit_point = true;

    graph.add_node(node1);
    graph.add_node(node2);
    graph.add_node(node3);

    REQUIRE(graph.get_nodes().size() == 3);
    REQUIRE(graph.get_node("node_1")->next_node_id == "node_2");
    REQUIRE(graph.get_node("node_2")->next_node_id == "node_3");
    REQUIRE(graph.get_node("node_3")->is_exit_point);
}

// ============================================================================
// DialogueGraphBuilder Tests
// ============================================================================

TEST_CASE("DialogueGraphBuilder basic", "[dialogue][graph]") {
    auto graph = make_dialogue("shop_dialogue")
        .title("SHOP_DIALOGUE_TITLE")
        .build();

    REQUIRE(graph != nullptr);
    REQUIRE(graph->get_id() == "shop_dialogue");
    REQUIRE(graph->get_title() == "SHOP_DIALOGUE_TITLE");
}

TEST_CASE("DialogueGraphBuilder with speakers", "[dialogue][graph]") {
    auto graph = make_dialogue("conversation")
        .title("CONVERSATION")
        .speaker("player", "PLAYER_NAME", "portrait_player")
        .speaker("merchant", "MERCHANT_NAME", "portrait_merchant")
        .build();

    REQUIRE(graph->get_speakers().size() == 2);
    REQUIRE(graph->get_speaker("player") != nullptr);
    REQUIRE(graph->get_speaker("merchant") != nullptr);
}

TEST_CASE("DialogueGraphBuilder with nodes", "[dialogue][graph]") {
    auto node1 = make_node("greeting")
        .speaker("merchant")
        .text("GREETING")
        .next("offer")
        .build();

    auto node2 = make_node("offer")
        .speaker("merchant")
        .text("OFFER")
        .choice("buy", "BUY", "purchase")
        .exit_choice("leave", "LEAVE")
        .build();

    auto graph = make_dialogue("shop")
        .title("SHOP")
        .speaker("merchant", "MERCHANT", "portrait")
        .node(node1)
        .node(node2)
        .entry("greeting")
        .build();

    REQUIRE(graph->get_nodes().size() == 2);
    REQUIRE(graph->get_default_entry() == "greeting");
    REQUIRE(graph->get_node("greeting") != nullptr);
    REQUIRE(graph->get_node("offer") != nullptr);
    REQUIRE(graph->get_node("offer")->choices.size() == 2);
}

TEST_CASE("DialogueGraphBuilder with metadata", "[dialogue][graph]") {
    auto graph = make_dialogue("quest_dialogue")
        .title("QUEST_DIALOGUE")
        .metadata("quest_id", "main_quest_01")
        .metadata("importance", "critical")
        .build();

    REQUIRE(graph->get_metadata("quest_id") == "main_quest_01");
    REQUIRE(graph->get_metadata("importance") == "critical");
}

TEST_CASE("DialogueGraphBuilder complete dialogue", "[dialogue][graph]") {
    // Build a complete simple dialogue
    auto node_greeting = make_node("greeting")
        .speaker("npc")
        .text("NPC_GREETING")
        .entry_point()
        .next("question")
        .build();

    auto node_question = make_node("question")
        .speaker("npc")
        .text("NPC_QUESTION")
        .choice("yes", "CHOICE_YES", "yes_response")
        .choice("no", "CHOICE_NO", "no_response")
        .build();

    auto node_yes = make_node("yes_response")
        .speaker("npc")
        .text("NPC_YES_RESPONSE")
        .exit_point()
        .build();

    auto node_no = make_node("no_response")
        .speaker("npc")
        .text("NPC_NO_RESPONSE")
        .exit_point()
        .build();

    auto graph = make_dialogue("simple_conversation")
        .title("SIMPLE_CONVERSATION")
        .speaker("npc", "NPC_NAME", "npc_portrait")
        .node(node_greeting)
        .node(node_question)
        .node(node_yes)
        .node(node_no)
        .entry("greeting")
        .build();

    REQUIRE(graph->get_id() == "simple_conversation");
    REQUIRE(graph->get_speakers().size() == 1);
    REQUIRE(graph->get_nodes().size() == 4);
    REQUIRE(graph->get_default_entry() == "greeting");

    auto* entry = graph->get_entry_node();
    REQUIRE(entry != nullptr);
    REQUIRE(entry->id == "greeting");
    REQUIRE(entry->is_entry_point);

    auto* question = graph->get_node("question");
    REQUIRE(question->choices.size() == 2);

    auto* yes_resp = graph->get_node("yes_response");
    REQUIRE(yes_resp->is_exit_point);
}
