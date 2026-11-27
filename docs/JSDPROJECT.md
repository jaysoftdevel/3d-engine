# Engine projects (.jsdproject), format 1

A `.jsdproject` manifest and its adjacent `<name>.assets/` folder form one portable, editable engine project. Move or archive them together. Resources are copied on Save/Save As and referenced relatively; no original authoring source is needed at playback. Manifests are readable JSON. Mesh and animation resources use native `.jsdmesh` and `.jsdclip` v1 binary data; these contain asset data, not executable prebuilds.

```json
{"format":"jsd-engine-project","version":1,"scene":{}}
```

`scene` contains the complete document, not only a model reference. Use the separate editor to author a populated document.

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

## Edit your own project

Use the separate editor and generic project APIs. Standalone player and sample-specific terrain generators have been removed from this SDK edition.

## Animation and simulation

Recorded animation is editable. Enable physics preview for chosen objects and bake it to author simulated motion. The solver fits primitive proxies, including dedicated collider nodes. Terrain sculpting changes the rendered surface and particle collision surface, not a rigid-body heightfield collider.

Transform fields offset a recorded relative track. Use Motion > Make animation editable to change individual samples; it preserves the currently offset path. Use Shape > Make geometry unique before topology edits to a shared instance. Materials remain shared unless you create and assign another material. Preview quality/complexity controls affect rendering work; full topology is retained in the project and evaluated for model export.

Model exchange remains OBJ/MTL, glTF 2.0 and GLB. Those formats cannot represent the entire engine project (terrain, engine effects/HUD, physics and application configuration); use `.jsdproject` to retain everything. Legacy `.jsdscene` files still load and can be resaved as `.jsdproject`.


Legacy terrain generator identifiers remain supported for previously authored projects. No sample scene or its assets are supplied.
