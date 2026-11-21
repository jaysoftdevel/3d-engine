# Graphics presets and autofocus

Press `C` in the showcase to open the graphics panel. Its five buttons apply **Minimal, Low, Medium, High, Max** immediately. Individual edits remain available on both pages; the panel then identifies the configuration as Custom. The background remains 30% transparent and text remains opaque. Click the header to change pages.

```bash
./run-showcase.sh --quality minimal
./run-showcase.sh --quality low
./run-showcase.sh --quality medium
./run-showcase.sh --quality high
./run-showcase.sh --quality max
./run-showcase.sh --quality high --write-settings my-graphics.ini
./run-showcase.sh --settings my-graphics.ini

# Graphics lab uses the same five preset names:
./run.sh --preset medium
./run.sh --preset max --write-settings max-settings.ini
```

The showcase also accepts `--preset`; `legacy`, `balanced`, and `ultra` remain aliases for Minimal, Medium, and Max. Explicit INI values load after preset selection. Configuration version 1 remains supported.

All presets use the native output resolution (`render_scale=1`) and textured models. No preset or runtime optimizer automatically reduces resolution, geometry, samples, effects or texture quality to chase a frame rate. Low tiers are explicit user choices.

| General engine preset | AA | Texture tier | Shadows | AO | Depth of field | View distance |
| --- | --- | --- | --- | --- | --- | --- |
| Minimal | Off | Lowest (64 px) | Off | Off | Off | 320 m |
| Low | FXAA | Low | Low | Off | Off | 100 m |
| Medium | FXAA | High | Medium | Medium | Off | 200 m |
| High | TAA | High | High | High | Off | 300 m |
| Max | TAA | Ultra | Ultra | Ultra | Ultra, autofocus | 500 m |

Matching general settings files are in `configs/minimal.ini`, `low.ini`, `medium.ini`, `high.ini`, and `max.ini`. Applications may use `rf::gfx::graphicsPreset(GraphicsPreset::High)` or the `GraphicsSettings::minimal/low/medium/high/max` factories. Existing `legacy`, `balanced`, and `ultra` factories remain available.

The showcase retains its scene-specific exposure, color, bloom and fog calibration across presets. Its High preset keeps the exact previously shipped default: 230 m view distance, Ultra shadows, Medium terrain/fog/reflections and TAA. This preserves its existing appearance. The Max preset explicitly increases technical quality and enables autofocus depth of field. Hardware limits are still reported through `SettingsReport`; unsupported features use the existing compatibility paths.

## Autofocus

When depth of field is enabled, autofocus is on by default. It measures a small nine-sample region around the camera's center using the resolved scene depth. The median rejects isolated edges; a 0.2% dead band avoids small focus fluctuations. Focus moves exponentially toward the subject using real frame time and reacquires immediately after a camera cut or history reset. Looking at empty sky moves focus toward the view-distance limit.

Focus selection and history stay in two 1x1 GPU textures. Normal frames never read focus back to the CPU and add one 1x1 pass only while autofocus depth of field is active. The blur retains the selected sample count, aperture, maximum radius and foreground rejection. Depth reconstruction uses the existing projection terms.

Page one contains **Autofocus** and **Focus response**. Turn Autofocus off to expose **Manual focus**. A larger response value adjusts faster. Settings save/load includes:

```ini
auto_focus=1
focus_adaptation=6
focus_distance=12
```

`focus_distance` applies in manual mode (`auto_focus=0`). The graphics lab also accepts `--auto-focus 0|1`, `--focus-speed VALUE`, and `--focus METERS`; an explicit `--focus` selects manual mode. The showcase’s `--debug-dir` can write the current GPU focus distance to `autofocus.txt` alongside the diagnostic images. This readback occurs only when diagnostics are requested.

## Processing and worker control

The engine sizes its background pool from the CPU cores available to the process, including Linux affinity restrictions. `JSD_WORKERS=N` sets a smaller background-worker budget; automatic render preparation also uses the calling render thread. Small command lists remain serial to avoid scheduling overhead. Large lists distribute their independent preparation while preserving draw order. OpenGL calls stay on the render thread.

```bash
JSD_WORKERS=4 ./run-showcase.sh --profile frame-times.csv
```

The profile retains its existing columns and adds `prepare_ms` and `workers`. These separate CPU command preparation from driver submission/GPU costs. New settings/statistics fields require rebuilding engine consumers; the ordinary source build rebuilds the engine and both examples together. See `PERFORMANCE.md` for measured results and limits.

Minimal uses a compact forward path with reduced surface/sky work and coarser distant terrain; see [Minimal rendering](MINIMAL_RENDERING.md). Other presets retain their rendering path.
