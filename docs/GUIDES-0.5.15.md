# Native world guides — JSD 0.5.15

`Renderer::lines(std::span<const LineSegment>, const Mat4& transform)` submits
world-space, constant-pixel-width guide segments. Call after begin() and before
render()/end(). The optional transform defaults to identity. All segment inputs
must be finite, widths must be positive and at most 128 pixels, and a frame may
contain at most two million segments.

Each segment supplies endpoints, RGBA tint and width. GPU vertex instancing
expands six corners, clips against the near plane, antialiases its edges and
checks scene depth. Guides composite after temporal/film processing and before
UI. They are intended for engineering overlays, not emissive physical objects;
they neither cast shadows nor contribute to reflection/lighting history.

Exact unchanged segment sequences reuse the existing VBO, including camera
changes. CPU validation/transformation uses the engine pool in large batches;
GL allocation, upload and draw remain on the owning thread. Statistics exposes
lineSegments, lineUploadBytes and linePreparationWorkers.

Rebuild all SDK consumers after updating: public structures gained fields.
Source identity is jsd.engine 0.5.15. Linux and Windows use the same GL 3.3 path.
