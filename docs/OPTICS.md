# Physical camera and photometry

Include `<frontier/renderer.hpp>` (also available through `<jsd/engine.hpp>`). These controls are opt-in; scenes using the earlier artistic light/camera fields retain their calibration.

```cpp
rf::gfx::Camera camera;
rf::gfx::CameraOptics lens;
lens.focalLengthMetres = 0.035f; lens.sensorHeightMetres = 0.024f;
lens.fNumber = 2.8f; lens.shutterSeconds = 1.f / 120; lens.iso = 100;
camera.optics = lens;
// Lens dimensions now determine vertical FOV. Enable the usual DoF/motion blur
// quality settings to use the lens for those effects. Autofocus remains on-GPU.
renderer.begin(camera, environment);
renderer.light(rf::gfx::pointLightLumens({0,3,0}, 800, 25));
renderer.light(rf::gfx::spotLightLumens({2,4,0}, {0,-1,0}, 1600, 30, .3f, .6f));
```

| Input | Meaning |
| --- | --- |
| Focal length / sensor height | metres; vertical FOV = 2 atan(sensorHeight / (2 focalLength)) |
| F-number | focal length / entrance pupil diameter |
| Shutter | seconds; motion blur spans shutter/frame-time, capped at 2 frame intervals |
| ISO | exposure sensitivity; used when `physicalExposure` is true |
| Point/spot flux | lumens; helpers convert to candela and normalize linear RGB luminance |
| Physical light source radius / influence range | metres; finite-source near-field clamp / explicit rendering cutoff |
| `Environment::sunIlluminanceLux` | direct-normal illuminance, lux; color is normalized to preserve it |

Physical point lights use inverse-square attenuation, with distance squared clamped by `sourceRadiusMetres²` near the source and an explicit cutoff at the light's influence range. Spot flux is normalized over the shader's smooth inner/outer cone profile, not just a uniform hard cone. Cone values are half angles in radians. The usual PBR BRDF supplies the incidence cosine. Selecting physical lights/sun or physical exposure also removes the legacy constant ambient floor when global illumination is off; ambient probes/HDR maps still have caller-selected intensity and units. Existing legacy lights retain their original falloff.

Lens DoF uses the thin-lens circle of confusion:

`c = f² |z-s| / (N z (s-f))`, then blur radius = `0.5 c × imageHeight / sensorHeight`.

Here `f`, `z` and `s` are focal length, fragment depth and focus distance in metres. Focus must be beyond the focal length. GPU autofocus supplies the same smoothed scene focus as before. `maximumBlur` and quality sample count still bound cost. Physical lens mode ignores the artistic `GraphicsSettings::aperture` coefficient. With no lens selected the earlier appearance remains. This remains a depth-buffer gather approximation: it cannot reconstruct hidden surfaces, lens aberrations or diffraction.

Set `lens.physicalExposure = true` to use photographic exposure: `EV100 = log2(N² / shutter × 100 / ISO)`, exposure multiplier `1 / (1.2 × 2^EV100)`. Exposure compensation remains in stops. Automatic exposure is bypassed in this mode. Calibrate the sun, local lights, HDR maps and emissive surfaces consistently before switching an artistic scene to this mode. `physicalExposure=false` (default) permits physical FOV/DoF/shutter with the existing artistic exposure.

The CPU optics helpers implement inverse-square illuminance, Beer–Lambert absorption per metre, exact unpolarized dielectric Fresnel reflection, and Snell refraction with total internal reflection reported as no transmitted direction. They support application-level ray/visibility calculations; they do not add transmitted rays to the raster renderer. `PhysicsWorld::occluded` can query opaque collision proxies for optical sight lines, but collider material alone does not define transparent optics. Existing sun shadow maps remain in use; local light helpers do not add local shadow maps. The renderer is linear RGB PBR, not spectral/path-traced optics.

Primary references: [bundled Jolt API](../third_party/JoltPhysics/Jolt/Physics), [Filament physical camera/photometry](https://google.github.io/filament/main/filament.html), [PBRT projective/thin-lens cameras](https://pbr-book.org/4ed/Cameras_and_Film/Projective_Camera_Models). Equations here are implemented directly in the engine and checked numerically and through native GL readbacks.
