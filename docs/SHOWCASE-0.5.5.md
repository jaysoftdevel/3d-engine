# Terrain and ore-gallery rework — engine 0.5.5

The showcase now uses a graded service road and mountain apron that meet the portal deck, a continuous inner vault and outer rock shell, and scanned geological outcrops. Coherent, deterministic terrain noise replaces the broad repeating wave pattern. The new `sundog-v2` terrain generator is shared by the showcase and exported projects; `sundog-v1` remains available for existing projects.

The gallery depicts ore handling and process-water recovery. A guarded crusher feeds the belt, a covered chute feeds the screening unit, and a sump connects to the water tanks. Tank drives are enclosed. The cutter turns about a horizontal supported spindle at the rock face. Dust is confined to ore handling and drilling, with a collection duct routed outside the portal. The open underground furnace, molten slag and decorative steam releases have been removed.

Roof beams terminate inside the excavated vault and rest on columns and footings. The mezzanine has a connected landing and a 42-tread staircase with approximately 182 mm risers, stringers and handrails. Rear cabinets leave the stair approach clear. The camera travels down the central aisle and under the cross-bridge instead of flying through the machinery zone. Interior lighting uses a dim neutral reflection environment and work fixtures, rather than outdoor sky reflections and decorative furnace glow.

## Performance with quality preserved

The same C++ renderer changes serve Windows and Linux:

- When sunlight contributes zero illumination, its shadow map is cleared without redrawing geometry into it. Shadow quality settings remain intact for lit exterior scenes.
- From inside the rock shell, an exterior scene is omitted only if the portal's conservative bounding sphere misses the actual camera frustum. Looking through the entrance, going outside the vault, changing aspect ratio, or capturing an editable project retains the exterior.
- Local-light bounds reject lights wholly outside the viewport. The projection bounds include off-axis perspective expansion and near-plane intersections, preserving visible lighting contributions.
- Terrain skirts reuse already evaluated edge vertices, keeping their heights, normals and topology.
- Cave surface generation shares evaluated vertices between adjacent triangles without changing tessellation density.

No quality preset, render scale, texture-resolution setting, antialiasing setting, effect quality, or view-distance setting was reduced. Geometry and effects were changed to match the revised physical scene. Performance measurements and platform verification are recorded in the update package; software-renderer timings should not be treated as GPU frame-rate predictions.
