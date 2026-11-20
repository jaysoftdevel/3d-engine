#pragma once
#include <frontier/renderer.hpp>
#include <cmath>
#include <iostream>
#include <string>

// Shared by the graphics lab and optional showcase; no engine ABI changes.
namespace rf::examples {
class GraphicsSettingsOverlay {
    int page = 0;

public:
    void draw(gfx::Renderer &renderer, gfx::GraphicsSettings &settings,
              bool click, float mouseX, float mouseY, bool closeHint = false) {
        using namespace gfx;
        auto number = [](float value) {
            return std::isfinite(value) ? std::to_string(int(value)) : std::string("invalid");
        };
        renderer.rect(850, 20, 410, 678, {.02f, .045f, .065f, .70f});
        renderer.text(870, 38, "GRAPHICS SETTINGS / PAGES", 2, {.3f, .9f, .8f, 1});
        bool changed = false;
        float rowY = 81;
        auto row = [&](const std::string &label, const std::string &value, auto change) {
            bool hover = mouseX > 865 && mouseX < 1245 && mouseY >= rowY && mouseY < rowY + 28;
            // Keep the background at 70% opacity; avoid stacking opaque row fills.
            if (hover)
                renderer.rect(865, rowY, 3, 28, {.3f, .9f, .8f, 1});
            renderer.text(875, rowY + 9, label, 1.4f);
            renderer.text(1080, rowY + 9, value, 1.4f, {.3f, .9f, .8f, 1});
            if (hover && click) {
                change();
                changed = true;
            }
            rowY += 35;
        };
        auto quality = [](Quality &q) { q = Quality((int(q) + 1) % 5); };
        if (page == 0) {
            row("ANTI ALIASING", name(settings.antiAliasing),
                [&] { settings.antiAliasing = AntiAliasing((int(settings.antiAliasing) + 1) % 5); });
            row("MSAA SAMPLES", number(settings.msaaSamples), [&] {
                settings.msaaSamples = settings.msaaSamples >= 8 ? 2 : settings.msaaSamples * 2;
            });
            row("TEXTURE QUALITY", name(settings.textureQuality), [&] {
                settings.textureQuality = TextureQuality((int(settings.textureQuality) + 1) % 4);
            });
            row("ANISOTROPY", number(settings.anisotropy) + "X", [&] {
                settings.anisotropy = settings.anisotropy >= 16 ? 1 : settings.anisotropy * 2;
            });
            row("SHADOW QUALITY", name(settings.shadows), [&] { quality(settings.shadows); });
            row("VIEW DISTANCE", number(settings.viewDistance) + "M", [&] {
                settings.viewDistance =
                    settings.viewDistance >= 500 ? 80 : settings.viewDistance + 60;
            });
            row("AMBIENT OCCLUSION", name(settings.ambientOcclusion),
                [&] { quality(settings.ambientOcclusion); });
            row("DEPTH OF FIELD", name(settings.depthOfField),
                [&] { quality(settings.depthOfField); });
            row("FOCUS DISTANCE", number(settings.focusDistance) + "M", [&] {
                settings.focusDistance =
                    settings.focusDistance >= 45 ? 5 : settings.focusDistance + 5;
            });
            row("GLOBAL ILLUM.", name(settings.globalIllumination), [&] {
                settings.globalIllumination =
                    GlobalIllumination((int(settings.globalIllumination) + 1) % 3);
            });
            row("GI QUALITY", name(settings.illuminationQuality),
                [&] { quality(settings.illuminationQuality); });
            row("SCREEN REFLECTIONS", name(settings.reflections),
                [&] { quality(settings.reflections); });
            row("EDGE FUSION", name(settings.edgeFusion), [&] { quality(settings.edgeFusion); });
            row("TERRAIN TESSELL.", name(settings.terrainTessellation),
                [&] { quality(settings.terrainTessellation); });
            row("TERRAIN BACKEND", name(settings.tessellationBackend), [&] {
                settings.tessellationBackend =
                    TessellationBackend((int(settings.tessellationBackend) + 1) % 3);
            });
            row("WIREFRAME", settings.wireframe ? "on" : "off",
                [&] { settings.wireframe = !settings.wireframe; });
        } else {
            row("RENDER SCALE", number(settings.renderScale * 100) + "%", [&] {
                settings.renderScale =
                    settings.renderScale >= 1.5f ? .5f : settings.renderScale + .25f;
            });
            row("BLOOM", name(settings.bloom), [&] { quality(settings.bloom); });
            row("BLOOM STRENGTH", number(settings.bloomStrength * 100), [&] {
                settings.bloomStrength =
                    settings.bloomStrength >= .5f ? 0 : settings.bloomStrength + .1f;
            });
            row("AUTO EXPOSURE", settings.autoExposure ? "on" : "off",
                [&] { settings.autoExposure = !settings.autoExposure; });
            row("EXPOSURE EV", number(settings.exposureCompensation), [&] {
                settings.exposureCompensation =
                    settings.exposureCompensation >= 3 ? -3 : settings.exposureCompensation + 1;
            });
            row("TONE MAPPER", name(settings.toneMapper),
                [&] { settings.toneMapper = ToneMapper((int(settings.toneMapper) + 1) % 3); });
            row("CONTRAST", number(settings.contrast * 100), [&] {
                settings.contrast = settings.contrast >= 1.5f ? .5f : settings.contrast + .25f;
            });
            row("SATURATION", number(settings.saturation * 100), [&] {
                settings.saturation = settings.saturation >= 2 ? 0 : settings.saturation + .25f;
            });
            row("TEMPERATURE", number(settings.temperature * 100), [&] {
                settings.temperature = settings.temperature >= 1 ? -1 : settings.temperature + .25f;
            });
            row("VIGNETTE", number(settings.vignette * 100),
                [&] { settings.vignette = settings.vignette >= 1 ? 0 : settings.vignette + .25f; });
            row("SHARPENING", number(settings.sharpening * 100), [&] {
                settings.sharpening = settings.sharpening >= 1 ? 0 : settings.sharpening + .2f;
            });
            row("MOTION BLUR", name(settings.motionBlur), [&] { quality(settings.motionBlur); });
            row("VOLUMETRIC FOG", name(settings.volumetricFog),
                [&] { quality(settings.volumetricFog); });
            row("GPU PROFILING", settings.gpuProfiling ? "on" : "off",
                [&] { settings.gpuProfiling = !settings.gpuProfiling; });
        }
        if (click && mouseX > 850 && mouseX < 1260 && mouseY > 20 && mouseY < 70)
            page = 1 - page;
        renderer.text(870, 640, "CLICK HEADER FOR NEXT PAGE", 1.25f, {.5f, .65f, .7f, 1});
        renderer.text(870, 660, closeHint ? "CLICK VALUES / C TO CLOSE" : "CLICK VALUES TO CYCLE", 1.4f, {.5f, .65f, .7f, 1});
        const auto &effective = renderer.settings().effective;
        renderer.text(870, 683,
                      std::string("ACTIVE: ") + name(effective.antiAliasing) + " / " +
                          name(effective.tessellationBackend),
                      1.2f, {.5f, .65f, .7f, 1});
        if (changed) {
            auto report = renderer.setSettings(settings);
            for (auto &m : report.adjustments)
                std::cout << "Adjusted: " << m << '\n';
        }
    }
};
} // namespace rf::examples
