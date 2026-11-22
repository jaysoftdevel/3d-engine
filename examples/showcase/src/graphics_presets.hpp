#pragma once
#include <frontier/settings.hpp>

namespace sundog {
inline rf::gfx::GraphicsSettings graphicsPreset(rf::gfx::GraphicsPreset preset) {
    using namespace rf::gfx;
    auto settings = rf::gfx::graphicsPreset(preset);
    // Scene calibration is independent of the selected technical quality tier.
    settings.bloomThreshold = 2;
    settings.bloomStrength = .065f;
    settings.autoExposure = preset>=GraphicsPreset::Medium;
    settings.exposureCompensation = -.25f;
    settings.exposureAdaptation=3.5f;settings.minExposureEV=-2.5f;settings.maxExposureEV=1.5f;
    settings.localExposure=preset>=GraphicsPreset::High?.16f:0;
    settings.fogDensity = .00065f;
    settings.fogHeightFalloff = .065f;
    settings.motionBlurStrength = .15f;
    settings.depthOfField=Quality::Off;settings.motionBlur=Quality::Off;
    settings.autoFocus=true;settings.focusAdaptation=2.5f;
    settings.aperture=.35f;settings.maximumBlur=2.5f;
    settings.renderScale=1;
    settings.vignette = .12f;
    settings.contrast = 1.04f;
    settings.saturation = .98f;
    settings.temporalWeight = .7f;
    settings.sharpening = .22f;
    settings.gpuProfiling = true;
    if (preset == GraphicsPreset::High) {
        // Allocate higher quality to the new hybrid lighting and reflections.
        settings.viewDistance = 230;
        settings.terrainTessellation = Quality::Medium;
        settings.edgeFusion = Quality::Off;
        settings.shadows = Quality::Ultra;
        settings.bloom = Quality::High;
        settings.volumetricFog = Quality::Medium;
        settings.motionBlur = Quality::Off;
        settings.depthOfField = Quality::Off;
        settings.reflections = Quality::High;
        settings.globalIllumination=GlobalIllumination::Hybrid;
        settings.stochasticLighting=true;settings.directLightSamples=8;
    }
    return settings;
}
} // namespace sundog
