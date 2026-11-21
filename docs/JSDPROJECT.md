# Engine projects (.jsdproject), format 1

A `.jsdproject` manifest and its adjacent `<name>.assets/` folder form one portable, editable engine project. Move or archive them together. Resources are copied on Save/Save As and referenced relatively; no files from the original showcase source folder are needed at playback. Manifests are readable JSON. Mesh and animation resources use native `.jsdmesh` and `.jsdclip` v1 binary data; these contain asset data, not executable prebuilds.

```json
{"format":"jsd-engine-project","version":1,"scene":{}}
```

`scene` contains the complete document, not only a model reference. See the delivered `projects/sundog/sundog.jsdproject` and the editor examples for populated documents.

| Field | Contents |
| --- | --- |
| project | SI units, loop, default quality, entry camera, 120 Hz tick rate, physics-preview flag, rendering style and credits |
| objects | Stable IDs and parents, local TRS/quaternion, visibility/locking, mesh reference or editable polygon data, material assignment, modifiers, animation, physics, lights/cameras and component data |
| materials | PBR colors, textures, emission, mapping, transparency and original engine material controls |
| meshLibrary | Shared indexed meshes, preserving original normals, UVs and vertex colors |
| terrain | Procedural base, SI origin/extent, grid resolution, float sculpt deltas and vertex tint map |
| environment | Bundled HDR, image processing, rotation, fog, sun/sky/ground and atmosphere |
| duration / fps / gravity | Timeline length, export cadence and gravity in m/s² |

Object kinds: 0 mesh, 1 point light, 2 spot light, 3 camera, 4 group, 5 emitter, 6 audio, 7 screen overlay, 8 collision proxy. Components retain visibility/material/property tracks, particle settings, audio sources and HUD layout. Unknown component metadata is retained through save and undo. This format describes engine scene content; arbitrary native game/application code remains in its own source project.

The shared API is `<jsd/project.hpp>` (document, asset bundling, modeling/interchange), `<jsd/project_simulation.hpp>` (SI preview/baking), and `<jsd/project_runtime.hpp>` (renderer and project camera). Link `JSD::GameEngine`. The engine has no dependency on the editor, ImGui or ImGuizmo. The editor has its own CMake entry point and links this API from the sibling engine or an installed SDK.

## Edit and run SUNDOG

From the sibling `jsd-editor` folder:

```bash
./run.sh --project ../3d-engine/projects/sundog/sundog.jsdproject
# Or use File > Open engine showcase.
./run.sh --project ../3d-engine/projects/sundog/sundog.jsdproject --terrain
```

From the engine:

```bash
./run-project.sh                         # the complete editable showcase
./run-project.sh --project path/to/mine.jsdproject
./run-showcase.sh --project path/to/mine.jsdproject
# Deterministic offscreen capture through the same engine renderer:
./bin/jsd-project-player --project projects/sundog/sundog.jsdproject --offscreen --time 40 --frames 2 --capture frame.ppm --validate
```

The player supports C for the 70% opaque graphics overlay, Space pause, Tab to explore with mouse/WASD, Space/Ctrl up/down while exploring, R restart, F1 HUD, F11 fullscreen and Escape quit. `--quality minimal|low|medium|high|max`, `--no-ui`, `--no-particles`, `--no-audio`, `--once` and `--settings` are available. Launchers rebuild incrementally so source updates are used. `JSD_BUILD_PROJECT_PLAYER=OFF` disables the standalone player.

The original procedural `jsd-showcase` target remains optional and independent. `jsd-showcase --export-project path/to/sundog.jsdproject --offscreen --no-audio` regenerates a full project from its actual source geometry and a deterministic 120 Hz, 160-second mission run. It records source model parts, not a screenshot or a single merged model. The shipped project runs with the engine player without building this exporter target.

## Animation and simulation

SUNDOG's mission, physics-solved cargo motion, vehicle suspension/wheel rotation and camera movement are baked at 120 Hz into editable tracks. Visibility, material switches, light values, particle velocity/emission, titles, HUD and ambience are retained. The baked playback preserves the original mission; changing terrain does not automatically replan a rover route or recompute the recorded cargo fall. Collision proxies and source mass/drag metadata remain editable. To author new simulated motion, enable physics preview for chosen objects and bake it; the current project solver fits primitive proxies, including dedicated collider nodes. Terrain sculpting changes the rendered surface and particle collision surface; it does not synthesize a rigid-body heightfield collider. Edit/add the collision proxies for new rigid-body interaction.

Transform fields offset a recorded relative track. Use Motion > Make animation editable to change individual samples; it preserves the currently offset path. Use Shape > Make geometry unique before topology edits to a shared instance. Materials remain shared unless you create and assign another material. Preview quality/complexity controls affect rendering work; full topology is retained in the project and evaluated for model export.

Model exchange remains OBJ/MTL, glTF 2.0 and GLB. Those formats cannot represent the entire engine project (terrain, engine effects/HUD, physics and application configuration); use `.jsdproject` to retain everything. Legacy `.jsdscene` files still load and can be resaved as `.jsdproject`.

## Extended refinery tour

The complete showcase now lasts 160 seconds, with continuous mountain/cave/refinery/gallery/return camera travel. `project.chapters` records navigation times. `environment.ambientIntensityKeys`, `sunIntensityKeys` and `fogDensityKeys` support step, linear and smooth interpolation for interior adaptation. The scene settings offset these recorded values. The **Mountain cave / Deep Refinery** group contains its editable geometry, lights, effects and floor collision proxies. New emitters can retain their configured floor using `emitter.useConfiguredFloor`; other emitters follow the project terrain as before.
