# Reflections, lighting and scenery

## Photographic skies and local atmosphere in 0.5.28

Load an HDR equirectangular image with `jsd::loadHdr` and create its environment once. Vary the two image weights from 0 to 1 as gameplay changes. `imageBackgroundWeight` controls the visible dome. `imageLightingWeight` controls irradiance, specular fallback, and the sky captured by local reflection probes. `imageRotation` rotates those paths together in radians. Probes retain their last complete capture while significant lighting changes trigger a new capture over subsequent frames.

```cpp
const auto sky = jsd::loadHdr("assets/daylight.hdr");
const auto skyImage = renderer.createEnvironment(sky.width, sky.height, sky.rgb, 1.6f);

environment.image = skyImage;
environment.imageBackgroundWeight = daylight * clearWeather;
environment.imageLightingWeight = daylight * (.25f + .75f * clearWeather);
environment.aerial = {.0012f, 2.f, 44.f, {.89f, .94f, 1.f, 1.f}};
```

`AerialPerspective` sets the extinction coefficient per metre at a reference elevation. Air density drops by a factor of approximately 2.72 per scale height. Set the coefficient to zero to use legacy distance fog. The analytic sky and depth pass works with volumetric fog disabled. With volumetrics enabled, nearby rays include light and shadow scattering and continue to the view distance through analytic haze. Thin local volumes are sampled over their ray intersections, which helps narrow steam vents render at low step counts.

Projects can set `environment.imageBackgroundWeight`, `environment.imageLightingWeight`, `environment.imageBackgroundWeightKeys`, and `environment.imageLightingWeightKeys`. The `environment.aerialPerspective` object accepts `extinctionPerMetre`, `referenceHeightMetres`, `scaleHeightMetres`, and `albedo` as an RGBA array. Existing projects retain full image weights by default and can enable aerial perspective by setting a positive coefficient.

## Detailed materials in 0.5.27

`Material::detail` specifies independent detail cells per metre and weights for color, relief, and roughness. The renderer filters the detail by pixel footprint. Set `surfaceWorld=false` for object-space detail on moving geometry or `surfaceUV=true` with metre-scaled mesh UVs. The lowest texture tier skips the extra detail.

The alpha channel of a packed ORM texture can hold water retention. White receives the full wetness value. Black receives zero at `wetnessVariation=1`. Existing materials default to uniform wetness and existing RGB ORM maps fill the entire film.

```cpp
rf::gfx::Material asphalt;
asphalt.detail = {43.f, .23f, .31f, .18f};
asphalt.wetness = rainAccumulation;
asphalt.wetnessVariation = .83f;
asphalt.clearcoatRoughness = .17f;
asphalt.ormTexture = renderer.stageTexture(width, height, packedRgba, false, false);

// Request near mips before submitting the surface each frame.
renderer.requestTexture(asphalt.ormTexture, 512, distanceMetres);
```

Disable compression for linear normal and packed ORM maps to retain independent channels. The same coarse-to-fine stream supports RGB color with BC1 and alpha-bearing RGBA with compression disabled. Texture requests prioritize the closest use. Project capture and runtime preserve the detail weights, coordinate mapping, and water-retention weight with the material.

The reflection pass traces the base and top coat at their own roughness. Reflection reconstruction and history reject samples across depth, normal, and roughness changes. A directional sky fallback and cached local probes supply offscreen radiance. Custom full-material fragment shaders can write coat roughness to the alpha of the normal attachment and coat weight to the alpha of the base color attachment.

Sun cascades filter around the receiver plane and use world-scale occluder distances to estimate penumbra width. `Environment::sunAngularRadius` sets the apparent emitter radius in radians. Projects can set `environment.sunAngularRadius`. Nearby geometry adds depth-based contact shadows. Spotlight depth bias scales with the projected texel footprint, and finite light sources broaden direct highlights.

Version 0.5.10 adds cached spotlight scene shadows, finite-emitter highlights, indirect-only ambient occlusion and improved exposure/bloom. See [LIGHTING-0.5.10.md](LIGHTING-0.5.10.md) for API examples, shadow-atlas changes, profiling and limits. The feature comparison below describes the original 0.5.8 foundation; the 0.5.10 additions remain custom raster and screen-space techniques.

This release extends the custom OpenGL engine shared by Windows and Linux. The Unreal Engine feature names in the request are reference targets; this package does not integrate Unreal Engine or advertise its systems as implemented.

| Requested feature | Implementation in this update | Scope and remaining differences |
| --- | --- | --- |
| MegaLights / stochastic direct lighting | Receiver-dependent importance sampling of tiled local lights, a selectable BRDF sample budget, screen-space contact shadows, spatial filtering and reprojected direct-light history | Rasterized scene depth supplies local visibility. There is no hardware ray tracing, full area-light visibility, or ReSTIR reservoir reuse. Candidate weighting still scales with the number of tiled lights (128 per tile, 1,024 submitted lights per frame). |
| Lumen / Lumen Lite | `Hybrid` combines scene-rasterized radiance/irradiance probes with screen-space diffuse gathering. `ProbeLite` omits that screen-space diffuse pass. | This is a bounded, periodically updated radiance approximation, not Lumen's distance fields, surface-card cache or multibounce solver. |
| Better reflections | Roughness-dependent screen-space tracing and temporally resolved radiance, combined with GGX-filtered scene captures and box projection | Two box-projected probes blend across shared coverage, with up to eight cached. The scene environment fills gaps in coverage. |
| Virtual Shadow Maps | Four texel-stabilized sun cascades in an atlas, overlap blending, blocker-dependent filter width and local screen contact shadows | These are cascaded shadow maps. Sparse virtual pages, residency feedback, page invalidation and local-light VSMs are not implemented. |
| Screen space + probes / fog scattering / Smolder-style volumetrics | Bounded, advected procedural density, local-light and sun scattering, emission, Beer–Lambert transmittance, half-resolution integration and depth-aware upsampling | Up to eight volumes and twelve volumetric lights. There is no Smolder integration, fluid simulation or multiple scattering. Opaque depth terminates the integration. |
| Substrate modular shading | Composable base, dielectric coat, wetness, dust, sheen and anisotropic GGX response; layer values are stored in projects | A fixed material stack, not Substrate's arbitrary closure graph or adaptive GBuffer. |
| Mesh terrain / GPU PCG | Existing streamed mesh terrain, plus seeded GPU instance placement, sampled terrain heights, slope rejection, exclusion regions and wind | Decorative placement executes in the vertex shader; height samples are uploaded from the shared terrain function. This is not a compute graph or GPU mesh-generation framework. |
| Control Rig dynamics / strand hair | Existing surface-aware rover suspension plus pinned guide-chain dynamics, scene collision queries and tapered strand ribbons. A flexible service lead demonstrates secondary motion. | Supports native guide-hair/cable use. No full-body Control Rig graph, groom editor, dense hair simulation, hair self-collision or dedicated strand-hair renderer is included. |
| Nanite | Opt-in resident mesh LODs generated by vertex clustering and selected by estimated projected error, used on detailed rock scenery | This is conventional LOD. Nanite's virtual geometry, meshlet hierarchy, streaming and software rasterizer are not implemented. |

Epic's technical references describe the larger systems: [MegaLights](https://dev.epicgames.com/documentation/en-us/unreal-engine/megalights-in-unreal-engine), [Lumen](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-technical-details-in-unreal-engine), [Virtual Shadow Maps](https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine) and [Nanite](https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-in-unreal-engine). They are not bundled dependencies.

## Use the new paths

```cpp
auto settings = rf::gfx::GraphicsSettings::high();
settings.globalIllumination = rf::gfx::GlobalIllumination::Hybrid;
settings.stochasticLighting = true;
settings.directLightSamples = 8; // accepted range: 1–32
renderer.setSettings(settings);

renderer.begin(camera, environment);
rf::gfx::ReflectionProbe probe;
probe.id = 1;
probe.position = {0, 3.7f, 7};
probe.halfExtent = {28, 16, 43};
probe.refreshFrames = 36;
renderer.reflectionProbe(probe);
// Submit meshes, lights and volumes, then renderer.end().
```

The `C` graphics overlay exposes `hybrid-probes`, `probe-lite`, stochastic-light enablement and sample count. The showcase High preset uses the new hybrid path and High reflection tracing. Existing minimal and deterministic paths remain selectable. INI settings also accept `global_illumination=probe-lite`, `stochastic_lighting=0` and `direct_light_samples=8`.

As of 0.5.9, every probe capture, including the first capture and revisions, renders at most one face per frame. After six faces, filtering runs in batches of at most six face/mip jobs per frame. A complete result is published after roughly fifteen rendered frames; the environment supplies reflections during initial warm-up and an existing valid capture stays visible during periodic refresh. `refreshFrames=0` holds a static capture. Increment `revision` after editing captured scenery; `resetHistory()` invalidates captures after a timeline reset. Captures include opaque submitted geometry and local lighting; transparent objects, particle sprites and fog are not captured. World/local procedural materials from 0.5.7 remain supported.

In 0.5.26 the capture and filter budgets are shared by both selected probes. The composite weights each probe by fragment position, volume coverage, and readiness. Fresh results fade in over 14 rendered frames. Call `resetHistory(false)` for camera cuts within the same scene to retain completed probes.

Reflections and screen-space indirect illumination trace at half internal resolution and use separate color histories. Motion vectors reproject history and previous depth and normals reject disocclusions. The resolve clamps reprojected color to compatible neighboring samples. A depth- and normal-aware composite applies the result at full internal resolution. With GPU profiling on, `reflectionMilliseconds`, `indirectMilliseconds`, and `probeMilliseconds` report the asynchronous timings when available. Reflections and indirect timings include trace and temporal resolve. Probe timings include submitted capture and filter work.

Set `CameraOptics::focusDistanceMetres` for a camera-follow or scripted focus plane. The override leaves renderer settings and temporal history intact.

## Streamed scenery

Create detail geometry with `Renderer::stageMesh`. Request it before submission with `requestMesh` and draw its coarse representation while it uploads. The renderer orders requests by distance and uploads up to 3 MiB of triangle-list source data per frame. It targets 96 MiB of resident detail geometry and evicts detail after 160 unused frames. The game retains source geometry and collision data on the CPU.

Use `stageTexture` for opaque RGB layers. It builds a complete mip chain, compresses opaque color mips to BC1 on capable drivers, and uploads a coarse mip tail immediately. `requestTexture` requests a finer edge size subject to the renderer texture-quality cap. Finer levels upload under a 512 KiB frame target. A mip larger than that target can upload on its own. Unsupported formats use RGBA and the same residency rules. Statistics report compressed bytes and per-frame geometry and texture uploads.

The full material path now has six color attachments, including albedo/coat and direct-light radiance used to avoid blurring generated surface detail during denoising. The minimal path retains one. Custom full-material fragment shaders need location 4 for base RGB/coat and location 5 for local direct radiance to participate in the new buffer contract. Antialiasing, render scale and quality controls remain available. First captures, shadow rendering and material sampling have real GPU costs; no target-hardware FPS improvement is claimed.

## Materials and volumes

`Material` appends four weights: `wetness`, `dust`, `sheen` (0–1), and `anisotropy` (−0.95–0.95). The base response is attenuated under the dielectric coat. Wetness darkens the base and reduces roughness; upward-facing dust increases roughness and reduces metallic response. Anisotropy follows valid mesh UV flow, with a geometric fallback. Existing aggregate fields keep their order.

```cpp
rf::gfx::FogVolume plume;
plume.position = {119, 15.5f, 123};
plume.halfExtent = {3, 2.8f, 3};
plume.density = .045f;
plume.velocity = {.09f, .22f, 0};
plume.albedo = {.48f, .39f, .25f, 1};
// Nonzero emission supports a luminous/smoldering volume.
renderer.fogVolume(plume);
```

Volumes integrate up to the visible opaque surface, with a 160 m integration limit. Noise is evaluated from world coordinates and advected by velocity. Volumetric local-light visibility is approximate; the sun uses the shadow atlas. Screen contact shadows cannot see hidden or offscreen occluders.

Projects preserve material layers under `Surface.engine`. Optional `environment.reflectionProbes` and `environment.fogVolumes` arrays preserve bounded lighting definitions; new showcase exports include them. Existing exported projects keep their authored scene data. GPU scatter is baked to ordinary placements for project capture. Guide-chain animation remains a native component; project exports retain its initial mesh rather than a groom/rig simulation.

## Scenery and secondary motion

`Renderer::createLodMesh(vertices, pixelError)` is intended for dense scenery whose fine UV seams can tolerate clustering. Near views retain the original mesh. As of 0.5.9, a positional edge check rejects generated levels that open a previously closed surface. The error value estimates vertex displacement on screen; it is not a guarantee of UV-seam or silhouette preservation. Use authored LODs or the ordinary `createMesh` path for sensitive assets. All generated levels are resident; no virtual geometry is streamed.

`Renderer::scatter(ProceduralScatter)` reuses a mesh and material. Candidate positions, yaw, scale, terrain slope tests and wind run on the GPU. Each submission accepts at most 32,768 candidates, two axis-aligned exclusion boxes, a finite height function and a revision for height-field edits. The engine caches up to 32 height fields and accepts eight scatter submissions per frame. Decorative ground cover has no automatic colliders. The showcase adds 5,000 seeded pebble/tuft candidates while keeping the rover lane and platform clear.

`jsd::StrandChain` in `jsd/strands.hpp` advances metre-based pinned guide points with acceleration, damping, length constraints and optional `PhysicsWorld` collision queries. `strandRibbon` converts a chain to tapered camera-facing triangles; use a double-sided material with sheen/anisotropy. The showcase's hanging service lead uses this secondary motion alongside the existing six-wheel suspension.

CSV profiles include capture faces, direct sample budget, shadow cascades, procedural candidates, LOD draws/triangle savings and fog-volume count. Version 0.5.9 appends `probe_filters`, recording additional filtering jobs. These support tuning on actual Windows/Linux hardware. Main/shadow draw and triangle counters exclude offscreen probe captures; capture-face and filter counts record that additional work separately.

Rebuild the engine, showcase and every native application/editor linked against it after applying. `Material`, `GraphicsSettings` and `Statistics` gained fields, so their binary layouts changed.
