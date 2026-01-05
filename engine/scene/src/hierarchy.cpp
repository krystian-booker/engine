#include <engine/scene/transform.hpp>
#include <engine/scene/world.hpp>
#include <engine/core/log.hpp>
#include <unordered_map>

namespace engine::scene {

namespace {

// Maximum iterations for hierarchy traversal to prevent infinite loops from corrupted data
constexpr size_t MAX_HIERARCHY_ITERATIONS = 100000;

struct RootList {
    Entity first = NullEntity;
    Entity last = NullEntity;
    mutable std::vector<Entity> cached;
    mutable bool dirty = true;
};

// Shared root map for all hierarchy operations
std::unordered_map<entt::registry*, RootList>& get_root_map() {
    static std::unordered_map<entt::registry*, RootList> root_map;
    return root_map;
}

RootList& roots(World& world) {
    auto& root_map = get_root_map();
    auto& registry = world.registry();
    auto& root_list = root_map[&registry];

    // Reset if the cached head/tail are no longer valid (e.g., after a clear)
    if ((root_list.first != NullEntity && !registry.valid(root_list.first)) ||
        (root_list.last != NullEntity && !registry.valid(root_list.last))) {
        root_list = RootList{};
    }

    // Verify linked list integrity - first entity should have prev=NullEntity and parent=NullEntity
    if (root_list.first != NullEntity) {
        auto* first_h = registry.try_get<Hierarchy>(root_list.first);
        if (!first_h || first_h->prev_sibling != NullEntity || first_h->parent != NullEntity) {
            root_list = RootList{};  // Reset corrupted list
        }
    }

    if (!root_list.cached.empty() && (root_list.cached.front() != NullEntity) &&
        !registry.valid(root_list.cached.front())) {
        root_list.cached.clear();
        root_list.dirty = true;
    }

    return root_list;
}

void mark_roots_dirty(RootList& root_list) {
    root_list.dirty = true;
}

void update_descendant_depths(entt::registry& registry, Entity entity, uint32_t depth) {
    auto* h = registry.try_get<Hierarchy>(entity);
    if (!h) return;

    h->depth = depth;
    Entity child = h->first_child;
    size_t iterations = 0;
    while (child != NullEntity && iterations++ < MAX_HIERARCHY_ITERATIONS) {
        update_descendant_depths(registry, child, depth + 1);
        auto* child_h = registry.try_get<Hierarchy>(child);
        child = child_h ? child_h->next_sibling : NullEntity;
    }
    if (iterations >= MAX_HIERARCHY_ITERATIONS) {
        core::log(core::LogLevel::Error, "Hierarchy corruption detected: infinite loop in update_descendant_depths");
    }
}

void detach_from_parent(entt::registry& registry, RootList& root_list, Entity child, Hierarchy& child_h) {
    Entity old_parent = child_h.parent;

    if (old_parent == NullEntity) {
        // Check if entity is actually in the root list before detaching
        // An entity is in the list if it has siblings OR is first/last in list
        bool is_in_root_list = (child_h.prev_sibling != NullEntity) ||
                               (child_h.next_sibling != NullEntity) ||
                               (root_list.first == child) ||
                               (root_list.last == child);

        if (!is_in_root_list) {
            // Entity is not in any list yet, nothing to detach
            return;
        }

        // Detach from root list
        Entity prev = child_h.prev_sibling;
        Entity next = child_h.next_sibling;

        if (prev != NullEntity) {
            registry.get<Hierarchy>(prev).next_sibling = next;
        } else {
            root_list.first = next;
        }

        if (next != NullEntity) {
            registry.get<Hierarchy>(next).prev_sibling = prev;
        } else {
            root_list.last = prev;
        }

        mark_roots_dirty(root_list);
    } else {
        // Detach from old parent
        auto& parent_h = registry.get<Hierarchy>(old_parent);

        if (child_h.prev_sibling != NullEntity) {
            registry.get<Hierarchy>(child_h.prev_sibling).next_sibling = child_h.next_sibling;
        } else {
            parent_h.first_child = child_h.next_sibling;
        }

        if (child_h.next_sibling != NullEntity) {
            registry.get<Hierarchy>(child_h.next_sibling).prev_sibling = child_h.prev_sibling;
        }

        parent_h.children_dirty = true;
    }

    child_h.parent = NullEntity;
    child_h.prev_sibling = NullEntity;
    child_h.next_sibling = NullEntity;
}

void attach_to_parent(entt::registry& registry, RootList& root_list, Entity child, Entity parent, Entity before_sibling, Hierarchy& child_h) {
    if (parent == NullEntity) {
        // Ensure the before_sibling is a root entity
        if (before_sibling != NullEntity) {
            auto* before_h = registry.try_get<Hierarchy>(before_sibling);
            if (!before_h || before_h->parent != NullEntity) {
                before_sibling = NullEntity;
            }
        }

        child_h.parent = NullEntity;
        child_h.depth = 0;

        if (before_sibling != NullEntity) {
            auto& before_h = registry.get<Hierarchy>(before_sibling);
            child_h.prev_sibling = before_h.prev_sibling;
            child_h.next_sibling = before_sibling;

            if (before_h.prev_sibling != NullEntity) {
                registry.get<Hierarchy>(before_h.prev_sibling).next_sibling = child;
            } else {
                root_list.first = child;
            }

            before_h.prev_sibling = child;
        } else {
            // Append to end of root list
            child_h.prev_sibling = root_list.last;
            child_h.next_sibling = NullEntity;

            if (root_list.last != NullEntity) {
                registry.get<Hierarchy>(root_list.last).next_sibling = child;
            } else {
                root_list.first = child;
            }

            root_list.last = child;
        }

        mark_roots_dirty(root_list);
        return;
    }

    // Ensure before_sibling belongs to the target parent
    if (before_sibling != NullEntity) {
        auto* before_h = registry.try_get<Hierarchy>(before_sibling);
        if (!before_h || before_h->parent != parent) {
            before_sibling = NullEntity;
        }
    }

    auto& parent_h = registry.get<Hierarchy>(parent);

    child_h.parent = parent;
    child_h.depth = parent_h.depth + 1;

    if (before_sibling != NullEntity) {
        auto& before_h = registry.get<Hierarchy>(before_sibling);
        child_h.prev_sibling = before_h.prev_sibling;
        child_h.next_sibling = before_sibling;

        if (before_h.prev_sibling != NullEntity) {
            registry.get<Hierarchy>(before_h.prev_sibling).next_sibling = child;
        } else {
            parent_h.first_child = child;
        }

        before_h.prev_sibling = child;
    } else {
        // Append to end of parent's children
        Entity last_child = parent_h.first_child;
        if (last_child == NullEntity) {
            parent_h.first_child = child;
            child_h.prev_sibling = NullEntity;
            child_h.next_sibling = NullEntity;
        } else {
            size_t iterations = 0;
            while (iterations++ < MAX_HIERARCHY_ITERATIONS) {
                auto* h = registry.try_get<Hierarchy>(last_child);
                if (!h || h->next_sibling == NullEntity) break;
                last_child = h->next_sibling;
            }

            if (auto* last_h = registry.try_get<Hierarchy>(last_child)) {
                last_h->next_sibling = child;
            }
            child_h.prev_sibling = last_child;
            child_h.next_sibling = NullEntity;
        }
    }

    parent_h.children_dirty = true;
}

} // namespace

void set_parent(World& world, Entity child, Entity parent) {
    auto& registry = world.registry();
    auto& root_list = roots(world);

    if (parent == child) {
        return;
    }

    // Ensure child has Hierarchy component
    if (!registry.all_of<Hierarchy>(child)) {
        registry.emplace<Hierarchy>(child);
    }

    // For default insertion, place before the current first child/root (Unity-style front insert)
    Entity before = NullEntity;
    if (parent != NullEntity) {
        if (!registry.all_of<Hierarchy>(parent)) {
            registry.emplace<Hierarchy>(parent);
        }
        before = registry.get<Hierarchy>(parent).first_child;
    } else {
        before = root_list.first;
    }

    set_parent(world, child, parent, before);
}

void set_parent(World& world, Entity child, Entity parent, Entity before_sibling) {
    auto& registry = world.registry();
    auto& root_list = roots(world);

    if (child == parent) {
        return;
    }

    // Ensure child has Hierarchy component
    if (!registry.all_of<Hierarchy>(child)) {
        registry.emplace<Hierarchy>(child);
    }
    auto& child_h = registry.get<Hierarchy>(child);

    if (before_sibling == child) {
        before_sibling = NullEntity;
    }

    // Prevent circular hierarchy
    if (parent != NullEntity && is_ancestor_of(world, child, parent)) {
        return;
    }

    Entity old_parent = child_h.parent;

    // Ensure target parent has Hierarchy so we can insert
    if (parent != NullEntity && !registry.all_of<Hierarchy>(parent)) {
        registry.emplace<Hierarchy>(parent);
    }

    // Detach from current parent/root list
    detach_from_parent(registry, root_list, child, child_h);

    // Attach to new parent/root at the requested position
    attach_to_parent(registry, root_list, child, parent, before_sibling, child_h);

    // Update depth for descendants (child depth already set during attach)
    Entity c = child_h.first_child;
    size_t iterations = 0;
    while (c != NullEntity && iterations++ < MAX_HIERARCHY_ITERATIONS) {
        update_descendant_depths(registry, c, child_h.depth + 1);
        auto* c_h = registry.try_get<Hierarchy>(c);
        c = c_h ? c_h->next_sibling : NullEntity;
    }

    // Mark old parent's cached children dirty if it changed
    if (old_parent != NullEntity && old_parent != parent) {
        if (auto* old_parent_h = registry.try_get<Hierarchy>(old_parent)) {
            old_parent_h->children_dirty = true;
        }
    }
}

void remove_parent(World& world, Entity child) {
    set_parent(world, child, NullEntity, NullEntity);
}

void detach_from_hierarchy(World& world, Entity child) {
    auto& registry = world.registry();
    auto& root_list = roots(world);

    auto* child_h = registry.try_get<Hierarchy>(child);
    if (!child_h) {
        return;
    }

    detach_from_parent(registry, root_list, child, *child_h);
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
        size_t iterations = 0;
        while (child != NullEntity && iterations++ < MAX_HIERARCHY_ITERATIONS) {
            h->cached_children.push_back(child);
            auto* child_h = registry.try_get<Hierarchy>(child);
            child = child_h ? child_h->next_sibling : NullEntity;
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
    size_t iterations = 0;
    while (child != NullEntity && iterations++ < MAX_HIERARCHY_ITERATIONS) {
        auto* child_h = registry.try_get<Hierarchy>(child);
        Entity next = child_h ? child_h->next_sibling : NullEntity;
        fn(child);
        child = next;
    }
}

std::vector<Entity> get_root_entities(World& world) {
    auto& registry = world.registry();
    auto& root_list = roots(world);

    // Lazy rebuild for worlds that existed before roots were populated
    if (root_list.first == NullEntity) {
        Entity last_root = NullEntity;
        auto view = registry.view<EntityInfo>();
        for (auto entity : view) {
            auto* h = registry.try_get<Hierarchy>(entity);
            if (!h) {
                h = &registry.emplace<Hierarchy>(entity);
            }

            if (h->parent != NullEntity) {
                continue;
            }

            h->prev_sibling = last_root;
            h->next_sibling = NullEntity;

            if (last_root != NullEntity) {
                if (auto* last_h = registry.try_get<Hierarchy>(last_root)) {
                    last_h->next_sibling = entity;
                }
            } else {
                root_list.first = entity;
            }

            last_root = entity;
        }

        root_list.last = last_root;
        root_list.dirty = true;
    }

    if (root_list.dirty) {
        root_list.cached.clear();
        Entity root = root_list.first;
        size_t iterations = 0;
        while (root != NullEntity && iterations++ < MAX_HIERARCHY_ITERATIONS) {
            root_list.cached.push_back(root);
            auto* h = registry.try_get<Hierarchy>(root);
            root = h ? h->next_sibling : NullEntity;
        }
        root_list.dirty = false;
    }

    return root_list.cached;
}

bool is_ancestor_of(World& world, Entity ancestor, Entity descendant) {
    auto& registry = world.registry();

    Entity current = descendant;
    size_t iterations = 0;
    while (current != NullEntity && iterations++ < MAX_HIERARCHY_ITERATIONS) {
        auto* h = registry.try_get<Hierarchy>(current);
        if (!h) break;

        if (h->parent == ancestor) {
            return true;
        }
        current = h->parent;
    }

    return false;
}

void reset_roots(World& world) {
    auto& root_list = roots(world);
    root_list.first = NullEntity;
    root_list.last = NullEntity;
    root_list.cached.clear();
    root_list.dirty = true;
}

} // namespace engine::scene
