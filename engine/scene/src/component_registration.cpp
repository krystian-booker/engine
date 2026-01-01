#include <engine/reflect/reflect.hpp>
#include <engine/scene/entity.hpp>
#include <engine/scene/transform.hpp>
#include <engine/scene/render_components.hpp>

// This file registers all built-in scene components with the reflection system.
// Components are automatically registered at static initialization time.

namespace {

using namespace engine::scene;
using namespace engine::reflect;

// Register EntityInfo component
struct EntityInfoRegistrar {
    EntityInfoRegistrar() {
        TypeRegistry::instance().register_component<EntityInfo>("EntityInfo",
            TypeMeta().set_display_name("Entity Info").set_description("Core entity identification"));

        TypeRegistry::instance().register_property<EntityInfo, &EntityInfo::name>("name",
            PropertyMeta().set_display_name("Name"));

        TypeRegistry::instance().register_property<EntityInfo, &EntityInfo::uuid>("uuid",
            PropertyMeta().set_display_name("UUID").set_read_only(true));

        TypeRegistry::instance().register_property<EntityInfo, &EntityInfo::enabled>("enabled",
            PropertyMeta().set_display_name("Enabled"));
    }
};
static EntityInfoRegistrar _entityinfo_registrar;

// Register LocalTransform component
struct LocalTransformRegistrar {
    LocalTransformRegistrar() {
        TypeRegistry::instance().register_component<LocalTransform>("LocalTransform",
            TypeMeta().set_display_name("Transform").set_description("Local space transformation"));

        TypeRegistry::instance().register_property<LocalTransform, &LocalTransform::position>("position",
            PropertyMeta().set_display_name("Position").set_category("Transform"));

        TypeRegistry::instance().register_property<LocalTransform, &LocalTransform::rotation>("rotation",
            PropertyMeta().set_display_name("Rotation").set_category("Transform"));

        TypeRegistry::instance().register_property<LocalTransform, &LocalTransform::scale>("scale",
            PropertyMeta().set_display_name("Scale").set_category("Transform"));
    }
};
static LocalTransformRegistrar _localtransform_registrar;

// Register WorldTransform component
struct WorldTransformRegistrar {
    WorldTransformRegistrar() {
        TypeRegistry::instance().register_component<WorldTransform>("WorldTransform",
            TypeMeta().set_display_name("World Transform").set_description("Computed world space transformation"));

        TypeRegistry::instance().register_property<WorldTransform, &WorldTransform::matrix>("matrix",
            PropertyMeta().set_display_name("Matrix").set_read_only(true));
    }
};
static WorldTransformRegistrar _worldtransform_registrar;

// Register Hierarchy component
struct HierarchyRegistrar {
    HierarchyRegistrar() {
        TypeRegistry::instance().register_component<Hierarchy>("Hierarchy",
            TypeMeta().set_display_name("Hierarchy").set_description("Parent-child relationships"));

        TypeRegistry::instance().register_property<Hierarchy, &Hierarchy::depth>("depth",
            PropertyMeta().set_display_name("Depth").set_read_only(true));
    }
};
static HierarchyRegistrar _hierarchy_registrar;

// Register MeshRenderer component
struct MeshRendererRegistrar {
    MeshRendererRegistrar() {
        TypeRegistry::instance().register_component<MeshRenderer>("MeshRenderer",
            TypeMeta().set_display_name("Mesh Renderer").set_description("Renders a mesh with a material"));

        TypeRegistry::instance().register_property<MeshRenderer, &MeshRenderer::render_layer>("render_layer",
            PropertyMeta().set_display_name("Render Layer").set_range(0, 255));

        TypeRegistry::instance().register_property<MeshRenderer, &MeshRenderer::visible>("visible",
            PropertyMeta().set_display_name("Visible"));

        TypeRegistry::instance().register_property<MeshRenderer, &MeshRenderer::cast_shadows>("cast_shadows",
            PropertyMeta().set_display_name("Cast Shadows"));

        TypeRegistry::instance().register_property<MeshRenderer, &MeshRenderer::receive_shadows>("receive_shadows",
            PropertyMeta().set_display_name("Receive Shadows"));
    }
};
static MeshRendererRegistrar _meshrenderer_registrar;

// Register Camera component
struct CameraRegistrar {
    CameraRegistrar() {
        TypeRegistry::instance().register_component<Camera>("Camera",
            TypeMeta().set_display_name("Camera").set_description("Camera for rendering viewpoints"));

        TypeRegistry::instance().register_property<Camera, &Camera::fov>("fov",
            PropertyMeta().set_display_name("Field of View").set_range(1.0f, 179.0f));

        TypeRegistry::instance().register_property<Camera, &Camera::near_plane>("near_plane",
            PropertyMeta().set_display_name("Near Plane").set_range(0.001f, 1000.0f));

        TypeRegistry::instance().register_property<Camera, &Camera::far_plane>("far_plane",
            PropertyMeta().set_display_name("Far Plane").set_range(1.0f, 100000.0f));

        TypeRegistry::instance().register_property<Camera, &Camera::aspect_ratio>("aspect_ratio",
            PropertyMeta().set_display_name("Aspect Ratio"));

        TypeRegistry::instance().register_property<Camera, &Camera::priority>("priority",
            PropertyMeta().set_display_name("Priority").set_range(0, 255));

        TypeRegistry::instance().register_property<Camera, &Camera::active>("active",
            PropertyMeta().set_display_name("Active"));

        TypeRegistry::instance().register_property<Camera, &Camera::orthographic>("orthographic",
            PropertyMeta().set_display_name("Orthographic"));

        TypeRegistry::instance().register_property<Camera, &Camera::ortho_size>("ortho_size",
            PropertyMeta().set_display_name("Ortho Size").set_range(0.1f, 1000.0f));
    }
};
static CameraRegistrar _camera_registrar;

// Register Light component
struct LightRegistrar {
    LightRegistrar() {
        TypeRegistry::instance().register_component<Light>("Light",
            TypeMeta().set_display_name("Light").set_description("Light source for illumination"));

        TypeRegistry::instance().register_property<Light, &Light::type>("type",
            PropertyMeta().set_display_name("Type"));

        TypeRegistry::instance().register_property<Light, &Light::color>("color",
            PropertyMeta().set_display_name("Color").set_color(true));

        TypeRegistry::instance().register_property<Light, &Light::intensity>("intensity",
            PropertyMeta().set_display_name("Intensity").set_range(0.0f, 100.0f));

        TypeRegistry::instance().register_property<Light, &Light::range>("range",
            PropertyMeta().set_display_name("Range").set_range(0.0f, 1000.0f));

        TypeRegistry::instance().register_property<Light, &Light::spot_inner_angle>("spot_inner_angle",
            PropertyMeta().set_display_name("Inner Angle").set_range(0.0f, 180.0f).set_angle(true));

        TypeRegistry::instance().register_property<Light, &Light::spot_outer_angle>("spot_outer_angle",
            PropertyMeta().set_display_name("Outer Angle").set_range(0.0f, 180.0f).set_angle(true));

        TypeRegistry::instance().register_property<Light, &Light::cast_shadows>("cast_shadows",
            PropertyMeta().set_display_name("Cast Shadows"));

        TypeRegistry::instance().register_property<Light, &Light::enabled>("enabled",
            PropertyMeta().set_display_name("Enabled"));
    }
};
static LightRegistrar _light_registrar;

// Register Skybox component
struct SkyboxRegistrar {
    SkyboxRegistrar() {
        TypeRegistry::instance().register_component<Skybox>("Skybox",
            TypeMeta().set_display_name("Skybox").set_description("Skybox/environment map"));

        TypeRegistry::instance().register_property<Skybox, &Skybox::intensity>("intensity",
            PropertyMeta().set_display_name("Intensity").set_range(0.0f, 10.0f));

        TypeRegistry::instance().register_property<Skybox, &Skybox::rotation>("rotation",
            PropertyMeta().set_display_name("Rotation").set_angle(true));
    }
};
static SkyboxRegistrar _skybox_registrar;

// Register ParticleEmitter component
struct ParticleEmitterRegistrar {
    ParticleEmitterRegistrar() {
        TypeRegistry::instance().register_component<ParticleEmitter>("ParticleEmitter",
            TypeMeta().set_display_name("Particle Emitter").set_description("Particle system emitter"));

        TypeRegistry::instance().register_property<ParticleEmitter, &ParticleEmitter::max_particles>("max_particles",
            PropertyMeta().set_display_name("Max Particles").set_range(1, 100000));

        TypeRegistry::instance().register_property<ParticleEmitter, &ParticleEmitter::emission_rate>("emission_rate",
            PropertyMeta().set_display_name("Emission Rate").set_range(0.0f, 10000.0f));

        TypeRegistry::instance().register_property<ParticleEmitter, &ParticleEmitter::lifetime>("lifetime",
            PropertyMeta().set_display_name("Lifetime").set_range(0.01f, 100.0f));

        TypeRegistry::instance().register_property<ParticleEmitter, &ParticleEmitter::initial_speed>("initial_speed",
            PropertyMeta().set_display_name("Initial Speed").set_range(0.0f, 1000.0f));

        TypeRegistry::instance().register_property<ParticleEmitter, &ParticleEmitter::initial_velocity_variance>("initial_velocity_variance",
            PropertyMeta().set_display_name("Velocity Variance"));

        TypeRegistry::instance().register_property<ParticleEmitter, &ParticleEmitter::start_color>("start_color",
            PropertyMeta().set_display_name("Start Color").set_color(true));

        TypeRegistry::instance().register_property<ParticleEmitter, &ParticleEmitter::end_color>("end_color",
            PropertyMeta().set_display_name("End Color").set_color(true));

        TypeRegistry::instance().register_property<ParticleEmitter, &ParticleEmitter::start_size>("start_size",
            PropertyMeta().set_display_name("Start Size").set_range(0.0f, 100.0f));

        TypeRegistry::instance().register_property<ParticleEmitter, &ParticleEmitter::end_size>("end_size",
            PropertyMeta().set_display_name("End Size").set_range(0.0f, 100.0f));

        TypeRegistry::instance().register_property<ParticleEmitter, &ParticleEmitter::gravity>("gravity",
            PropertyMeta().set_display_name("Gravity"));

        TypeRegistry::instance().register_property<ParticleEmitter, &ParticleEmitter::enabled>("enabled",
            PropertyMeta().set_display_name("Enabled"));
    }
};
static ParticleEmitterRegistrar _particleemitter_registrar;

// Register primitive math types for reflection
struct MathTypesRegistrar {
    MathTypesRegistrar() {
        using namespace engine::core;

        // Vec2
        TypeRegistry::instance().register_type<Vec2>("Vec2");
        // Vec3
        TypeRegistry::instance().register_type<Vec3>("Vec3");
        // Vec4
        TypeRegistry::instance().register_type<Vec4>("Vec4");
        // Quat
        TypeRegistry::instance().register_type<Quat>("Quat");
        // Mat4
        TypeRegistry::instance().register_type<Mat4>("Mat4");
    }
};
static MathTypesRegistrar _mathtypes_registrar;

} // anonymous namespace
