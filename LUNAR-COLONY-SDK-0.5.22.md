# Lunar-colony SDK cleanup (0.5.22)

Engine compatibility version remains 0.5.22. Both native and editable Sundog content, installed project directories, sample terrain code, standalone lab/player apps, and old validation artifacts are removed. The updater archives obsolete directories including any locally added projects to an external sibling backup. It also archives guarded local build caches to prevent stale install scripts recreating the projects.

The renderer, shader sources, material recipes, high/max presets, terrain, physics, SDL2, Jolt, audio, asset loading and generic project APIs are unchanged. Generic regression sources and fixtures remain available with `./test.sh` or `build.bat --test`; they are not installed. The legacy Sundog terrain generator identifiers are no longer accepted by project validation. Generic editor APIs remain; the editor itself is not modified or tested.

Rebuild Lunar Colony against this source tree. For custom external install prefixes or custom build directories, rerun the appropriate clean command with the original environment overrides before rebuilding; this updater only archives the source-local outputs.
