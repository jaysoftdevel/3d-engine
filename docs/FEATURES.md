# Implemented features and boundaries

## Graphics settings

| Control | Available behavior |
| --- | --- |
| Anti-aliasing | Off, FXAA, reference SMAA 1x, hardware MSAA 2/4/8 samples when supported, TAA |
| Texture quality | Low/Medium/High/Ultra cap dimensions at 512/1024/2048/4096; retained source allows reallocation; mipmaps |
| Anisotropy | 1 (off), 2, 4, 8, 16; driver limit is reported |
| Shadows | Off/Low/Medium/High/Ultra; single directional shadow map, 512–4096 resolution, PCF kernels |
| View distance | 32–800 world units; projection, distance culling, terrain streaming and distance fog |
| Ambient occlusion | SSAO with bilateral filtering; quality controls sampling |
| Depth of field | Depth-based blur with focus distance, aperture strength and maximum radius |
| Global illumination | Off, hemispherical ambient probe, screen-space one-bounce approximation; quality controls samples |
| Screen-space reflections | Depth ray marching, hit refinement, edge fading, roughness/Fresnel weighting |
| Edge fusion | Depth/normal-aware seam smoothing; independent of AA |
| Terrain tessellation | Quality-dependent subdivision; real GL tessellation stages on supported contexts; worker-built CPU mesh fallback on GL 3.3 |
| Render scale | 0.5–2.0 internal dimensions; full-resolution UI; Catmull–Rom reconstruction with optional sharpening |
| Temporal AA/reconstruction | Halton jitter, per-object and camera motion, neighborhood history clamping, depth disocclusion, reactive surfaces |
| Bloom | HDR threshold extraction, downsample pyramid, separable blur and multilevel combination |
| Exposure | Manual EV compensation, automatic log-luminance mip reduction and temporal adaptation |
| Tone mapping | ACES approximation, Reinhard, neutral curve; SDR output |
| Color grading | Contrast, saturation, warm/cool balance, vignette |
| Motion blur | Depth-aware motion-vector sampling; quality-dependent sample count and strength |
| Volumetric fog | Ray-marched exponential height density, directional scattering, shadow-map visibility; quality-dependent steps |
| Atmosphere/clouds | Analytic sky plus animated 3D-noise ray-marched clouds, 8–32 view steps and sampled cloud self-shadowing |
| Profiling | CPU submission time, asynchronous GPU elapsed queries, draws/triangles/post passes/light overflow |

Quality selectors generally offer Off/Low/Medium/High/Ultra. `GraphicsSettings` stores requested values; `resolveSettings` reports effective values and adjustments. Presets and INI serialization cover all controls. High/Ultra opt into TAA, bloom, auto exposure and volumetric fog. Legacy keeps the additional effects disabled.

## Materials, lighting and geometry

PBR uses a GGX microfacet BRDF, metallic/roughness, albedo, tangent normal maps reconstructed from derivatives, packed occlusion/roughness/metallic and emissive maps. Albedo/emissive image sampling is sRGB; normal/ORM sampling is linear. Alpha masks, sorted alpha blending, additive surfaces and double-sided materials are supported. Transparency does not use order-independent storage.

An HDR equirectangular image is precomputed into GGX specular cubemap mips, diffuse irradiance and a BRDF integration LUT; the environment also draws the background. The reflection cubemap is 128 pixels per face; the sky retains the full-resolution source. Environment rotation applies to background and lighting. PDF-selected source mip levels reduce prefilter aliasing. An optional lighting highlight limit clips a separate downsampled lighting source while retaining full-range background radiance; use it when a separate sun supplies direct illumination. Point and spot lighting use CPU screen-tile bins uploaded as texture buffers: 1,024 lights maximum, 128 entries per 16x16 tile, with overflow reported. Local lights are unshadowed; the sun uses one map. No cascaded/local-light shadow atlas, baked lightmap pipeline or probe blending volumes exist in this release.

Materials additionally support triplanar projection, world-scale macro variation, clearcoat, an occlusion strength and UV-weighted procedural wind displacement. Wind displacement is marked reactive by the caller; it has no cloth/physics simulation. Geometry helpers provide beveled boxes, UV spheres, toruses, seeded rocks, curved tubes and transformed mesh merges.

GPU mesh upload deduplicates complete position/normal/color/UV vertices when indexed storage is smaller. Geometry and seams are preserved; this is not simplification. `updateMesh` uses an unindexed streaming path and retains its VAO/VBO, avoiding per-frame hashing and GL object churn for CPU-skinned/deforming assets. Inverse-transpose normal matrices are prepared once per instance, depth-only shadows use a minimal vertex shader, and static geometry skips wind trigonometry. Draw transforms, bounds, material keys and transparent ordering are prepared once per frame and shared across passes; uniform locations and scratch buffers are retained. Light bins use flat bounded prefix ranges instead of per-tile vector allocations.

Full-size presentation bypasses unnecessary cubic reconstruction. Bloom combines adjacent Gaussian taps through bilinear filtering; screen-space GI skips zero-contribution samples before additional fetches, and back-facing sunlight skips unused PCF queries. Quality settings, sampling budgets, mesh triangles and effect availability are preserved.

Draws support full matrices, quaternions via math helpers and instancing of matching opaque mesh/material groups. Supply stable, unique `Draw.objectId` values for rigid-object motion. ID zero receives camera motion only; mark deforming geometry reactive. CPU-skinned assets are automatically reactive. Call `resetHistory()` after teleports or scene discontinuities; large camera changes and allocation/settings changes also reset history.

## Game runtime

| System | Available API |
| --- | --- |
| ECS | Typed component pools, generational handles, entity lifecycle, iteration |
| Scene graph | Parent/local/world transforms, cycle checks |
| Simulation | Fixed time step with accumulator and bounded catch-up, interpolation factor |
| Physics | Jolt 5.7.0 boxes/spheres/capsules, static/dynamic/sensor bodies, mass/friction/restitution, CCD, gravity, sleeping, impulses, velocity, ray casts, contact start/end events |
| Assets | cgltf glTF/GLB, triangle meshes, indices/attributes, images, material maps, hierarchy, skin/inverse bind, TRS linear/STEP/cubic animation; CPU skinning |
| Images | stb_image PNG/JPEG and common image formats; float HDR loader |
| Audio | WAV PCM16/float32 mono/stereo, resampling, looping, up to 128 voices, positional attenuation/stereo panning; SDL float callback or offline mixing |
| Input | Scancode actions, axes, edges, mouse deltas, focus-loss clearing |
| Particles | Legacy mesh emitter retained; new VfxEmitter dust/steam/sparks/sand/gravel, 120 Hz CPU simulation, rates/bursts, drag/wind/turbulence, lifetime/color/size evolution, height-surface collision, GPU-instanced soft billboards and mesh gravel |
| Cinematics | Hermite camera position/target tracks, smooth field of view, looping; textured UI and finite framebuffer diagnostics |
| LOD | Caller-provided distance ranges and hysteresis; no automatic simplification |
| Scheduling | Existing thread pool plus CPU task dependency graph with read/write hazards and timings |
| Frame timing | FrameRateCounter averages completed real frame durations; sample CSV profiling includes frame/CPU/GPU times |
| Development | File timestamp watcher, transactional material GLSL reload, public GPU mesh update |

The task graph orders CPU callbacks; it is not a Vulkan GPU resource graph or an asynchronous compute scheduler. Render resource access is single-threaded. ECS is single-threaded unless the application supplies synchronization. Physics uses Jolt workers internally; the wrapper's create/read/step calls are serialized by the application. Drain `contacts()` regularly. Physics limits are 8,192 bodies, 32,768 pairs and a 16 MiB temporary allocator; exhaustion raises an error. Audio device behavior depends on the host; attenuation/panning are not HRTF acoustics.

The loader accepts uncompressed triangle assets up to the documented code limits (128 MiB per file, 3 million expanded vertices per primitive). External resources must remain under the allowed asset root. Draco, meshopt, KTX2/Basis textures, morph targets, animation blending/state machines, material extensions, arbitrary glTF scene selection and GPU skinning are not implemented. Supply packed ORM maps; independent glTF occlusion textures and secondary UV sets are not imported. Shader reload replaces the standard mesh material program; custom shaders must retain its attribute/uniform/output contract and do not replace the tessellated terrain program.

## Particle boundaries

Simulation is CPU based, not GPU compute. Renderer accepts up to 131,072 billboard sprites per frame; individual VfxEmitters cap at 65,536 live particles. Sprites use copied scene depth for soft intersections and premultiplied HDR blending, rendered after opaque effects and before temporal reconstruction. This is not volumetric fluid smoke, order-independent transparency or multi-bounce particle lighting. Gravel collides with a caller-supplied height surface, not arbitrary Jolt bodies; rigid cargo is simulated separately by Jolt. Particle surface contacts do not form accumulating terrain deposits. Billboard pixels write a reactive mask; gravel supplies object motion IDs. See PARTICLES.md.

## Future backend and production work

`Capabilities.hardwareRayTracing`, `vulkanBackend`, `vendorUpscaling` and `frameGeneration` are false. No placeholder switch silently enables these. Vulkan/DX12 ray tracing, ReSTIR/path tracing, neural radiance caching, FSR/DLSS/XeSS SDK integration, mesh shaders, virtualized geometry/texturing, GPU-driven occlusion, GPU simulation/skinning, hair/cloth, robust character controller/navigation, multiplayer/rollback, streaming asset database, editor/cooker, console support, HDR10 display output and large-world precision remain future work. Current Linux OpenGL functionality is implemented and exercised; Windows portability has not been validated for this SDK.
