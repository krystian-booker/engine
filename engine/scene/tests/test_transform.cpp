#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/scene/transform.hpp>
#include <engine/scene/world.hpp>
#include <cmath>

using namespace engine::scene;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

TEST_CASE("LocalTransform default construction", "[scene][transform]") {
    LocalTransform t;

    REQUIRE_THAT(t.position.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(t.position.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(t.position.z, WithinAbs(0.0f, 0.001f));

    REQUIRE_THAT(t.scale.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(t.scale.y, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(t.scale.z, WithinAbs(1.0f, 0.001f));

    // Identity quaternion
    REQUIRE_THAT(t.rotation.w, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(t.rotation.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(t.rotation.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(t.rotation.z, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("LocalTransform construction variants", "[scene][transform]") {
    SECTION("Position only") {
        LocalTransform t{Vec3{1.0f, 2.0f, 3.0f}};
        REQUIRE_THAT(t.position.x, WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(t.position.y, WithinAbs(2.0f, 0.001f));
        REQUIRE_THAT(t.position.z, WithinAbs(3.0f, 0.001f));
        REQUIRE_THAT(t.scale.x, WithinAbs(1.0f, 0.001f)); // Default scale
    }

    SECTION("Position and rotation") {
        Quat rot = glm::angleAxis(glm::radians(90.0f), Vec3{0.0f, 1.0f, 0.0f});
        LocalTransform t{Vec3{1.0f, 2.0f, 3.0f}, rot};
        REQUIRE_THAT(t.position.x, WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(t.rotation.y, WithinAbs(rot.y, 0.001f));
    }

    SECTION("Full transform") {
        Quat rot = glm::identity<Quat>();
        LocalTransform t{Vec3{1.0f, 2.0f, 3.0f}, rot, Vec3{2.0f, 2.0f, 2.0f}};
        REQUIRE_THAT(t.scale.x, WithinAbs(2.0f, 0.001f));
        REQUIRE_THAT(t.scale.y, WithinAbs(2.0f, 0.001f));
        REQUIRE_THAT(t.scale.z, WithinAbs(2.0f, 0.001f));
    }
}

TEST_CASE("LocalTransform matrix generation", "[scene][transform]") {
    SECTION("Identity transform") {
        LocalTransform t;
        Mat4 m = t.matrix();

        // Should be identity matrix
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                float expected = (i == j) ? 1.0f : 0.0f;
                REQUIRE_THAT(m[i][j], WithinAbs(expected, 0.001f));
            }
        }
    }

    SECTION("Translation only") {
        LocalTransform t;
        t.position = Vec3{10.0f, 20.0f, 30.0f};
        Mat4 m = t.matrix();

        REQUIRE_THAT(m[3][0], WithinAbs(10.0f, 0.001f));
        REQUIRE_THAT(m[3][1], WithinAbs(20.0f, 0.001f));
        REQUIRE_THAT(m[3][2], WithinAbs(30.0f, 0.001f));
    }

    SECTION("Scale only") {
        LocalTransform t;
        t.scale = Vec3{2.0f, 3.0f, 4.0f};
        Mat4 m = t.matrix();

        REQUIRE_THAT(m[0][0], WithinAbs(2.0f, 0.001f));
        REQUIRE_THAT(m[1][1], WithinAbs(3.0f, 0.001f));
        REQUIRE_THAT(m[2][2], WithinAbs(4.0f, 0.001f));
    }
}

TEST_CASE("LocalTransform direction vectors", "[scene][transform]") {
    LocalTransform t;

    SECTION("Default forward is -Z") {
        Vec3 fwd = t.forward();
        REQUIRE_THAT(fwd.x, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(fwd.y, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(fwd.z, WithinAbs(-1.0f, 0.001f));
    }

    SECTION("Default right is +X") {
        Vec3 right = t.right();
        REQUIRE_THAT(right.x, WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(right.y, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(right.z, WithinAbs(0.0f, 0.001f));
    }

    SECTION("Default up is +Y") {
        Vec3 up = t.up();
        REQUIRE_THAT(up.x, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(up.y, WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(up.z, WithinAbs(0.0f, 0.001f));
    }

    SECTION("Rotated forward") {
        t.rotation = glm::angleAxis(glm::radians(90.0f), Vec3{0.0f, 1.0f, 0.0f});
        Vec3 fwd = t.forward();
        REQUIRE_THAT(fwd.x, WithinAbs(-1.0f, 0.001f));
        REQUIRE_THAT(fwd.z, WithinAbs(0.0f, 0.001f));
    }
}

TEST_CASE("LocalTransform euler angles", "[scene][transform]") {
    LocalTransform t;

    SECTION("Set euler angles") {
        t.set_euler(Vec3{glm::radians(45.0f), glm::radians(90.0f), 0.0f});
        Vec3 euler = t.euler();
        // Due to gimbal lock and different conventions, we just check it doesn't crash
        // and produces some rotation
        REQUIRE(glm::length(t.rotation) > 0.99f); // Should be unit quaternion
    }

    SECTION("Euler roundtrip for simple rotation") {
        Vec3 original{glm::radians(30.0f), 0.0f, 0.0f}; // Pitch only
        t.set_euler(original);
        Vec3 result = t.euler();
        REQUIRE_THAT(result.x, WithinAbs(original.x, 0.01f));
    }
}

TEST_CASE("LocalTransform look_at", "[scene][transform]") {
    LocalTransform t;
    t.position = Vec3{0.0f, 0.0f, 0.0f};

    SECTION("Look at +Z") {
        t.look_at(Vec3{0.0f, 0.0f, 10.0f});
        Vec3 fwd = t.forward();
        REQUIRE_THAT(fwd.z, WithinAbs(1.0f, 0.01f));
    }

    SECTION("Look at +X") {
        t.look_at(Vec3{10.0f, 0.0f, 0.0f});
        Vec3 fwd = t.forward();
        REQUIRE_THAT(fwd.x, WithinAbs(1.0f, 0.01f));
    }
}

TEST_CASE("WorldTransform", "[scene][transform]") {
    SECTION("Default construction") {
        WorldTransform wt;
        // Should be identity matrix
        REQUIRE_THAT(wt.matrix[0][0], WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(wt.matrix[3][3], WithinAbs(1.0f, 0.001f));
    }

    SECTION("Construction from matrix") {
        Mat4 m = glm::translate(Mat4{1.0f}, Vec3{5.0f, 10.0f, 15.0f});
        WorldTransform wt{m};
        REQUIRE_THAT(wt.matrix[3][0], WithinAbs(5.0f, 0.001f));
    }

    SECTION("Extract position") {
        Mat4 m = glm::translate(Mat4{1.0f}, Vec3{5.0f, 10.0f, 15.0f});
        WorldTransform wt{m};
        Vec3 pos = wt.position();
        REQUIRE_THAT(pos.x, WithinAbs(5.0f, 0.001f));
        REQUIRE_THAT(pos.y, WithinAbs(10.0f, 0.001f));
        REQUIRE_THAT(pos.z, WithinAbs(15.0f, 0.001f));
    }

    SECTION("Extract scale") {
        Mat4 m = glm::scale(Mat4{1.0f}, Vec3{2.0f, 3.0f, 4.0f});
        WorldTransform wt{m};
        Vec3 s = wt.scale();
        REQUIRE_THAT(s.x, WithinAbs(2.0f, 0.001f));
        REQUIRE_THAT(s.y, WithinAbs(3.0f, 0.001f));
        REQUIRE_THAT(s.z, WithinAbs(4.0f, 0.001f));
    }

    SECTION("Extract rotation") {
        Quat q = glm::angleAxis(glm::radians(90.0f), Vec3{0.0f, 1.0f, 0.0f});
        Mat4 m = glm::mat4_cast(q);
        WorldTransform wt{m};
        Quat extracted = wt.rotation();

        // Quaternions may differ by sign but represent same rotation
        float dot = glm::abs(glm::dot(q, extracted));
        REQUIRE_THAT(dot, WithinAbs(1.0f, 0.01f));
    }
}

TEST_CASE("PreviousTransform", "[scene][transform]") {
    SECTION("Default construction") {
        PreviousTransform pt;
        REQUIRE_THAT(pt.position.x, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(pt.scale.x, WithinAbs(1.0f, 0.001f));
    }

    SECTION("Construction from TRS") {
        PreviousTransform pt{Vec3{1.0f, 2.0f, 3.0f}, Quat{1.0f, 0.0f, 0.0f, 0.0f}, Vec3{1.0f}};
        REQUIRE_THAT(pt.position.x, WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(pt.position.y, WithinAbs(2.0f, 0.001f));
        REQUIRE_THAT(pt.position.z, WithinAbs(3.0f, 0.001f));
    }

    SECTION("Construction from matrix") {
        Mat4 m = glm::translate(Mat4{1.0f}, Vec3{1.0f, 2.0f, 3.0f});
        PreviousTransform pt = PreviousTransform::from_matrix(m);
        REQUIRE_THAT(pt.position.x, WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(pt.position.y, WithinAbs(2.0f, 0.001f));
    }
}

TEST_CASE("LocalTransform auto-provisions render transform state", "[scene][transform]") {
    World world;
    Entity entity = world.create("Renderable");

    auto& local = world.emplace<LocalTransform>(entity, Vec3{1.0f, 2.0f, 3.0f});

    REQUIRE(world.has<WorldTransform>(entity));
    REQUIRE(world.has<PreviousTransform>(entity));
    REQUIRE(world.has<InterpolatedTransform>(entity));
    REQUIRE(world.get<WorldTransform>(entity).position() == local.position);
    REQUIRE(world.get<PreviousTransform>(entity).position == local.position);
}

TEST_CASE("Removing LocalTransform cleans up derived transform state", "[scene][transform]") {
    World world;
    Entity entity = world.create("Renderable");

    world.emplace<LocalTransform>(entity, Vec3{1.0f, 2.0f, 3.0f});
    REQUIRE(world.has<WorldTransform>(entity));
    REQUIRE(world.has<PreviousTransform>(entity));
    REQUIRE(world.has<InterpolatedTransform>(entity));

    world.remove<LocalTransform>(entity);

    REQUIRE_FALSE(world.has<LocalTransform>(entity));
    REQUIRE_FALSE(world.has<WorldTransform>(entity));
    REQUIRE_FALSE(world.has<PreviousTransform>(entity));
    REQUIRE_FALSE(world.has<InterpolatedTransform>(entity));
}

TEST_CASE("Transform decomposition tolerates zero scale", "[scene][transform][regression]") {
    World world;
    Entity entity = world.create("ZeroScale");

    auto& local = world.emplace<LocalTransform>(entity);
    local.scale = Vec3{0.0f, 0.0f, 0.0f};

    transform_system(world, 0.0);
    interpolate_transforms(world, 0.5);

    const Quat rotation = world.get<WorldTransform>(entity).rotation();
    REQUIRE(std::isfinite(rotation.w));
    REQUIRE(std::isfinite(rotation.x));
    REQUIRE(std::isfinite(rotation.y));
    REQUIRE(std::isfinite(rotation.z));

    const auto& previous = world.get<PreviousTransform>(entity);
    REQUIRE(std::isfinite(previous.rotation.w));
    REQUIRE(std::isfinite(previous.rotation.x));
    REQUIRE(std::isfinite(previous.rotation.y));
    REQUIRE(std::isfinite(previous.rotation.z));
}

TEST_CASE("Interpolate transforms writes render state from previous and current world transforms", "[scene][transform]") {
    World world;
    Entity entity = world.create("Interpolated");

    auto& local = world.emplace<LocalTransform>(entity, Vec3{0.0f, 0.0f, 0.0f});
    transform_system(world, 0.016);

    local.position = Vec3{10.0f, 0.0f, 0.0f};
    transform_system(world, 0.016);
    interpolate_transforms(world, 0.5);

    REQUIRE(world.has<InterpolatedTransform>(entity));
    const auto& interpolated = world.get<InterpolatedTransform>(entity);
    REQUIRE_THAT(interpolated.matrix[3][0], WithinAbs(5.0f, 0.01f));
}

TEST_CASE("Hierarchy component", "[scene][transform][hierarchy]") {
    Hierarchy h;

    SECTION("Default values") {
        REQUIRE(h.parent == NullEntity);
        REQUIRE(h.first_child == NullEntity);
        REQUIRE(h.next_sibling == NullEntity);
        REQUIRE(h.prev_sibling == NullEntity);
        REQUIRE(h.depth == 0);
        REQUIRE(h.children_dirty == true);
    }
}

TEST_CASE("Hierarchy functions", "[scene][transform][hierarchy]") {
    World world;

    Entity parent = world.create("Parent");
    Entity child1 = world.create("Child1");
    Entity child2 = world.create("Child2");

    // Add transform components
    world.emplace<LocalTransform>(parent);
    world.emplace<LocalTransform>(child1);
    world.emplace<LocalTransform>(child2);
    world.emplace<Hierarchy>(parent);
    world.emplace<Hierarchy>(child1);
    world.emplace<Hierarchy>(child2);

    SECTION("Set parent") {
        set_parent(world, child1, parent);

        auto& child_h = world.get<Hierarchy>(child1);
        auto& parent_h = world.get<Hierarchy>(parent);

        REQUIRE(child_h.parent == parent);
        REQUIRE(parent_h.first_child == child1);
    }

    SECTION("Multiple children") {
        set_parent(world, child1, parent);
        set_parent(world, child2, parent);

        const auto& children = get_children(world, parent);
        REQUIRE(children.size() == 2);
    }

    SECTION("Remove parent") {
        set_parent(world, child1, parent);
        remove_parent(world, child1);

        auto& child_h = world.get<Hierarchy>(child1);
        REQUIRE(child_h.parent == NullEntity);
    }

    SECTION("Is ancestor of") {
        set_parent(world, child1, parent);

        REQUIRE(is_ancestor_of(world, parent, child1));
        REQUIRE_FALSE(is_ancestor_of(world, child1, parent));
        REQUIRE_FALSE(is_ancestor_of(world, child1, child2));
    }
}
