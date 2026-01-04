// This file registers all render components with the reflection system.
// Components are automatically registered at static initialization time.

#include <engine/reflect/reflect.hpp>
#include <engine/render/animation_state_machine.hpp>
#include <engine/render/lod.hpp>
#include <engine/render/ik.hpp>
#include <engine/render/camera_effects.hpp>
#include <engine/render/blend_shapes.hpp>
#include <engine/render/decal_system.hpp>
#include <engine/render/motion_blur.hpp>
#include <engine/render/dof.hpp>
#include <engine/render/reflection_probes.hpp>
#include <engine/render/light_probes.hpp>
#include <engine/render/occlusion_culling.hpp>
#include <engine/render/instancing.hpp>
#include <engine/render/render_to_texture.hpp>
#include <engine/render/root_motion.hpp>

namespace {

using namespace engine::render;
using namespace engine::reflect;

// Register AnimatorComponent
struct AnimatorComponentRegistrar {
    AnimatorComponentRegistrar() {
        TypeRegistry::instance().register_component<AnimatorComponent>("AnimatorComponent",
            TypeMeta().set_display_name("Animator").set_description("Animation state machine and skeletal animation"));

        TypeRegistry::instance().register_property<AnimatorComponent, &AnimatorComponent::apply_root_motion>(
            "apply_root_motion",
            PropertyMeta().set_display_name("Apply Root Motion"));
    }
};
static AnimatorComponentRegistrar _animator_registrar;

// Register LODComponent
struct LODComponentRegistrar {
    LODComponentRegistrar() {
        TypeRegistry::instance().register_component<LODComponent>("LODComponent",
            TypeMeta().set_display_name("LOD").set_description("Level of detail system"));

        TypeRegistry::instance().register_property<LODComponent, &LODComponent::enabled>("enabled",
            PropertyMeta().set_display_name("Enabled"));

        TypeRegistry::instance().register_property<LODComponent, &LODComponent::use_custom_bias>("use_custom_bias",
            PropertyMeta().set_display_name("Use Custom Bias"));

        TypeRegistry::instance().register_property<LODComponent, &LODComponent::custom_bias>("custom_bias",
            PropertyMeta().set_display_name("Custom Bias").set_range(-10.0f, 10.0f));
    }
};
static LODComponentRegistrar _lod_registrar;

// Register IKComponent
struct IKComponentRegistrar {
    IKComponentRegistrar() {
        TypeRegistry::instance().register_component<IKComponent>("IKComponent",
            TypeMeta().set_display_name("IK").set_description("Inverse kinematics for skeletal animation"));

        TypeRegistry::instance().register_property<IKComponent, &IKComponent::foot_ik_enabled>("foot_ik_enabled",
            PropertyMeta().set_display_name("Foot IK Enabled"));

        TypeRegistry::instance().register_property<IKComponent, &IKComponent::look_at_enabled>("look_at_enabled",
            PropertyMeta().set_display_name("Look At Enabled"));

        TypeRegistry::instance().register_property<IKComponent, &IKComponent::hand_ik_enabled>("hand_ik_enabled",
            PropertyMeta().set_display_name("Hand IK Enabled"));
    }
};
static IKComponentRegistrar _ik_registrar;

// Register CameraControllerComponent
struct CameraControllerComponentRegistrar {
    CameraControllerComponentRegistrar() {
        TypeRegistry::instance().register_component<CameraControllerComponent>("CameraControllerComponent",
            TypeMeta().set_display_name("Camera Controller").set_description("Camera control modes and effects"));

        TypeRegistry::instance().register_property<CameraControllerComponent, &CameraControllerComponent::mode>("mode",
            PropertyMeta().set_display_name("Mode"));

        TypeRegistry::instance().register_property<CameraControllerComponent, &CameraControllerComponent::enable_shake>(
            "enable_shake",
            PropertyMeta().set_display_name("Enable Shake"));

        TypeRegistry::instance().register_property<CameraControllerComponent, &CameraControllerComponent::shake_multiplier>(
            "shake_multiplier",
            PropertyMeta().set_display_name("Shake Multiplier").set_range(0.0f, 10.0f));

        TypeRegistry::instance().register_property<CameraControllerComponent, &CameraControllerComponent::follow_target_entity>(
            "follow_target_entity",
            PropertyMeta().set_display_name("Follow Target Entity"));
    }
};
static CameraControllerComponentRegistrar _camera_controller_registrar;

// Register BlendShapeComponent
struct BlendShapeComponentRegistrar {
    BlendShapeComponentRegistrar() {
        TypeRegistry::instance().register_component<BlendShapeComponent>("BlendShapeComponent",
            TypeMeta().set_display_name("Blend Shape").set_description("Morph target / blend shape deformation"));

        TypeRegistry::instance().register_property<BlendShapeComponent, &BlendShapeComponent::vertices_dirty>(
            "vertices_dirty",
            PropertyMeta().set_display_name("Vertices Dirty").set_read_only(true));

        TypeRegistry::instance().register_property<BlendShapeComponent, &BlendShapeComponent::use_gpu_deformation>(
            "use_gpu_deformation",
            PropertyMeta().set_display_name("Use GPU Deformation"));
    }
};
static BlendShapeComponentRegistrar _blend_shape_registrar;

// Register DecalComponent
struct DecalComponentRegistrar {
    DecalComponentRegistrar() {
        TypeRegistry::instance().register_component<DecalComponent>("DecalComponent",
            TypeMeta().set_display_name("Decal").set_description("Projected decal attached to entity"));

        TypeRegistry::instance().register_property<DecalComponent, &DecalComponent::local_offset>("local_offset",
            PropertyMeta().set_display_name("Local Offset"));

        TypeRegistry::instance().register_property<DecalComponent, &DecalComponent::local_rotation>("local_rotation",
            PropertyMeta().set_display_name("Local Rotation"));

        TypeRegistry::instance().register_property<DecalComponent, &DecalComponent::follow_entity>("follow_entity",
            PropertyMeta().set_display_name("Follow Entity"));
    }
};
static DecalComponentRegistrar _decal_registrar;

// Register MotionVectorComponent
struct MotionVectorComponentRegistrar {
    MotionVectorComponentRegistrar() {
        TypeRegistry::instance().register_component<MotionVectorComponent>("MotionVectorComponent",
            TypeMeta().set_display_name("Motion Vector").set_description("Per-object motion vectors for motion blur"));

        TypeRegistry::instance().register_property<MotionVectorComponent, &MotionVectorComponent::first_frame>(
            "first_frame",
            PropertyMeta().set_display_name("First Frame").set_read_only(true));

        TypeRegistry::instance().register_property<MotionVectorComponent, &MotionVectorComponent::enabled>("enabled",
            PropertyMeta().set_display_name("Enabled"));
    }
};
static MotionVectorComponentRegistrar _motion_vector_registrar;

// Register DOFComponent
struct DOFComponentRegistrar {
    DOFComponentRegistrar() {
        TypeRegistry::instance().register_component<DOFComponent>("DOFComponent",
            TypeMeta().set_display_name("Depth of Field").set_description("Per-camera depth of field settings"));

        TypeRegistry::instance().register_property<DOFComponent, &DOFComponent::enabled>("enabled",
            PropertyMeta().set_display_name("Enabled"));

        TypeRegistry::instance().register_property<DOFComponent, &DOFComponent::override_global>("override_global",
            PropertyMeta().set_display_name("Override Global"));
    }
};
static DOFComponentRegistrar _dof_registrar;

// Register ReflectionProbeComponent
struct ReflectionProbeComponentRegistrar {
    ReflectionProbeComponentRegistrar() {
        TypeRegistry::instance().register_component<ReflectionProbeComponent>("ReflectionProbeComponent",
            TypeMeta().set_display_name("Reflection Probe").set_description("Environment reflection probe"));

        TypeRegistry::instance().register_property<ReflectionProbeComponent, &ReflectionProbeComponent::auto_update>(
            "auto_update",
            PropertyMeta().set_display_name("Auto Update"));
    }
};
static ReflectionProbeComponentRegistrar _reflection_probe_registrar;

// Register LightProbeVolumeComponent
struct LightProbeVolumeComponentRegistrar {
    LightProbeVolumeComponentRegistrar() {
        TypeRegistry::instance().register_component<LightProbeVolumeComponent>("LightProbeVolumeComponent",
            TypeMeta().set_display_name("Light Probe Volume").set_description("Indirect lighting probe volume"));

        TypeRegistry::instance().register_property<LightProbeVolumeComponent, &LightProbeVolumeComponent::auto_update>(
            "auto_update",
            PropertyMeta().set_display_name("Auto Update"));

        TypeRegistry::instance().register_property<LightProbeVolumeComponent, &LightProbeVolumeComponent::update_interval>(
            "update_interval",
            PropertyMeta().set_display_name("Update Interval").set_range(0.0f, 60.0f));
    }
};
static LightProbeVolumeComponentRegistrar _light_probe_volume_registrar;

// Register OcclusionCullableComponent
struct OcclusionCullableComponentRegistrar {
    OcclusionCullableComponentRegistrar() {
        TypeRegistry::instance().register_component<OcclusionCullableComponent>("OcclusionCullableComponent",
            TypeMeta().set_display_name("Occlusion Cullable").set_description("Object that can be culled by occlusion system"));

        TypeRegistry::instance().register_property<OcclusionCullableComponent, &OcclusionCullableComponent::use_temporal>(
            "use_temporal",
            PropertyMeta().set_display_name("Use Temporal Coherence"));
    }
};
static OcclusionCullableComponentRegistrar _occlusion_cullable_registrar;

// Register OccluderComponent
struct OccluderComponentRegistrar {
    OccluderComponentRegistrar() {
        TypeRegistry::instance().register_component<OccluderComponent>("OccluderComponent",
            TypeMeta().set_display_name("Occluder").set_description("Object that occludes other objects"));

        TypeRegistry::instance().register_property<OccluderComponent, &OccluderComponent::is_static>("is_static",
            PropertyMeta().set_display_name("Is Static"));
    }
};
static OccluderComponentRegistrar _occluder_registrar;

// Register InstancedRendererComponent
struct InstancedRendererComponentRegistrar {
    InstancedRendererComponentRegistrar() {
        TypeRegistry::instance().register_component<InstancedRendererComponent>("InstancedRendererComponent",
            TypeMeta().set_display_name("Instanced Renderer").set_description("GPU instanced mesh renderer"));

        TypeRegistry::instance().register_property<InstancedRendererComponent, &InstancedRendererComponent::lod_bias>(
            "lod_bias",
            PropertyMeta().set_display_name("LOD Bias").set_range(0.0f, 10.0f));

        TypeRegistry::instance().register_property<InstancedRendererComponent, &InstancedRendererComponent::custom_data>(
            "custom_data",
            PropertyMeta().set_display_name("Custom Data"));
    }
};
static InstancedRendererComponentRegistrar _instanced_renderer_registrar;

// Register RenderToTextureComponent
struct RenderToTextureComponentRegistrar {
    RenderToTextureComponentRegistrar() {
        TypeRegistry::instance().register_component<RenderToTextureComponent>("RenderToTextureComponent",
            TypeMeta().set_display_name("Render To Texture").set_description("Renders camera to a texture"));

        TypeRegistry::instance().register_property<RenderToTextureComponent, &RenderToTextureComponent::width>("width",
            PropertyMeta().set_display_name("Width").set_range(1, 8192));

        TypeRegistry::instance().register_property<RenderToTextureComponent, &RenderToTextureComponent::height>("height",
            PropertyMeta().set_display_name("Height").set_range(1, 8192));

        TypeRegistry::instance().register_property<RenderToTextureComponent, &RenderToTextureComponent::has_depth>("has_depth",
            PropertyMeta().set_display_name("Has Depth"));

        TypeRegistry::instance().register_property<RenderToTextureComponent, &RenderToTextureComponent::update_rate>(
            "update_rate",
            PropertyMeta().set_display_name("Update Rate").set_range(0, 60));

        TypeRegistry::instance().register_property<RenderToTextureComponent, &RenderToTextureComponent::needs_update>(
            "needs_update",
            PropertyMeta().set_display_name("Needs Update"));
    }
};
static RenderToTextureComponentRegistrar _rtt_registrar;

// Register RootMotionComponent
struct RootMotionComponentRegistrar {
    RootMotionComponentRegistrar() {
        TypeRegistry::instance().register_component<RootMotionComponent>("RootMotionComponent",
            TypeMeta().set_display_name("Root Motion").set_description("Root motion extraction and application"));

        TypeRegistry::instance().register_property<RootMotionComponent, &RootMotionComponent::enabled>("enabled",
            PropertyMeta().set_display_name("Enabled"));

        TypeRegistry::instance().register_property<RootMotionComponent, &RootMotionComponent::external_velocity>(
            "external_velocity",
            PropertyMeta().set_display_name("External Velocity"));
    }
};
static RootMotionComponentRegistrar _root_motion_registrar;

} // anonymous namespace
