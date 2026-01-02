#include <engine/navigation/navmesh_builder.hpp>
#include <engine/core/log.hpp>

#include <Recast.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>

#include <thread>
#include <chrono>

namespace engine::navigation {

// Helper to report Recast errors
static void recast_log(void* /*ctx*/, rcLogCategory category, const char* msg, int len) {
    std::string message(msg, len);
    switch (category) {
        case RC_LOG_ERROR:
            core::log(core::LogLevel::Error, "Recast: {}", message);
            break;
        case RC_LOG_WARNING:
            core::log(core::LogLevel::Warn, "Recast: {}", message);
            break;
        case RC_LOG_PROGRESS:
            core::log(core::LogLevel::Debug, "Recast: {}", message);
            break;
    }
}

void NavMeshInputGeometry::add_mesh(
    const Vec3* verts, size_t vert_count,
    const uint32_t* inds, size_t index_count,
    const Mat4& transform,
    uint8_t area_type)
{
    size_t base_vertex = vertices.size();
    size_t base_tri = indices.size() / 3;

    // Transform and add vertices
    for (size_t i = 0; i < vert_count; ++i) {
        Vec4 v = transform * Vec4(verts[i], 1.0f);
        vertices.push_back(Vec3(v));
    }

    // Add indices (offset by base vertex)
    for (size_t i = 0; i < index_count; ++i) {
        indices.push_back(static_cast<uint32_t>(base_vertex) + inds[i]);
    }

    // Add area types for new triangles
    size_t new_tris = index_count / 3;
    area_types.resize(base_tri + new_tris, area_type);
}

void NavMeshInputGeometry::clear() {
    vertices.clear();
    indices.clear();
    area_types.clear();
    bounds = AABB();
}

void NavMeshInputGeometry::compute_bounds() {
    if (vertices.empty()) {
        bounds = AABB();
        return;
    }

    bounds.min = vertices[0];
    bounds.max = vertices[0];

    for (const auto& v : vertices) {
        bounds.min = glm::min(bounds.min, v);
        bounds.max = glm::max(bounds.max, v);
    }
}

NavMeshBuilder::NavMeshBuilder() = default;
NavMeshBuilder::~NavMeshBuilder() = default;

NavMeshBuildResult NavMeshBuilder::build(
    const NavMeshInputGeometry& geometry,
    const NavMeshSettings& settings,
    BuildProgressCallback progress)
{
    m_building = true;
    m_cancel_requested = false;
    auto result = build_internal(geometry, settings, progress);
    m_building = false;
    return result;
}

NavMeshBuildResult NavMeshBuilder::build_from_world(
    scene::World& /*world*/,
    const NavMeshSettings& settings,
    uint32_t /*layer_mask*/,
    BuildProgressCallback progress)
{
    // TODO: Extract geometry from World's MeshRenderer components
    // For now, return a placeholder
    NavMeshInputGeometry geometry;

    // This would iterate through entities with MeshRenderer and Transform components
    // and extract their geometry into NavMeshInputGeometry

    core::log(core::LogLevel::Warn, "NavMeshBuilder::build_from_world not fully implemented - use build() with explicit geometry");

    return build(geometry, settings, progress);
}

std::future<NavMeshBuildResult> NavMeshBuilder::build_async(
    NavMeshInputGeometry geometry,
    NavMeshSettings settings,
    BuildProgressCallback progress)
{
    return std::async(std::launch::async, [this, geom = std::move(geometry), settings, progress]() {
        return build(geom, settings, progress);
    });
}

void NavMeshBuilder::cancel_build() {
    m_cancel_requested = true;
}

NavMeshBuildResult NavMeshBuilder::build_internal(
    const NavMeshInputGeometry& geometry,
    const NavMeshSettings& settings,
    BuildProgressCallback progress)
{
    NavMeshBuildResult result;
    result.input_vertices = static_cast<int>(geometry.vertices.size());
    result.input_triangles = static_cast<int>(geometry.indices.size() / 3);

    auto start_time = std::chrono::steady_clock::now();

    if (geometry.vertices.empty() || geometry.indices.empty()) {
        result.error_message = "Empty geometry";
        return result;
    }

    if (progress) progress(0.0f, "Initializing");

    // Compute bounds if not provided
    NavMeshInputGeometry geom = geometry;
    if (geom.bounds.min == Vec3(0.0f) && geom.bounds.max == Vec3(0.0f)) {
        geom.compute_bounds();
    }

    // Create Recast context
    rcContext ctx;
    ctx.enableLog(true);
    ctx.enableTimer(true);

    // Create heightfield
    rcHeightfield* heightfield = rcAllocHeightfield();
    if (!heightfield) {
        result.error_message = "Failed to allocate heightfield";
        return result;
    }

    // Calculate grid size
    int grid_width, grid_height;
    rcCalcGridSize(
        &geom.bounds.min[0], &geom.bounds.max[0],
        settings.cell_size,
        &grid_width, &grid_height);

    if (!rcCreateHeightfield(&ctx, *heightfield,
                              grid_width, grid_height,
                              &geom.bounds.min[0], &geom.bounds.max[0],
                              settings.cell_size, settings.cell_height)) {
        rcFreeHeightField(heightfield);
        result.error_message = "Failed to create heightfield";
        return result;
    }

    if (m_cancel_requested) {
        rcFreeHeightField(heightfield);
        result.error_message = "Build cancelled";
        return result;
    }

    if (progress) progress(0.1f, "Rasterizing triangles");

    // Prepare triangle data
    const int num_tris = static_cast<int>(geom.indices.size() / 3);
    std::vector<unsigned char> tri_areas(num_tris, 0);

    // Set area types
    if (!geom.area_types.empty()) {
        for (int i = 0; i < num_tris && i < static_cast<int>(geom.area_types.size()); ++i) {
            tri_areas[i] = geom.area_types[i];
        }
    }

    // Mark walkable triangles
    rcMarkWalkableTriangles(&ctx,
                            settings.agent_max_slope,
                            reinterpret_cast<const float*>(geom.vertices.data()),
                            static_cast<int>(geom.vertices.size()),
                            reinterpret_cast<const int*>(geom.indices.data()),
                            num_tris,
                            tri_areas.data());

    // Rasterize triangles
    if (!rcRasterizeTriangles(&ctx,
                               reinterpret_cast<const float*>(geom.vertices.data()),
                               static_cast<int>(geom.vertices.size()),
                               reinterpret_cast<const int*>(geom.indices.data()),
                               tri_areas.data(),
                               num_tris,
                               *heightfield,
                               static_cast<int>(std::ceil(settings.agent_max_climb / settings.cell_height)))) {
        rcFreeHeightField(heightfield);
        result.error_message = "Failed to rasterize triangles";
        return result;
    }

    if (m_cancel_requested) {
        rcFreeHeightField(heightfield);
        result.error_message = "Build cancelled";
        return result;
    }

    if (progress) progress(0.3f, "Filtering walkable surfaces");

    // Filter walkable surfaces
    rcFilterLowHangingWalkableObstacles(&ctx,
                                         static_cast<int>(std::ceil(settings.agent_max_climb / settings.cell_height)),
                                         *heightfield);
    rcFilterLedgeSpans(&ctx,
                       static_cast<int>(std::ceil(settings.agent_height / settings.cell_height)),
                       static_cast<int>(std::ceil(settings.agent_max_climb / settings.cell_height)),
                       *heightfield);
    rcFilterWalkableLowHeightSpans(&ctx,
                                   static_cast<int>(std::ceil(settings.agent_height / settings.cell_height)),
                                   *heightfield);

    if (progress) progress(0.4f, "Building compact heightfield");

    // Create compact heightfield
    rcCompactHeightfield* compact = rcAllocCompactHeightfield();
    if (!compact) {
        rcFreeHeightField(heightfield);
        result.error_message = "Failed to allocate compact heightfield";
        return result;
    }

    if (!rcBuildCompactHeightfield(&ctx,
                                    static_cast<int>(std::ceil(settings.agent_height / settings.cell_height)),
                                    static_cast<int>(std::ceil(settings.agent_max_climb / settings.cell_height)),
                                    *heightfield,
                                    *compact)) {
        rcFreeHeightField(heightfield);
        rcFreeCompactHeightfield(compact);
        result.error_message = "Failed to build compact heightfield";
        return result;
    }

    rcFreeHeightField(heightfield);
    heightfield = nullptr;

    if (m_cancel_requested) {
        rcFreeCompactHeightfield(compact);
        result.error_message = "Build cancelled";
        return result;
    }

    if (progress) progress(0.5f, "Eroding walkable area");

    // Erode walkable area by agent radius
    if (!rcErodeWalkableArea(&ctx,
                              static_cast<int>(std::ceil(settings.agent_radius / settings.cell_size)),
                              *compact)) {
        rcFreeCompactHeightfield(compact);
        result.error_message = "Failed to erode walkable area";
        return result;
    }

    if (progress) progress(0.6f, "Building distance field");

    // Build distance field
    if (!rcBuildDistanceField(&ctx, *compact)) {
        rcFreeCompactHeightfield(compact);
        result.error_message = "Failed to build distance field";
        return result;
    }

    if (progress) progress(0.65f, "Building regions");

    // Build regions
    if (!rcBuildRegions(&ctx, *compact, 0,
                         settings.min_region_area,
                         settings.merge_region_area)) {
        rcFreeCompactHeightfield(compact);
        result.error_message = "Failed to build regions";
        return result;
    }

    if (m_cancel_requested) {
        rcFreeCompactHeightfield(compact);
        result.error_message = "Build cancelled";
        return result;
    }

    if (progress) progress(0.7f, "Building contours");

    // Create contour set
    rcContourSet* contours = rcAllocContourSet();
    if (!contours) {
        rcFreeCompactHeightfield(compact);
        result.error_message = "Failed to allocate contour set";
        return result;
    }

    if (!rcBuildContours(&ctx, *compact,
                          settings.max_edge_error,
                          static_cast<int>(settings.max_edge_length / settings.cell_size),
                          *contours)) {
        rcFreeCompactHeightfield(compact);
        rcFreeContourSet(contours);
        result.error_message = "Failed to build contours";
        return result;
    }

    if (progress) progress(0.8f, "Building polygon mesh");

    // Create polygon mesh
    rcPolyMesh* poly_mesh = rcAllocPolyMesh();
    if (!poly_mesh) {
        rcFreeCompactHeightfield(compact);
        rcFreeContourSet(contours);
        result.error_message = "Failed to allocate poly mesh";
        return result;
    }

    if (!rcBuildPolyMesh(&ctx, *contours,
                          settings.max_verts_per_poly,
                          *poly_mesh)) {
        rcFreeCompactHeightfield(compact);
        rcFreeContourSet(contours);
        rcFreePolyMesh(poly_mesh);
        result.error_message = "Failed to build poly mesh";
        return result;
    }

    if (progress) progress(0.85f, "Building detail mesh");

    // Create detail mesh
    rcPolyMeshDetail* detail_mesh = rcAllocPolyMeshDetail();
    if (!detail_mesh) {
        rcFreeCompactHeightfield(compact);
        rcFreeContourSet(contours);
        rcFreePolyMesh(poly_mesh);
        result.error_message = "Failed to allocate detail mesh";
        return result;
    }

    if (!rcBuildPolyMeshDetail(&ctx, *poly_mesh, *compact,
                                settings.detail_sample_distance,
                                settings.detail_sample_max_error,
                                *detail_mesh)) {
        rcFreeCompactHeightfield(compact);
        rcFreeContourSet(contours);
        rcFreePolyMesh(poly_mesh);
        rcFreePolyMeshDetail(detail_mesh);
        result.error_message = "Failed to build detail mesh";
        return result;
    }

    // Free intermediate data
    rcFreeCompactHeightfield(compact);
    rcFreeContourSet(contours);

    if (m_cancel_requested) {
        rcFreePolyMesh(poly_mesh);
        rcFreePolyMeshDetail(detail_mesh);
        result.error_message = "Build cancelled";
        return result;
    }

    if (progress) progress(0.9f, "Creating Detour navmesh");

    // Update poly flags (simplified - just mark all as walkable)
    for (int i = 0; i < poly_mesh->npolys; ++i) {
        poly_mesh->flags[i] = 1;  // Walkable
    }

    // Create Detour navmesh data
    dtNavMeshCreateParams params;
    std::memset(&params, 0, sizeof(params));

    params.verts = poly_mesh->verts;
    params.vertCount = poly_mesh->nverts;
    params.polys = poly_mesh->polys;
    params.polyAreas = poly_mesh->areas;
    params.polyFlags = poly_mesh->flags;
    params.polyCount = poly_mesh->npolys;
    params.nvp = poly_mesh->nvp;

    params.detailMeshes = detail_mesh->meshes;
    params.detailVerts = detail_mesh->verts;
    params.detailVertsCount = detail_mesh->nverts;
    params.detailTris = detail_mesh->tris;
    params.detailTriCount = detail_mesh->ntris;

    params.walkableHeight = settings.agent_height;
    params.walkableRadius = settings.agent_radius;
    params.walkableClimb = settings.agent_max_climb;
    params.cs = settings.cell_size;
    params.ch = settings.cell_height;
    params.buildBvTree = true;

    rcVcopy(params.bmin, poly_mesh->bmin);
    rcVcopy(params.bmax, poly_mesh->bmax);

    unsigned char* nav_data = nullptr;
    int nav_data_size = 0;

    if (!dtCreateNavMeshData(&params, &nav_data, &nav_data_size)) {
        rcFreePolyMesh(poly_mesh);
        rcFreePolyMeshDetail(detail_mesh);
        result.error_message = "Failed to create Detour navmesh data";
        return result;
    }

    rcFreePolyMesh(poly_mesh);
    rcFreePolyMeshDetail(detail_mesh);

    // Create Detour navmesh
    dtNavMesh* navmesh = dtAllocNavMesh();
    if (!navmesh) {
        dtFree(nav_data);
        result.error_message = "Failed to allocate Detour navmesh";
        return result;
    }

    dtStatus status = navmesh->init(nav_data, nav_data_size, DT_TILE_FREE_DATA);
    if (dtStatusFailed(status)) {
        dtFree(nav_data);
        dtFreeNavMesh(navmesh);
        result.error_message = "Failed to initialize Detour navmesh";
        return result;
    }

    if (progress) progress(1.0f, "Complete");

    // Create result
    result.navmesh = std::make_unique<NavMesh>();
    result.navmesh->m_navmesh.reset(navmesh);
    result.navmesh->m_settings = settings;
    result.success = true;
    result.output_polygons = result.navmesh->get_polygon_count();
    result.output_tiles = result.navmesh->get_tile_count();
    result.build_time_ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start_time).count();

    core::log(core::LogLevel::Info, "NavMesh built: {} polygons, {} tiles, {:.1f}ms",
              result.output_polygons, result.output_tiles, result.build_time_ms);

    return result;
}

} // namespace engine::navigation
