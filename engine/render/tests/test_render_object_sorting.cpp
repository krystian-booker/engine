#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/render/render_pipeline.hpp>
#include <algorithm>
#include <vector>

using namespace engine::render;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// Re-implement the sorting logic from render_pipeline.cpp to test independently.

namespace {

void sort_front_to_back(const Vec3& cam_pos, std::vector<const RenderObject*>& objects) {
    std::sort(objects.begin(), objects.end(),
        [&cam_pos](const RenderObject* a, const RenderObject* b) {
            Vec3 pos_a = Vec3(a->transform[3]);
            Vec3 pos_b = Vec3(b->transform[3]);
            float dist_a = glm::length(pos_a - cam_pos);
            float dist_b = glm::length(pos_b - cam_pos);
            return dist_a < dist_b;
        });
}

void sort_back_to_front(const Vec3& cam_pos, std::vector<const RenderObject*>& objects) {
    std::sort(objects.begin(), objects.end(),
        [&cam_pos](const RenderObject* a, const RenderObject* b) {
            Vec3 pos_a = Vec3(a->transform[3]);
            Vec3 pos_b = Vec3(b->transform[3]);
            float dist_a = glm::length(pos_a - cam_pos);
            float dist_b = glm::length(pos_b - cam_pos);
            return dist_a > dist_b;
        });
}

RenderObject make_object_at(const Vec3& pos) {
    RenderObject obj;
    obj.transform = glm::translate(Mat4(1.0f), pos);
    return obj;
}

} // anonymous namespace

// --- Front-to-back sorting ---

TEST_CASE("Front-to-back sort: nearest first", "[render][sorting]") {
    Vec3 cam_pos(0, 0, 0);

    RenderObject near_obj = make_object_at(Vec3(0, 0, -2));
    RenderObject mid_obj = make_object_at(Vec3(0, 0, -5));
    RenderObject far_obj = make_object_at(Vec3(0, 0, -10));

    std::vector<const RenderObject*> objects = {&far_obj, &near_obj, &mid_obj};
    sort_front_to_back(cam_pos, objects);

    // Nearest (2) should be first, farthest (10) last
    REQUIRE_THAT(Vec3(objects[0]->transform[3]).z, WithinAbs(-2.0f, 0.001f));
    REQUIRE_THAT(Vec3(objects[1]->transform[3]).z, WithinAbs(-5.0f, 0.001f));
    REQUIRE_THAT(Vec3(objects[2]->transform[3]).z, WithinAbs(-10.0f, 0.001f));
}

TEST_CASE("Front-to-back sort: ascending distance", "[render][sorting]") {
    Vec3 cam_pos(5, 0, 0);

    RenderObject a = make_object_at(Vec3(15, 0, 0));  // dist=10
    RenderObject b = make_object_at(Vec3(8, 0, 0));   // dist=3
    RenderObject c = make_object_at(Vec3(25, 0, 0));  // dist=20

    std::vector<const RenderObject*> objects = {&a, &b, &c};
    sort_front_to_back(cam_pos, objects);

    float prev_dist = 0.0f;
    for (const auto* obj : objects) {
        float dist = glm::length(Vec3(obj->transform[3]) - cam_pos);
        REQUIRE(dist >= prev_dist);
        prev_dist = dist;
    }
}

// --- Back-to-front sorting ---

TEST_CASE("Back-to-front sort: farthest first", "[render][sorting]") {
    Vec3 cam_pos(0, 0, 0);

    RenderObject near_obj = make_object_at(Vec3(0, 0, -2));
    RenderObject mid_obj = make_object_at(Vec3(0, 0, -5));
    RenderObject far_obj = make_object_at(Vec3(0, 0, -10));

    std::vector<const RenderObject*> objects = {&near_obj, &far_obj, &mid_obj};
    sort_back_to_front(cam_pos, objects);

    REQUIRE_THAT(Vec3(objects[0]->transform[3]).z, WithinAbs(-10.0f, 0.001f));
    REQUIRE_THAT(Vec3(objects[1]->transform[3]).z, WithinAbs(-5.0f, 0.001f));
    REQUIRE_THAT(Vec3(objects[2]->transform[3]).z, WithinAbs(-2.0f, 0.001f));
}

TEST_CASE("Back-to-front sort: descending distance", "[render][sorting]") {
    Vec3 cam_pos(0, 0, 0);

    RenderObject a = make_object_at(Vec3(3, 0, 0));   // dist=3
    RenderObject b = make_object_at(Vec3(10, 0, 0));  // dist=10
    RenderObject c = make_object_at(Vec3(1, 0, 0));   // dist=1

    std::vector<const RenderObject*> objects = {&a, &b, &c};
    sort_back_to_front(cam_pos, objects);

    float prev_dist = std::numeric_limits<float>::max();
    for (const auto* obj : objects) {
        float dist = glm::length(Vec3(obj->transform[3]) - cam_pos);
        REQUIRE(dist <= prev_dist);
        prev_dist = dist;
    }
}

// --- Edge cases ---

TEST_CASE("Sort single object", "[render][sorting]") {
    Vec3 cam_pos(0, 0, 0);
    RenderObject obj = make_object_at(Vec3(5, 0, 0));
    std::vector<const RenderObject*> objects = {&obj};

    sort_front_to_back(cam_pos, objects);
    REQUIRE(objects.size() == 1);
    REQUIRE(objects[0] == &obj);

    sort_back_to_front(cam_pos, objects);
    REQUIRE(objects.size() == 1);
    REQUIRE(objects[0] == &obj);
}

TEST_CASE("Sort empty list", "[render][sorting]") {
    Vec3 cam_pos(0, 0, 0);
    std::vector<const RenderObject*> objects;

    sort_front_to_back(cam_pos, objects);
    REQUIRE(objects.empty());

    sort_back_to_front(cam_pos, objects);
    REQUIRE(objects.empty());
}

TEST_CASE("Sort objects at equal distances", "[render][sorting]") {
    Vec3 cam_pos(0, 0, 0);

    RenderObject a = make_object_at(Vec3(5, 0, 0));   // dist=5
    RenderObject b = make_object_at(Vec3(0, 5, 0));   // dist=5
    RenderObject c = make_object_at(Vec3(0, 0, 5));   // dist=5

    std::vector<const RenderObject*> objects = {&a, &b, &c};

    // Should not crash, all have equal distance
    sort_front_to_back(cam_pos, objects);
    REQUIRE(objects.size() == 3);

    sort_back_to_front(cam_pos, objects);
    REQUIRE(objects.size() == 3);
}

// --- Opaque/transparent partitioning ---

TEST_CASE("Opaque/transparent partitioning by blend_mode", "[render][sorting]") {
    RenderObject opaque1;
    opaque1.blend_mode = 0;  // Opaque

    RenderObject alpha_test;
    alpha_test.blend_mode = 1;  // AlphaTest (renders with opaque)

    RenderObject transparent1;
    transparent1.blend_mode = 2;  // AlphaBlend

    RenderObject transparent2;
    transparent2.blend_mode = 3;  // Additive

    RenderObject transparent3;
    transparent3.blend_mode = 4;  // Multiply

    std::vector<const RenderObject*> all = {
        &transparent1, &opaque1, &transparent2, &alpha_test, &transparent3
    };

    // Partition: blend_mode <= 1 is opaque, >= 2 is transparent
    auto it = std::partition(all.begin(), all.end(),
        [](const RenderObject* obj) { return obj->blend_mode <= 1; });

    std::vector<const RenderObject*> opaque(all.begin(), it);
    std::vector<const RenderObject*> transparent(it, all.end());

    REQUIRE(opaque.size() == 2);
    REQUIRE(transparent.size() == 3);

    // All opaque items have blend_mode <= 1
    for (const auto* obj : opaque) {
        REQUIRE(obj->blend_mode <= 1);
    }

    // All transparent items have blend_mode >= 2
    for (const auto* obj : transparent) {
        REQUIRE(obj->blend_mode >= 2);
    }
}

TEST_CASE("All opaque objects", "[render][sorting]") {
    RenderObject a, b, c;
    a.blend_mode = 0;
    b.blend_mode = 0;
    c.blend_mode = 1;

    std::vector<const RenderObject*> all = {&a, &b, &c};
    auto it = std::partition(all.begin(), all.end(),
        [](const RenderObject* obj) { return obj->blend_mode <= 1; });

    REQUIRE(it == all.end());  // No transparent objects
}

TEST_CASE("All transparent objects", "[render][sorting]") {
    RenderObject a, b;
    a.blend_mode = 2;
    b.blend_mode = 3;

    std::vector<const RenderObject*> all = {&a, &b};
    auto it = std::partition(all.begin(), all.end(),
        [](const RenderObject* obj) { return obj->blend_mode <= 1; });

    REQUIRE(it == all.begin());  // All transparent
}
