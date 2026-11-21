# Minimal rendering / Lowest textures (engine 0.5.2)

Minimal is biased toward a longer visible world with less surface and screen-processing work. It keeps native output resolution and textured geometry.

| Setting / work | Previous Minimal | New Minimal |
| --- | --- | --- |
| View distance | 80 m | 320 m |
| World texture edge limit | 512 px (Low) | 64 px (Lowest) |
| Scene color attachments | Four | One HDR color + depth |
| Opaque depth prepass | Extra geometry pass | Omitted |
| Material processing | Full PBR/maps/triplanar blending | Coarse base color/emission, geometric normals, cheap diffuse/specular/local lighting |
| Procedural sky/cloud work | Could remain active through scene atmosphere | Static/HDR sky; procedural atmosphere/cloud integration skipped |
| Terrain chunks | 32 m | 64 m with progressively coarser distant grids |
| Unused screen-space lighting pass | Executed | Replaced by a color blit |
| Other quality tiers | Existing behavior | Existing behavior retained |

`GraphicsSettings::minimal()` / `graphicsPreset(GraphicsPreset::Minimal)` selects this configuration. No scene, showcase, project-format or editor code change is required. Engine consumers obtain it through the ordinary engine API; rebuild an installed engine SDK when updating it.

`TextureQuality::Lowest` is appended as value 4; the existing Low/Medium/High/Ultra values 0–3 and their 512/1024/2048/4096 pixel budgets retain their meanings. Settings files accept `texture_quality=lowest`, and the graphics laboratory accepts `--textures lowest`. Original pixels remain available so raising texture quality restores detail. Lowest suppresses normal/packed ORM texture sampling, macro-noise and clearcoat detail; average roughness/metalness values from the low-resolution ORM texture preserve the base material character. The compact renderer also uses one dominant projection axis instead of three texture projections. Screen images are identified on first use and retain at least a 2048 px budget (within source/driver limits) for readable HUD artwork; world control/sign textures remain in the world texture budget. HDR environment/sky sources are separate from that budget.

The compact path is selected only when Lowest textures and the compatible disabled effect combination are active. Enabling AA, shadows, AO, DoF, GI, reflections, edge fusion, bloom, auto exposure, motion blur, volumetric fog or terrain tessellation restores the full render targets and processing needed by that feature. A custom material shader also uses the full path. Transparent geometry, alpha cutouts, local lights, emission, terrain tint, particle depth intersections and tone mapping remain supported. Minimal uses an inexpensive hemisphere fill rather than environment-probe shading; it is intentionally a reduced-detail presentation.

`Statistics::lightweightRendering` and `sceneColorAttachments` expose the active path. Minimal omits normal/material/motion targets; `captureBuffers()` writes neutral placeholders for those files and labels them in `pipeline.txt`. Color, lighting and final-frame validation remain real readbacks. Higher modes retain the full buffer dumps.

Minimal terrain retains coarse near geometry and uses 4/2/1 divisions per 64 m chunk at increasing distances, with skirts joining LOD edges. Far relief is intentionally simplified. This changes rendering only; authored meshes, project terrain data, collision bodies and SI simulation are untouched.

The performance evidence shipped with this update uses Linux/Mesa software rendering at 960×540, LP_NUM_THREADS=4, 35 frames per scene, and medians after five warm-up frames. It compares the previous 80 m preset against the new 320 m preset, not equal-distance scenes. It is not a desktop GPU performance guarantee. See the package's VALIDATION.json for measurements and checks.

Previously saved INI values remain explicit overrides. Reselect Minimal in the graphics panel or load the updated `configs/minimal.ini` to adopt the new preset.
