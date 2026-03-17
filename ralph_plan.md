# Plan: Match Render Test to Golden Image (RMSE ≤ 0.10 / ≥90% match)

## Context

The render_test sample renders a deterministic PBR scene. A Blender Cycles script generates the golden reference. We need to close the gap to ≥90% similarity (RMSE ≤ 0.10). This plan is for autonomous iterative execution via Ralph Loop.

**Constraints**:
- NEVER modify `create_golden_scene.py` or regenerate `golden.png`
- Only modify `samples/render_test/src/main.cpp` and engine render pipeline code
- When there is an architectural gap in the render pipeline, fix it properly — don't fake results

## Architectural Fixes Already Implemented

The following architectural changes are in place and should NOT be reverted:

### 1. Hemisphere Ambient (GI Approximation)
- **Files**: `uniforms.sh`, `lighting.sh`, `fs_pbr.sc`, `bgfx_renderer.cpp`, `renderer.hpp`
- Per-pixel hemisphere ambient blending ground/sky colors based on surface normal Y
- Replaces the old "Bounce" directional light hack
- Configured via `renderer->set_hemisphere_ambient(ground_rgb, shadow_min, sky_rgb)`

### 2. Screen-Space Refraction
- **Files**: `render_target.hpp` (OpaqueCopy view), `types.hpp` (ior/transmission), `render_pipeline.hpp/cpp` (opaque copy RT + blit), `bgfx_renderer.cpp` (uniforms + texture), `fs_pbr.sc` (refraction shader)
- Copies opaque HDR scene before transparent pass
- Transparent materials with `transmission > 0` sample refracted background via screen UVs
- Fresnel-based blend between refracted background and surface lighting

### 3. Split Ambient Shadow
- **File**: `fs_pbr.sc`
- Diffuse IBL uses `mix(shadow_min, 1.0, shadowFactor)` (configurable via hemisphere ground.w)
- Specular IBL uses `mix(0.7, 1.0, shadowFactor)` (metallic reflections dimmed less)

### 4. IBL / Emissive Calibration
- IBL intensity kept at 2.0 (provides needed ambient fill; the bright fallback cubemap approximates GI)
- Shadow_min set to 0.0: IBL diffuse is fully shadow-tracked, hemisphere provides unconditional fill
- Hemisphere NOT multiplied by SSAO: far-field irradiance isn't affected by local screen-space occlusion
- Emissive material reduced from {8, 2, 0.5} to {3, 0.75, 0.18} (was overshooting golden)
- Emissive point light increased from 5.0/12.0 to 15.0/20.0 (redistributes light to surrounding surfaces)

### Current RMSE: 0.1947 (down from original 0.216)

### Error Analysis (at RMSE 0.1947)
- 71% of error from too-DARK pixels, 29% from too-bright
- Right side (emissive area) contributes 41% of total MSE with RMSE 0.34
- These are structural GI gaps: Blender's path tracing illuminates surfaces via indirect bounces
- Parameter tuning has reached diminishing returns — remaining gap requires SSGI or light probes

### Additional Changes (Ralph Loop Iterations)
- Clear color calibrated to match golden's navy background (0x080A1CFF)
- Scene-appropriate dark IBL cubemaps created (matching golden's dark environment)
- AgX saturation boost removed (1.3→1.0 — was over-saturating)
- Sun intensity reduced 2.0→1.7, exposure raised 0.27→0.32 (rebalances dark/bright ratio)
- Fill light warmed from cool blue to neutral warm, intensity 0.3→0.4
- GI bounce fill light added behind right cube
- Hemisphere SSAO decoupled (far-field irradiance fix)

## Iteration Protocol

Each iteration:

### Step 1: Build
```bash
cmake --build /home/krystian/repos/engine/out/build/Linux-Debug 2>&1 | tail -20
```

### Step 2: Capture Screenshot
```bash
cd /home/krystian/repos/engine/out/build/Linux-Debug/bin && \
  ./render_test --screenshot=/tmp/render_output.png --screenshot-frame=60
```

### Step 3: Compare
```bash
/home/krystian/repos/engine/out/build/Linux-Debug/bin/image_compare \
  /home/krystian/repos/engine/samples/render_test/golden/golden.png \
  /tmp/render_output.png \
  --threshold=0.10 \
  --diff=/tmp/render_diff.png
```

### Step 4: Evaluate
- Read RMSE from stdout
- View `/tmp/render_diff.png`, `/tmp/render_output.png`, and golden image visually
- Run region analysis:
```bash
python3 -c "
from PIL import Image
import numpy as np
img = np.array(Image.open('/tmp/render_diff.png'))[:,:,:3]
h, w = img.shape[:2]
for name, r in [('top-left', img[:h//2,:w//2]), ('top-right', img[:h//2,w//2:]),
                ('bottom-left', img[h//2:,:w//2]), ('bottom-right', img[h//2:,w//2:])]:
    print(f'{name}: mean_diff={r.mean():.2f}, max_diff={r.max()}')
print(f'Overall mean: {img.mean():.2f}')
"
```

### Step 5: Check Exit
- RMSE ≤ 0.10 → **STOP, report success**
- RMSE regressed from prior iteration → **revert last change**, try next fix
- Iteration count > 50 → **STOP, report best RMSE**

### Step 6: Diagnose Architecture issues in our render pipeline or with the parameters in the render_test, rebuild, loop to Step 1

## Decision Rules

1. **Golden is truth**
2. **Fix architecture, don't fake** — when there's an architectural gap, implement proper support

## Critical Files

| File | Purpose |
|------|---------|
| `samples/render_test/src/main.cpp` | Scene config — parameter tuning goes here |
| `engine/render/shaders/fs_tonemap.sc` | Tone mapping (AgX, exposure, gamma) |
| `engine/render/shaders/fs_pbr.sc` | PBR shader — hemisphere ambient, refraction, split ambient shadow |
| `engine/render/src/bgfx_renderer.cpp` | Texture binding, uniforms, hemisphere/refraction upload |
| `engine/render/src/render_pipeline.cpp` | Opaque copy pass |
| `engine/render/include/engine/render/types.hpp` | MaterialData with ior, transmission |
| `engine/render/shaders/common/uniforms.sh` | Hemisphere + refraction uniforms |
| `engine/render/shaders/common/lighting.sh` | evaluateHemisphereAmbient function |

## Next Steps to Reach 0.10

## Verification

Success = `image_compare` reports RMSE ≤ 0.10 with exit code 0. Visual inspection should show:
- Overall brightness matches golden
- Colors are saturated, not washed out
- Shadows visible under spheres on ground plane
- Shadow-side pixels ~sRGB 60-80 (not pitch black)
- SSAO darkening in crevices (SSAO corner, sphere contacts)
- Glass ball shows refracted background, matching Cycles output
- Metallic spheres retain specular reflections in shadow areas
