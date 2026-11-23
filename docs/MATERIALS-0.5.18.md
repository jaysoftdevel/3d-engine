# JSD 0.5.18 photovoltaic material

SurfaceKind::PhotovoltaicCells (16) adds metre-scale photovoltaic cells,
derivative-filtered seams, fine bus conductors and per-cell finish variation.
Existing material ordinals are unchanged. Both renderer material validation
and runtime project recipe validation accept the appended kind.

The shader consumes the existing supplied surface-coordinate basis. A moving
collector should provide its moving pivot and orthonormal local axes for both
its transformed vertices and surface mapping. No world-space or animated
texture projection is needed. The game sets a coated, slightly metallic silicon
response, and applies the recipe only to the front of the collector plates.

Persistent shader programs invalidate on source/version changes and compile
once on the first start with this update. This does not change power simulation.
