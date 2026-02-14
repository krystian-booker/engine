"""
Golden Reference Scene Generator for render_test
Recreates the exact same scene as samples/render_test/src/main.cpp in Blender Cycles.

Usage:
  # Create .blend file and render golden image:
  blender --background --python create_golden_scene.py

  # Just create .blend file (no render):
  blender --background --python create_golden_scene.py -- --no-render
"""

import bpy
import math
import sys
import os
from mathutils import Vector

# ---------------------------------------------------------------------------
# Coordinate conversion: Engine (Y-up, right-handed) -> Blender (Z-up, right-handed)
# Engine (x, y, z) -> Blender (x, -z, y)
# ---------------------------------------------------------------------------

def e2b_pos(x, y, z):
    return (x, -z, y)

def e2b_scale(sx, sy, sz):
    return (sx, sz, sy)

def e2b_dir(dx, dy, dz):
    return (dx, -dz, dy)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def clear_scene():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    for block in [bpy.data.meshes, bpy.data.materials, bpy.data.cameras,
                  bpy.data.lights, bpy.data.images]:
        for item in block:
            block.remove(item)

def create_pbr_material(name, base_color, metallic=0.0, roughness=0.5,
                        emission=(0, 0, 0), emission_strength=1.0, alpha=1.0):
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    bsdf = nodes.get("Principled BSDF")

    bsdf.inputs["Base Color"].default_value = (*base_color, 1.0)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness

    if any(c > 0 for c in emission):
        bsdf.inputs["Emission Color"].default_value = (*emission, 1.0)
        bsdf.inputs["Emission Strength"].default_value = emission_strength

    if alpha < 1.0:
        mat.use_backface_culling = False
        bsdf.inputs["Alpha"].default_value = alpha
        # Transmission gives a more physically-correct glass look
        bsdf.inputs["Transmission Weight"].default_value = 1.0 - alpha
        bsdf.inputs["IOR"].default_value = 1.45

    return mat

def add_cube(name, location, scale, material):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=location)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = scale
    obj.data.materials.append(material)
    return obj

def add_sphere(name, location, radius_scale, material, segments=64, rings=32):
    bpy.ops.mesh.primitive_uv_sphere_add(
        radius=1.0, segments=segments, ring_count=rings, location=location)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = radius_scale
    bpy.ops.object.shade_smooth()
    obj.data.materials.append(material)
    return obj

# ---------------------------------------------------------------------------
# Scene construction (mirrors samples/render_test/src/main.cpp exactly)
# ---------------------------------------------------------------------------

def build_scene():
    clear_scene()

    # ===== Camera =====
    # Engine: position (0, 6, 14), looking at (0, 1, 0), FOV 55 (vertical), near 0.1, far 200
    cam_data = bpy.data.cameras.new(name="Camera")
    cam_data.lens_unit = 'FOV'
    cam_data.angle = math.radians(55)  # Vertical FOV
    cam_data.sensor_fit = 'VERTICAL'
    cam_data.clip_start = 0.1
    cam_data.clip_end = 200.0

    cam_obj = bpy.data.objects.new("Camera", cam_data)
    bpy.context.collection.objects.link(cam_obj)
    bpy.context.scene.camera = cam_obj

    cam_pos = Vector(e2b_pos(0, 6, 14))
    look_at = Vector(e2b_pos(0, 1, 0))
    cam_obj.location = cam_pos
    direction = look_at - cam_pos
    cam_obj.rotation_euler = direction.to_track_quat('-Z', 'Y').to_euler()

    # ===== Sun Light =====
    # Engine: direction (-0.4, -1.0, -0.3), color (1.0, 0.95, 0.9), intensity 2.0, shadows on
    sun_data = bpy.data.lights.new(name="Sun", type='SUN')
    sun_data.color = (1.0, 0.95, 0.9)
    sun_data.energy = 2.0
    sun_data.use_shadow = True
    sun_data.angle = math.radians(0.526)  # Sun angular size for soft shadows

    sun_obj = bpy.data.objects.new("Sun", sun_data)
    bpy.context.collection.objects.link(sun_obj)
    sun_dir = Vector(e2b_dir(-0.4, -1.0, -0.3)).normalized()
    sun_obj.rotation_euler = sun_dir.to_track_quat('-Z', 'Y').to_euler()

    # ===== Fill Light =====
    # Engine: direction (0.5, -0.3, 0.5), color (0.6, 0.7, 1.0), intensity 0.3, no shadows
    fill_data = bpy.data.lights.new(name="Fill", type='SUN')
    fill_data.color = (0.6, 0.7, 1.0)
    fill_data.energy = 0.3
    fill_data.use_shadow = False

    fill_obj = bpy.data.objects.new("Fill", fill_data)
    bpy.context.collection.objects.link(fill_obj)
    fill_dir = Vector(e2b_dir(0.5, -0.3, 0.5)).normalized()
    fill_obj.rotation_euler = fill_dir.to_track_quat('-Z', 'Y').to_euler()

    # ===== Ground Plane =====
    # Engine: position (0,0,0), scale (20, 0.1, 20), grey matte
    ground_mat = create_pbr_material("Ground", (0.5, 0.5, 0.52), roughness=0.95)
    add_cube("Ground", e2b_pos(0, 0, 0), e2b_scale(20, 0.1, 20), ground_mat)

    # ===== 5x5 PBR Sphere Grid =====
    # Metallic 0->1 across X, roughness 0.1->1.0 across Z
    GRID = 5
    SPACING = 2.0
    START = -(GRID - 1) * SPACING * 0.5

    for ix in range(GRID):
        for iz in range(GRID):
            metallic = ix / (GRID - 1)
            roughness = 0.1 + 0.9 * iz / (GRID - 1)

            if metallic > 0.3:
                albedo = (1.0, 0.86, 0.57)   # Gold
            else:
                albedo = (0.9, 0.1, 0.1)     # Red dielectric

            mat = create_pbr_material(
                f"PBR_{ix}_{iz}", albedo, metallic=metallic, roughness=roughness)

            ex = START + ix * SPACING
            ez = START + iz * SPACING
            # Engine: position (ex, 1.0, ez), scale (0.8, 0.8, 0.8)
            add_sphere(f"PBRSphere_{ix}_{iz}",
                       e2b_pos(ex, 1.0, ez),
                       e2b_scale(0.8, 0.8, 0.8), mat)

    # ===== Shadow Casters (tall cubes) =====
    # Engine: albedo (0.3, 0.3, 0.35), roughness 0.6
    shadow_mat = create_pbr_material("ShadowCube", (0.3, 0.3, 0.35), roughness=0.6)

    # Engine: position (6, 2, -2), scale (1, 4, 1)
    add_cube("ShadowCube_0", e2b_pos(6, 2, -2), e2b_scale(1, 4, 1), shadow_mat)
    # Engine: position (-6, 2, 1), scale (1, 4, 1)
    add_cube("ShadowCube_1", e2b_pos(-6, 2, 1), e2b_scale(1, 4, 1), shadow_mat)

    # ===== Emissive Sphere (bloom test) =====
    # Engine: albedo (1, 0.3, 0.1), emissive (8, 2, 0.5), roughness 0.3
    emissive_mat = create_pbr_material(
        "Emissive", (1.0, 0.3, 0.1), roughness=0.3,
        emission=(8.0, 2.0, 0.5), emission_strength=1.0)
    # Engine: position (6, 1.5, 2), scale (1, 1, 1)
    add_sphere("EmissiveSphere", e2b_pos(6, 1.5, 2), e2b_scale(1, 1, 1), emissive_mat)

    # ===== SSAO Corner (concave crevice) =====
    # Engine: albedo (0.7, 0.7, 0.72), roughness 0.9
    ssao_mat = create_pbr_material("SSAOCorner", (0.7, 0.7, 0.72), roughness=0.9)

    # Large cube: position (-6, 1.5, -3), scale (3, 3, 3)
    add_cube("SSAOCubeLarge", e2b_pos(-6, 1.5, -3), e2b_scale(3, 3, 3), ssao_mat)
    # Small nested cube: position (-4.8, 0.4, -1.8), scale (0.8, 0.8, 0.8)
    add_cube("SSAOCubeSmall", e2b_pos(-4.8, 0.4, -1.8), e2b_scale(0.8, 0.8, 0.8), ssao_mat)

    # ===== Glass Sphere (transparency test) =====
    # Engine: albedo (0.6, 0.8, 1.0, 0.35), roughness 0.1, transparent
    glass_mat = create_pbr_material(
        "Glass", (0.6, 0.8, 1.0), roughness=0.1, alpha=0.35)
    # Engine: position (3, 1.2, 5), scale (1.2, 1.2, 1.2)
    add_sphere("GlassSphere", e2b_pos(3, 1.2, 5), e2b_scale(1.2, 1.2, 1.2), glass_mat)

# ---------------------------------------------------------------------------
# Render settings
# ---------------------------------------------------------------------------

def configure_render():
    scene = bpy.context.scene

    # --- Cycles ---
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 512
    scene.cycles.use_denoising = True
    scene.cycles.denoiser = 'OPENIMAGEDENOISE'

    # Prefer GPU if available
    prefs = bpy.context.preferences.addons.get('cycles')
    if prefs:
        cprefs = prefs.preferences
        cprefs.refresh_devices()
        # Try CUDA/OptiX/HIP
        for compute_type in ['OPTIX', 'CUDA', 'HIP', 'NONE']:
            try:
                cprefs.compute_device_type = compute_type
                cprefs.refresh_devices()
                for device in cprefs.devices:
                    device.use = True
                if compute_type != 'NONE':
                    scene.cycles.device = 'GPU'
                break
            except Exception:
                continue

    # --- Resolution (match engine default 1280x720) ---
    scene.render.resolution_x = 1280
    scene.render.resolution_y = 720
    scene.render.resolution_percentage = 100

    # --- Output ---
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_depth = '8'

    # --- World background ---
    # Engine clear_color: 0x1a1a2eFF = sRGB(26, 26, 46)
    # Convert to linear: sRGB_to_linear(x/255)
    def srgb_to_linear(c):
        c = c / 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4

    bg_r = srgb_to_linear(0x1a)
    bg_g = srgb_to_linear(0x1a)
    bg_b = srgb_to_linear(0x2e)

    world = bpy.data.worlds.get("World")
    if not world:
        world = bpy.data.worlds.new("World")
    scene.world = world
    world.use_nodes = True
    bg_node = world.node_tree.nodes.get("Background")
    if bg_node:
        bg_node.inputs["Color"].default_value = (bg_r, bg_g, bg_b, 1.0)
        bg_node.inputs["Strength"].default_value = 1.0

    # --- Color management ---
    # AgX is closest to ACES among Blender's built-in transforms
    try:
        scene.view_settings.view_transform = 'AgX'
    except TypeError:
        scene.view_settings.view_transform = 'Filmic'  # Fallback for older Blender
    scene.view_settings.look = 'None'
    scene.view_settings.exposure = 0.0
    scene.view_settings.gamma = 1.0

    # --- Compositor: Bloom (Glare node) ---
    # Engine bloom: threshold 1.5, intensity 0.15
    try:
        scene.use_nodes = True
        tree = scene.node_tree
        if tree is None:
            raise AttributeError("node_tree is None")

        # Clear existing nodes
        for node in tree.nodes:
            tree.nodes.remove(node)

        # Create nodes
        rl_node = tree.nodes.new('CompositorNodeRLayers')
        rl_node.location = (0, 0)

        glare_node = tree.nodes.new('CompositorNodeGlare')
        glare_node.location = (300, 0)
        glare_node.glare_type = 'BLOOM'
        glare_node.threshold = 1.5
        glare_node.mix = 0.15      # Maps to bloom intensity
        glare_node.quality = 'HIGH'

        comp_node = tree.nodes.new('CompositorNodeComposite')
        comp_node.location = (600, 0)

        # Link: Render Layers -> Glare -> Composite
        tree.links.new(rl_node.outputs["Image"], glare_node.inputs["Image"])
        tree.links.new(glare_node.outputs["Image"], comp_node.inputs["Image"])
        print("Compositor bloom configured")
    except (AttributeError, Exception) as e:
        print(f"Compositor bloom skipped (Blender API change): {e}")
        # Bloom will be absent from golden image — still a valid reference
        # for core PBR, shadows, AO, and transparency

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    build_scene()
    configure_render()

    # Determine output directory (same directory as this script, or current dir)
    script_dir = os.path.dirname(os.path.abspath(__file__)) if "__file__" in dir() else os.getcwd()
    blend_path = os.path.join(script_dir, "golden_scene.blend")
    golden_path = os.path.join(script_dir, "golden.png")

    # Save .blend file
    bpy.ops.wm.save_as_mainfile(filepath=blend_path)
    print(f"Scene saved: {blend_path}")

    # Render unless --no-render is passed
    argv = sys.argv
    if "--" in argv:
        script_args = argv[argv.index("--") + 1:]
    else:
        script_args = []

    if "--no-render" not in script_args:
        bpy.context.scene.render.filepath = golden_path
        print(f"Rendering golden image to: {golden_path}")
        bpy.ops.render.render(write_still=True)
        print(f"Golden image saved: {golden_path}")
    else:
        print("Skipping render (--no-render)")
