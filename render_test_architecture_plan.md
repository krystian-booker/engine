# Plan: Close the Remaining Architectural Gap to the `render_test` Golden

## Summary
- Treat `golden.png` as the acceptance truth. Treat `create_golden_scene.py` as the authored scene spec, not the full rendering spec.
- Do not keep solving this with `render_test` value tuning. The Blender script gives the intended camera/material/light setup, but it does not encode Cycles' transport quality: multi-bounce diffuse GI, scene-derived reflections, and physically better transmission are missing from the current engine, so matching values alone cannot match the golden.
- Implement the missing engine features in this order:
  1. Wire screen-space reflections into the render pipeline so metals and glass see the actual scene.
  2. Wire diffuse light probes into PBR shading so the right side gets real indirect bounce instead of heuristic fill.
  3. After those features land, re-sync `render_test` scene values to the Blender script and remove compensation-only tuning where it diverges.

## Implementation Changes
### 1. Make the scene spec canonical again
- Freeze `samples/render_test/golden/create_golden_scene.py` and `golden.png` as immutable references.
- Use the Blender script as the canonical source for authored scene content: camera transform/FOV, object transforms, material albedo/metallic/roughness/transmission intent, light transforms/colors/intensities, and compositor intent.
- Update `samples/render_test/src/main.cpp` only after the engine features land:
  - revert temporary compensation-only overrides back toward the script-authored scene;
  - keep only engine-specific toggles that are not part of authored content, such as enabling SSR / probes / refraction.
- Rule: if a scene value exists only to compensate for a missing engine feature, remove it once that feature is implemented.

### 2. Integrate SSR end-to-end in the pipeline
- Reuse the existing SSR system in `engine/render/src/ssr.cpp`; do not create a second reflection path.
- Complete the missing pipeline wiring:
  - add SSR config storage to `RenderPipelineConfig`;
  - initialize, resize, and shut down the SSR system from `RenderPipeline`;
  - execute SSR after `MainOpaque` and before the opaque copy used for refraction, so transparent refraction sees reflected opaque content.
- Use a minimal first integration:
  - trace + composite only;
  - disable temporal resolve and Hi-Z for the first pass;
  - keep SSR view IDs explicit and non-colliding in `RenderView`.
- Fix the current SSR implementation gaps:
  - create/bind fullscreen quad geometry inside `SSRSystem` the same way SSAO/PostProcess already do;
  - ensure all SSR passes bind a framebuffer or target view explicitly.
- Supply the missing roughness input without expanding the GBuffer footprint:
  - pack material roughness into the existing GBuffer alpha;
  - keep world normals in RGB exactly as today;
  - update the GBuffer shader and the GBuffer submission path so material PBR params are uploaded for that pass.
- Update SSR shaders to read roughness from the packed source consistently.
- Default SSR settings for `render_test`:
  - full resolution;
  - intensity `1.0`;
  - roughness threshold `0.75`;
  - edge fade tuned conservatively to avoid obvious border artifacts.

### 3. Integrate diffuse GI through light probes
- Use the existing light-probe system as the engine's first real diffuse GI path; do not add more fake bounce lights.
- Wire probes into the renderer and PBR shader:
  - upload active probe-volume data each frame;
  - bind probe textures/uniforms during opaque shading;
  - sample probe irradiance in the PBR fragment shader as a separate diffuse-indirect term.
- Keep hemisphere ambient as a fallback only:
  - when no valid probe data exists, current hemisphere behavior stays active;
  - when probe data exists, hemisphere ambient is reduced to a small fallback floor instead of being the main GI source.
- For `render_test`, create one probe volume covering the full test set: sphere grid, cubes, emissive sphere, glass sphere, and nearby ground.
- Bake probes from actual scene content, not hand-authored coefficients:
  - include direct lights, emissive surfaces, and occlusion;
  - store irradiance in the existing SH-based probe representation;
  - expose bake/update as an engine-side operation so this becomes a reusable feature, not a sample-only hack.
- Probe defaults for this sample:
  - low probe count, biased toward the right/emissive side only if the system supports non-uniform density;
  - otherwise a uniform grid dense enough to resolve the emissive bounce lobe around the right cube and metallic row.

### 4. Use refraction as a physically better transmission path, not just a blend trick
- Keep the opaque-copy refraction path already added.
- After SSR lands, allow transmissive materials to pick up:
  - the reflected opaque scene through SSR on glancing angles;
  - refracted opaque scene through the existing opaque-copy path on head-on angles.
- Keep the current Fresnel-based split, but validate it against the golden once SSR is live before doing any further material tuning.

## Test Plan
- Headless regression loop remains the acceptance mechanism:
  - build;
  - run `render_test --headless --screenshot=... --screenshot-frame=60`;
  - compare against `golden.png` with `image_compare`.
- Milestone acceptance:
  - SSR integration alone should materially improve the right-side metallic/glass mismatch and lower RMSE from the current ~0.194 baseline;
  - probe GI integration should remove the remaining structural "too dark on the right / fake fill elsewhere" error and bring RMSE to `<= 0.10`.
- Add focused engine tests:
  - GBuffer roughness packing test: roughness survives the GBuffer path without changing SSAO normals.
  - Pipeline integration test: SSR pass runs only when enabled and composites into HDR before transparent/refraction.
  - Probe sampling test: PBR shading changes when valid probe data is bound and falls back cleanly when none is present.
- Visual checks after the final sync:
  - metallic row reflects scene content instead of only the constant cubemap;
  - glass sphere has visible reflected edges and refracted background;
  - right-side ground and objects receive warm indirect lift without flattening the whole frame;
  - shadow-side pixels stay readable without global fake fill.

## Assumptions And Defaults
- `golden.png` is the permanent truth for this sample.
- `create_golden_scene.py` is the authored-scene reference, but it is not sufficient by itself to guarantee a match because Blender/Cycles adds transport quality that our engine still lacks.
- The correct long-term workflow is:
  1. authored scene values come from the Blender script;
  2. engine quality comes from SSR + probe GI + refraction;
  3. `render_test` stops carrying compensation-only tuning once those features exist.
- If probe baking infrastructure proves too incomplete to finish in one pass, the implementation order still stays the same: ship SSR first, then light-probe GI next. Do not spend more time on ad hoc sample-value tuning before those two engine features are in place.
