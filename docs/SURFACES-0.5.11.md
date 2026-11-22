# Aperiodic materials — JSD 0.5.11

This release extends the public `rf::gfx::Material` API and both native OpenGL
shader paths. Rebuild source consumers against the new headers and libraries.
The JSD project identity remains `jsd.engine`, source version 0.5.11.

| SurfaceKind | Value | Recipe |
| --- | --- | --- |
| Regolith | 7 | Domain-warped mineral color, grit and micro relief |
| HullPanels | 8 | Staggered panels, seams, bolts, coating variation |
| InteriorPanels | 9 | Smaller pressure liner panels and fine wear |
| RobotArmor | 10 | Small armor plates, scuffs and exposed metal chips |
| DeckPlating | 11 | Access plates and antialiased grip pattern |
| FabricWeave | 12 | Seeded dye variation and filtered fine weave |
| Console | 13 | Animated emissive instrument bars |

Existing SurfaceKind values 0–6 retain their identities. The renderer and project
loader accept the expanded range. Recipes use deterministic integer cell hashes;
they do not sample or repeat a noise texture. Manufactured panel spacing remains
regular, with seeded row staggering and local tone variation.

```cpp
rf::gfx::Material m;
m.surface = rf::gfx::SurfaceKind::RobotArmor;
m.surfaceSeed = 1027;
m.textureScale = 1.0f;
m.surfaceUV = true;
// Supply mesh UVs measured in metres in a stable object-local basis.
```

`surfaceUV` is appended to Material and defaults to false. True uses mesh UVs
for material evaluation, including bump-gradient transfer through screen
partials. This supports world-batched animated meshes without consuming another
vertex attribute. Existing `surfaceWorld` behavior is retained when surfaceUV
is false. Mesh UVs used for base-color maps are also used by the procedural
recipe: use a separate material/mesh when a consumer needs different UV spaces.
Material batching includes surfaceUV so incompatible coordinate modes cannot
merge. Project material `engine.surfaceUV` is passed through by the runtime.

Set `Environment.atmosphere.time` each frame for Console emission animation.
Other recipes are time-stable. Derivative filters fade fine grains, grip and
weave at distance. `normalScale=0` skips expensive micro relief while retaining
coarse color/panel detail. `textureScale` sets the physical pattern scale.
The effect is raster procedural PBR; it does not add displacement geometry.
