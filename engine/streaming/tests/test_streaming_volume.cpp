#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/streaming/streaming_volume.hpp>

using namespace engine::streaming;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// ============================================================================
// VolumeShape enum Tests
// ============================================================================

TEST_CASE("VolumeShape enum", "[streaming][volume]") {
    REQUIRE(static_cast<uint8_t>(VolumeShape::Box) == 0);
    REQUIRE(static_cast<uint8_t>(VolumeShape::Sphere) == 1);
    REQUIRE(static_cast<uint8_t>(VolumeShape::Capsule) == 2);
    REQUIRE(static_cast<uint8_t>(VolumeShape::Cylinder) == 3);
}

// ============================================================================
// VolumeEvent enum Tests
// ============================================================================

TEST_CASE("VolumeEvent enum", "[streaming][volume]") {
    REQUIRE(static_cast<uint8_t>(VolumeEvent::Enter) == 0);
    REQUIRE(static_cast<uint8_t>(VolumeEvent::Exit) == 1);
    REQUIRE(static_cast<uint8_t>(VolumeEvent::Stay) == 2);
}

// ============================================================================
// StreamingVolume Tests
// ============================================================================

TEST_CASE("StreamingVolume defaults", "[streaming][volume]") {
    StreamingVolume vol;

    REQUIRE(vol.name.empty());
    REQUIRE(vol.shape == VolumeShape::Box);
    REQUIRE_THAT(vol.scale.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(vol.scale.y, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(vol.scale.z, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(vol.box_extents.x, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(vol.sphere_radius, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(vol.capsule_radius, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(vol.capsule_height, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(vol.cylinder_radius, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(vol.cylinder_height, WithinAbs(10.0f, 0.001f));
    REQUIRE(vol.load_cells.empty());
    REQUIRE(vol.unload_cells.empty());
    REQUIRE(vol.preload_cells.empty());
    REQUIRE_THAT(vol.fade_distance, WithinAbs(5.0f, 0.001f));
    REQUIRE_FALSE(vol.block_until_loaded);
    REQUIRE_THAT(vol.blocking_timeout, WithinAbs(10.0f, 0.001f));
    REQUIRE(vol.enabled == true);
    REQUIRE_FALSE(vol.one_shot);
    REQUIRE(vol.player_only == true);
    REQUIRE(vol.activation_layers == 0xFFFFFFFF);
    REQUIRE_FALSE(vol.is_active);
    REQUIRE_FALSE(vol.was_inside);
    REQUIRE_THAT(vol.current_fade, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("StreamingVolume box configuration", "[streaming][volume]") {
    StreamingVolume vol;
    vol.name = "town_entrance";
    vol.shape = VolumeShape::Box;
    vol.position = Vec3{100.0f, 0.0f, 200.0f};
    vol.box_extents = Vec3{20.0f, 10.0f, 20.0f};
    vol.load_cells = {"town_center", "town_market"};
    vol.unload_cells = {"wilderness"};

    REQUIRE(vol.name == "town_entrance");
    REQUIRE(vol.shape == VolumeShape::Box);
    REQUIRE_THAT(vol.position.x, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(vol.box_extents.x, WithinAbs(20.0f, 0.001f));
    REQUIRE(vol.load_cells.size() == 2);
    REQUIRE(vol.unload_cells.size() == 1);
}

TEST_CASE("StreamingVolume sphere configuration", "[streaming][volume]") {
    StreamingVolume vol;
    vol.name = "arena_trigger";
    vol.shape = VolumeShape::Sphere;
    vol.position = Vec3{0.0f, 0.0f, 0.0f};
    vol.sphere_radius = 50.0f;
    vol.load_cells = {"arena"};

    REQUIRE(vol.shape == VolumeShape::Sphere);
    REQUIRE_THAT(vol.sphere_radius, WithinAbs(50.0f, 0.001f));
}

TEST_CASE("StreamingVolume blocking configuration", "[streaming][volume]") {
    StreamingVolume vol;
    vol.name = "level_transition";
    vol.block_until_loaded = true;
    vol.blocking_timeout = 30.0f;
    vol.one_shot = true;

    REQUIRE(vol.block_until_loaded);
    REQUIRE_THAT(vol.blocking_timeout, WithinAbs(30.0f, 0.001f));
    REQUIRE(vol.one_shot);
}

// ============================================================================
// StreamingVolumeComponent Tests
// ============================================================================

TEST_CASE("StreamingVolumeComponent defaults", "[streaming][component]") {
    StreamingVolumeComponent comp;

    REQUIRE(comp.volume_name.empty());
    REQUIRE_FALSE(comp.use_entity_bounds);
    REQUIRE_FALSE(comp.use_inline_volume);
}

TEST_CASE("StreamingVolumeComponent with reference", "[streaming][component]") {
    StreamingVolumeComponent comp;
    comp.volume_name = "my_volume";
    comp.use_entity_bounds = false;
    comp.use_inline_volume = false;

    REQUIRE(comp.volume_name == "my_volume");
}

TEST_CASE("StreamingVolumeComponent with inline volume", "[streaming][component]") {
    StreamingVolumeComponent comp;
    comp.use_inline_volume = true;
    comp.inline_volume.name = "inline_vol";
    comp.inline_volume.shape = VolumeShape::Sphere;
    comp.inline_volume.sphere_radius = 25.0f;

    REQUIRE(comp.use_inline_volume);
    REQUIRE(comp.inline_volume.name == "inline_vol");
    REQUIRE(comp.inline_volume.shape == VolumeShape::Sphere);
}

// ============================================================================
// StreamingPortalComponent Tests
// ============================================================================

TEST_CASE("StreamingPortalComponent defaults", "[streaming][portal]") {
    StreamingPortalComponent comp;

    REQUIRE(comp.cell_a.empty());
    REQUIRE(comp.cell_b.empty());
    REQUIRE_THAT(comp.width, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(comp.height, WithinAbs(3.0f, 0.001f));
    REQUIRE(comp.bidirectional == true);
    REQUIRE(comp.occlude == true);
}

TEST_CASE("StreamingPortalComponent configuration", "[streaming][portal]") {
    StreamingPortalComponent comp;
    comp.cell_a = "indoor";
    comp.cell_b = "outdoor";
    comp.position = Vec3{10.0f, 0.0f, 0.0f};
    comp.normal = Vec3{1.0f, 0.0f, 0.0f};
    comp.width = 4.0f;
    comp.height = 2.5f;
    comp.bidirectional = true;
    comp.occlude = false;

    REQUIRE(comp.cell_a == "indoor");
    REQUIRE(comp.cell_b == "outdoor");
    REQUIRE_THAT(comp.position.x, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(comp.normal.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(comp.width, WithinAbs(4.0f, 0.001f));
    REQUIRE_THAT(comp.height, WithinAbs(2.5f, 0.001f));
    REQUIRE_FALSE(comp.occlude);
}

// ============================================================================
// StreamingVolumeFactory Tests
// ============================================================================

TEST_CASE("StreamingVolumeFactory create_box", "[streaming][factory]") {
    auto vol = StreamingVolumeFactory::create_box(
        "test_box",
        Vec3{50.0f, 0.0f, 50.0f},
        Vec3{10.0f, 5.0f, 10.0f},
        {"cell_a", "cell_b"}
    );

    REQUIRE(vol.name == "test_box");
    REQUIRE(vol.shape == VolumeShape::Box);
    REQUIRE_THAT(vol.position.x, WithinAbs(50.0f, 0.001f));
    REQUIRE_THAT(vol.box_extents.x, WithinAbs(10.0f, 0.001f));
    REQUIRE(vol.load_cells.size() == 2);
}

TEST_CASE("StreamingVolumeFactory create_sphere", "[streaming][factory]") {
    auto vol = StreamingVolumeFactory::create_sphere(
        "test_sphere",
        Vec3{0.0f, 0.0f, 0.0f},
        25.0f,
        {"main_area"}
    );

    REQUIRE(vol.name == "test_sphere");
    REQUIRE(vol.shape == VolumeShape::Sphere);
    REQUIRE_THAT(vol.sphere_radius, WithinAbs(25.0f, 0.001f));
    REQUIRE(vol.load_cells.size() == 1);
}

TEST_CASE("StreamingVolumeFactory create_loading_zone", "[streaming][factory]") {
    auto vol = StreamingVolumeFactory::create_loading_zone(
        "loading_zone",
        Vec3{100.0f, 0.0f, 100.0f},
        Vec3{5.0f, 3.0f, 5.0f},
        {"next_level"},
        true  // block
    );

    REQUIRE(vol.name == "loading_zone");
    REQUIRE(vol.block_until_loaded);
    REQUIRE(vol.load_cells.size() == 1);
}

TEST_CASE("StreamingVolumeFactory create_level_transition", "[streaming][factory]") {
    auto vol = StreamingVolumeFactory::create_level_transition(
        "level_door",
        Vec3{0.0f, 0.0f, 0.0f},
        Vec3{2.0f, 3.0f, 2.0f},
        {"level_2"},
        {"level_1"}
    );

    REQUIRE(vol.name == "level_door");
    REQUIRE(vol.load_cells.size() == 1);
    REQUIRE(vol.unload_cells.size() == 1);
    REQUIRE(vol.block_until_loaded);
}

// ============================================================================
// PortalGraph Tests
// ============================================================================

TEST_CASE("PortalGraph clear", "[streaming][portal][graph]") {
    PortalGraph graph;
    graph.clear();

    REQUIRE(graph.adjacency.empty());
}

TEST_CASE("PortalGraph add_portal", "[streaming][portal][graph]") {
    PortalGraph graph;

    PortalGraph::PortalEdge edge;
    edge.target_cell = "room_b";
    edge.portal_center = Vec3{5.0f, 0.0f, 0.0f};
    edge.portal_normal = Vec3{1.0f, 0.0f, 0.0f};
    edge.width = 2.0f;
    edge.height = 3.0f;

    graph.add_portal("room_a", edge);

    const auto* portals = graph.get_portals_from("room_a");
    REQUIRE(portals != nullptr);
    REQUIRE(portals->size() == 1);
    REQUIRE((*portals)[0].target_cell == "room_b");
}

TEST_CASE("PortalGraph get_portals_from nonexistent", "[streaming][portal][graph]") {
    PortalGraph graph;

    const auto* portals = graph.get_portals_from("nonexistent");
    REQUIRE(portals == nullptr);
}

// ============================================================================
// StreamingVolumeManager Tests
// ============================================================================

TEST_CASE("StreamingVolumeManager clear_volumes", "[streaming][manager]") {
    StreamingVolumeManager manager;
    manager.clear_volumes();

    REQUIRE(manager.get_all_volume_names().empty());
}

TEST_CASE("StreamingVolumeManager add and get volume", "[streaming][manager]") {
    StreamingVolumeManager manager;

    StreamingVolume vol;
    vol.name = "test_vol";
    vol.position = Vec3{10.0f, 0.0f, 10.0f};

    manager.add_volume(vol);

    StreamingVolume* found = manager.get_volume("test_vol");
    REQUIRE(found != nullptr);
    REQUIRE(found->name == "test_vol");
}

TEST_CASE("StreamingVolumeManager remove_volume", "[streaming][manager]") {
    StreamingVolumeManager manager;

    StreamingVolume vol;
    vol.name = "removable";
    manager.add_volume(vol);

    REQUIRE(manager.get_volume("removable") != nullptr);

    manager.remove_volume("removable");

    REQUIRE(manager.get_volume("removable") == nullptr);
}

TEST_CASE("StreamingVolumeManager get_all_volume_names", "[streaming][manager]") {
    StreamingVolumeManager manager;

    StreamingVolume v1; v1.name = "vol1";
    StreamingVolume v2; v2.name = "vol2";
    StreamingVolume v3; v3.name = "vol3";

    manager.add_volume(v1);
    manager.add_volume(v2);
    manager.add_volume(v3);

    auto names = manager.get_all_volume_names();
    REQUIRE(names.size() == 3);
}

TEST_CASE("StreamingVolumeManager set_volume_enabled", "[streaming][manager]") {
    StreamingVolumeManager manager;

    StreamingVolume vol;
    vol.name = "toggle_vol";
    vol.enabled = true;
    manager.add_volume(vol);

    REQUIRE(manager.is_volume_enabled("toggle_vol"));

    manager.set_volume_enabled("toggle_vol", false);
    REQUIRE_FALSE(manager.is_volume_enabled("toggle_vol"));

    manager.set_volume_enabled("toggle_vol", true);
    REQUIRE(manager.is_volume_enabled("toggle_vol"));
}
