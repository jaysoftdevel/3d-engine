# Showcase camera, clarity and frame pacing — 0.5.9

This update requires engine 0.5.8 and rebuilds the shared Windows/Linux engine and showcase. Rebuild any native editor or application linked against the engine as well; `Statistics` gained a field.

## Camera and motion

The camera uses a continuous 0.3 m sphere sweep against the original static render triangles, including mountains, cave walls, platforms, railings and equipment. Triangle faces, edges and vertices participate, so a thin panel or a glancing corner cannot be skipped by a centre ray. A BVH accelerates queries, while contact sliding preserves movement along obstructions. Terrain and live gameplay collision bodies supplement the static geometry for the exploration camera. Decorative particles, ground-cover scatter and moving parts without gameplay collision proxies remain decorative.

The complete 160-second playback path is swept at startup and interpolated between safe samples. Revised gallery waypoints avoid the ore machinery. The live camera sweeps once more against current gameplay bodies. Camera and rover presentation use the fractional simulation step, instead of repeating 120 Hz poses on faster displays. Free-flight velocity eases into motion and contact slides along surfaces. Simulation remains fixed at 120 Hz; presentation interpolation does not change rover suspension or cargo physics.

VSync is enabled by default. If the driver refuses the swap interval, the showcase falls back to a 60 FPS software limit. An explicit `--frame-limit 60` sets a software limit; `--no-vsync` allows uncapped rendering when no limit is supplied. Frame pacing cannot compensate for frames that exceed the hardware's rendering budget.

## Sharp defaults and optional lens effects

All showcase presets now use render scale 1, with depth of field and motion blur off. These defaults also override lens blur and render scale loaded from older settings files. Resolution, antialiasing, reflection, lighting, shadow and material controls remain available. Temporal history is reduced for motion measured in pixels, reducing trailing blur during travel.

Optional effects are deliberately mild:

```bash
./run-showcase.sh --quality high --cinematic-dof
./run-showcase.sh --quality high --motion-blur
./run-showcase.sh --quality high --render-scale 1 --frame-limit 60
```

Use the same options with `run-showcase.bat` on Windows. `--cinematic-dof` enables Medium depth of field with aperture 0.35, autofocus adaptation 2.5 and a 2.5-pixel maximum blur. Graphics settings remain adjustable during the session with `C`. Explicit `--render-scale` accepts 0.5–2; values below 1 reduce internal resolution.

## Complete rock volumes

The open-backed scanned scenery has been replaced by seeded, closed procedural rock volumes with the existing world-space material evaluation. Main mountain masses have complete sides, roofs and buried bottom caps. Their foundations follow the minimum terrain elevation under the footprint. The cave exterior has an eroded ridge and a sloping, fractured entrance face rather than a flat exposed section. Its rear closure fits the excavated arch, replacing an oversized rectangular wall that protruded through the outer silhouette. Gallery outcrops use closed rock geometry. Safe LOD generation checks positional edge closure and rejects levels that would open a closed source mesh or reverse a face.

## Bounded reflection updates

Initial, revised and periodic reflection-probe updates now capture at most one face per rendered frame. Filtering runs separately in batches of at most six face/mip jobs. Double buffering publishes a complete result after warm-up while keeping a previous capture visible during refresh. The initial environment fallback lasts about fifteen rendered frames. This removes the former six-face capture and 54-filter burst from a single frame without reducing probe resolution or material quality.

Use `--profile frames.csv` to record frame, CPU submission, GPU and preparation times. `probe_faces` and `probe_filters` expose the extra probe workload. GPU timing requires a supported driver. Performance must be assessed on the target GPU; software-rendered validation does not establish a hardware FPS result.

## Linux script permissions

The previous update recorded `build.sh` as 0644. Launchers now invoke nested shell scripts through `bash`, and update creation records mode-only changes and normalizes first-party shell entry points to 0755. The dependency installer's expected engine version is corrected as well. Apply with `bash apply.sh --engine /path/to/3d-engine` so extracting the updater without execute bits is supported.
