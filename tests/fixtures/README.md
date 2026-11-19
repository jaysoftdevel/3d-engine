# Engine decoder fixtures

These tiny PNG, Radiance HDR, WAV and animated/skinned glTF files are engine test input data. They are independent of the optional internal showcase. Regenerate from the engine directory with `python3 tests/generate_fixtures.py` (Python standard library only).

The source files are included so no generator step is needed to build/test. When tests are enabled, CMake installs copies under `share/jsd-game-engine/test-fixtures`. The regression executable first uses those installed fixtures relative to its prefix, then the source fixture path for build-tree testing. Missing fixtures fail clearly rather than silently skipping checks.
