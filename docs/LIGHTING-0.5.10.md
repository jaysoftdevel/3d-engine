# Cinematic lighting — 0.5.10

This release improves the custom Windows/Linux OpenGL renderer and recalibrates the native showcase. It uses scene shadows, finite-emitter shading and photographic exposure to improve depth, surface response and the distinction between sunlight, indirect fill and practical lamps.

## Direct lighting and shadows

Spotlights can now cast shadows from actual scene geometry, including blockers outside the camera view. Four persistent shadow maps share a depth atlas with the existing four sun cascades. The renderer selects important visible spotlight requests, retains their IDs and updates at most two maps per rendered frame. Unchanged lights and caster geometry reuse cached maps. Moving/deforming geometry, streamed terrain, light transforms and explicit revisions invalidate affected captures. Stable selections normally refresh within two frames. A newly selected light uses contact shadows while its map warms.

Shadow quality controls atlas resolution and filtering. High/Ultra use blocker-dependent soft filtering; lower tiers use a smaller fixed filter. Masked materials cast alpha-tested shadows; blended particles and translucent materials do not. The same selected maps occlude practical-light scattering in fog.

```cpp
auto lamp = rf::gfx::spotLightLumens(
    {0, 5, 0}, {0, -1, 0}, 3200, 18,
    .65f, 1.1f, rf::gfx::lightColorTemperature(4300));
lamp.sourceRadiusMetres = .25f;
lamp.castShadow = true;
lamp.shadowId = 42;       // Stable, unique nonzero ID for this light.
// Increment shadowRevision after changing external caster resources.
renderer.light(lamp);
```

Shadowed spots require an outer half-angle below 90 degrees and range above 0.1 m. Point lights retain contact shadows; omnidirectional shadow cubemaps are not implemented. Requests beyond the four selected maps retain their ordinary lighting/contact-shadow path. This is a bounded raster-shadow system, not MegaLights ray tracing or sparse Virtual Shadow Maps.

The atlas has dimensions `S × 2S`: four sun tiles in its lower half and four local-light tiles in its upper half, each `S/2 × S/2`. It uses the existing raw and comparison samplers, retaining compatibility with OpenGL 3.3's sixteen fragment texture units. The extra depth region adds memory, and updating a map adds geometry work. Driver texture limits reduce the effective shadow tier when needed. Custom fragment shaders that sample the sun atlas must adopt the new UV packing.

## Surface response and indirect light

`sourceRadiusMetres` now also broadens glossy highlights using a spherical representative-point approximation with a GGX energy correction. Diffuse lighting retains the light's centre direction. This approximates a finite emitter without additional per-light BRDF samples; it does not integrate a rectangular light or trace area-light visibility.

The GGX denominator retains narrow highlights at low roughness. Fresnel cosines are clamped to their valid range, and HDR intermediate colours are bounded below the half-float storage ceiling to prevent invalid pixels from intense highlights. Finite-emitter shadows and highlights remain approximations.

Forward shading and the lighting composite share an indirect-light calculation. Ambient occlusion now attenuates indirect illumination, while direct lighting and emissive surfaces retain their energy. Environment diffuse lighting includes Fresnel attenuation. The existing screen-space bounce, scene probes and roughness-filtered reflections remain in use. There is no additional G-buffer attachment or full-scene ray-tracing pass.

## Exposure and bloom

Auto exposure meters weighted log luminance on the GPU. It favours central geometry, reduces the influence of sky, deep black surfaces and small intense emitters, and adapts in logarithmic exposure stops. Entering bright areas adapts faster than entering dark areas.

New controls are available through `GraphicsSettings` and settings files:

```ini
min_exposure_ev=-2.5
max_exposure_ev=1.5
local_exposure=0.16
```

The bounds control the automatic exposure multiplier in stops, independently of `exposure_compensation`; they are not EV100 camera settings. Generic defaults retain the previous multiplier range of 1/16–16. Reversed bounds are normalized. `local_exposure` accepts 0–1, defaults to zero and is also available in the `C` overlay. It applies a modest depth-aware exposure correction rather than blurring the image. Physical camera exposure continues to bypass automatic exposure.

Bloom uses a soft threshold relative to exposure, giving bright fixtures a smoother onset as brightness changes. Depth of field and motion blur remain off in every showcase preset, with native render scale retained. Medium and higher showcase presets enable calibrated eye adaptation; High/Max add restrained local exposure.

## Showcase and projects

The sun and practical lights use warmer, luminance-normalized light colours. `lightColorTemperature(kelvin)` integrates a Planck spectrum with analytic CIE matching functions and returns a nonnegative linear-sRGB colour with luminance one, for 1000–25000 K. It approximates blackbody chromaticity; the renderer remains RGB.

Cave fixtures now emit from below their housings, use finite-size shadowed spotlights, and have reduced unoccluded ambient fill. Spot direction, cones, emitter radius, shadow settings and native intensity units survive project capture. New exports carry the revised exposure style. Existing exports retain their authored data until exported again. The cave entrance's outward winding and normals are corrected so its bedrock receives daylight rather than exposing back faces.

CSV profiling appends `shadowed_spots` and `spot_shadow_updates` alongside the existing probe counters. `--debug-dir` adds `lighting-state.txt` with the metered exposure and local-shadow state. Debug capture reads back GPU data; ordinary frames do not read exposure back to the CPU.

Rebuild the engine, showcase and all native editors/applications linked against it: `Light`, `GraphicsSettings` and `Statistics` changed binary layout. The Windows batch workflow remains supported. Native Windows compiler/driver validation is still required on Windows.

## Technical references

- [Epic: MegaLights](https://dev.epicgames.com/documentation/en-us/unreal-engine/megalights-in-unreal-engine) and [Lumen technical details](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-technical-details-in-unreal-engine) describe the larger reference systems; this update does not integrate those systems.
- [Epic: auto exposure](https://dev.epicgames.com/documentation/en-us/unreal-engine/auto-exposure-in-unreal-engine) provides the photographic adaptation reference. This implementation uses weighted log metering rather than Unreal's histogram or local-exposure implementation.
- [Brian Karis: Real Shading in Unreal Engine 4](https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf), finite-emitter approximation and GGX normalization.
- [Wyman, Sloan and Shirley: CIE matching-function fits](https://jcgt.org/published/0002/02/01/), coefficients used by the colour-temperature helper.
