#include <engine/physics/cloth.hpp>
#include <engine/physics/physics_world.hpp>
#include <cmath>
#include <algorithm>

namespace engine::physics {

// Distance constraint between two particles
struct DistanceConstraint {
    uint32_t p1, p2;
    float rest_length;
    float stiffness;
};

// Bend constraint for three particles (prevents sharp folds)
struct BendConstraint {
    uint32_t p1, p2, p3;  // p2 is the center vertex
    float rest_angle;
    float stiffness;
};

// Internal implementation
struct Cloth::Impl {
    // Particle data
    std::vector<Vec3> positions;
    std::vector<Vec3> prev_positions;
    std::vector<Vec3> velocities;
    std::vector<float> inv_masses;  // Inverse mass (0 = fixed)

    // Constraints
    std::vector<DistanceConstraint> distance_constraints;
    std::vector<BendConstraint> bend_constraints;

    // Mesh data
    std::vector<uint32_t> indices;
    std::vector<Vec2> uvs;

    // Grid info (if using grid)
    uint32_t grid_width = 0;
    uint32_t grid_height = 0;

    // Wind accumulator
    Vec3 accumulated_wind{0.0f};
    float wind_time = 0.0f;

    // Collision cache
    std::vector<PhysicsBodyId> nearby_bodies;
};

Cloth::Cloth() : m_impl(std::make_unique<Impl>()) {}

Cloth::~Cloth() {
    shutdown();
}

Cloth::Cloth(Cloth&&) noexcept = default;
Cloth& Cloth::operator=(Cloth&&) noexcept = default;

void Cloth::init(PhysicsWorld& world, const ClothComponent& settings) {
    if (m_initialized) {
        shutdown();
    }

    m_world = &world;
    m_settings = settings;

    // Generate mesh if using grid
    if (m_settings.mesh.use_grid) {
        generate_grid_mesh();
    } else {
        // Copy custom mesh data
        m_impl->positions = m_settings.mesh.vertices;
        m_impl->indices = m_settings.mesh.indices;
        m_impl->uvs = m_settings.mesh.uvs;
    }

    // Store initial positions for reset
    m_initial_positions = m_impl->positions;
    m_initial_attachments = m_settings.attachments;

    // Initialize velocities and masses
    uint32_t vertex_count = static_cast<uint32_t>(m_impl->positions.size());
    m_impl->prev_positions = m_impl->positions;
    m_impl->velocities.resize(vertex_count, Vec3{0.0f});

    // Distribute mass evenly
    float mass_per_vertex = m_settings.mass / static_cast<float>(vertex_count);
    m_impl->inv_masses.resize(vertex_count, 1.0f / mass_per_vertex);

    // Apply attachments (set inverse mass to 0 for fixed vertices)
    for (const auto& attachment : m_settings.attachments) {
        if (attachment.vertex_index < vertex_count &&
            attachment.type == AttachmentType::Fixed) {
            m_impl->inv_masses[attachment.vertex_index] = 0.0f;

            // Set position if world-attached
            if (!attachment.attach_to_entity) {
                m_impl->positions[attachment.vertex_index] = attachment.world_position;
            }
        }
    }

    // Create constraints
    create_constraints();

    // Initialize state
    m_state.positions = m_impl->positions;
    m_state.velocities = m_impl->velocities;
    m_state.normals.resize(vertex_count, Vec3{0.0f, 0.0f, 1.0f});

    update_normals();
    update_bounds();

    m_initialized = true;
}

void Cloth::shutdown() {
    if (!m_initialized) return;

    m_impl->positions.clear();
    m_impl->prev_positions.clear();
    m_impl->velocities.clear();
    m_impl->inv_masses.clear();
    m_impl->distance_constraints.clear();
    m_impl->bend_constraints.clear();
    m_impl->indices.clear();
    m_impl->uvs.clear();

    m_state = ClothState{};
    m_world = nullptr;
    m_initialized = false;
}

void Cloth::generate_grid_mesh() {
    const auto& grid = m_settings.mesh.grid;
    uint32_t w = grid.width_segments + 1;
    uint32_t h = grid.height_segments + 1;

    m_impl->grid_width = w;
    m_impl->grid_height = h;

    // Generate vertices
    m_impl->positions.clear();
    m_impl->uvs.clear();

    float dx = grid.width / static_cast<float>(grid.width_segments);
    float dy = grid.height / static_cast<float>(grid.height_segments);
    float start_x = -grid.width * 0.5f;
    float start_y = grid.height * 0.5f;  // Top-down

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            Vec3 pos{
                start_x + x * dx,
                start_y - y * dy,
                0.0f
            };
            m_impl->positions.push_back(pos);

            Vec2 uv{
                static_cast<float>(x) / static_cast<float>(grid.width_segments),
                static_cast<float>(y) / static_cast<float>(grid.height_segments)
            };
            m_impl->uvs.push_back(uv);
        }
    }

    // Generate indices (triangles)
    m_impl->indices.clear();
    for (uint32_t y = 0; y < grid.height_segments; ++y) {
        for (uint32_t x = 0; x < grid.width_segments; ++x) {
            uint32_t i0 = y * w + x;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + w;
            uint32_t i3 = i2 + 1;

            // First triangle
            m_impl->indices.push_back(i0);
            m_impl->indices.push_back(i2);
            m_impl->indices.push_back(i1);

            // Second triangle
            m_impl->indices.push_back(i1);
            m_impl->indices.push_back(i2);
            m_impl->indices.push_back(i3);
        }
    }
}

void Cloth::create_constraints() {
    if (!m_settings.mesh.use_grid) {
        // For custom meshes, create constraints from triangle edges
        // This is a simplified version - production would use edge detection
        std::vector<std::pair<uint32_t, uint32_t>> edges;

        for (size_t i = 0; i < m_impl->indices.size(); i += 3) {
            uint32_t i0 = m_impl->indices[i];
            uint32_t i1 = m_impl->indices[i + 1];
            uint32_t i2 = m_impl->indices[i + 2];

            auto add_edge = [&](uint32_t a, uint32_t b) {
                if (a > b) std::swap(a, b);
                edges.emplace_back(a, b);
            };

            add_edge(i0, i1);
            add_edge(i1, i2);
            add_edge(i2, i0);
        }

        // Remove duplicates
        std::sort(edges.begin(), edges.end());
        edges.erase(std::unique(edges.begin(), edges.end()), edges.end());

        // Create distance constraints
        for (const auto& [p1, p2] : edges) {
            Vec3 diff = m_impl->positions[p2] - m_impl->positions[p1];
            float length = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

            DistanceConstraint dc;
            dc.p1 = p1;
            dc.p2 = p2;
            dc.rest_length = length;
            dc.stiffness = m_settings.edge_stiffness;
            m_impl->distance_constraints.push_back(dc);
        }
        return;
    }

    // Grid-based constraint generation
    uint32_t w = m_impl->grid_width;
    uint32_t h = m_impl->grid_height;

    auto get_index = [w](uint32_t x, uint32_t y) { return y * w + x; };

    auto add_distance_constraint = [&](uint32_t p1, uint32_t p2, float stiffness) {
        Vec3 diff = m_impl->positions[p2] - m_impl->positions[p1];
        float length = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

        DistanceConstraint dc;
        dc.p1 = p1;
        dc.p2 = p2;
        dc.rest_length = length;
        dc.stiffness = stiffness;
        m_impl->distance_constraints.push_back(dc);
    };

    // Structural constraints (horizontal and vertical)
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            uint32_t idx = get_index(x, y);

            // Right neighbor
            if (x < w - 1) {
                add_distance_constraint(idx, get_index(x + 1, y), m_settings.edge_stiffness);
            }

            // Bottom neighbor
            if (y < h - 1) {
                add_distance_constraint(idx, get_index(x, y + 1), m_settings.edge_stiffness);
            }
        }
    }

    // Shear constraints (diagonal)
    for (uint32_t y = 0; y < h - 1; ++y) {
        for (uint32_t x = 0; x < w - 1; ++x) {
            uint32_t idx = get_index(x, y);

            // Diagonal right-down
            add_distance_constraint(idx, get_index(x + 1, y + 1), m_settings.shear_stiffness);

            // Diagonal left-down
            if (x > 0) {
                add_distance_constraint(get_index(x, y), get_index(x - 1, y + 1), m_settings.shear_stiffness);
            }
        }
    }

    // Bend constraints (skip one vertex for bending resistance)
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            uint32_t idx = get_index(x, y);

            // Skip one horizontal
            if (x < w - 2) {
                add_distance_constraint(idx, get_index(x + 2, y), m_settings.bend_stiffness);
            }

            // Skip one vertical
            if (y < h - 2) {
                add_distance_constraint(idx, get_index(x, y + 2), m_settings.bend_stiffness);
            }
        }
    }
}

void Cloth::set_enabled(bool enabled) {
    m_enabled = enabled;
    if (enabled && m_state.is_sleeping) {
        wake_up();
    }
}

void Cloth::set_position(const Vec3& pos) {
    Vec3 delta = pos - m_position;
    m_position = pos;

    // Move all particles
    for (auto& p : m_impl->positions) {
        p = p + delta;
    }
    for (auto& p : m_impl->prev_positions) {
        p = p + delta;
    }

    update_bounds();
}

Vec3 Cloth::get_position() const {
    return m_position;
}

void Cloth::set_rotation(const Quat& rot) {
    m_rotation = rot;
    // Note: Full rotation would require rotating all vertices around center
    // This is typically not needed for cloth attached at fixed points
}

Quat Cloth::get_rotation() const {
    return m_rotation;
}

void Cloth::teleport(const Vec3& pos, const Quat& rot) {
    // Store old center
    Vec3 old_center = m_state.center;

    // Calculate transform
    Vec3 translation = pos - old_center;

    // Apply to all particles
    for (size_t i = 0; i < m_impl->positions.size(); ++i) {
        m_impl->positions[i] = m_impl->positions[i] + translation;
        m_impl->prev_positions[i] = m_impl->positions[i];
        m_impl->velocities[i] = Vec3{0.0f};
    }

    m_position = pos;
    m_rotation = rot;

    update_bounds();
}

void Cloth::attach_vertex(uint32_t vertex_index, const Vec3& world_position) {
    if (vertex_index >= m_impl->positions.size()) return;

    m_impl->positions[vertex_index] = world_position;
    m_impl->prev_positions[vertex_index] = world_position;
    m_impl->inv_masses[vertex_index] = 0.0f;

    ClothAttachment attachment;
    attachment.vertex_index = vertex_index;
    attachment.type = AttachmentType::Fixed;
    attachment.attach_to_entity = false;
    attachment.world_position = world_position;
    m_settings.attachments.push_back(attachment);
}

void Cloth::attach_vertex_to_entity(uint32_t vertex_index, uint32_t entity_id, const Vec3& local_offset) {
    if (vertex_index >= m_impl->positions.size()) return;

    m_impl->inv_masses[vertex_index] = 0.0f;

    ClothAttachment attachment;
    attachment.vertex_index = vertex_index;
    attachment.type = AttachmentType::Fixed;
    attachment.attach_to_entity = true;
    attachment.entity_id = entity_id;
    attachment.local_offset = local_offset;
    m_settings.attachments.push_back(attachment);
}

void Cloth::detach_vertex(uint32_t vertex_index) {
    if (vertex_index >= m_impl->positions.size()) return;

    // Restore mass
    float mass_per_vertex = m_settings.mass / static_cast<float>(m_impl->positions.size());
    m_impl->inv_masses[vertex_index] = 1.0f / mass_per_vertex;

    // Remove from attachments
    auto it = std::remove_if(m_settings.attachments.begin(), m_settings.attachments.end(),
        [vertex_index](const ClothAttachment& a) { return a.vertex_index == vertex_index; });
    m_settings.attachments.erase(it, m_settings.attachments.end());
}

void Cloth::detach_all() {
    float mass_per_vertex = m_settings.mass / static_cast<float>(m_impl->positions.size());
    for (size_t i = 0; i < m_impl->inv_masses.size(); ++i) {
        m_impl->inv_masses[i] = 1.0f / mass_per_vertex;
    }
    m_settings.attachments.clear();
}

void Cloth::set_attachment_position(uint32_t vertex_index, const Vec3& world_position) {
    if (vertex_index >= m_impl->positions.size()) return;

    for (auto& attachment : m_settings.attachments) {
        if (attachment.vertex_index == vertex_index) {
            attachment.world_position = world_position;
            m_impl->positions[vertex_index] = world_position;
            break;
        }
    }
}

bool Cloth::is_vertex_attached(uint32_t vertex_index) const {
    if (vertex_index >= m_impl->inv_masses.size()) return false;
    return m_impl->inv_masses[vertex_index] == 0.0f;
}

void Cloth::set_vertex_position(uint32_t index, const Vec3& position) {
    if (index < m_impl->positions.size()) {
        m_impl->positions[index] = position;
    }
}

Vec3 Cloth::get_vertex_position(uint32_t index) const {
    if (index < m_impl->positions.size()) {
        return m_impl->positions[index];
    }
    return Vec3{0.0f};
}

void Cloth::set_vertex_velocity(uint32_t index, const Vec3& velocity) {
    if (index < m_impl->velocities.size()) {
        m_impl->velocities[index] = velocity;
    }
}

Vec3 Cloth::get_vertex_velocity(uint32_t index) const {
    if (index < m_impl->velocities.size()) {
        return m_impl->velocities[index];
    }
    return Vec3{0.0f};
}

void Cloth::set_vertex_mass(uint32_t index, float mass) {
    if (index < m_impl->inv_masses.size() && mass > 0.0f) {
        m_impl->inv_masses[index] = 1.0f / mass;
    }
}

float Cloth::get_vertex_mass(uint32_t index) const {
    if (index < m_impl->inv_masses.size() && m_impl->inv_masses[index] > 0.0f) {
        return 1.0f / m_impl->inv_masses[index];
    }
    return 0.0f;  // Fixed vertex
}

uint32_t Cloth::get_vertex_count() const {
    return static_cast<uint32_t>(m_impl->positions.size());
}

void Cloth::add_force(const Vec3& force) {
    for (size_t i = 0; i < m_impl->velocities.size(); ++i) {
        if (m_impl->inv_masses[i] > 0.0f) {
            m_impl->velocities[i] = m_impl->velocities[i] + force * m_impl->inv_masses[i];
        }
    }
    wake_up();
}

void Cloth::add_force_at_vertex(uint32_t index, const Vec3& force) {
    if (index < m_impl->velocities.size() && m_impl->inv_masses[index] > 0.0f) {
        m_impl->velocities[index] = m_impl->velocities[index] + force * m_impl->inv_masses[index];
    }
    wake_up();
}

void Cloth::add_impulse(const Vec3& impulse) {
    for (size_t i = 0; i < m_impl->velocities.size(); ++i) {
        if (m_impl->inv_masses[i] > 0.0f) {
            m_impl->velocities[i] = m_impl->velocities[i] + impulse;
        }
    }
    wake_up();
}

void Cloth::add_impulse_at_vertex(uint32_t index, const Vec3& impulse) {
    if (index < m_impl->velocities.size() && m_impl->inv_masses[index] > 0.0f) {
        m_impl->velocities[index] = m_impl->velocities[index] + impulse;
    }
    wake_up();
}

void Cloth::add_explosion_force(const Vec3& center, float force, float radius) {
    float radius_sq = radius * radius;

    for (size_t i = 0; i < m_impl->positions.size(); ++i) {
        if (m_impl->inv_masses[i] <= 0.0f) continue;

        Vec3 diff = m_impl->positions[i] - center;
        float dist_sq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;

        if (dist_sq < radius_sq && dist_sq > 0.0001f) {
            float dist = std::sqrt(dist_sq);
            float falloff = 1.0f - (dist / radius);
            Vec3 dir = diff * (1.0f / dist);
            Vec3 impulse = dir * (force * falloff * m_impl->inv_masses[i]);
            m_impl->velocities[i] = m_impl->velocities[i] + impulse;
        }
    }
    wake_up();
}

void Cloth::set_wind_mode(ClothWindMode mode) {
    m_settings.wind_mode = mode;
}

void Cloth::set_wind(const Vec3& direction, float strength) {
    float len = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (len > 0.0001f) {
        m_settings.wind.direction = direction * (1.0f / len);
    }
    m_settings.wind.strength = strength;
}

void Cloth::set_wind_turbulence(float turbulence, float frequency) {
    m_settings.wind.turbulence = turbulence;
    m_settings.wind.turbulence_frequency = frequency;
}

void Cloth::apply_wind(const Vec3& wind_velocity) {
    m_impl->accumulated_wind = wind_velocity;
    wake_up();
}

void Cloth::get_render_data(std::vector<Vec3>& positions,
                            std::vector<Vec3>& normals,
                            std::vector<uint32_t>& indices) const {
    positions = m_state.positions;
    normals = m_state.normals;
    indices = m_impl->indices;
}

void Cloth::set_stiffness(float edge, float bend, float shear) {
    m_settings.edge_stiffness = edge;
    m_settings.bend_stiffness = bend;
    m_settings.shear_stiffness = shear;

    // Update existing constraints
    for (auto& c : m_impl->distance_constraints) {
        // Determine constraint type based on original stiffness value
        // This is a simplification; production code would track constraint types
        c.stiffness = edge;  // Default to edge stiffness
    }
}

void Cloth::set_damping(float damping) {
    m_settings.damping = damping;
}

void Cloth::set_mass(float total_mass) {
    m_settings.mass = total_mass;

    float mass_per_vertex = total_mass / static_cast<float>(m_impl->positions.size());
    for (size_t i = 0; i < m_impl->inv_masses.size(); ++i) {
        if (m_impl->inv_masses[i] > 0.0f) {  // Don't change fixed vertices
            m_impl->inv_masses[i] = 1.0f / mass_per_vertex;
        }
    }
}

void Cloth::set_gravity(const Vec3& gravity) {
    m_settings.custom_gravity = gravity;
}

void Cloth::set_use_gravity(bool use) {
    m_settings.use_gravity = use;
}

void Cloth::set_collision_enabled(bool world, bool dynamic, bool self) {
    m_settings.collision.world_collision = world;
    m_settings.collision.dynamic_collision = dynamic;
    m_settings.collision.self_collision = self;
}

void Cloth::set_collision_margin(float margin) {
    m_settings.collision.collision_margin = margin;
}

void Cloth::set_collision_mask(uint16_t mask) {
    m_settings.collision.collision_mask = mask;
}

void Cloth::wake_up() {
    m_state.is_sleeping = false;
    m_settings.is_sleeping = false;
}

void Cloth::put_to_sleep() {
    m_state.is_sleeping = true;
    m_settings.is_sleeping = true;

    // Zero velocities
    for (auto& v : m_impl->velocities) {
        v = Vec3{0.0f};
    }
}

void Cloth::set_solver_iterations(int iterations) {
    m_settings.solver_iterations = std::max(1, iterations);
}

void Cloth::set_substep_delta(float delta) {
    m_settings.substep_delta = std::max(1.0f / 240.0f, delta);
}

void Cloth::reset() {
    if (!m_initialized) return;

    // Restore initial positions
    m_impl->positions = m_initial_positions;
    m_impl->prev_positions = m_initial_positions;

    // Reset velocities
    for (auto& v : m_impl->velocities) {
        v = Vec3{0.0f};
    }

    // Restore attachments
    m_settings.attachments = m_initial_attachments;

    // Reset masses
    float mass_per_vertex = m_settings.mass / static_cast<float>(m_impl->positions.size());
    for (size_t i = 0; i < m_impl->inv_masses.size(); ++i) {
        m_impl->inv_masses[i] = 1.0f / mass_per_vertex;
    }

    // Apply attachments
    for (const auto& attachment : m_settings.attachments) {
        if (attachment.vertex_index < m_impl->positions.size() &&
            attachment.type == AttachmentType::Fixed) {
            m_impl->inv_masses[attachment.vertex_index] = 0.0f;
        }
    }

    // Update state
    m_state.positions = m_impl->positions;
    m_state.is_sleeping = false;
    m_settings.is_sleeping = false;

    update_normals();
    update_bounds();
}

void Cloth::update(float dt) {
    if (!m_initialized || !m_enabled) return;
    if (m_state.is_sleeping) return;

    // Substep simulation
    float remaining = dt;
    float substep = m_settings.substep_delta;

    while (remaining > 0.0f) {
        float step = std::min(remaining, substep);
        remaining -= step;

        // Apply external forces
        apply_gravity(step);
        apply_wind_forces(step);

        // Verlet integration
        for (size_t i = 0; i < m_impl->positions.size(); ++i) {
            if (m_impl->inv_masses[i] <= 0.0f) continue;

            Vec3 vel = m_impl->positions[i] - m_impl->prev_positions[i];

            // Apply damping
            vel = vel * (1.0f - m_settings.damping);

            m_impl->prev_positions[i] = m_impl->positions[i];
            m_impl->positions[i] = m_impl->positions[i] + vel;
        }

        // Solve constraints multiple times for stability
        for (int iter = 0; iter < m_settings.solver_iterations; ++iter) {
            solve_attachment_constraints();
            solve_distance_constraints();
        }

        // Collision detection/response
        if (m_settings.type == ClothType::Interactive) {
            solve_collision_constraints();
        }
    }

    // Update velocities from position delta
    float inv_dt = 1.0f / dt;
    for (size_t i = 0; i < m_impl->velocities.size(); ++i) {
        Vec3 diff = m_impl->positions[i] - m_impl->prev_positions[i];
        m_impl->velocities[i] = diff * inv_dt;
    }

    // Update state
    m_state.positions = m_impl->positions;
    m_state.velocities = m_impl->velocities;

    update_normals();
    update_bounds();
    check_sleep_state();
}

void Cloth::update_bounds() {
    if (m_impl->positions.empty()) return;

    m_state.bounds_min = m_impl->positions[0];
    m_state.bounds_max = m_impl->positions[0];
    Vec3 sum{0.0f};

    for (const auto& p : m_impl->positions) {
        m_state.bounds_min.x = std::min(m_state.bounds_min.x, p.x);
        m_state.bounds_min.y = std::min(m_state.bounds_min.y, p.y);
        m_state.bounds_min.z = std::min(m_state.bounds_min.z, p.z);

        m_state.bounds_max.x = std::max(m_state.bounds_max.x, p.x);
        m_state.bounds_max.y = std::max(m_state.bounds_max.y, p.y);
        m_state.bounds_max.z = std::max(m_state.bounds_max.z, p.z);

        sum = sum + p;
    }

    float inv_count = 1.0f / static_cast<float>(m_impl->positions.size());
    m_state.center = sum * inv_count;
}

void Cloth::update_normals() {
    if (m_impl->positions.empty()) return;

    // Reset normals
    for (auto& n : m_state.normals) {
        n = Vec3{0.0f};
    }

    // Compute face normals and accumulate to vertices
    for (size_t i = 0; i < m_impl->indices.size(); i += 3) {
        uint32_t i0 = m_impl->indices[i];
        uint32_t i1 = m_impl->indices[i + 1];
        uint32_t i2 = m_impl->indices[i + 2];

        Vec3 v0 = m_impl->positions[i0];
        Vec3 v1 = m_impl->positions[i1];
        Vec3 v2 = m_impl->positions[i2];

        Vec3 e1 = v1 - v0;
        Vec3 e2 = v2 - v0;

        // Cross product
        Vec3 normal{
            e1.y * e2.z - e1.z * e2.y,
            e1.z * e2.x - e1.x * e2.z,
            e1.x * e2.y - e1.y * e2.x
        };

        m_state.normals[i0] = m_state.normals[i0] + normal;
        m_state.normals[i1] = m_state.normals[i1] + normal;
        m_state.normals[i2] = m_state.normals[i2] + normal;
    }

    // Normalize
    for (auto& n : m_state.normals) {
        float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        if (len > 0.0001f) {
            n = n * (1.0f / len);
        } else {
            n = Vec3{0.0f, 0.0f, 1.0f};
        }
    }
}

void Cloth::apply_gravity(float dt) {
    if (!m_settings.use_gravity) return;

    Vec3 gravity = m_settings.custom_gravity;
    Vec3 displacement = gravity * (dt * dt);

    for (size_t i = 0; i < m_impl->positions.size(); ++i) {
        if (m_impl->inv_masses[i] > 0.0f) {
            m_impl->positions[i] = m_impl->positions[i] + displacement;
        }
    }
}

void Cloth::apply_wind_forces(float dt) {
    if (m_settings.wind_mode == ClothWindMode::None) return;

    // Update wind time for turbulence
    m_impl->wind_time += dt;

    Vec3 wind_vel{0.0f};

    if (m_settings.wind_mode == ClothWindMode::Global || m_settings.wind_mode == ClothWindMode::Local) {
        wind_vel = m_settings.wind.direction * m_settings.wind.strength;
    }

    // Add accumulated wind
    wind_vel = wind_vel + m_impl->accumulated_wind;
    m_impl->accumulated_wind = Vec3{0.0f};

    // Apply turbulence
    if (m_settings.wind.turbulence > 0.0f) {
        float t = m_impl->wind_time * m_settings.wind.turbulence_frequency;
        float turb_x = std::sin(t * 1.1f) * m_settings.wind.turbulence;
        float turb_y = std::sin(t * 1.3f + 1.0f) * m_settings.wind.turbulence * 0.5f;
        float turb_z = std::sin(t * 0.9f + 2.0f) * m_settings.wind.turbulence;

        wind_vel.x += turb_x * m_settings.wind.strength;
        wind_vel.y += turb_y * m_settings.wind.strength;
        wind_vel.z += turb_z * m_settings.wind.strength;
    }

    // Apply wind force based on normal direction
    for (size_t i = 0; i < m_impl->positions.size(); ++i) {
        if (m_impl->inv_masses[i] <= 0.0f) continue;

        // Wind affects cloth based on surface orientation
        Vec3 normal = m_state.normals[i];
        float dot = normal.x * wind_vel.x + normal.y * wind_vel.y + normal.z * wind_vel.z;
        float factor = std::abs(dot) * m_settings.wind.drag_coefficient;

        Vec3 force = wind_vel * (factor * dt * dt * m_impl->inv_masses[i]);
        m_impl->positions[i] = m_impl->positions[i] + force;
    }
}

void Cloth::check_sleep_state() {
    if (m_settings.sleep_threshold <= 0.0f) return;

    float total_energy = 0.0f;

    for (const auto& v : m_impl->velocities) {
        total_energy += v.x * v.x + v.y * v.y + v.z * v.z;
    }

    m_state.total_kinetic_energy = total_energy;

    if (total_energy < m_settings.sleep_threshold * static_cast<float>(m_impl->velocities.size())) {
        put_to_sleep();
    }
}

void Cloth::sync_attachments() {
    // This would sync entity-attached vertices with entity transforms
    // Implementation depends on how entity transforms are accessed
    // For now, just ensure fixed positions are maintained
    for (const auto& attachment : m_settings.attachments) {
        if (attachment.vertex_index >= m_impl->positions.size()) continue;

        if (!attachment.attach_to_entity) {
            m_impl->positions[attachment.vertex_index] = attachment.world_position;
        }
    }
}

void Cloth::solve_distance_constraints() {
    for (const auto& c : m_impl->distance_constraints) {
        Vec3 p1 = m_impl->positions[c.p1];
        Vec3 p2 = m_impl->positions[c.p2];

        Vec3 diff = p2 - p1;
        float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

        if (dist < 0.0001f) continue;

        float error = dist - c.rest_length;
        Vec3 dir = diff * (1.0f / dist);

        float w1 = m_impl->inv_masses[c.p1];
        float w2 = m_impl->inv_masses[c.p2];
        float total_w = w1 + w2;

        if (total_w < 0.0001f) continue;

        Vec3 correction = dir * (error * c.stiffness / total_w);

        m_impl->positions[c.p1] = m_impl->positions[c.p1] + correction * w1;
        m_impl->positions[c.p2] = m_impl->positions[c.p2] - correction * w2;
    }
}

void Cloth::solve_bend_constraints() {
    // Bend constraints are handled as distance constraints in our simplified model
    // A more accurate implementation would use dihedral angles
}

void Cloth::solve_attachment_constraints() {
    for (const auto& attachment : m_settings.attachments) {
        if (attachment.vertex_index >= m_impl->positions.size()) continue;

        if (attachment.type == AttachmentType::Fixed) {
            if (!attachment.attach_to_entity) {
                m_impl->positions[attachment.vertex_index] = attachment.world_position;
            }
        } else if (attachment.type == AttachmentType::Spring) {
            Vec3 target = attachment.world_position;
            Vec3 current = m_impl->positions[attachment.vertex_index];
            Vec3 diff = target - current;
            float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

            if (dist > attachment.max_distance) {
                Vec3 dir = diff * (1.0f / dist);
                float correction = dist - attachment.max_distance;
                m_impl->positions[attachment.vertex_index] =
                    m_impl->positions[attachment.vertex_index] + dir * correction;
            }
        }
    }
}

void Cloth::solve_collision_constraints() {
    if (!m_world) return;

    float margin = m_settings.collision.collision_margin;

    for (size_t i = 0; i < m_impl->positions.size(); ++i) {
        if (m_impl->inv_masses[i] <= 0.0f) continue;

        Vec3 pos = m_impl->positions[i];

        // Simple ground plane collision
        if (pos.y < margin) {
            m_impl->positions[i].y = margin;
        }

        // For full collision, would query PhysicsWorld for nearby shapes
        // and resolve penetration
    }
}

// Factory functions
ClothComponent make_cape(float width, float height) {
    ClothComponent cloth;
    cloth.type = ClothType::Visual;
    cloth.mesh.use_grid = true;
    cloth.mesh.grid.width = width;
    cloth.mesh.grid.height = height;
    cloth.mesh.grid.width_segments = static_cast<uint32_t>(width * 8);
    cloth.mesh.grid.height_segments = static_cast<uint32_t>(height * 8);

    cloth.mass = width * height * 0.3f;  // Lightweight fabric
    cloth.edge_stiffness = 0.9f;
    cloth.bend_stiffness = 0.05f;
    cloth.shear_stiffness = 0.4f;
    cloth.damping = 0.15f;

    // Attach top row
    uint32_t top_count = cloth.mesh.grid.width_segments + 1;
    for (uint32_t i = 0; i < top_count; ++i) {
        ClothAttachment attach;
        attach.vertex_index = i;
        attach.type = AttachmentType::Fixed;
        attach.attach_to_entity = true;  // Will be attached to character
        cloth.attachments.push_back(attach);
    }

    cloth.wind_mode = ClothWindMode::Global;
    cloth.wind.strength = 1.0f;

    return cloth;
}

ClothComponent make_flag(float width, float height) {
    ClothComponent cloth;
    cloth.type = ClothType::Visual;
    cloth.mesh.use_grid = true;
    cloth.mesh.grid.width = width;
    cloth.mesh.grid.height = height;
    cloth.mesh.grid.width_segments = static_cast<uint32_t>(width * 6);
    cloth.mesh.grid.height_segments = static_cast<uint32_t>(height * 6);

    cloth.mass = width * height * 0.2f;
    cloth.edge_stiffness = 0.95f;
    cloth.bend_stiffness = 0.02f;
    cloth.shear_stiffness = 0.3f;
    cloth.damping = 0.1f;

    // Attach left edge (flagpole)
    uint32_t rows = cloth.mesh.grid.height_segments + 1;
    uint32_t cols = cloth.mesh.grid.width_segments + 1;
    for (uint32_t y = 0; y < rows; ++y) {
        ClothAttachment attach;
        attach.vertex_index = y * cols;  // Left column
        attach.type = AttachmentType::Fixed;
        cloth.attachments.push_back(attach);
    }

    cloth.wind_mode = ClothWindMode::Turbulent;
    cloth.wind.strength = 2.0f;
    cloth.wind.turbulence = 0.5f;

    return cloth;
}

ClothComponent make_curtain(float width, float height) {
    ClothComponent cloth;
    cloth.type = ClothType::Interactive;
    cloth.mesh.use_grid = true;
    cloth.mesh.grid.width = width;
    cloth.mesh.grid.height = height;
    cloth.mesh.grid.width_segments = static_cast<uint32_t>(width * 4);
    cloth.mesh.grid.height_segments = static_cast<uint32_t>(height * 4);

    cloth.mass = width * height * 0.8f;  // Heavier fabric
    cloth.edge_stiffness = 0.85f;
    cloth.bend_stiffness = 0.15f;
    cloth.shear_stiffness = 0.5f;
    cloth.damping = 0.25f;

    // Attach top row at intervals (curtain rings)
    uint32_t cols = cloth.mesh.grid.width_segments + 1;
    for (uint32_t i = 0; i < cols; i += 3) {
        ClothAttachment attach;
        attach.vertex_index = i;
        attach.type = AttachmentType::Sliding;  // Can slide on rod
        cloth.attachments.push_back(attach);
    }

    cloth.collision.world_collision = true;
    cloth.collision.dynamic_collision = true;

    return cloth;
}

ClothComponent make_banner(float width, float height) {
    ClothComponent cloth;
    cloth.type = ClothType::Visual;
    cloth.mesh.use_grid = true;
    cloth.mesh.grid.width = width;
    cloth.mesh.grid.height = height;
    cloth.mesh.grid.width_segments = static_cast<uint32_t>(width * 5);
    cloth.mesh.grid.height_segments = static_cast<uint32_t>(height * 10);

    cloth.mass = width * height * 0.4f;
    cloth.edge_stiffness = 0.9f;
    cloth.bend_stiffness = 0.08f;
    cloth.shear_stiffness = 0.4f;
    cloth.damping = 0.12f;

    // Attach top corners and center
    uint32_t cols = cloth.mesh.grid.width_segments + 1;
    ClothAttachment top_left, top_right, top_center;
    top_left.vertex_index = 0;
    top_left.type = AttachmentType::Fixed;
    top_right.vertex_index = cols - 1;
    top_right.type = AttachmentType::Fixed;
    top_center.vertex_index = cols / 2;
    top_center.type = AttachmentType::Fixed;

    cloth.attachments.push_back(top_left);
    cloth.attachments.push_back(top_center);
    cloth.attachments.push_back(top_right);

    cloth.wind_mode = ClothWindMode::Global;

    return cloth;
}

ClothComponent make_tablecloth(float width, float height) {
    ClothComponent cloth;
    cloth.type = ClothType::Interactive;
    cloth.mesh.use_grid = true;
    cloth.mesh.grid.width = width;
    cloth.mesh.grid.height = height;
    cloth.mesh.grid.width_segments = static_cast<uint32_t>(width * 5);
    cloth.mesh.grid.height_segments = static_cast<uint32_t>(height * 5);

    cloth.mass = width * height * 0.5f;
    cloth.edge_stiffness = 0.8f;
    cloth.bend_stiffness = 0.2f;
    cloth.shear_stiffness = 0.6f;
    cloth.damping = 0.3f;

    // No attachments - lies freely on table
    cloth.collision.world_collision = true;
    cloth.collision.dynamic_collision = true;

    cloth.use_gravity = true;
    cloth.wind_mode = ClothWindMode::None;

    return cloth;
}

ClothComponent make_rope(float length, int segments) {
    ClothComponent cloth;
    cloth.type = ClothType::Interactive;
    cloth.mesh.use_grid = true;
    cloth.mesh.grid.width = 0.02f;  // Very thin
    cloth.mesh.grid.height = length;
    cloth.mesh.grid.width_segments = 1;
    cloth.mesh.grid.height_segments = static_cast<uint32_t>(segments);

    cloth.mass = length * 0.5f;
    cloth.edge_stiffness = 0.99f;    // Very stiff
    cloth.bend_stiffness = 0.3f;
    cloth.shear_stiffness = 0.5f;
    cloth.damping = 0.2f;

    // Attach top
    ClothAttachment top;
    top.vertex_index = 0;
    top.type = AttachmentType::Fixed;
    cloth.attachments.push_back(top);

    cloth.collision.world_collision = true;
    cloth.collision.dynamic_collision = true;

    return cloth;
}

} // namespace engine::physics
