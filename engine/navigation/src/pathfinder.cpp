#include <engine/navigation/pathfinder.hpp>
#include <engine/core/log.hpp>

#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>
#include <DetourCommon.h>

#include <random>

namespace engine::navigation {

namespace {
float random_unit()
{
    thread_local std::random_device rd;
    thread_local std::mt19937 gen(rd());
    thread_local std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(gen);
}
} // namespace

// Deleter implementations
void Pathfinder::QueryDeleter::operator()(dtNavMeshQuery* query) const {
    if (query) {
        dtFreeNavMeshQuery(query);
    }
}

void Pathfinder::FilterDeleter::operator()(dtQueryFilter* filter) const {
    delete filter;
}

float PathResult::total_distance() const {
    if (path.size() < 2) return 0.0f;

    float dist = 0.0f;
    for (size_t i = 1; i < path.size(); ++i) {
        dist += glm::length(path[i] - path[i - 1]);
    }
    return dist;
}

Pathfinder::Pathfinder()
    : m_poly_path(MAX_PATH_POLYS)
    , m_straight_path(MAX_PATH_POLYS) {
}

Pathfinder::~Pathfinder() {
    shutdown();
}

bool Pathfinder::init(NavMesh* navmesh, int max_nodes) {
    if (!navmesh || !navmesh->is_valid()) {
        core::log(core::LogLevel::Error, "Pathfinder: Invalid navmesh");
        return false;
    }

    m_navmesh = navmesh;

    // Create query object
    dtNavMeshQuery* query = dtAllocNavMeshQuery();
    if (!query) {
        core::log(core::LogLevel::Error, "Pathfinder: Failed to allocate query object");
        return false;
    }

    dtStatus status = query->init(navmesh->get_detour_navmesh(), max_nodes);
    if (dtStatusFailed(status)) {
        dtFreeNavMeshQuery(query);
        core::log(core::LogLevel::Error, "Pathfinder: Failed to initialize query");
        return false;
    }

    m_query.reset(query);

    // Create filter
    m_filter.reset(new dtQueryFilter());
    update_filter();

    core::log(core::LogLevel::Info, "Pathfinder initialized with {} nodes", max_nodes);
    return true;
}

void Pathfinder::shutdown() {
    m_query.reset();
    m_filter.reset();
    m_navmesh = nullptr;
}

void Pathfinder::set_area_costs(const NavAreaCosts& costs) {
    m_area_costs = costs;
    update_filter();
}

void Pathfinder::set_area_enabled(NavAreaType area, bool enabled) {
    uint16_t flag = 1 << static_cast<int>(area);
    if (enabled) {
        m_include_flags |= flag;
        m_exclude_flags &= ~flag;
    } else {
        m_include_flags &= ~flag;
        m_exclude_flags |= flag;
    }
    update_filter();
}

bool Pathfinder::is_area_enabled(NavAreaType area) const {
    uint16_t flag = 1 << static_cast<int>(area);
    return (m_include_flags & flag) != 0;
}

void Pathfinder::update_filter() {
    if (!m_filter) return;

    m_filter->setIncludeFlags(m_include_flags);
    m_filter->setExcludeFlags(m_exclude_flags);

    for (int i = 0; i < 64; ++i) {
        m_filter->setAreaCost(i, m_area_costs.costs[i]);
    }
}

PathResult Pathfinder::find_path(const Vec3& start, const Vec3& end) {
    return find_path(start, end, Vec3{2.0f, 4.0f, 2.0f});
}

PathResult Pathfinder::find_path(const Vec3& start, const Vec3& end, const Vec3& search_extents) {
    PathResult result;

    if (!m_query || !m_filter) {
        return result;
    }

    // Find start polygon
    dtPolyRef start_poly;
    float start_nearest[3];
    dtStatus status = m_query->findNearestPoly(
        &start[0], &search_extents[0], m_filter.get(),
        &start_poly, start_nearest);

    if (dtStatusFailed(status) || start_poly == 0) {
        return result;
    }

    // Find end polygon
    dtPolyRef end_poly;
    float end_nearest[3];
    status = m_query->findNearestPoly(
        &end[0], &search_extents[0], m_filter.get(),
        &end_poly, end_nearest);

    if (dtStatusFailed(status) || end_poly == 0) {
        return result;
    }

    // Find polygon path
    int poly_count = 0;
    status = m_query->findPath(
        start_poly, end_poly,
        start_nearest, end_nearest,
        m_filter.get(),
        m_poly_path.data(), &poly_count, MAX_PATH_POLYS);

    if (dtStatusFailed(status) || poly_count == 0) {
        return result;
    }

    // Store polygon path for debugging
    result.polys.resize(poly_count);
    for (int i = 0; i < poly_count; ++i) {
        result.polys[i] = m_poly_path[i];
    }

    // Check if partial path
    result.partial = (m_poly_path[poly_count - 1] != end_poly);

    // Get actual end point (might differ if partial)
    float actual_end[3];
    if (result.partial) {
        m_query->closestPointOnPoly(m_poly_path[poly_count - 1], end_nearest, actual_end, nullptr);
    } else {
        dtVcopy(actual_end, end_nearest);
    }

    // Find straight path (string pulling)
    float straight_path_pts[MAX_PATH_POLYS * 3];
    unsigned char straight_path_flags[MAX_PATH_POLYS];
    dtPolyRef straight_path_polys[MAX_PATH_POLYS];
    int straight_count = 0;

    status = m_query->findStraightPath(
        start_nearest, actual_end,
        m_poly_path.data(), poly_count,
        straight_path_pts, straight_path_flags, straight_path_polys,
        &straight_count, MAX_PATH_POLYS,
        DT_STRAIGHTPATH_ALL_CROSSINGS);

    if (dtStatusFailed(status) || straight_count == 0) {
        // Fall back to polygon centers
        result.path.push_back(Vec3(start_nearest[0], start_nearest[1], start_nearest[2]));
        for (int i = 0; i < poly_count; ++i) {
            Vec3 center = get_polygon_center(m_poly_path[i]);
            if (center != Vec3(0.0f)) {
                result.path.push_back(center);
            }
        }
        result.path.push_back(Vec3(actual_end[0], actual_end[1], actual_end[2]));
    } else {
        // Use straight path
        result.path.reserve(straight_count);
        for (int i = 0; i < straight_count; ++i) {
            result.path.push_back(Vec3(
                straight_path_pts[i * 3],
                straight_path_pts[i * 3 + 1],
                straight_path_pts[i * 3 + 2]));
        }
    }

    result.success = true;
    return result;
}

PathResult Pathfinder::find_straight_path(const Vec3& start, const Vec3& end) {
    PathResult result;

    if (!m_query || !m_filter) {
        return result;
    }

    // Just find the two endpoints on navmesh and connect directly
    NavPointResult start_pt = find_nearest_point(start);
    NavPointResult end_pt = find_nearest_point(end);

    if (!start_pt.valid || !end_pt.valid) {
        return result;
    }

    // Check if direct path is clear
    if (is_path_clear(start_pt.point, end_pt.point)) {
        result.path.push_back(start_pt.point);
        result.path.push_back(end_pt.point);
        result.success = true;
    } else {
        // Fall back to normal pathfinding
        return find_path(start, end);
    }

    return result;
}

NavPointResult Pathfinder::find_nearest_point(const Vec3& point, float search_radius) {
    NavPointResult result;

    if (!m_query || !m_filter) {
        return result;
    }

    Vec3 extents(search_radius, search_radius * 2, search_radius);
    float nearest[3];

    dtStatus status = m_query->findNearestPoly(
        &point[0], &extents[0], m_filter.get(),
        &result.poly, nearest);

    if (dtStatusFailed(status) || result.poly == 0) {
        return result;
    }

    result.point = Vec3(nearest[0], nearest[1], nearest[2]);
    result.valid = true;
    return result;
}

NavPointResult Pathfinder::find_random_point() {
    NavPointResult result;

    if (!m_query || !m_filter) {
        return result;
    }

    float random_pt[3];
    dtStatus status = m_query->findRandomPoint(
        m_filter.get(), random_unit,
        &result.poly, random_pt);

    if (dtStatusFailed(status)) {
        return result;
    }

    result.point = Vec3(random_pt[0], random_pt[1], random_pt[2]);
    result.valid = true;
    return result;
}

NavPointResult Pathfinder::find_random_point_around(const Vec3& center, float radius) {
    NavPointResult result;

    if (!m_query || !m_filter) {
        return result;
    }

    // First find the polygon at center
    NavPointResult center_pt = find_nearest_point(center);
    if (!center_pt.valid) {
        return result;
    }

    float random_pt[3];
    dtStatus status = m_query->findRandomPointAroundCircle(
        center_pt.poly, &center_pt.point[0], radius,
        m_filter.get(), random_unit,
        &result.poly, random_pt);

    if (dtStatusFailed(status)) {
        return result;
    }

    result.point = Vec3(random_pt[0], random_pt[1], random_pt[2]);
    result.valid = true;
    return result;
}

bool Pathfinder::is_point_on_navmesh(const Vec3& point, float tolerance) {
    NavPointResult pt = find_nearest_point(point, tolerance);
    if (!pt.valid) return false;

    float dist = glm::length(point - pt.point);
    return dist <= tolerance;
}

NavPointResult Pathfinder::project_point(const Vec3& point, const Vec3& search_extents) {
    NavPointResult result;

    if (!m_query || !m_filter) {
        return result;
    }

    dtPolyRef poly;
    float nearest[3];

    dtStatus status = m_query->findNearestPoly(
        &point[0], &search_extents[0], m_filter.get(),
        &poly, nearest);

    if (dtStatusFailed(status) || poly == 0) {
        return result;
    }

    // Get height at point
    float height;
    status = m_query->getPolyHeight(poly, nearest, &height);

    if (dtStatusSucceed(status)) {
        result.point = Vec3(nearest[0], height, nearest[2]);
    } else {
        result.point = Vec3(nearest[0], nearest[1], nearest[2]);
    }

    result.poly = poly;
    result.valid = true;
    return result;
}

NavRaycastResult Pathfinder::raycast(const Vec3& start, const Vec3& end) {
    NavRaycastResult result;

    if (!m_query || !m_filter) {
        return result;
    }

    // Find start polygon
    NavPointResult start_pt = find_nearest_point(start);
    if (!start_pt.valid) {
        return result;
    }

    float t;
    float hit_normal[3];
    int path_count;

    dtStatus status = m_query->raycast(
        start_pt.poly, &start_pt.point[0], &end[0],
        m_filter.get(), &t, hit_normal,
        m_poly_path.data(), &path_count, MAX_PATH_POLYS);

    if (dtStatusFailed(status)) {
        return result;
    }

    if (t < 1.0f) {
        // Hit something
        result.hit = true;
        result.hit_distance = t * glm::length(end - start_pt.point);
        result.hit_point = start_pt.point + t * (end - start_pt.point);
        result.hit_normal = Vec3(hit_normal[0], hit_normal[1], hit_normal[2]);
        if (path_count > 0) {
            result.hit_poly = m_poly_path[path_count - 1];
        }
    }

    return result;
}

bool Pathfinder::is_path_clear(const Vec3& start, const Vec3& end) {
    if (!m_query || !m_filter) {
        return false;
    }

    NavRaycastResult hit = raycast(start, end);
    return !hit.hit;
}

float Pathfinder::get_path_distance(const Vec3& start, const Vec3& end) {
    PathResult path = find_path(start, end);
    if (!path.success) {
        return -1.0f;  // Unreachable
    }
    return path.total_distance();
}

bool Pathfinder::is_reachable(const Vec3& from, const Vec3& to) {
    PathResult path = find_path(from, to);
    return path.success && !path.partial;
}

std::vector<NavPolyRef> Pathfinder::find_polygons_in_radius(const Vec3& center, float radius) {
    std::vector<NavPolyRef> polys;

    if (!m_query || !m_filter) {
        return polys;
    }

    NavPointResult center_pt = find_nearest_point(center);
    if (!center_pt.valid) {
        return polys;
    }

    dtPolyRef result_polys[256];
    dtPolyRef result_parents[256];
    float result_costs[256];
    int result_count = 0;

    dtStatus status = m_query->findPolysAroundCircle(
        center_pt.poly, &center_pt.point[0], radius,
        m_filter.get(),
        result_polys, result_parents, result_costs,
        &result_count, 256);

    if (dtStatusSucceed(status)) {
        polys.reserve(result_count);
        for (int i = 0; i < result_count; ++i) {
            polys.push_back(result_polys[i]);
        }
    }

    return polys;
}

NavPolyRef Pathfinder::get_polygon_at(const Vec3& point, const Vec3& search_extents) {
    NavPointResult pt = project_point(point, search_extents);
    return pt.poly;
}

Vec3 Pathfinder::get_polygon_center(NavPolyRef poly) {
    if (!m_navmesh || poly == INVALID_NAV_POLY_REF) {
        return Vec3(0.0f);
    }

    const dtNavMesh* mesh = m_navmesh->get_detour_navmesh();
    const dtMeshTile* tile = nullptr;
    const dtPoly* p = nullptr;

    if (dtStatusFailed(mesh->getTileAndPolyByRef(poly, &tile, &p))) {
        return Vec3(0.0f);
    }

    Vec3 center(0.0f);
    for (unsigned int i = 0; i < p->vertCount; ++i) {
        const float* v = &tile->verts[p->verts[i] * 3];
        center += Vec3(v[0], v[1], v[2]);
    }
    center /= static_cast<float>(p->vertCount);

    return center;
}

} // namespace engine::navigation
