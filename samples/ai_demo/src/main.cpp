// AI Demo - Demonstrates behavior trees, perception, patrol, and combat
//
// Controls:
//   WASD - Move player
//   ESC  - Quit
//
// AI Behavior:
//   Green  = Patrolling waypoints
//   Yellow = Alerted/Investigating
//   Orange = Chasing player
//   Red    = Attacking player

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <engine/core/application.hpp>
#include <engine/core/log.hpp>
#include <engine/core/time.hpp>
#include <engine/core/input.hpp>
#include <engine/scene/scene.hpp>
#include <engine/render/renderer.hpp>

// AI includes
#include <engine/ai/ai.hpp>
#include <engine/ai/behavior_tree.hpp>
#include <engine/ai/bt_composites.hpp>
#include <engine/ai/bt_decorators.hpp>
#include <engine/ai/bt_nodes.hpp>
#include <engine/ai/ai_components.hpp>
#include <engine/ai/perception.hpp>
#include <engine/ai/blackboard.hpp>

using namespace engine::core;
using namespace engine::scene;
namespace render = engine::render;
namespace ai = engine::ai;

// AI States for visual feedback
enum class AIState {
    Patrol,
    Alert,
    Chase,
    Attack
};

// Component to track AI visual state
struct AIVisualState {
    AIState state = AIState::Patrol;
};

// Tag component to mark the player entity
struct PlayerTag {};

// Helper to get color for AI state
uint32_t get_state_color(AIState state) {
    switch (state) {
        case AIState::Patrol: return 0x00FF00FF; // Green
        case AIState::Alert:  return 0xFFFF00FF; // Yellow
        case AIState::Chase:  return 0xFF8000FF; // Orange
        case AIState::Attack: return 0xFF0000FF; // Red
        default: return 0xFFFFFFFF;
    }
}

const char* get_state_name(AIState state) {
    switch (state) {
        case AIState::Patrol: return "Patrol";
        case AIState::Alert:  return "Alert";
        case AIState::Chase:  return "Chase";
        case AIState::Attack: return "Attack";
        default: return "Unknown";
    }
}

class AIDemoApp : public Application {
public:
    AIDemoApp() = default;

protected:
    void on_init() override {
        log(LogLevel::Info, "[AIDemo] AI Demo starting...");

        auto* renderer = get_renderer();
        if (!renderer) {
            log(LogLevel::Error, "[AIDemo] Renderer not available");
            quit();
            return;
        }

        // Create meshes
        m_cube_mesh = renderer->create_primitive(render::PrimitiveMesh::Cube, 1.0f);
        m_sphere_mesh = renderer->create_primitive(render::PrimitiveMesh::Sphere, 0.3f);

        // Create the ECS world and scheduler
        m_world = std::make_unique<World>();
        m_scheduler = std::make_unique<Scheduler>();

        // Register systems
        m_scheduler->add(Phase::FixedUpdate, transform_system, "transform", 0);

        // Create floor
        create_floor();

        // Create player
        create_player();

        // Create AI agent
        create_ai_agent();

        // Create waypoint markers
        create_waypoint_markers();

        // Create camera
        create_camera();

        log(LogLevel::Info, "[AIDemo] AI Demo initialized");
        log(LogLevel::Info, "[AIDemo] Controls: WASD to move player, ESC to quit");
    }

    void on_shutdown() override {
        log(LogLevel::Info, "[AIDemo] AI Demo shutting down...");

        m_world.reset();
        m_scheduler.reset();

        if (auto* renderer = get_renderer()) {
            renderer->destroy_mesh(m_cube_mesh);
            renderer->destroy_mesh(m_sphere_mesh);
        }
    }

    void on_fixed_update(double dt) override {
        float fdt = static_cast<float>(dt);

        // Update player movement
        update_player_movement(fdt);

        // Update AI perception and behavior
        update_ai(fdt);

        // Run ECS systems
        if (m_scheduler && m_world) {
            m_scheduler->run(*m_world, dt, Phase::FixedUpdate);
        }
    }

    void on_update(double dt) override {
        (void)dt;

        // Check for ESC to quit
        if (Input::key_pressed(Key::Escape)) {
            quit();
        }
    }

    void on_render(double alpha) override {
        auto* renderer = get_renderer();
        if (!renderer || !m_world) return;

        // Configure view
        render::ViewConfig view_config;
        view_config.render_target = render::RenderTargetHandle{};
        view_config.clear_color_enabled = true;
        view_config.clear_color = 0x1a1a2eff; // Dark blue-gray
        view_config.clear_depth_enabled = true;
        view_config.clear_depth = 1.0f;
        view_config.viewport_width = static_cast<uint16_t>(window_width());
        view_config.viewport_height = static_cast<uint16_t>(window_height());
        renderer->configure_view(static_cast<render::RenderView>(0), view_config);

        renderer->begin_frame();
        renderer->clear(0x1a1a2eff, 1.0f);

        // Set up camera - top-down view
        float aspect = static_cast<float>(window_width()) / static_cast<float>(window_height());
        Mat4 view = glm::lookAt(
            Vec3{0.0f, 25.0f, 15.0f},  // Camera position (high up, slightly angled)
            Vec3{0.0f, 0.0f, 0.0f},    // Look at center
            Vec3{0.0f, 1.0f, 0.0f}     // Up vector
        );
        Mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
        renderer->set_camera(view, proj);

        // Render all mesh renderers
        auto mesh_view = m_world->view<WorldTransform, MeshRenderer>();
        for (auto [entity, world_tf, mesh_renderer] : mesh_view.each()) {
            if (!mesh_renderer.visible) continue;

            render::DrawCall call;
            call.mesh = render::MeshHandle{mesh_renderer.mesh.id};
            call.material = render::MaterialHandle{mesh_renderer.material.id};
            call.transform = world_tf.matrix;
            call.render_layer = mesh_renderer.render_layer;
            call.cast_shadows = mesh_renderer.cast_shadows;

            renderer->queue_draw(call);
        }

        renderer->flush();
        renderer->end_frame();

        (void)alpha;
    }

private:
    void create_floor() {
        Entity floor = m_world->create("Floor");
        m_world->emplace<LocalTransform>(floor, Vec3{0.0f, -0.5f, 0.0f}, Quat{1, 0, 0, 0}, Vec3{30.0f, 1.0f, 30.0f});
        m_world->emplace<WorldTransform>(floor);
        m_world->emplace<PreviousTransform>(floor);
        m_world->emplace<MeshRenderer>(floor, MeshRenderer{
            MeshHandle{m_cube_mesh.id},
            MaterialHandle{},
            0, true, false, true
        });
    }

    void create_player() {
        m_player = m_world->create("Player");
        m_world->emplace<LocalTransform>(m_player, Vec3{0.0f, 0.5f, 8.0f});
        m_world->emplace<WorldTransform>(m_player);
        m_world->emplace<PreviousTransform>(m_player);
        m_world->emplace<MeshRenderer>(m_player, MeshRenderer{
            MeshHandle{m_cube_mesh.id},
            MaterialHandle{},
            0, true, true, true
        });
        m_world->emplace<PlayerTag>(m_player);

        // Add noise emitter so AI can hear player when moving
        ai::AINoiseEmitterComponent noise;
        noise.noise_radius = 5.0f;
        noise.loudness = 0.5f;
        noise.is_continuous = false;
        noise.noise_type = "footsteps";
        m_world->emplace<ai::AINoiseEmitterComponent>(m_player, noise);

        log(LogLevel::Info, "[AIDemo] Player created at (0, 0.5, 8)");
    }

    void create_ai_agent() {
        m_ai_agent = m_world->create("AIAgent");
        m_world->emplace<LocalTransform>(m_ai_agent, Vec3{0.0f, 0.5f, -8.0f});
        m_world->emplace<WorldTransform>(m_ai_agent);
        m_world->emplace<PreviousTransform>(m_ai_agent);
        m_world->emplace<MeshRenderer>(m_ai_agent, MeshRenderer{
            MeshHandle{m_cube_mesh.id},
            MaterialHandle{},
            0, true, true, true
        });

        // AI Controller with behavior tree
        ai::AIControllerComponent controller;
        controller.enabled = true;
        controller.update_interval = 0.1f;
        controller.ensure_blackboard();
        controller.behavior_tree = create_ai_behavior_tree();
        m_world->emplace<ai::AIControllerComponent>(m_ai_agent, std::move(controller));

        // Patrol component with waypoints in a square pattern
        ai::AIPatrolComponent patrol;
        patrol.type = ai::AIPatrolComponent::PatrolType::Loop;
        patrol.waypoints = {
            Vec3{-8.0f, 0.5f, -8.0f},
            Vec3{ 8.0f, 0.5f, -8.0f},
            Vec3{ 8.0f, 0.5f,  0.0f},
            Vec3{-8.0f, 0.5f,  0.0f}
        };
        patrol.patrol_speed = 3.0f;
        patrol.wait_time_min = 1.0f;
        patrol.wait_time_max = 2.0f;
        m_world->emplace<ai::AIPatrolComponent>(m_ai_agent, patrol);

        // Perception component
        ai::AIPerceptionComponent perception;
        perception.sight_enabled = true;
        perception.sight_range = 15.0f;
        perception.sight_angle = 120.0f;
        perception.hearing_enabled = true;
        perception.hearing_range = 10.0f;
        perception.awareness_gain_rate = 2.0f;
        perception.awareness_decay_rate = 0.3f;
        perception.awareness_threshold = 0.8f;
        perception.memory_duration = 8.0f;
        perception.faction = "enemy";
        perception.hostile_factions = {"player"};
        m_world->emplace<ai::AIPerceptionComponent>(m_ai_agent, perception);

        // Combat component
        ai::AICombatComponent combat;
        combat.attack_range = 2.0f;
        combat.attack_cooldown = 1.5f;
        combat.max_chase_distance = 25.0f;
        m_world->emplace<ai::AICombatComponent>(m_ai_agent, combat);

        // Visual state tracking
        m_world->emplace<AIVisualState>(m_ai_agent);

        log(LogLevel::Info, "[AIDemo] AI Agent created at (0, 0.5, -8)");
    }

    ai::BehaviorTreePtr create_ai_behavior_tree() {
        auto tree = std::make_shared<ai::BehaviorTree>("AIAgentBT");

        // Root selector - tries behaviors in priority order
        auto* root = tree->set_root<ai::BTSelector>("Root");

        // 1. Combat sequence: HasTarget && InRange -> Attack
        auto* combat_seq = root->add_child<ai::BTSequence>("CombatSequence");
        combat_seq->add_child(ai::make_condition("HasTarget", [](const ai::BTContext& ctx) {
            return ctx.blackboard->get_entity(ai::bb::TARGET_ENTITY) != NullEntity;
        }));
        combat_seq->add_child(ai::make_condition("InAttackRange", [](const ai::BTContext& ctx) {
            return ctx.blackboard->get_bool(ai::bb::IN_ATTACK_RANGE, false);
        }));
        combat_seq->add_child(ai::make_condition("CanAttack", [](const ai::BTContext& ctx) {
            return ctx.blackboard->get_bool(ai::bb::CAN_ATTACK, false);
        }));
        combat_seq->add_child(ai::make_action("Attack", [this](ai::BTContext& ctx) {
            set_ai_state(AIState::Attack);
            ctx.blackboard->set<float>(ai::bb::LAST_ATTACK_TIME, 0.0f);
            log(LogLevel::Info, "[AIDemo] AI ATTACKS!");
            return ai::BTStatus::Success;
        }));

        // 2. Chase sequence: HasTarget && Aware -> MoveTo target
        auto* chase_seq = root->add_child<ai::BTSequence>("ChaseSequence");
        chase_seq->add_child(ai::make_condition("HasTarget", [](const ai::BTContext& ctx) {
            return ctx.blackboard->get_entity(ai::bb::TARGET_ENTITY) != NullEntity;
        }));
        chase_seq->add_child(ai::make_condition("IsAware", [](const ai::BTContext& ctx) {
            float awareness = ctx.blackboard->get_float("awareness", 0.0f);
            return awareness >= 0.8f;
        }));
        chase_seq->add_child(ai::make_action("ChaseTarget", [this](ai::BTContext& ctx) {
            set_ai_state(AIState::Chase);
            Vec3 target_pos = ctx.blackboard->get_position(ai::bb::TARGET_POSITION);
            move_ai_towards(target_pos, 5.0f, ctx.delta_time);
            return ai::BTStatus::Running;
        }));

        // 3. Investigate sequence: IsAlerted -> MoveTo last known position
        auto* investigate_seq = root->add_child<ai::BTSequence>("InvestigateSequence");
        investigate_seq->add_child(ai::make_condition("IsAlerted", [](const ai::BTContext& ctx) {
            return ctx.blackboard->get_bool(ai::bb::IS_ALERTED, false);
        }));
        investigate_seq->add_child(ai::make_action("Investigate", [this](ai::BTContext& ctx) {
            set_ai_state(AIState::Alert);
            Vec3 last_pos = ctx.blackboard->get_position(ai::bb::LAST_KNOWN_POSITION);
            float dist = move_ai_towards(last_pos, 3.0f, ctx.delta_time);
            if (dist < 1.0f) {
                // Reached investigation point, clear alert
                ctx.blackboard->set_bool(ai::bb::IS_ALERTED, false);
                return ai::BTStatus::Success;
            }
            return ai::BTStatus::Running;
        }));

        // 4. Patrol action
        root->add_child(ai::make_action("Patrol", [this](ai::BTContext& ctx) {
            set_ai_state(AIState::Patrol);
            patrol_ai(ctx.delta_time);
            return ai::BTStatus::Running;
        }));

        return tree;
    }

    void create_waypoint_markers() {
        auto* patrol = m_world->try_get<ai::AIPatrolComponent>(m_ai_agent);
        if (!patrol) return;

        for (size_t i = 0; i < patrol->waypoints.size(); ++i) {
            const Vec3& wp = patrol->waypoints[i];
            Entity marker = m_world->create("Waypoint" + std::to_string(i));
            m_world->emplace<LocalTransform>(marker, Vec3{wp.x, 0.1f, wp.z}, Quat{1, 0, 0, 0}, Vec3{0.5f, 0.1f, 0.5f});
            m_world->emplace<WorldTransform>(marker);
            m_world->emplace<PreviousTransform>(marker);
            m_world->emplace<MeshRenderer>(marker, MeshRenderer{
                MeshHandle{m_cube_mesh.id},
                MaterialHandle{},
                0, true, false, false
            });
        }
    }

    void create_camera() {
        Entity camera = m_world->create("MainCamera");
        m_world->emplace<LocalTransform>(camera, Vec3{0.0f, 25.0f, 15.0f});
        m_world->emplace<WorldTransform>(camera);
        m_world->emplace<Camera>(camera);
    }

    void update_player_movement(float dt) {
        if (!m_world->valid(m_player)) return;

        auto& transform = m_world->get<LocalTransform>(m_player);
        Vec3 movement{0.0f};
        const float speed = 8.0f;

        if (Input::key_down(Key::W)) movement.z -= 1.0f;
        if (Input::key_down(Key::S)) movement.z += 1.0f;
        if (Input::key_down(Key::A)) movement.x -= 1.0f;
        if (Input::key_down(Key::D)) movement.x += 1.0f;

        if (glm::length(movement) > 0.01f) {
            movement = glm::normalize(movement) * speed * dt;
            transform.position += movement;

            // Clamp to play area
            transform.position.x = glm::clamp(transform.position.x, -14.0f, 14.0f);
            transform.position.z = glm::clamp(transform.position.z, -14.0f, 14.0f);

            // Emit footstep noise
            auto* noise = m_world->try_get<ai::AINoiseEmitterComponent>(m_player);
            if (noise) {
                noise->trigger_noise = true;
            }
        }
    }

    void update_ai(float dt) {
        if (!m_world->valid(m_ai_agent) || !m_world->valid(m_player)) return;

        auto* controller = m_world->try_get<ai::AIControllerComponent>(m_ai_agent);
        auto* perception = m_world->try_get<ai::AIPerceptionComponent>(m_ai_agent);
        auto* combat = m_world->try_get<ai::AICombatComponent>(m_ai_agent);

        if (!controller || !perception || !combat || !controller->blackboard) return;

        // Get positions
        auto& ai_transform = m_world->get<LocalTransform>(m_ai_agent);
        auto& player_transform = m_world->get<LocalTransform>(m_player);
        Vec3 ai_pos = ai_transform.position;
        Vec3 player_pos = player_transform.position;

        // Calculate distance and direction
        Vec3 to_player = player_pos - ai_pos;
        float distance = glm::length(to_player);
        Vec3 ai_forward = Vec3{0.0f, 0.0f, -1.0f}; // Default forward

        // Simple perception check (no actual perception system, just distance-based)
        float awareness = controller->blackboard->get_float("awareness", 0.0f);

        // Check if player is in sight range and FOV
        bool in_sight_range = distance < perception->sight_range;
        bool in_fov = true; // Simplified - assume always in FOV for demo
        if (in_sight_range && in_fov && distance > 0.01f) {
            // Build awareness
            float gain = perception->awareness_gain_rate * dt;
            if (distance < perception->instant_awareness_distance) {
                awareness = 1.0f; // Instant awareness at close range
            } else {
                awareness = glm::min(awareness + gain, 1.0f);
            }

            // Update blackboard
            controller->blackboard->set_entity(ai::bb::TARGET_ENTITY, m_player);
            controller->blackboard->set_position(ai::bb::TARGET_POSITION, player_pos);
            controller->blackboard->set_float(ai::bb::TARGET_DISTANCE, distance);
            controller->blackboard->set_position(ai::bb::LAST_KNOWN_POSITION, player_pos);

            if (awareness >= perception->awareness_threshold) {
                controller->blackboard->set_bool(ai::bb::IS_ALERTED, true);
            }
        } else {
            // Decay awareness
            float decay = perception->awareness_decay_rate * dt;
            awareness = glm::max(awareness - decay, 0.0f);

            if (awareness < 0.1f) {
                controller->blackboard->set_entity(ai::bb::TARGET_ENTITY, NullEntity);
            }
        }

        controller->blackboard->set<float>("awareness", awareness);

        // Update combat state
        combat->time_since_attack += dt;
        bool in_attack_range = combat->in_attack_range(distance);
        bool can_attack = combat->can_attack();
        controller->blackboard->set_bool(ai::bb::IN_ATTACK_RANGE, in_attack_range);
        controller->blackboard->set_bool(ai::bb::CAN_ATTACK, can_attack);

        // Tick behavior tree
        if (controller->should_update(dt) && controller->behavior_tree) {
            ai::BTContext ctx;
            ctx.world = m_world.get();
            ctx.entity = m_ai_agent;
            ctx.blackboard = controller->blackboard.get();
            ctx.delta_time = dt;

            controller->behavior_tree->tick(ctx);
        }
    }

    void set_ai_state(AIState state) {
        if (!m_world->valid(m_ai_agent)) return;

        auto* visual = m_world->try_get<AIVisualState>(m_ai_agent);
        if (visual && visual->state != state) {
            log(LogLevel::Debug, "[AIDemo] AI state: {} -> {}", get_state_name(visual->state), get_state_name(state));
            visual->state = state;
        }
    }

    float move_ai_towards(const Vec3& target, float speed, float dt) {
        if (!m_world->valid(m_ai_agent)) return 0.0f;

        auto& transform = m_world->get<LocalTransform>(m_ai_agent);
        Vec3 to_target = target - transform.position;
        to_target.y = 0.0f; // Keep on ground plane

        float distance = glm::length(to_target);
        if (distance > 0.1f) {
            Vec3 direction = glm::normalize(to_target);
            float move_dist = glm::min(speed * dt, distance);
            transform.position += direction * move_dist;

            // Face movement direction
            float angle = std::atan2(direction.x, direction.z);
            transform.rotation = glm::angleAxis(angle, Vec3{0.0f, 1.0f, 0.0f});
        }

        return distance;
    }

    void patrol_ai(float dt) {
        if (!m_world->valid(m_ai_agent)) return;

        auto* patrol = m_world->try_get<ai::AIPatrolComponent>(m_ai_agent);
        if (!patrol || patrol->waypoints.empty()) return;

        Vec3 target_wp = patrol->get_current_waypoint();
        float dist = move_ai_towards(target_wp, patrol->patrol_speed, dt);

        // Check if arrived at waypoint
        if (dist < patrol->arrival_distance) {
            if (patrol->is_waiting) {
                patrol->time_at_waypoint += dt;
                if (patrol->time_at_waypoint >= patrol->current_wait_time) {
                    patrol->is_waiting = false;
                    patrol->advance_waypoint();
                }
            } else {
                // Start waiting
                patrol->is_waiting = true;
                patrol->time_at_waypoint = 0.0f;
                patrol->current_wait_time = patrol->wait_time_min +
                    static_cast<float>(rand()) / RAND_MAX * (patrol->wait_time_max - patrol->wait_time_min);
            }
        }
    }

private:
    std::unique_ptr<World> m_world;
    std::unique_ptr<Scheduler> m_scheduler;

    render::MeshHandle m_cube_mesh;
    render::MeshHandle m_sphere_mesh;

    Entity m_player = NullEntity;
    Entity m_ai_agent = NullEntity;
};

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    AIDemoApp app;
    return app.run();
}
#else
int main(int argc, char** argv) {
    AIDemoApp app;
    return app.run(argc, argv);
}
#endif
