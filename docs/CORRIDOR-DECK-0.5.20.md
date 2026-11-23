# JSD 0.5.20 corridor deck material

SurfaceKind::CorridorDeck is appended as recipe 17. Renderer and project-file
validation accept it; all existing ordinals are unchanged. The game and engine
versions are aligned at 0.70.0 / 0.5.20, including the Windows source guards.

The procedural surface combines 0.60 by 0.90 metre panels, dark gasket joints,
beveled lips, slotted fasteners, fine diamond grip and per-panel finish/wear.
It changes metallic response and roughness across the material and filters fine
detail with screen derivatives. Consumers can supply an object/route-local
surface-coordinate basis, as with existing mapped materials. Geometry remains
the consumer's collision/pressure authority.

New shader sources invalidate their persistent cached programs automatically.
No new sampled texture asset, network download or per-frame texture generation
is required.
