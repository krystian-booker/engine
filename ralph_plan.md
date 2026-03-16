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

### Step 6: Apply next parameter change, rebuild, loop to Step 1

## Parameter Sweep (Autonomous Calibration)

All architectural fixes are in place. The remaining work is parameter tuning. Change **one parameter per iteration** to enable attribution and clean reverts.

### Tunable Parameters

| Parameter | File | Current (Optimized) | Swept Range | Notes |
|-----------|------|---------------------|-------------|-------|
| exposure | `main.cpp` tonemap_config | **0.27** | 0.24 – 0.32 | Sweet spot, insensitive to small changes |
| IBL intensity | `main.cpp` set_ibl_intensity | **2.0** | 0.5 – 2.5 | 2.0 optimal; lower hurts ambient fill |
| hemisphere ground | `main.cpp` set_hemisphere_ambient | **{5.0, 4.0, 3.2}** | 0.45 – 8.0 | High HDR values needed for fill; biggest lever |
| hemisphere sky | `main.cpp` set_hemisphere_ambient | **{0.01, 0.01, 0.01}** | 0.01 – 0.30 | Near-zero to avoid brightening ground |
| hemisphere shadow min | `main.cpp` set_hemisphere_ambient | **0.0** | 0.0 – 0.5 | 0.0 optimal: IBL tracks shadows, hemisphere unconditional |
| bloom threshold | `main.cpp` bloom_config | **1.5** | 1.0 – 2.0 | Insensitive in this range |
| bloom intensity | `main.cpp` bloom_config | **0.12** | 0.08 – 0.16 | 0.12 optimal |
| specular ambient shadow | `fs_pbr.sc` | **0.7** | 0.5 – 0.7 | Insensitive |
| emissive material | `main.cpp` create_emissive_sphere | **{3.0, 0.75, 0.18}** | 1.0 – 8.0 | Reduced from 8.0 to match golden brightness |
| emissive point light | `main.cpp` create_emissive_sphere | **15.0 / range 20.0** | 5.0 – 30.0 | Increased to spread warm light |
| bounce light intensity | `main.cpp` create_lights | **0.5** | 0.3 – 0.5 | 0.5 optimal |
| refraction distortion | `bgfx_renderer.cpp` | **0.15** | 0.05 – 0.30 | Not swept (minor impact) |

### Sweep Strategy
1. Start with **exposure** — biggest single-parameter lever
2. Then **IBL intensity** — affects global ambient balance
3. Then **hemisphere ground/sky** — affects shadow fill
4. Then **bloom** and **specular ambient shadow** — fine tuning
5. After each pass through all parameters, do a second pass at finer steps if RMSE is close to target

## Decision Rules

1. **One logical change per iteration** — enables attribution and clean reverts
2. **Revert on regression** — if RMSE increases, undo before trying next fix
3. **Golden is truth** — only modify engine/render_test code, never the golden
4. **Diminishing returns** — if 3 consecutive iterations improve by < 0.003, skip to next parameter
5. **Fix architecture, don't fake** — when there's an architectural gap, implement proper support

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

Parameter tuning has converged at RMSE 0.2139. The remaining 0.114 gap requires architectural work:

1. **Proper IBL cubemap** — Replace the bright fallback cubemap with one matching the golden's dark environment (sRGB ~26, 26, 46). This eliminates phantom metallic reflections and reduces ground brightness. Expected impact: ~0.02 RMSE.

2. **Screen-space GI (SSGI)** — Add a screen-space global illumination pass that bounces light from bright surfaces to nearby dark surfaces. This would illuminate cube faces near the emissive sphere and fill in GI-occluded ground areas. Expected impact: ~0.03-0.05 RMSE.

3. **Multi-bounce AO** — Enhance SSAO with color-bleeding / multi-bounce approximation (like Jimenez 2016). This adds warm colored fill in occluded areas rather than just darkening. Expected impact: ~0.01 RMSE.

4. **Proper environment map for the scene** — Generate a low-res environment cubemap from the scene itself (either offline or at runtime) to provide correct indirect specular/diffuse. This replaces both the fallback IBL and the hemisphere approximation with a physically-based solution. Expected impact: ~0.03-0.05 RMSE.

## Verification

Success = `image_compare` reports RMSE ≤ 0.10 with exit code 0. Visual inspection should show:
- Overall brightness matches golden
- Colors are saturated, not washed out
- Shadows visible under spheres on ground plane
- Shadow-side pixels ~sRGB 60-80 (not pitch black)
- SSAO darkening in crevices (SSAO corner, sphere contacts)
- Glass ball shows refracted background, matching Cycles output
- Metallic spheres retain specular reflections in shadow areas
