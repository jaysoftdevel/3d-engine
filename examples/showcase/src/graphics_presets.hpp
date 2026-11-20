#pragma once
#include <frontier/settings.hpp>

namespace sundog {
inline rf::gfx::GraphicsSettings graphicsPreset(rf::gfx::GraphicsPreset preset) {
    using namespace rf::gfx;
    auto settings = rf::gfx::graphicsPreset(preset);
    // Scene calibration is independent of the selected technical quality tier.
    settings.bloomThreshold = 2;
    settings.bloomStrength = .065f;
    settings.autoExposure = false;
    settings.exposureCompensation = -.25f;
    settings.fogDensity = .00065f;
    settings.fogHeightFalloff = .065f;
    settings.motionBlurStrength = .2f;
    settings.vignette = .12f;
    settings.contrast = 1.02f;
    settings.saturation = 1.03f;
    settings.temporalWeight = .86f;
    settings.sharpening = .22f;
    settings.gpuProfiling = true;
    if (preset == GraphicsPreset::High) {
        // Retain the exact previously shipped default showcase appearance.
        settings.viewDistance = 230;
        settings.terrainTessellation = Quality::Medium;
        settings.edgeFusion = Quality::Off;
        settings.shadows = Quality::Ultra;
        settings.bloom = Quality::High;
        settings.volumetricFog = Quality::Medium;
        settings.motionBlur = Quality::Low;
        settings.depthOfField = Quality::Off;
        settings.reflections = Quality::Medium;
    }
    return settings;
}
} // namespace sundog
