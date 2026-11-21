# SUNDOG / Kestrel Deep expansion

The autoplay sequence is now 160 seconds. It stays in one world: there are no scene cuts, loading transitions or scene crossfades. The camera accelerates over the mountain pass, descends to the portal, tours the refinery and extraction gallery, then flies back to the original opening pose. The loop matches camera position, direction and travel velocity.

| Time | Location / camera sequence |
| --- | --- |
| 0–58 s | Original Kestrel Relay recovery mission |
| 58–74 s | Fast aerial mountain transit and cave approach |
| 74–104 s | Tunnel entry and elevated refinery tour |
| 104–132 s | Furnace, slag handling and mineral extraction gallery |
| 132–145 s | Mezzanine overlook and continuous departure |
| 145–160 s | Return flight to the opening camera |

The excavated arch expands from a 16 m access tunnel to a 52 m process hall. It contains pressure vessels with flanges, inspection ports, valves and transfer pipes; a supported conveyor with moving ore; rotating crusher/turbine/drill rotors; an emissive furnace and slag trough; a slatted gantry, stairs and handrails; a telemetry control bay; mineral formations, utility cabinets and a drill rig. Twenty-six additional local lights and eight seeded steam/dust/ember emitters supply distinct warm furnace and cool work zones. Interior ambient, sun and fog values transition smoothly at entry/exit. The generated signage/control textures use DejaVu fonts under the existing bundled notice.

Run the native target with `./run-showcase.sh`; open/edit the same content in `projects/sundog/sundog.jsdproject`. The engine player runs that project through `./run-project.sh`. Use `--time 80`, `--time 92`, `--time 116` or `--time 136` to inspect the added views. All geometry, materials, lights, effects, animation and camera/environment tracks are exported; the hierarchy has a dedicated **Mountain cave / Deep Refinery** group. Engine/editor build projects remain separate.

Regenerate with the native `jsd-showcase --export-project projects/sundog/sundog.jsdproject --offscreen --no-audio`. Animation is sampled at 120 Hz. The camera route and collision-volume checks are in `examples/showcase/tests/physics_tests.cpp`. Existing outpost/cargo/rover physics checks remain in that suite.

This is a visual process-plant demonstration, with authored machine motion and seeded particle effects. It does not add a production/economy simulation. The original project solver and terrain/collision-proxy limitations remain as documented in `docs/JSDPROJECT.md`.
