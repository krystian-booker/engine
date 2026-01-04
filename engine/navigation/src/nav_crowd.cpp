#include <engine/navigation/nav_crowd.hpp>
#include <engine/core/log.hpp>

#include <DetourCrowd.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>
#include <DetourCommon.h>

namespace engine::navigation {

// Custom deleters
void NavCrowd::CrowdDeleter::operator()(dtCrowd* crowd) const {
    if (crowd) {
        dtFreeCrowd(crowd);
    }
}

void NavCrowd::QueryDeleter::operator()(dtNavMeshQuery* query) const {
    if (query) {
        dtFreeNavMeshQuery(query);
    }
}

NavCrowd::NavCrowd() = default;
NavCrowd::~NavCrowd() {
    shutdown();
}

bool NavCrowd::init(NavMesh* navmesh, int max_agents) {
    if (!navmesh || !navmesh->is_valid()) {
        core::log(core::LogLevel::Error, "NavCrowd: Invalid navmesh");
        return false;
    }

    m_navmesh = navmesh;
    m_max_agents = max_agents;

    // Create crowd
    dtCrowd* crowd = dtAllocCrowd();
    if (!crowd) {
        core::log(core::LogLevel::Error, "NavCrowd: Failed to allocate crowd");
        return false;
    }

    if (!crowd->init(max_agents, navmesh->get_detour_navmesh()->getParams()->orig[0] + 5.0f, navmesh->get_detour_navmesh())) {
        dtFreeCrowd(crowd);
        core::log(core::LogLevel::Error, "NavCrowd: Failed to initialize crowd");
        return false;
    }

    m_crowd.reset(crowd);

    // Set up obstacle avoidance params (4 quality levels)
    dtObstacleAvoidanceParams params;

    // Quality 0 - Low
    std::memcpy(&params, crowd->getObstacleAvoidanceParams(0), sizeof(dtObstacleAvoidanceParams));
    params.velBias = 0.5f;
    params.adaptiveDivs = 5;
    params.adaptiveRings = 2;
    params.adaptiveDepth = 1;
    crowd->setObstacleAvoidanceParams(0, &params);

    // Quality 1 - Medium Low
    std::memcpy(&params, crowd->getObstacleAvoidanceParams(0), sizeof(dtObstacleAvoidanceParams));
    params.velBias = 0.5f;
    params.adaptiveDivs = 5;
    params.adaptiveRings = 2;
    params.adaptiveDepth = 2;
    crowd->setObstacleAvoidanceParams(1, &params);

    // Quality 2 - Medium High
    std::memcpy(&params, crowd->getObstacleAvoidanceParams(0), sizeof(dtObstacleAvoidanceParams));
    params.velBias = 0.5f;
    params.adaptiveDivs = 7;
    params.adaptiveRings = 2;
    params.adaptiveDepth = 3;
    crowd->setObstacleAvoidanceParams(2, &params);

    // Quality 3 - High
    std::memcpy(&params, crowd->getObstacleAvoidanceParams(0), sizeof(dtObstacleAvoidanceParams));
    params.velBias = 0.5f;
    params.adaptiveDivs = 7;
    params.adaptiveRings = 3;
    params.adaptiveDepth = 3;
    crowd->setObstacleAvoidanceParams(3, &params);

    // Create query for finding nearest polys
    dtNavMeshQuery* query = dtAllocNavMeshQuery();
    if (!query || dtStatusFailed(query->init(navmesh->get_detour_navmesh(), 2048))) {
        if (query) dtFreeNavMeshQuery(query);
        core::log(core::LogLevel::Error, "NavCrowd: Failed to create nav query");
        return false;
    }
    m_query.reset(query);

    core::log(core::LogLevel::Info, "NavCrowd initialized with max {} agents", max_agents);
    return true;
}

void NavCrowd::shutdown() {
    m_query.reset();
    m_crowd.reset();
    m_navmesh = nullptr;
}

bool NavCrowd::find_nearest_poly(const Vec3& position, Vec3& out_nearest, uint64_t& out_poly) const {
    if (!m_query) return false;

    dtQueryFilter filter;
    filter.setIncludeFlags(0xFFFF);
    filter.setExcludeFlags(0);

    const float extents[3] = {2.0f, 4.0f, 2.0f};
    float nearest[3];
    dtPolyRef poly;

    dtStatus status = m_query->findNearestPoly(&position[0], extents, &filter, &poly, nearest);
    if (dtStatusFailed(status) || poly == 0) {
        return false;
    }

    out_nearest = Vec3(nearest[0], nearest[1], nearest[2]);
    out_poly = poly;
    return true;
}

CrowdAgentHandle NavCrowd::add_agent(const Vec3& position, const CrowdAgentParams& params) {
    CrowdAgentHandle handle;

    if (!m_crowd) {
        return handle;
    }

    // Find position on navmesh
    Vec3 nearest;
    uint64_t poly;
    if (!find_nearest_poly(position, nearest, poly)) {
        core::log(core::LogLevel::Warn, "NavCrowd: Could not find navmesh position for agent");
        return handle;
    }

    // Set up agent params
    dtCrowdAgentParams ap;
    std::memset(&ap, 0, sizeof(ap));
    ap.radius = params.radius;
    ap.height = params.height;
    ap.maxAcceleration = params.max_acceleration;
    ap.maxSpeed = params.max_speed;
    ap.collisionQueryRange = ap.radius * 12.0f;
    ap.pathOptimizationRange = ap.radius * 30.0f;
    ap.separationWeight = params.separation_weight;
    ap.obstacleAvoidanceType = static_cast<unsigned char>(params.obstacle_avoidance_type);
    ap.updateFlags = params.update_flags;
    ap.queryFilterType = 0;

    int idx = m_crowd->addAgent(&nearest[0], &ap);
    if (idx < 0) {
        core::log(core::LogLevel::Warn, "NavCrowd: Failed to add agent (crowd may be full)");
        return handle;
    }

    handle.index = idx;
    return handle;
}

void NavCrowd::remove_agent(CrowdAgentHandle handle) {
    if (!m_crowd || !handle.valid()) return;
    m_crowd->removeAgent(handle.index);
}

void NavCrowd::update_agent_params(CrowdAgentHandle handle, const CrowdAgentParams& params) {
    if (!m_crowd || !handle.valid()) return;

    const dtCrowdAgent* ag = m_crowd->getAgent(handle.index);
    if (!ag || !ag->active) return;

    dtCrowdAgentParams ap;
    std::memcpy(&ap, &ag->params, sizeof(ap));
    ap.radius = params.radius;
    ap.height = params.height;
    ap.maxAcceleration = params.max_acceleration;
    ap.maxSpeed = params.max_speed;
    ap.separationWeight = params.separation_weight;
    ap.obstacleAvoidanceType = static_cast<unsigned char>(params.obstacle_avoidance_type);
    ap.updateFlags = params.update_flags;

    m_crowd->updateAgentParameters(handle.index, &ap);
}

void NavCrowd::set_target(CrowdAgentHandle handle, const Vec3& target) {
    if (!m_crowd || !handle.valid()) return;

    // Find target position on navmesh
    Vec3 nearest;
    uint64_t poly;
    if (!find_nearest_poly(target, nearest, poly)) {
        core::log(core::LogLevel::Warn, "NavCrowd: Could not find navmesh position for target");
        return;
    }

    m_crowd->requestMoveTarget(handle.index, static_cast<dtPolyRef>(poly), &nearest[0]);
}

void NavCrowd::set_velocity(CrowdAgentHandle handle, const Vec3& velocity) {
    if (!m_crowd || !handle.valid()) return;
    m_crowd->requestMoveVelocity(handle.index, &velocity[0]);
}

void NavCrowd::stop(CrowdAgentHandle handle) {
    if (!m_crowd || !handle.valid()) return;
    m_crowd->resetMoveTarget(handle.index);
}

void NavCrowd::warp(CrowdAgentHandle handle, const Vec3& position) {
    if (!m_crowd || !handle.valid()) return;

    // Find position on navmesh
    Vec3 nearest;
    uint64_t poly;
    if (!find_nearest_poly(position, nearest, poly)) {
        core::log(core::LogLevel::Warn, "NavCrowd: Could not find navmesh position for warp");
        return;
    }

    // Remove and re-add agent at new position
    const dtCrowdAgent* ag = m_crowd->getAgent(handle.index);
    if (!ag || !ag->active) return;

    dtCrowdAgentParams params;
    std::memcpy(&params, &ag->params, sizeof(params));

    m_crowd->removeAgent(handle.index);
    int new_idx = m_crowd->addAgent(&nearest[0], &params);

    // Note: handle.index is now invalid, caller should get new handle
    if (new_idx < 0) {
        core::log(core::LogLevel::Warn, "NavCrowd: Failed to re-add agent after warp");
    }
}

void NavCrowd::update(float dt) {
    if (!m_crowd) return;
    m_crowd->update(dt, nullptr);
}

CrowdAgentState NavCrowd::get_agent_state(CrowdAgentHandle handle) const {
    CrowdAgentState state;

    if (!m_crowd || !handle.valid()) return state;

    const dtCrowdAgent* ag = m_crowd->getAgent(handle.index);
    if (!ag || !ag->active) return state;

    state.position = Vec3(ag->npos[0], ag->npos[1], ag->npos[2]);
    state.velocity = Vec3(ag->vel[0], ag->vel[1], ag->vel[2]);
    state.desired_velocity = Vec3(ag->dvel[0], ag->dvel[1], ag->dvel[2]);

    if (ag->targetState == DT_CROWDAGENT_TARGET_VALID ||
        ag->targetState == DT_CROWDAGENT_TARGET_VELOCITY) {
        state.has_target = true;
        state.target = Vec3(ag->targetPos[0], ag->targetPos[1], ag->targetPos[2]);
    }

    state.partial_path = ag->partial;

    // Check if at target
    if (state.has_target) {
        float dist = glm::length(state.position - state.target);
        state.at_target = (dist < ag->params.radius * 2.0f);
    }

    return state;
}

Vec3 NavCrowd::get_agent_position(CrowdAgentHandle handle) const {
    if (!m_crowd || !handle.valid()) return Vec3(0.0f);

    const dtCrowdAgent* ag = m_crowd->getAgent(handle.index);
    if (!ag || !ag->active) return Vec3(0.0f);

    return Vec3(ag->npos[0], ag->npos[1], ag->npos[2]);
}

Vec3 NavCrowd::get_agent_velocity(CrowdAgentHandle handle) const {
    if (!m_crowd || !handle.valid()) return Vec3(0.0f);

    const dtCrowdAgent* ag = m_crowd->getAgent(handle.index);
    if (!ag || !ag->active) return Vec3(0.0f);

    return Vec3(ag->vel[0], ag->vel[1], ag->vel[2]);
}

bool NavCrowd::has_reached_target(CrowdAgentHandle handle, float threshold) const {
    if (!m_crowd || !handle.valid()) return false;

    const dtCrowdAgent* ag = m_crowd->getAgent(handle.index);
    if (!ag || !ag->active) return false;

    if (ag->targetState != DT_CROWDAGENT_TARGET_VALID) return false;

    Vec3 pos(ag->npos[0], ag->npos[1], ag->npos[2]);
    Vec3 target(ag->targetPos[0], ag->targetPos[1], ag->targetPos[2]);
    float dist = glm::length(pos - target);

    return dist < threshold;
}

int NavCrowd::get_active_agent_count() const {
    if (!m_crowd) return 0;
    return m_crowd->getAgentCount();
}

} // namespace engine::navigation
