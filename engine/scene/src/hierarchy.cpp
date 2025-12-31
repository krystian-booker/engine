#include <engine/scene/transform.hpp>
#include <engine/scene/world.hpp>

namespace engine::scene {

void set_parent(World& world, Entity child, Entity parent) {
    auto& registry = world.registry();

    // Ensure child has Hierarchy component
    if (!registry.all_of<Hierarchy>(child)) {
        registry.emplace<Hierarchy>(child);
    }
    auto& child_h = registry.get<Hierarchy>(child);

    // If already parented, remove from old parent first
    if (child_h.parent != NullEntity) {
        remove_parent(world, child);
    }

    // If parent is null, we're done (just unparented)
    if (parent == NullEntity) {
        return;
    }

    // Prevent circular hierarchy
    if (is_ancestor_of(world, child, parent)) {
        return;
    }

    // Ensure parent has Hierarchy component
    if (!registry.all_of<Hierarchy>(parent)) {
        registry.emplace<Hierarchy>(parent);
    }
    auto& parent_h = registry.get<Hierarchy>(parent);

    // Set child's parent
    child_h.parent = parent;
    child_h.depth = parent_h.depth + 1;

    // Add child to parent's linked list
    child_h.next_sibling = parent_h.first_child;
    child_h.prev_sibling = NullEntity;

    if (parent_h.first_child != NullEntity) {
        auto& first_child_h = registry.get<Hierarchy>(parent_h.first_child);
        first_child_h.prev_sibling = child;
    }

    parent_h.first_child = child;
    parent_h.children_dirty = true;

    // Update depth for all descendants
    std::function<void(Entity, uint32_t)> update_depth = [&](Entity e, uint32_t depth) {
        auto& h = registry.get<Hierarchy>(e);
        h.depth = depth;
        Entity c = h.first_child;
        while (c != NullEntity) {
            update_depth(c, depth + 1);
            c = registry.get<Hierarchy>(c).next_sibling;
        }
    };

    Entity c = child_h.first_child;
    while (c != NullEntity) {
        update_depth(c, child_h.depth + 1);
        c = registry.get<Hierarchy>(c).next_sibling;
    }
}

void remove_parent(World& world, Entity child) {
    auto& registry = world.registry();

    auto* child_h = registry.try_get<Hierarchy>(child);
    if (!child_h || child_h->parent == NullEntity) {
        return;
    }

    Entity parent = child_h->parent;
    auto& parent_h = registry.get<Hierarchy>(parent);

    // Remove from siblings linked list
    if (child_h->prev_sibling != NullEntity) {
        auto& prev_h = registry.get<Hierarchy>(child_h->prev_sibling);
        prev_h.next_sibling = child_h->next_sibling;
    } else {
        // Child is first child
        parent_h.first_child = child_h->next_sibling;
    }

    if (child_h->next_sibling != NullEntity) {
        auto& next_h = registry.get<Hierarchy>(child_h->next_sibling);
        next_h.prev_sibling = child_h->prev_sibling;
    }

    // Clear child's parent info
    child_h->parent = NullEntity;
    child_h->prev_sibling = NullEntity;
    child_h->next_sibling = NullEntity;
    child_h->depth = 0;

    parent_h.children_dirty = true;

    // Update depth for all descendants
    std::function<void(Entity, uint32_t)> update_depth = [&](Entity e, uint32_t depth) {
        auto& h = registry.get<Hierarchy>(e);
        h.depth = depth;
        Entity c = h.first_child;
        while (c != NullEntity) {
            update_depth(c, depth + 1);
            c = registry.get<Hierarchy>(c).next_sibling;
        }
    };

    Entity c = child_h->first_child;
    while (c != NullEntity) {
        update_depth(c, 1);
        c = registry.get<Hierarchy>(c).next_sibling;
    }
}

const std::vector<Entity>& get_children(World& world, Entity parent) {
    static std::vector<Entity> empty;
    auto& registry = world.registry();

    auto* h = registry.try_get<Hierarchy>(parent);
    if (!h) {
        return empty;
    }

    if (h->children_dirty) {
        h->cached_children.clear();
        Entity child = h->first_child;
        while (child != NullEntity) {
            h->cached_children.push_back(child);
            child = registry.get<Hierarchy>(child).next_sibling;
        }
        h->children_dirty = false;
    }

    return h->cached_children;
}

void iterate_children(World& world, Entity parent, std::function<void(Entity)> fn) {
    auto& registry = world.registry();

    auto* h = registry.try_get<Hierarchy>(parent);
    if (!h) return;

    Entity child = h->first_child;
    while (child != NullEntity) {
        Entity next = registry.get<Hierarchy>(child).next_sibling;
        fn(child);
        child = next;
    }
}

std::vector<Entity> get_root_entities(World& world) {
    std::vector<Entity> roots;
    auto& registry = world.registry();

    auto view = registry.view<EntityInfo>();
    for (auto entity : view) {
        auto* h = registry.try_get<Hierarchy>(entity);
        if (!h || h->parent == NullEntity) {
            roots.push_back(entity);
        }
    }

    return roots;
}

bool is_ancestor_of(World& world, Entity ancestor, Entity descendant) {
    auto& registry = world.registry();

    Entity current = descendant;
    while (current != NullEntity) {
        auto* h = registry.try_get<Hierarchy>(current);
        if (!h) break;

        if (h->parent == ancestor) {
            return true;
        }
        current = h->parent;
    }

    return false;
}

} // namespace engine::scene
