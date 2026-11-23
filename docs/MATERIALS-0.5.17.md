# JSD 0.5.17 material update

PressureDoor (14) adds the shared pressure-plating finish; SolarShield (15) adds
the transparent UV-cut/screening appearance. Existing material ordinals remain
unchanged. HullPanels receives engineered seams, fasteners and coated-metal
response. Console is now a stable display substrate; applications supply actual
screen content rather than the shader generating imaginary changing values.

SolarShield uses a fine, derivative-filtered frit pattern, tint and
sun-dependent opacity. This approximates the appearance of a protective film;
it is not spectral UV transport. The minimum shader uses procedural roughness
and metalness in lighting, matching the full renderer's material response.
Changed sources/version invalidate the persistent program cache automatically.
