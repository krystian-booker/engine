#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>

namespace engine::scene {

Entity World::create() {
    Entity e = m_registry.create();
    auto& info = m_registry.emplace<EntityInfo>(e);
    info.uuid = m_next_uuid++;
    info.name = "Entity_" + std::to_string(info.uuid);
    return e;
}

Entity World::create(const std::string& name) {
    Entity e = m_registry.create();
    auto& info = m_registry.emplace<EntityInfo>(e);
    info.uuid = m_next_uuid++;
    info.name = name;
    return e;
}

void World::destroy(Entity e) {
    if (!valid(e)) return;

    // If entity has hierarchy, handle parent/child relationships
    if (auto* hierarchy = m_registry.try_get<Hierarchy>(e)) {
        // Remove from parent
        if (hierarchy->parent != NullEntity) {
            remove_parent(*this, e);
        }

        // Destroy all children recursively
        Entity child = hierarchy->first_child;
        while (child != NullEntity) {
            Entity next = NullEntity;
            if (auto* child_h = m_registry.try_get<Hierarchy>(child)) {
                next = child_h->next_sibling;
            }
            destroy(child);
            child = next;
        }
    }

    m_registry.destroy(e);
}

bool World::valid(Entity e) const {
    return m_registry.valid(e);
}

void World::clear() {
    m_registry.clear();
    m_next_uuid = 1;
}

Entity World::find_by_name(const std::string& name) const {
    auto view = m_registry.view<EntityInfo>();
    for (auto [entity, info] : view.each()) {
        if (info.name == name) {
            return entity;
        }
    }
    return NullEntity;
}

} // namespace engine::scene
