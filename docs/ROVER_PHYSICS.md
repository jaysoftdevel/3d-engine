# Rover wheel motion and terrain contact

The rover now uses the engine's `jsd::RollingWheel` odometry component for each of its six wheels. The component derives rotation from signed movement through the wheel's rolling plane:

- Rolling tangent: normalized axle direction crossed with the ground normal.
- Signed travel: center displacement projected onto the average rolling tangent.
- Angle in radians: signed distance divided by the effective radius.
- Revolutions: signed distance divided by `2 * pi * radius`.

This gives opposite local rotation signs for the mirrored left/right meshes while producing the same physical forward rolling direction in world space. Reverse travel reverses rotation. Vertical suspension motion and sideways displacement do not create artificial spin.

The radius is measured from the baked wheel mesh around its actual local axle, including the outer tread vertices. The previous hard-coded 0.66 m value has been removed. The measured radius is approximately **0.714739336 m**, with circumference **4.490839693 m**. The 38 m approach therefore produces approximately **8.461669219 wheel revolutions**. Tyres and rims share the same transform and angular phase.

## Simulation and terrain

Wheel state updates during simulation steps, independently of rendering. Angular travel accumulates in double precision; only the final bounded angular phase is converted for rendering. Rendering an already simulated state does not advance the wheels. Stop, reverse and restart behavior follow actual travel rather than an elapsed-time multiplier.

The chassis pitch, roll and height follow samples underneath the six wheel locations. Each wheel follows its own local terrain contact plane; the support calculation includes axle inclination. This replaces the artificial chassis bob and the previous wheel-ground clearance. The prescribed showcase route remains kinematic; the engine's existing Jolt solver continues to simulate rigid cargo. This update adds grounded rolling/contact calculations rather than a tyre-force or drivetrain model.

The mission clock accumulates in double precision. A zero-duration update leaves the simulation unchanged; negative/nonfinite/oversized steps are rejected. On a mission loop, wheel histories are reset at the start position and only the remainder of the new cycle advances physics and particles. The reset jump does not count as travelled distance. Dust emission and trail velocity use measured rover motion, so a stationary rover no longer emits drive dust.

## Engine use

```cpp
jsd::RollingWheel wheel(worldRadiusMetres);
wheel.reset(center, positiveMeshAxle, surfaceNormal);

// Once per positive simulation step:
wheel.advance(newCenter, newAxle, newNormal, seconds);
double angle = wheel.angle();          // cumulative radians
float visiblePhase = float(wheel.phase());
```

Pass the radius after applying the model's visual scale. The axle is the mesh's positive rotation axis in world space. Use `reset` after a teleport or explicit repositioning. Pause by omitting simulation updates. Contact frames and timesteps are validated before changing odometry.

## Validation

The source tests cover circumference-to-revolution correspondence, forward/reverse movement, mirrored mounts, no-slip ground velocity, stationary wheels, suspension/lateral movement, inclined terrain, invalid input, and consistent travel at 30/60/120/240 Hz. The optional showcase tests also exercise all six wheels over the complete route, terrain-plane support, stopping, repeated rendering and resets.

Native render validation inspects the actual submitted wheel/rim matrices and measures the radius from the mesh uploaded to the renderer. It checks moving, stopped and loop-boundary states. All six wheels use the correct rotation sign and distance/radius relationship; measured ground-contact velocity residual is below 0.000007 m/s in the checked flat-road sequence. Results are recorded in `ROVER_PHYSICS_VALIDATION.json`.

The earlier processing benchmarks remain historical measurements of that update. Correcting wheel orientation, ground clearance and dust intentionally changes rover pixels in new captures.
