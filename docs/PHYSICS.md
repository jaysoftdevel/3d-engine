# SI rigid-body physics

The engine's public physics layer uses bundled Jolt 5.7.0. Include `<jsd/engine.hpp>` or `<jsd/physics.hpp>`. Build and link the usual `JSD::GameEngine` or `JSD::GameEngineStatic` target. No additional runtime or dependency download is needed.

| Quantity | Unit / convention |
| --- | --- |
| Position, dimensions, ray endpoints, collision skin | metres; right-handed, +Y up |
| Time | seconds; double precision frame accumulator |
| Mass / density | kg / kg per cubic metre |
| Linear velocity / acceleration | m/s / m/s² |
| Angular velocity | radians/s, world-space vector |
| Force / linear impulse | N / N·s |
| Torque / angular impulse | N·m / kg·m²/s |
| Inertia / energy | kg·m² / joules |
| Linear and angular damping | inverse seconds; velocity multiplier exp(-rate × dt) |
| Friction / restitution / drag coefficient | dimensionless |

`jsd::si` supplies dimension-checked scalar quantities and metre/millimetre conversion. For example `si::Mass * si::Acceleration` produces `si::Force`. Existing vector APIs remain ordinary `V3` values in the units above; they do not silently convert centimetres, degrees or frames.

## A moving body

```cpp
jsd::WorldSettings settings;
settings.gravityMps2 = {0, -9.80665f, 0}; // Earth; Moon: -1.62, Mars: about -3.72
settings.fixedStepSeconds = 1.0 / 120;
jsd::PhysicsWorld physics(settings);
jsd::BodySettings floor;
floor.motion = jsd::MotionType::Static;
floor.position = {0, -0.5f, 0}; floor.halfExtent = {20, 0.5f, 20};
physics.box(floor);
jsd::BodySettings load;
load.position = {0, 4, 0}; load.halfExtent = {0.5f, 0.5f, 0.5f};
load.massMode = jsd::MassMode::Density; load.densityKgPerM3 = 600;
load.linearDampingPerSecond = 0.05f;
load.continuous = true;
auto body = physics.box(load); // 1 m³, 600 kg; shape-derived inertia
// Once per completed frame; callback runs before EACH fixed physics tick:
auto frame = physics.advance(frameSeconds, [&](auto &world, float dt) {
    (void)dt;
    world.force(body, {1200, 0, 0}); // N, not an impulse
});
auto pose = physics.interpolatedState(body, frame.interpolationAlpha);
// Submit rf::compose(pose.position, pose.rotation) to the renderer.
```

`frameSeconds` is the application's measured frame duration. Do not also call `step()` for the same interval. `advance()` retains excess time when its work budget is exhausted; inspect `behind`/`pendingSeconds`, catch up or explicitly pause/reset the clock. It never silently skips physics time. `interpolatedState` blends the previous/current solver poses with the usual one-tick rendering latency. `step(seconds)` is an alternative for already fixed-step callers; it subdivides intervals up to 0.25 seconds at the configured step.

`force`, `forceAt` and `torque` accumulate for the next `step` interval, including its internal substeps, then clear. Use the per-tick callback above with `advance`; do not queue a frame force and expect it to persist over multiple ticks. `continuousForce` replaces a persistent force, and `clearForces` clears all queued/persistent forces. Impulses act immediately and must be issued once for the event, not once per rendered frame. Off-centre force/impulse uses the world-space lever arm relative to the body's centre of mass. All supplied shapes are centred on that origin.

Boxes use **half extents**. Spheres use a radius; Y-aligned cylinders use half height and radius; capsules use half the cylindrical section height and radius, with hemispherical caps added. Mass can be explicit kg (default) or shape volume × density. Inertia comes from the actual collision primitive and selected mass. Prefer explicit loaded mass for hollow equipment, or an effective bulk density: a hollow rover is not a solid block of steel. `massProperties`, `kineticEnergyJoules`, `potentialEnergyJoules` and `statistics` expose the calculated values. Static/kinematic bodies have infinite effective collision mass (inverse mass zero), while retaining their configured mass/inertia for a later dynamic transition.

Optional `dragCoefficient` and `dragAreaM2` add quadratic air drag relative to `WorldSettings::windMps`. Air density defaults to 1.225 kg/m³; set zero for vacuum. Isolated drag uses its analytic velocity decay, split from gravity/external force integration, avoiding direction reversal under strong drag. Damping is separate from drag and contact friction. Gravity is uniform, configurable per world and scaled per body; changing it wakes dynamic bodies.

## Cinematic movement and blockers

Choose an explicit `MotionType`: `Static`, `Kinematic` or `Dynamic`. The earlier `dynamic` boolean remains supported when `motion == Legacy`.

```cpp
jsd::BodySettings actor;
actor.motion = jsd::MotionType::Kinematic;
actor.halfExtent = {0.6f, 0.8f, 0.6f}; actor.position = {0, 1, 0};
auto controlled = physics.box(actor);
auto move = physics.moveKinematic(controlled, {4, 1, 0}, {}, 1.f / 120);
// move.blocked, blocker, normal, acceptedPosition and acceptedRotation explain the result.
physics.step(1.f / 120); // render the actual solver state, not the requested target
// Release at the accepted pose, without replacing the object:
physics.motionType(controlled, jsd::MotionType::Dynamic);
```

Changing to dynamic motion preserves the instantaneous linear/angular velocity; static/kinematic transitions clear it. A completed kinematic plan stops at its target, so release during motion before its duration ends to inherit motion.

A shape sweep stops translation before a blocker with a configurable skin (default 2 mm). Angular travel uses conservative shape inflation, so a long object cannot freely rotate through an obstacle; it may stop earlier than an exact rotational sweep, especially near surfaces. Translation sweeps use the actual primitive and current orientation. `KinematicMotor::moveTowards` additionally limits acceleration and speed, brakes on approach and stops on a blocker; the caller advances the solver. Movement requests may span several simulation ticks.

Kinematic actors obey their accepted targets and have infinite effective mass. This is deliberate cinematic control, not a motor/tyre force model. A sweep tests the current world, not all future trajectories of other moving bodies; rapidly converging controlled actors need application-level planning and frequent ticks. Start actors outside geometry. Use dynamic bodies for physically resolved pushing, recoil, friction and restitution. `continuous=true` enables Jolt linear CCD for fast bodies; finite timesteps still have accuracy limits. Explicit `teleport` bypasses collision and resets interpolation history; reserve it for spawning/resetting.

`rayCast`, `occluded` and `sweep` return real collider hits, fractions and surface normals. Queries exclude sensors by default; `QueryFilter` can include them or ignore a body. Controlled sweeps always ignore their own body and can ignore one extra body (e.g. a cargo carrier). Contact start events include position, normal (A toward B) and sensor status; end events identify the pair, with no current manifold. Drain `contacts()` regularly. Shapes do not come from render meshes automatically.

## Showcase integration

The rover has a kinematic collision proxy. It follows its existing route when clear, stops at colliders and resumes at bounded speed. Wheel rotation and dust use accepted travel distance. Structural boxes/cylinders share the visible compound transforms, including the landing slab. Fine cables, foliage and scanned background formations remain visual detail. Terrain contact for the rover's wheels still uses the sampled terrain fit; this is not a suspension/drivetrain solver.

The crane acquires a 25 kg case at its actual mounted position, moves that same body through swept targets, then changes it to dynamic at release. Rendered cargo and the attached hook follow the solver pose. The case falls onto the visible slab and settles through contact friction/damping. Mission restart removes the case and resets physical/rolling history.

## Scheduling and limits

Jolt handles broad/narrow phase collision, contact constraints, restitution, friction, sleeping and CCD. Physics calls and render submission are serialized by the application; Jolt jobs run internally. The default physics pool uses up to eight available CPU workers (respects affinity); set `WorldSettings::workers` explicitly for a workload that benefits from more. Sleeping and broad-phase culling avoid solving distant static detail every tick. Default capacities: 8,192 bodies, 32,768 body pairs, 16,384 contact constraints and a 16 MiB temporary pool with heap fallback for bursts. Body/pair/contact budgets scale with `maxBodies`. Solver capacity failures raise errors. Body handles include Jolt generations; retired handles and invalid/nonfinite inputs are rejected.

This layer currently provides uniform gravity and primitive rigid bodies. It does not implement deformable bodies, fluids, arbitrary mesh/heightfield colliders, joints, gyroscopic spacecraft modelling, relativistic effects or coupled spectral light transport. Solver values are single precision; keep playable coordinates near the origin and use sensible metre-scale geometry. Physics and graphics are separate systems connected by poses and visibility queries.

Run `./test.sh --with-showcase` for analytic physics, graphics, rover and actual showcase integration checks. Direct CMake builds expose the corresponding CTest cases. See [physics validation](PHYSICS_VALIDATION.json) and [physical optics](OPTICS.md).
