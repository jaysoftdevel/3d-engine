#include "frontier/settings.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace rf::gfx {
const char *name(GraphicsPreset preset) {
    static const char *names[] = {"minimal", "low", "medium", "high", "max"};
    return names[std::clamp(int(preset), 0, 4)];
}
bool parseGraphicsPreset(std::string_view text, GraphicsPreset &preset) {
    if (text == "legacy") text = "minimal";
    if (text == "balanced") text = "medium";
    if (text == "ultra") text = "max";
    for (int i = 0; i < 5; ++i)
        if (text == name(GraphicsPreset(i))) {
            preset = GraphicsPreset(i);
            return true;
        }
    return false;
}
GraphicsSettings GraphicsSettings::minimal() {
    auto s = legacy();
    s.textureQuality = TextureQuality::Lowest;
    s.anisotropy = 1;
    s.viewDistance = 320;
    return s;
}
GraphicsSettings GraphicsSettings::medium() { return balanced(); }
GraphicsSettings GraphicsSettings::max() {
    auto s = ultra();
    s.depthOfField = s.motionBlur = s.volumetricFog = s.edgeFusion = s.bloom = Quality::Ultra;
    s.terrainTessellation = Quality::Ultra;
    s.reflections = s.ambientOcclusion = s.illuminationQuality = Quality::Ultra;
    return s;
}
GraphicsSettings graphicsPreset(GraphicsPreset preset) {
    switch (preset) {
    case GraphicsPreset::Minimal: return GraphicsSettings::minimal();
    case GraphicsPreset::Low: return GraphicsSettings::low();
    case GraphicsPreset::Medium: return GraphicsSettings::medium();
    case GraphicsPreset::High: return GraphicsSettings::high();
    case GraphicsPreset::Max: return GraphicsSettings::max();
    }
    return GraphicsSettings::medium();
}

const char *name(AntiAliasing q) {
    static const char *n[] = {"off", "fxaa", "smaa", "msaa", "taa"};
    return n[std::clamp(int(q), 0, 4)];
}
const char *name(Quality q) {
    static const char *n[] = {"off", "low", "medium", "high", "ultra"};
    return n[std::clamp(int(q), 0, 4)];
}
const char *name(TextureQuality q) {
    static const char *n[] = {"low", "medium", "high", "ultra", "lowest"};
    return n[std::clamp(int(q), 0, 4)];
}
const char *name(GlobalIllumination q) {
    static const char *n[] = {"off", "ambient-probe", "screen-space"};
    return n[std::clamp(int(q), 0, 2)];
}
const char *name(TessellationBackend q) {
    static const char *n[] = {"automatic", "mesh", "hardware"};
    return n[std::clamp(int(q), 0, 2)];
}
const char *name(ToneMapper q) {
    static const char *n[] = {"aces", "reinhard", "neutral"};
    return n[std::clamp(int(q), 0, 2)];
}
int textureEdgeLimit(TextureQuality q){return std::array{512,1024,2048,4096,64}[std::clamp(int(q),0,4)];}
GraphicsSettings GraphicsSettings::legacy() {
    GraphicsSettings s;
    s.antiAliasing = AntiAliasing::Off;
    s.shadows = Quality::Off;
    s.ambientOcclusion = Quality::Off;
    s.globalIllumination = GlobalIllumination::Off;
    s.edgeFusion = Quality::Off;
    s.terrainTessellation = Quality::Off;
    s.viewDistance = 230;
    s.tessellationBackend = TessellationBackend::Mesh;
    return s;
}
GraphicsSettings GraphicsSettings::low() {
    auto s = legacy();
    s.antiAliasing = AntiAliasing::FXAA;
    s.textureQuality = TextureQuality::Low;
    s.anisotropy = 2;
    s.viewDistance = 100;
    s.shadows = Quality::Low;
    s.globalIllumination = GlobalIllumination::AmbientProbe;
    s.terrainTessellation = Quality::Low;
    return s;
}
GraphicsSettings GraphicsSettings::balanced() {
    return {};
}
GraphicsSettings GraphicsSettings::high() {
    auto s = balanced();
    s.antiAliasing = AntiAliasing::TAA;
    s.bloom = Quality::High;
    s.autoExposure = true;
    s.volumetricFog = Quality::Medium;
    s.textureQuality = TextureQuality::High;
    s.anisotropy = 16;
    s.shadows = Quality::High;
    s.ambientOcclusion = Quality::High;
    s.globalIllumination = GlobalIllumination::ScreenSpace;
    s.illuminationQuality = Quality::High;
    s.reflections = Quality::High;
    s.edgeFusion = Quality::Medium;
    s.terrainTessellation = Quality::High;
    s.viewDistance = 300;
    return s;
}
GraphicsSettings GraphicsSettings::ultra() {
    auto s = high();
    s.textureQuality = TextureQuality::Ultra;
    s.shadows = Quality::Ultra;
    s.ambientOcclusion = Quality::Ultra;
    s.illuminationQuality = Quality::Ultra;
    s.reflections = Quality::Ultra;
    s.edgeFusion = Quality::High;
    s.terrainTessellation = Quality::Ultra;
    s.viewDistance = 500;
    return s;
}
SettingsReport resolveSettings(GraphicsSettings requested, const Capabilities &c) {
    SettingsReport r{requested, requested, {}};
    auto &s = r.effective;
    auto note = [&](std::string n) { r.adjustments.push_back(std::move(n)); };
    auto quality = [&](Quality &q) {
        int v = std::clamp(int(q), 0, 4);
        if (v != int(q))
            note("Invalid quality value clamped");
        q = Quality(v);
    };
    for (auto *q : {&s.shadows, &s.ambientOcclusion, &s.depthOfField, &s.illuminationQuality, &s.reflections,
                    &s.edgeFusion, &s.terrainTessellation, &s.bloom, &s.motionBlur, &s.volumetricFog})
        quality(*q);
    s.antiAliasing = AntiAliasing(std::clamp(int(s.antiAliasing), 0, 4));
    s.textureQuality = TextureQuality(std::clamp(int(s.textureQuality), 0, 4));
    s.globalIllumination = GlobalIllumination(std::clamp(int(s.globalIllumination), 0, 2));
    s.tessellationBackend = TessellationBackend(std::clamp(int(s.tessellationBackend), 0, 2));
    auto originalTexture = s.textureQuality;
    if(textureEdgeLimit(s.textureQuality)>c.maxTextureSize){auto chosen=TextureQuality::Lowest;for(auto tier:{TextureQuality::Lowest,TextureQuality::Low,TextureQuality::Medium,TextureQuality::High,TextureQuality::Ultra})if(textureEdgeLimit(tier)<=c.maxTextureSize&&textureEdgeLimit(tier)<=textureEdgeLimit(originalTexture))chosen=tier;s.textureQuality=chosen;}
    if (s.textureQuality != originalTexture)
        note("Texture quality reduced to maximum driver texture size");
    auto originalShadow = s.shadows;
    while (int(s.shadows) > 0 && std::array{0, 512, 1024, 2048, 4096}[int(s.shadows)] > c.maxTextureSize)
        s.shadows = Quality(int(s.shadows) - 1);
    if (s.shadows != originalShadow)
        note("Shadow quality reduced to maximum driver texture size");
    auto number = [&](float &v, float lo, float hi, float fallback) {
        if (!std::isfinite(v)) {
            v = fallback;
            note("Nonfinite setting replaced");
        }
        float n = std::clamp(v, lo, hi);
        if (n != v)
            note("Numeric setting clamped");
        v = n;
    };
    number(s.motionBlurStrength, 0, 2, .5f);
    number(s.fogDensity, 0, .2f, .015f);
    number(s.fogHeightFalloff, 0, 2, .12f);
    number(s.renderScale, .5f, 2, 1);
    number(s.sharpening, 0, 1, .15f);
    number(s.temporalWeight, 0, .97f, .9f);
    number(s.bloomThreshold, 0, 20, 1);
    number(s.bloomStrength, 0, 3, .12f);
    number(s.exposureCompensation, -10, 10, 0);
    number(s.exposureAdaptation, .1f, 20, 3);
    number(s.contrast, .1f, 3, 1);
    number(s.saturation, 0, 3, 1);
    number(s.temperature, -1, 1, 0);
    number(s.vignette, 0, 1, 0);
    s.toneMapper = ToneMapper(std::clamp(int(s.toneMapper), 0, 2));
    number(s.viewDistance, 32, 800, 200);
    number(s.focusDistance, .2f, 800, 12);
    number(s.focusAdaptation, .1f, 30, 6);
    number(s.aperture, .1f, 16, 2);
    number(s.maximumBlur, 0, 24, 8);
    int asked = s.msaaSamples;
    s.msaaSamples = 1;
    for (int n : {2, 4, 8})
        if (n <= asked && n <= c.maxSamples)
            s.msaaSamples = n;
    if (s.msaaSamples != asked)
        note("MSAA sample count limited to supported 1/2/4/8 values");
    if (s.antiAliasing == AntiAliasing::MSAA && s.msaaSamples < 2) {
        s.antiAliasing = AntiAliasing::FXAA;
        note("MSAA unavailable; FXAA selected");
    }
    int aniso = 1;
    for (int n : {2, 4, 8, 16})
        if (n <= s.anisotropy && n <= c.maxAnisotropy && c.anisotropicFiltering)
            aniso = n;
    if (aniso != s.anisotropy)
        note("Anisotropy limited by driver capability");
    s.anisotropy = aniso;
    if (s.tessellationBackend == TessellationBackend::Hardware && !c.hardwareTessellation) {
        s.tessellationBackend = TessellationBackend::Mesh;
        note("Hardware tessellation unavailable; subdivided mesh selected");
    }
    if (s.tessellationBackend == TessellationBackend::Automatic)
        s.tessellationBackend =
            c.hardwareTessellation ? TessellationBackend::Hardware : TessellationBackend::Mesh;
    return r;
}
bool saveSettings(const std::filesystem::path &p, const GraphicsSettings &s, std::string &error) {
    try {
        if (p.has_parent_path())
            std::filesystem::create_directories(p.parent_path());
        auto temp = p;
        temp += ".tmp";
        std::ofstream o(temp);
        if (!o)
            throw std::runtime_error("Cannot open settings file");
        o << std::setprecision(9) << "version=1\nanti_aliasing=" << name(s.antiAliasing)
          << "\nmsaa_samples=" << s.msaaSamples << "\ntexture_quality=" << name(s.textureQuality)
          << "\nanisotropy=" << s.anisotropy << "\nshadows=" << name(s.shadows)
          << "\nview_distance=" << s.viewDistance << "\nambient_occlusion=" << name(s.ambientOcclusion)
          << "\ndepth_of_field=" << name(s.depthOfField) << "\nfocus_distance=" << s.focusDistance
          << "\naperture=" << s.aperture << "\nmaximum_blur=" << s.maximumBlur
          << "\nglobal_illumination=" << name(s.globalIllumination)
          << "\nillumination_quality=" << name(s.illuminationQuality)
          << "\nreflections=" << name(s.reflections) << "\nedge_fusion=" << name(s.edgeFusion)
          << "\nterrain_tessellation=" << name(s.terrainTessellation)
          << "\ntessellation_backend=" << name(s.tessellationBackend) << "\nwireframe=" << s.wireframe
          << '\n';
        o << "render_scale=" << s.renderScale << "\nsharpening=" << s.sharpening
          << "\ntemporal_weight=" << s.temporalWeight << "\nbloom=" << name(s.bloom)
          << "\nbloom_threshold=" << s.bloomThreshold << "\nbloom_strength=" << s.bloomStrength
          << "\nauto_exposure=" << s.autoExposure << "\nexposure_compensation=" << s.exposureCompensation
          << "\nexposure_adaptation=" << s.exposureAdaptation << "\ntone_mapper=" << name(s.toneMapper)
          << "\ncontrast=" << s.contrast << "\nsaturation=" << s.saturation
          << "\ntemperature=" << s.temperature << "\nvignette=" << s.vignette
          << "\ngpu_profiling=" << s.gpuProfiling << '\n';
        o << "motion_blur=" << name(s.motionBlur) << "\nmotion_blur_strength=" << s.motionBlurStrength
          << "\nvolumetric_fog=" << name(s.volumetricFog) << "\nfog_density=" << s.fogDensity
          << "\nfog_height_falloff=" << s.fogHeightFalloff << '\n';
        o << "auto_focus=" << s.autoFocus << "\nfocus_adaptation=" << s.focusAdaptation << '\n';
        o.close();
        if (!o)
            throw std::runtime_error("Settings write failed");
        std::filesystem::rename(temp, p);
        return true;
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
bool loadSettings(const std::filesystem::path &p, GraphicsSettings &result, std::string &error) {
    try {
        std::ifstream in(p);
        if (!in)
            throw std::runtime_error("Cannot read settings");
        GraphicsSettings s = result;
        std::string line;
        bool version = false;
        size_t size = 0;
        auto parseEnum = []<class E>(const std::string &v, E &out, int max) {
            for (int n = 0; n <= max; n++)
                if (v == name(E(n))) {
                    out = E(n);
                    return;
                }
            throw std::runtime_error("Unknown setting enum: " + v);
        };
        auto integer = [](const std::string &v) {
            size_t n = 0;
            int x = std::stoi(v, &n);
            if (n != v.size())
                throw std::runtime_error("Invalid integer setting");
            return x;
        };
        auto real = [](const std::string &v) {
            size_t n = 0;
            float x = std::stof(v, &n);
            if (n != v.size() || !std::isfinite(x))
                throw std::runtime_error("Invalid float setting");
            return x;
        };
        while (std::getline(in, line)) {
            size += line.size();
            if (size > 65536)
                throw std::runtime_error("Settings file too large");
            if (line.empty() || line[0] == '#')
                continue;
            auto equal = line.find('=');
            if (equal == std::string::npos)
                throw std::runtime_error("Expected key=value");
            auto key = line.substr(0, equal), v = line.substr(equal + 1);
            if (!v.empty() && v.back() == '\r')
                v.pop_back();
            if (key == "version") {
                if (integer(v) != 1)
                    throw std::runtime_error("Unsupported settings version");
                version = true;
            } else if (key == "anti_aliasing")
                parseEnum(v, s.antiAliasing, 4);
            else if (key == "msaa_samples")
                s.msaaSamples = integer(v);
            else if (key == "texture_quality")
                parseEnum(v, s.textureQuality, 4);
            else if (key == "anisotropy")
                s.anisotropy = integer(v);
            else if (key == "view_distance")
                s.viewDistance = real(v);
            else if (key == "auto_focus") {
                int value = integer(v);
                if (value != 0 && value != 1) throw std::runtime_error("invalid autofocus boolean");
                s.autoFocus = value;
            } else if (key == "focus_adaptation")
                s.focusAdaptation = real(v);
            else if (key == "focus_distance")
                s.focusDistance = real(v);
            else if (key == "aperture")
                s.aperture = real(v);
            else if (key == "maximum_blur")
                s.maximumBlur = real(v);
            else if (key == "shadows")
                parseEnum(v, s.shadows, 4);
            else if (key == "ambient_occlusion")
                parseEnum(v, s.ambientOcclusion, 4);
            else if (key == "depth_of_field")
                parseEnum(v, s.depthOfField, 4);
            else if (key == "global_illumination")
                parseEnum(v, s.globalIllumination, 2);
            else if (key == "illumination_quality")
                parseEnum(v, s.illuminationQuality, 4);
            else if (key == "reflections")
                parseEnum(v, s.reflections, 4);
            else if (key == "edge_fusion")
                parseEnum(v, s.edgeFusion, 4);
            else if (key == "terrain_tessellation")
                parseEnum(v, s.terrainTessellation, 4);
            else if (key == "tessellation_backend")
                parseEnum(v, s.tessellationBackend, 2);
            else if (key == "render_scale")
                s.renderScale = real(v);
            else if (key == "sharpening")
                s.sharpening = real(v);
            else if (key == "temporal_weight")
                s.temporalWeight = real(v);
            else if (key == "bloom")
                parseEnum(v, s.bloom, 4);
            else if (key == "bloom_threshold")
                s.bloomThreshold = real(v);
            else if (key == "bloom_strength")
                s.bloomStrength = real(v);
            else if (key == "auto_exposure" || key == "gpu_profiling") {
                int value = integer(v);
                if (value != 0 && value != 1)
                    throw std::runtime_error("invalid boolean");
                if (key == "auto_exposure")
                    s.autoExposure = value;
                else
                    s.gpuProfiling = value;
            } else if (key == "exposure_compensation")
                s.exposureCompensation = real(v);
            else if (key == "exposure_adaptation")
                s.exposureAdaptation = real(v);
            else if (key == "tone_mapper")
                parseEnum(v, s.toneMapper, 2);
            else if (key == "contrast")
                s.contrast = real(v);
            else if (key == "saturation")
                s.saturation = real(v);
            else if (key == "temperature")
                s.temperature = real(v);
            else if (key == "vignette")
                s.vignette = real(v);
            else if (key == "motion_blur")
                parseEnum(v, s.motionBlur, 4);
            else if (key == "motion_blur_strength")
                s.motionBlurStrength = real(v);
            else if (key == "volumetric_fog")
                parseEnum(v, s.volumetricFog, 4);
            else if (key == "fog_density")
                s.fogDensity = real(v);
            else if (key == "fog_height_falloff")
                s.fogHeightFalloff = real(v);
            else if (key == "wireframe") {
                int b = integer(v);
                if (b != 0 && b != 1)
                    throw std::runtime_error("Invalid boolean setting");
                s.wireframe = b;
            }
        }
        if (!version)
            throw std::runtime_error("Missing settings version");
        result = s;
        return true;
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
} // namespace rf::gfx
