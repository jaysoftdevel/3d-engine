# Particle and cinematic integration

Include `<jsd/engine.hpp>` or the narrower `<jsd/particles.hpp>`. All types belong to `jsd`; renderer types are in `jsd::gfx` (also available as `rf::gfx`). The original `ParticleEmitter` API is retained.

```cpp
#include <jsd/engine.hpp>

auto settings = jsd::VfxSettings::dust();
settings.origin = {0, .2f, 0};
settings.rate = 45;
settings.capacity = 2048;
jsd::VfxEmitter dust(settings);

// Each simulation update:
dust.setOrigin(wheelContact);
dust.setEmitting(roverMoving);
dust.update(dt);                 // internal 120 Hz simulation

// Each rendering frame, after renderer.begin(camera, environment):
dust.draw(renderer);             // soft instanced billboards
// renderer.end(); then present
```

Factories `dust()`, `steam()`, `sparks()`, `sand()` and `gravel()` supply editable starting values. Configure `rate`, `lifetime`, `lifetimeVariation`, `size`, `sizeGrowth`, `spread`, `spawnRadius`, `gravity`, `wind`, `drag`, `turbulence`, linear HDR `start`/`end` colors, `softness`, `stretch`, `restitution`, `friction`, `floor`, `collision`, `capacity` and deterministic `seed` before constructing an emitter. Configuration is held by value. Use `setOrigin` and `setVelocity` to move a source during simulation; `burst(n)` immediately spawns up to available capacity. `setEmitting(false)` stops continuous emission while existing particles finish. `clear()` retires particles and resets the deterministic sequence/time/contact count. Emitters are movable, not copyable.

## Surface contact and gravel

```cpp
auto settings = jsd::VfxSettings::gravel();
settings.origin = {2, 3, 5};
jsd::VfxEmitter gravel(settings);
gravel.setSurface([](float x, float z) { return terrainHeight(x, z); });
auto gravelMesh = renderer.createMesh(jsd::gfx::makeDetailedRock(1337, 6));
// Simulation:
gravel.update(dt);
// During a frame:
gravel.draw(renderer, gravelMesh);
```

Without a height callback, contact uses `settings.floor`. Surface collisions are ballistic height-field responses, with restitution and horizontal friction. They do not collide with arbitrary rigid bodies. The game separately uses Jolt for its cargo. Mesh gravel uses opaque instancing, lighting, shadows and unique motion IDs; supply a small centered unit-scale mesh. Caller owns/destroys it before the renderer is destroyed. Gravel particles vanish at lifetime end; deposits are not accumulated.

Simulation is CPU based and capped at 65,536 particles per emitter. Updates accept at most one second per call. For repeatable offline output, use fixed 1/120 updates or a stable frame cadence. Dense gravel costs geometry and shadow draws; use small meshes and modest rates. Sprite batch maximum is 131,072 per frame. Exceeding renderer limits raises an error.

## Renderer-level sprites

`renderer.particles(span<gfx::ParticleSprite>)` accepts world position, velocity, linear HDR color, size, rotation, stretch, softness, age and `ParticleStyle::{Dust,Steam,Spark,Sand}`. It copies instances into the current frame. Dust/steam use procedural 3D-noise opacity, sparks stretch along projected velocity and emit HDR color, sand uses compact granular billboards. A copied opaque scene-depth texture avoids framebuffer feedback while fading intersections. One instanced GPU draw handles all sprite styles. Sprites do not cast shadows or self-shadow; steam is not a fluid simulation. Reactive masks limit TAA trails at translucent edges. There is no GPU compute simulation or sorting/OIT guarantee for intersecting translucent plumes.

## Cinematic camera

```cpp
jsd::CameraTrack track({
    {0,  {0, 2, -8}, {0, 1, 0}, 55},
    {6,  {5, 3, -2}, {0, 1, 0}, 48},
    {12, {0, 2, -8}, {0, 1, 0}, 55}
});
auto camera = track.sample(elapsed);   // loops by default
```

Key times must increase strictly and begin at zero. Position/target use Hermite interpolation; FOV changes smoothly. Match beginning/end positions, targets and FOVs for a continuous loop. This is a camera track API, not a general cutscene/editor timeline. Reset simulation, emitters and `renderer.resetHistory()` after a discontinuous restart.
