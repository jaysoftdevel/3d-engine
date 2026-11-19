#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace rf::gfx {
enum class AntiAliasing { Off, FXAA, SMAA, MSAA, TAA };
enum class ToneMapper { ACES, Reinhard, Neutral };
enum class Quality { Off, Low, Medium, High, Ultra };
enum class TextureQuality { Low, Medium, High, Ultra };
enum class GlobalIllumination { Off, AmbientProbe, ScreenSpace };
enum class TessellationBackend { Automatic, Mesh, Hardware };

struct GraphicsSettings {
    AntiAliasing antiAliasing = AntiAliasing::FXAA;
    int msaaSamples = 4;
    TextureQuality textureQuality = TextureQuality::High;
    int anisotropy = 8;
    Quality shadows = Quality::Medium;
    float viewDistance = 200;
    Quality ambientOcclusion = Quality::Medium;
    Quality depthOfField = Quality::Off;
    float focusDistance = 12, aperture = 2, maximumBlur = 8;
    GlobalIllumination globalIllumination = GlobalIllumination::AmbientProbe;
    Quality illuminationQuality = Quality::Medium;
    Quality reflections = Quality::Off;
    Quality edgeFusion = Quality::Low;
    Quality terrainTessellation = Quality::Medium;
    TessellationBackend tessellationBackend = TessellationBackend::Automatic;
    bool wireframe = false;
    float renderScale = 1, sharpening = .15f, temporalWeight = .9f;
    Quality bloom = Quality::Off;
    float bloomThreshold = 1, bloomStrength = .12f;
    bool autoExposure = false, gpuProfiling = false;
    float exposureCompensation = 0, exposureAdaptation = 3;
    ToneMapper toneMapper = ToneMapper::ACES;
    float contrast = 1, saturation = 1, temperature = 0, vignette = 0;
    Quality motionBlur = Quality::Off, volumetricFog = Quality::Off;
    float motionBlurStrength = .5f, fogDensity = .015f, fogHeightFalloff = .12f;
    bool operator==(const GraphicsSettings &) const = default;
    static GraphicsSettings legacy();
    static GraphicsSettings low();
    static GraphicsSettings balanced();
    static GraphicsSettings high();
    static GraphicsSettings ultra();
};

struct Capabilities {
    std::string renderer, version;
    int major = 3, minor = 3, maxSamples = 1, maxTextureSize = 2048, maxTessellationLevel = 1;
    float maxAnisotropy = 1;
    bool anisotropicFiltering = false, hardwareTessellation = false;
    bool temporalAA = true, imageBasedLighting = true, localLights = true;
    bool hardwareRayTracing = false, vulkanBackend = false, vendorUpscaling = false, frameGeneration = false;
};
struct SettingsReport {
    GraphicsSettings requested, effective;
    std::vector<std::string> adjustments;
};
SettingsReport resolveSettings(GraphicsSettings requested, const Capabilities &caps);
const char *name(AntiAliasing value);
const char *name(Quality value);
const char *name(TextureQuality value);
const char *name(GlobalIllumination value);
const char *name(TessellationBackend value);
const char *name(ToneMapper value);
bool saveSettings(const std::filesystem::path &, const GraphicsSettings &, std::string &error);
bool loadSettings(const std::filesystem::path &, GraphicsSettings &, std::string &error);
} // namespace rf::gfx
