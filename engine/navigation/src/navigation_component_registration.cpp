#include <engine/reflect/reflect.hpp>
#include <engine/navigation/nav_agent.hpp>
#include <engine/navigation/nav_obstacle.hpp>
#include <engine/navigation/nav_behaviors.hpp>
#include <engine/navigation/navmesh_builder.hpp>

// This file registers all navigation components with the reflection system.
// Components are automatically registered at static initialization time.

namespace {

using namespace engine::navigation;
using namespace engine::reflect;

// Register NavAgentComponent
struct NavAgentComponentRegistrar {
    NavAgentComponentRegistrar() {
        TypeRegistry::instance().register_component<NavAgentComponent>("NavAgentComponent",
            TypeMeta().set_display_name("Nav Agent").set_description("Navigation agent for pathfinding and movement"));

        // Movement settings
        TypeRegistry::instance().register_property<NavAgentComponent, &NavAgentComponent::speed>("speed",
            PropertyMeta().set_display_name("Speed").set_category("Movement").set_range(0.0f, 100.0f));

        TypeRegistry::instance().register_property<NavAgentComponent, &NavAgentComponent::acceleration>("acceleration",
            PropertyMeta().set_display_name("Acceleration").set_category("Movement").set_range(0.0f, 100.0f));

        TypeRegistry::instance().register_property<NavAgentComponent, &NavAgentComponent::deceleration>("deceleration",
            PropertyMeta().set_display_name("Deceleration").set_category("Movement").set_range(0.0f, 100.0f));

        TypeRegistry::instance().register_property<NavAgentComponent, &NavAgentComponent::turning_speed>("turning_speed",
            PropertyMeta().set_display_name("Turning Speed").set_category("Movement").set_range(0.0f, 720.0f));

        // Path following settings
        TypeRegistry::instance().register_property<NavAgentComponent, &NavAgentComponent::path_radius>("path_radius",
            PropertyMeta().set_display_name("Path Radius").set_category("Path Following").set_range(0.1f, 10.0f));

        TypeRegistry::instance().register_property<NavAgentComponent, &NavAgentComponent::stopping_distance>("stopping_distance",
            PropertyMeta().set_display_name("Stopping Distance").set_category("Path Following").set_range(0.0f, 10.0f));

        TypeRegistry::instance().register_property<NavAgentComponent, &NavAgentComponent::height>("height",
            PropertyMeta().set_display_name("Height").set_category("Path Following").set_range(0.1f, 10.0f));

        // Avoidance settings
        TypeRegistry::instance().register_property<NavAgentComponent, &NavAgentComponent::avoidance_radius>("avoidance_radius",
            PropertyMeta().set_display_name("Avoidance Radius").set_category("Avoidance").set_range(0.1f, 10.0f));

        TypeRegistry::instance().register_property<NavAgentComponent, &NavAgentComponent::avoidance_priority>("avoidance_priority",
            PropertyMeta().set_display_name("Avoidance Priority").set_category("Avoidance").set_range(0, 99));

        // Crowd settings
        TypeRegistry::instance().register_property<NavAgentComponent, &NavAgentComponent::use_crowd>("use_crowd",
            PropertyMeta().set_display_name("Use Crowd").set_category("Crowd"));

        TypeRegistry::instance().register_property<NavAgentComponent, &NavAgentComponent::separation_weight>("separation_weight",
            PropertyMeta().set_display_name("Separation Weight").set_category("Crowd").set_range(0.0f, 10.0f));

        // Path settings
        TypeRegistry::instance().register_property<NavAgentComponent, &NavAgentComponent::auto_repath>("auto_repath",
            PropertyMeta().set_display_name("Auto Repath").set_category("Path"));

        TypeRegistry::instance().register_property<NavAgentComponent, &NavAgentComponent::repath_interval>("repath_interval",
            PropertyMeta().set_display_name("Repath Interval").set_category("Path").set_range(0.1f, 10.0f));

        TypeRegistry::instance().register_property<NavAgentComponent, &NavAgentComponent::corner_threshold>("corner_threshold",
            PropertyMeta().set_display_name("Corner Threshold").set_category("Path").set_range(0.01f, 1.0f));

        // Debug
        TypeRegistry::instance().register_property<NavAgentComponent, &NavAgentComponent::debug_draw>("debug_draw",
            PropertyMeta().set_display_name("Debug Draw").set_category("Debug"));

        // Note: Runtime state fields (state, target, velocity, path, etc.) are not registered
        // as they are transient and should not be serialized
    }
};
static NavAgentComponentRegistrar _navagent_registrar;

// Register NavMeshSource component
struct NavMeshSourceRegistrar {
    NavMeshSourceRegistrar() {
        TypeRegistry::instance().register_component<NavMeshSource>("NavMeshSource",
            TypeMeta().set_display_name("NavMesh Source").set_description("Static geometry for navmesh building"));

        TypeRegistry::instance().register_property<NavMeshSource, &NavMeshSource::area_type>("area_type",
            PropertyMeta().set_display_name("Area Type").set_range(0, 63));

        TypeRegistry::instance().register_property<NavMeshSource, &NavMeshSource::enabled>("enabled",
            PropertyMeta().set_display_name("Enabled"));

        // Note: vertices and indices are not registered - they are large data
        // that should be handled separately via asset loading
    }
};
static NavMeshSourceRegistrar _navmeshsource_registrar;

// Register OffMeshLinkComponent
struct OffMeshLinkComponentRegistrar {
    OffMeshLinkComponentRegistrar() {
        TypeRegistry::instance().register_component<OffMeshLinkComponent>("OffMeshLinkComponent",
            TypeMeta().set_display_name("Off-Mesh Link").set_description("Jump, ladder, or door connection between navmesh areas"));

        TypeRegistry::instance().register_property<OffMeshLinkComponent, &OffMeshLinkComponent::start_offset>("start_offset",
            PropertyMeta().set_display_name("Start Offset"));

        TypeRegistry::instance().register_property<OffMeshLinkComponent, &OffMeshLinkComponent::end_offset>("end_offset",
            PropertyMeta().set_display_name("End Offset"));

        TypeRegistry::instance().register_property<OffMeshLinkComponent, &OffMeshLinkComponent::radius>("radius",
            PropertyMeta().set_display_name("Radius").set_range(0.1f, 10.0f));

        TypeRegistry::instance().register_property<OffMeshLinkComponent, &OffMeshLinkComponent::enabled>("enabled",
            PropertyMeta().set_display_name("Enabled"));

        // Note: flags and area are enums - registered as their underlying type
        // For full enum support, these would need enum reflection
    }
};
static OffMeshLinkComponentRegistrar _offmeshlink_registrar;

// Register NavObstacleComponent
struct NavObstacleComponentRegistrar {
    NavObstacleComponentRegistrar() {
        TypeRegistry::instance().register_component<NavObstacleComponent>("NavObstacleComponent",
            TypeMeta().set_display_name("Nav Obstacle").set_description("Dynamic navigation obstacle that blocks pathfinding"));

        // Shape configuration
        TypeRegistry::instance().register_property<NavObstacleComponent, &NavObstacleComponent::cylinder_radius>("cylinder_radius",
            PropertyMeta().set_display_name("Cylinder Radius").set_category("Shape").set_range(0.1f, 50.0f));

        TypeRegistry::instance().register_property<NavObstacleComponent, &NavObstacleComponent::cylinder_height>("cylinder_height",
            PropertyMeta().set_display_name("Cylinder Height").set_category("Shape").set_range(0.1f, 50.0f));

        TypeRegistry::instance().register_property<NavObstacleComponent, &NavObstacleComponent::half_extents>("half_extents",
            PropertyMeta().set_display_name("Half Extents").set_category("Shape"));

        TypeRegistry::instance().register_property<NavObstacleComponent, &NavObstacleComponent::offset>("offset",
            PropertyMeta().set_display_name("Offset").set_category("Transform"));

        TypeRegistry::instance().register_property<NavObstacleComponent, &NavObstacleComponent::enabled>("enabled",
            PropertyMeta().set_display_name("Enabled"));

        // Note: shape is an enum - for full enum reflection support would need enum registry
        // Runtime state fields (handle, needs_update, last_position, last_y_rotation) not registered
    }
};
static NavObstacleComponentRegistrar _navobstacle_registrar;

// Register NavBehaviorComponent
struct NavBehaviorComponentRegistrar {
    NavBehaviorComponentRegistrar() {
        TypeRegistry::instance().register_component<NavBehaviorComponent>("NavBehaviorComponent",
            TypeMeta().set_display_name("Nav Behavior").set_description("Automatic navigation behaviors like patrol, wander, follow"));

        TypeRegistry::instance().register_property<NavBehaviorComponent, &NavBehaviorComponent::enabled>("enabled",
            PropertyMeta().set_display_name("Enabled"));

        // Wander settings
        TypeRegistry::instance().register_property<NavBehaviorComponent, &NavBehaviorComponent::wander_radius>("wander_radius",
            PropertyMeta().set_display_name("Wander Radius").set_category("Wander").set_range(1.0f, 100.0f));

        TypeRegistry::instance().register_property<NavBehaviorComponent, &NavBehaviorComponent::wander_wait_min>("wander_wait_min",
            PropertyMeta().set_display_name("Min Wait Time").set_category("Wander").set_range(0.0f, 60.0f));

        TypeRegistry::instance().register_property<NavBehaviorComponent, &NavBehaviorComponent::wander_wait_max>("wander_wait_max",
            PropertyMeta().set_display_name("Max Wait Time").set_category("Wander").set_range(0.0f, 60.0f));

        TypeRegistry::instance().register_property<NavBehaviorComponent, &NavBehaviorComponent::wander_origin>("wander_origin",
            PropertyMeta().set_display_name("Wander Origin").set_category("Wander"));

        // Patrol settings
        TypeRegistry::instance().register_property<NavBehaviorComponent, &NavBehaviorComponent::patrol_loop>("patrol_loop",
            PropertyMeta().set_display_name("Loop").set_category("Patrol"));

        TypeRegistry::instance().register_property<NavBehaviorComponent, &NavBehaviorComponent::patrol_wait_time>("patrol_wait_time",
            PropertyMeta().set_display_name("Wait Time").set_category("Patrol").set_range(0.0f, 60.0f));

        // Follow settings
        TypeRegistry::instance().register_property<NavBehaviorComponent, &NavBehaviorComponent::follow_target>("follow_target",
            PropertyMeta().set_display_name("Target Entity").set_category("Follow"));

        TypeRegistry::instance().register_property<NavBehaviorComponent, &NavBehaviorComponent::follow_distance>("follow_distance",
            PropertyMeta().set_display_name("Follow Distance").set_category("Follow").set_range(0.5f, 50.0f));

        TypeRegistry::instance().register_property<NavBehaviorComponent, &NavBehaviorComponent::follow_update_rate>("follow_update_rate",
            PropertyMeta().set_display_name("Update Rate").set_category("Follow").set_range(0.1f, 5.0f));

        // Flee settings
        TypeRegistry::instance().register_property<NavBehaviorComponent, &NavBehaviorComponent::flee_from>("flee_from",
            PropertyMeta().set_display_name("Flee From").set_category("Flee"));

        TypeRegistry::instance().register_property<NavBehaviorComponent, &NavBehaviorComponent::flee_distance>("flee_distance",
            PropertyMeta().set_display_name("Flee Distance").set_category("Flee").set_range(1.0f, 100.0f));

        // Note: type is an enum - needs enum reflection for full support
        // Note: patrol_points is a vector - needs collection reflection for full support
        // Runtime state fields not registered
    }
};
static NavBehaviorComponentRegistrar _navbehavior_registrar;

} // anonymous namespace
