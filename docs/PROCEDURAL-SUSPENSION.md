# Generated surfaces and collision-aware suspension — 0.5.7

The engine and showcase use the same implementation on Linux and Windows. Rebuild the engine, showcase and any native applications or editor linked against the engine after this update: `Material` gained fields and its binary layout changed. Existing source using the previous aggregate fields keeps their order.

## Materials

`rf::gfx::Material` accepts `surface`, `surfaceSeed` and `surfaceWorld`. `SurfaceKind` provides `Rock`, `Soil`, `Concrete`, `PaintedMetal`, `BareMetal` and `Rubber`, alongside the default `Textured` mode for existing assets.

```cpp
rf::gfx::Material stone;
stone.surface = rf::gfx::SurfaceKind::Rock;
stone.surfaceSeed = 481;
stone.surfaceWorld = true;
stone.albedo = {.26f, .205f, .155f, 1};
stone.roughness = .9f;
```

For these recipes the fragment shader computes color variation, roughness and fine normal relief from seeded three-dimensional coordinate hashes and smooth fields at several physical scales. It does not sample a repeating color/normal/ORM bitmap or a wrapping noise volume. Nearby coordinates remain continuous; distant positions produce different detail. The recipe is deterministic, so surfaces do not change randomly between frames. Fine detail fades with pixel footprint to reduce distant aliasing. This adds shading relief, not displaced collision geometry.

`surfaceWorld=true` gives adjoining scenery a continuous world-space field. Set it to `false` for moving objects so their surface detail follows their mesh-local coordinates. `textureScale` controls the coordinate scale and `normalScale` controls fine relief. Valid seeds are 0 through 2147483647. The project format saves these settings under `Surface.engine`; older projects default to `Textured`.

The showcase uses generated recipes for its terrain, rocks, cave, concrete, metal and tires. Imported rock geometry remains available. Signs, screens, emissive imagery and UI still use their authored images. Existing rendering quality settings and antialiasing choices remain available. Generated shading has a different GPU workload; this update does not claim a measured performance gain.

## Surface contact

`jsd::wheelSupport` probes a circular tire footprint against `PhysicsWorld` colliders and an optional terrain height function. It samples the tire circle and refines collider/height boundaries, allowing a tire to start climbing a curb before its center reaches the edge. The result contains the support point, effective contact normal, collider body and required hub height. Pass the vehicle body in `QueryFilter::ignore` to exclude self-collision. Collider geometry must represent any visible object that should support the wheels.

`jsd::SuspensionSpring` supplies a critically damped response for heave and angular settling. The showcase combines six individual wheel contacts with damped chassis heave, pitch and roll, finite downward wheel travel, contact-based wheel rotation and arms, piston rods and coils drawn from the current hub positions. Grounded tires emit dust on the terrain; tires on the raised platform do not emit terrain dust.

The demonstration platform has a visible/collidable 320 mm step. The approach camera moves closer around 18–22 seconds, showing each axle climbing the edge and the suspension responding. Moving supports are queried each simulation tick.

The showcase retains its prescribed, collision-checked kinematic route. This is geometric tire support and suspension animation, not a full dynamic drivetrain, tire deformation or force-based vehicle simulation. The support probe is intended for ground-facing surfaces, not vertical-wheel or overhead driving.
