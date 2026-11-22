# Directional sky sphere — JSD 0.5.13

`Environment::stars` adds an opt-in infinite directional starfield to the native
sky pass. Its fields are `enabled`, `intensity`, `seed` and `orientation` (Mat4).
The catalog is evaluated from the inverse camera projection/view, not screen
coordinates or camera position. It therefore tracks yaw, pitch, FOV and aspect
without translation parallax or a texture seam. Keep the seed stable between
frames. Orientation maps rendered directions to the desired sky reference frame.

```cpp
rf::gfx::Environment environment;
environment.atmosphere.enabled = false; // vacuum, e.g. Moon
// Black background, rather than atmospheric blue.
environment.skyColor = {0.0003f, 0.0005f, 0.0008f, 1};
environment.stars.enabled = true;
environment.stars.intensity = 2.0f;
```

A seeded integer lattice over a narrow spherical shell supplies sparse, warm/cool
star profiles. Screen derivatives filter subpixel energy. No star positions are
regenerated over time. Atmosphere-enabled scenes fade stars with solar elevation;
cloud transmittance also occludes them. Airless star skies include a small solar
disc and avoid Rayleigh/haze/cloud scattering. Default environments retain their
previous star-free behavior. New fields are appended; rebuild SDK consumers.
Project document serialization is unchanged; these controls are native API fields.

The existing sky pass provides background depth/motion and scene occlusion in
both compact and deferred paths. It does not add weather to an airless body or
alter material textures. Lunar Colony 0.62.0 binds its viewport correction so the
sky reference also remains consistent at orbit-camera poles.
