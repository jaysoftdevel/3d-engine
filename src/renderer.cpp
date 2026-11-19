#include "../third_party/smaa/Textures/SearchTex.h"
#include "internal.hpp"
#include "shaders.hpp"
#include "smaa_area.hpp"
#include "smaa_shader.hpp"
#include <cstddef>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

namespace rf::gfx {
using namespace vl;
namespace {
Context fromPlatform(Platform &p) {
    return {[&p](const char *n) { return p.resolve(n); },
            [&p] {
                p.dimensions();
                return std::pair{p.width, p.height};
            },
            p.offscreen};
}
} // namespace
Renderer::Renderer(Platform &p, GraphicsSettings s) : Renderer(fromPlatform(p), s) {}
Renderer::Renderer(Context c, GraphicsSettings s) : impl(std::make_unique<Impl>(std::move(c), s)) {
    impl->primitives[0] = createMesh(makeBox());
    impl->primitives[1] = createMesh(makeRock());
    impl->primitives[2] = createMesh(makeCylinder());
}
Renderer::~Renderer() = default;
Renderer::Impl::Impl(Context c, GraphicsSettings s) : context(std::move(c)) {
    if (!context.resolve || !context.dimensions)
        throw std::invalid_argument("Context requires resolver and dimensions callbacks");
    gl.load(context.resolve);
    caps.renderer = reinterpret_cast<const char *>(gl.GetString(GL_RENDERER));
    caps.version = reinterpret_cast<const char *>(gl.GetString(GL_VERSION));
    gl.GetIntegerv(0x821B, &caps.major);
    gl.GetIntegerv(0x821C, &caps.minor);
    if (caps.major < 3 || (caps.major == 3 && caps.minor < 3))
        throw std::runtime_error("Frontier requires desktop OpenGL 3.3 or newer");
    gl.GetIntegerv(0x0D33, &caps.maxTextureSize);
    gl.GetIntegerv(0x8D57, &caps.maxSamples);
    int colorSamples = 1, depthSamples = 1;
    gl.GetIntegerv(0x910E, &colorSamples);
    gl.GetIntegerv(0x910F, &depthSamples);
    caps.maxSamples = std::max(1, std::min({caps.maxSamples, colorSamples, depthSamples}));
    int extensions = 0;
    gl.GetIntegerv(0x821D, &extensions);
    for (int i = 0; i < extensions; i++) {
        std::string e = reinterpret_cast<const char *>(gl.GetStringi(0x1F03, i));
        if (e == "GL_EXT_texture_filter_anisotropic" || e == "GL_ARB_texture_filter_anisotropic")
            caps.anisotropicFiltering = true;
    }
    if (caps.anisotropicFiltering)
        gl.GetFloatv(0x84FF, &caps.maxAnisotropy);
    caps.hardwareTessellation = (caps.major >= 4) && gl.PatchParameteri;
    if (caps.hardwareTessellation)
        gl.GetIntegerv(0x8E7E, &caps.maxTessellationLevel);
    report = resolveSettings(s, caps);
    gl.GenBuffers(1, &instanceBuffer);
    gl.GenVertexArrays(1, &fullscreenVao);
    sceneProgram = program(shader::meshVertex, shader::sceneFragment);
    shadowProgram = program(shader::meshVertex, shader::shadowFragment);
    if (caps.hardwareTessellation) {
        std::string fs = shader::sceneFragment;
        fs.replace(0, 17, "#version 400 core");
        tessProgram = program(shader::tessVertex, fs, shader::tessControl, shader::tessEvaluation);
        tessShadowProgram = program(shader::tessVertex, "#version 400 core\nvoid main(){}",
                                    shader::tessControl, shader::tessEvaluation);
    }
    aoProgram = program(shader::fullscreen, shader::aoFragment);
    aoBlurProgram = program(shader::fullscreen, shader::aoBlur);
    lightingProgram = program(shader::fullscreen, shader::lightingFragment);
    dofProgram = program(shader::fullscreen, shader::dofFragment);
    tonemapProgram = program(shader::fullscreen, shader::tonemapFragment);
    copyProgram = program(shader::fullscreen, shader::copyFragment);
    fxaaProgram = program(shader::fullscreen, shader::fxaaFragment);
    uiProgram = program(shader::uiVertex, shader::uiFragment);
    gl.GenVertexArrays(1, &uiVao);
    gl.GenBuffers(1, &uiVbo);
    gl.BindVertexArray(uiVao);
    gl.BindBuffer(GL_ARRAY_BUFFER, uiVbo);
    gl.EnableVertexAttribArray(0);
    gl.VertexAttribPointer(0, 2, GL_FLOAT, 0, sizeof(UiVertex), nullptr);
    gl.EnableVertexAttribArray(1);
    gl.VertexAttribPointer(1, 4, GL_FLOAT, 0, sizeof(UiVertex),
                           reinterpret_cast<void *>(offsetof(UiVertex, c)));
    initializeSmaa();
    initializeAdvanced();
    initializeCinematic();
}
Renderer::Impl::~Impl() {
    releaseCinematic();
    releaseAdvanced();
    releaseTargets();
    for (auto &[k, t] : tiles) {
        (void)k;
        destroy(t);
    }
    for (auto &[id, m] : meshes) {
        (void)id;
        destroy(m);
    }
    for (auto &[id, t] : textures) {
        (void)id;
        gl.DeleteTextures(1, &t.object);
    }
    for (unsigned p : {sceneProgram, shadowProgram, tessProgram, tessShadowProgram, aoProgram, aoBlurProgram,
                       lightingProgram, dofProgram, tonemapProgram, copyProgram, fxaaProgram, uiProgram,
                       smaaEdgeProgram, smaaWeightProgram, smaaBlendProgram})
        if (p)
            gl.DeleteProgram(p);
    gl.DeleteTextures(1, &smaaArea);
    gl.DeleteTextures(1, &smaaSearch);
    gl.DeleteBuffers(1, &instanceBuffer);
    gl.DeleteBuffers(1, &uiVbo);
    gl.DeleteVertexArrays(1, &uiVao);
    gl.DeleteVertexArrays(1, &fullscreenVao);
}
unsigned Renderer::Impl::program(const std::string &vs, const std::string &fs, const char *tcs,
                                 const char *tes) {
    std::vector<unsigned> shaders;
    unsigned p = gl.CreateProgram();
    try {
        auto attach = [&](unsigned type, const std::string &source) {
            unsigned s = gl.CreateShader(type);
            shaders.push_back(s);
            const char *text = source.c_str();
            gl.ShaderSource(s, 1, &text, nullptr);
            gl.CompileShader(s);
            int ok = 0;
            gl.GetShaderiv(s, GL_COMPILE_STATUS, &ok);
            if (!ok) {
                char message[8192]{};
                gl.GetShaderInfoLog(s, sizeof(message), nullptr, message);
                throw std::runtime_error(std::string("Shader compile: ") + message);
            }
            gl.AttachShader(p, s);
        };
        attach(GL_VERTEX_SHADER, vs);
        if (tcs)
            attach(0x8E88, tcs);
        if (tes)
            attach(0x8E87, tes);
        attach(GL_FRAGMENT_SHADER, fs);
        gl.LinkProgram(p);
        int ok = 0;
        gl.GetProgramiv(p, GL_LINK_STATUS, &ok);
        if (!ok) {
            char message[8192]{};
            gl.GetProgramInfoLog(p, sizeof(message), nullptr, message);
            throw std::runtime_error(std::string("Shader link: ") + message);
        }
        for (auto s : shaders)
            gl.DeleteShader(s);
        return p;
    } catch (...) {
        for (auto s : shaders)
            gl.DeleteShader(s);
        gl.DeleteProgram(p);
        throw;
    }
}
GpuMesh Renderer::Impl::upload(std::span<const Vertex> v) {
    GpuMesh m;
    m.count = v.size();
    gl.GenVertexArrays(1, &m.vao);
    gl.GenBuffers(1, &m.vbo);
    gl.BindVertexArray(m.vao);
    gl.BindBuffer(GL_ARRAY_BUFFER, m.vbo);
    gl.BufferData(GL_ARRAY_BUFFER, v.size_bytes(), v.data(), 0x88E4);
    size_t offsets[] = {offsetof(Vertex, p), offsetof(Vertex, n), offsetof(Vertex, c), offsetof(Vertex, u)};
    for (unsigned i = 0; i < 4; i++) {
        gl.EnableVertexAttribArray(i);
        gl.VertexAttribPointer(i,
                               i == 2   ? 4
                               : i == 3 ? 2
                                        : 3,
                               GL_FLOAT, 0, sizeof(Vertex), reinterpret_cast<void *>(offsets[i]));
    }
    gl.BindBuffer(GL_ARRAY_BUFFER, instanceBuffer);
    for (unsigned col = 0; col < 4; col++)
        for (unsigned kind = 0; kind < 2; kind++) {
            unsigned attribute = 4 + kind * 4 + col;
            gl.EnableVertexAttribArray(attribute);
            gl.VertexAttribPointer(attribute, 4, GL_FLOAT, 0, sizeof(GpuInstance),
                                   reinterpret_cast<void *>((kind ? offsetof(GpuInstance, previous)
                                                                  : offsetof(GpuInstance, model)) +
                                                            col * 4 * sizeof(float)));
            gl.VertexAttribDivisor(attribute, 1);
        }
    gl.EnableVertexAttribArray(12);
    gl.VertexAttribPointer(12, 4, GL_FLOAT, 0, sizeof(GpuInstance),
                           reinterpret_cast<void *>(offsetof(GpuInstance, color)));
    gl.VertexAttribDivisor(12, 1);
    if (!v.empty()) {
        V3 lo = v[0].p, hi = lo;
        for (auto &p : v) {
            lo = {std::min(lo.x, p.p.x), std::min(lo.y, p.p.y), std::min(lo.z, p.p.z)};
            hi = {std::max(hi.x, p.p.x), std::max(hi.y, p.p.y), std::max(hi.z, p.p.z)};
        }
        m.center = (lo + hi) * .5f;
        for (auto &p : v)
            m.radius = std::max(m.radius, distance(p.p, m.center));
    }
    return m;
}
void Renderer::Impl::destroy(GpuMesh &m) {
    if (m.vbo)
        gl.DeleteBuffers(1, &m.vbo);
    if (m.vao)
        gl.DeleteVertexArrays(1, &m.vao);
    m = {};
}
void Renderer::Impl::destroy(Tile &t) {
    destroy(t.mesh);
    if (t.heightMap)
        gl.DeleteTextures(1, &t.heightMap);
    t = {};
}
unsigned Renderer::Impl::texture(int w, int h, unsigned internal, unsigned format, unsigned type,
                                 const void *data) {
    unsigned tex = 0;
    gl.GenTextures(1, &tex);
    gl.BindTexture(GL_TEXTURE_2D, tex);
    gl.TexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, format, type, data);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}
void Renderer::Impl::upload(GpuTexture &t) {
    int cap =
        std::min(caps.maxTextureSize, std::array{512, 1024, 2048, 4096}[int(settings().textureQuality)]);
    float scale = std::min(1.f, float(cap) / std::max(t.width, t.height));
    int w = std::max(1, int(t.width * scale)), h = std::max(1, int(t.height * scale));
    std::vector<unsigned char> pixels(size_t(w) * h * 4);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int x0 = x * t.width / w, x1 = std::max(x0 + 1, (x + 1) * t.width / w), y0 = y * t.height / h,
                y1 = std::max(y0 + 1, (y + 1) * t.height / h);
            unsigned sum[4]{};
            for (int sy = y0; sy < y1; sy++)
                for (int sx = x0; sx < x1; sx++)
                    for (int c = 0; c < 4; c++)
                        sum[c] += t.pixels[(sy * t.width + sx) * 4 + c];
            unsigned count = (x1 - x0) * (y1 - y0);
            for (int c = 0; c < 4; c++)
                pixels[(y * w + x) * 4 + c] = sum[c] / count;
        }
    if (!t.object)
        gl.GenTextures(1, &t.object);
    gl.BindTexture(GL_TEXTURE_2D, t.object);
    gl.TexImage2D(GL_TEXTURE_2D, 0, t.srgb ? 0x8C43 : GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                  pixels.data());
    gl.GenerateMipmap(GL_TEXTURE_2D);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 0x2703);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x2901);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x2901);
    if (caps.anisotropicFiltering)
        gl.TexParameterf(GL_TEXTURE_2D, 0x84FE, settings().anisotropy);
    t.uploadedWidth = w;
    t.uploadedHeight = h;
}
const Capabilities &Renderer::capabilities() const {
    return impl->caps;
}
const SettingsReport &Renderer::settings() const {
    return impl->report;
}
const Statistics &Renderer::statistics() const {
    return impl->stats;
}
SettingsReport Renderer::setSettings(GraphicsSettings s) {
    auto old = impl->report.effective;
    auto next = resolveSettings(s, impl->caps);
    impl->report = next;
    if (old != next.effective) {
        impl->buffersDirty = true;
        impl->historyValid = false;
        if (old.textureQuality != next.effective.textureQuality ||
            old.anisotropy != next.effective.anisotropy)
            for (auto &[id, t] : impl->textures) {
                (void)id;
                impl->upload(t);
            }
    }
    return next;
}
MeshId Renderer::createMesh(std::span<const Vertex> v) {
    if (v.empty() || v.size() % 3 || v.size() > 3000000)
        throw std::invalid_argument("Mesh must contain a bounded triangle list");
    for (auto &p : v)
        for (float f : {p.p.x, p.p.y, p.p.z, p.n.x, p.n.y, p.n.z, p.c.r, p.c.g, p.c.b, p.c.a, p.u, p.v})
            if (!std::isfinite(f))
                throw std::invalid_argument("Mesh contains nonfinite vertices");
    for (auto &p : v)
        if (dot(p.n, p.n) < 1e-12f)
            throw std::invalid_argument("Mesh contains a zero-length normal");
    unsigned id = impl->nextMesh++;
    impl->meshes.emplace(id, impl->upload(v));
    return {id};
}
void Renderer::destroyMesh(MeshId id) {
    for (auto primitive : impl->primitives)
        if (primitive == id)
            throw std::invalid_argument("Built-in primitive meshes are engine-owned");
    auto it = impl->meshes.find(id.value);
    if (it != impl->meshes.end()) {
        impl->destroy(it->second);
        impl->meshes.erase(it);
    }
}
TextureId Renderer::createTexture(int w, int h, std::span<const unsigned char> bytes, bool srgb) {
    if (w <= 0 || h <= 0 || w > 16384 || h > 16384 || size_t(w) * h * 4 != bytes.size() ||
        bytes.size() > 256 * 1024 * 1024)
        throw std::invalid_argument("Invalid bounded RGBA texture");
    GpuTexture t;
    t.width = w;
    t.height = h;
    t.srgb = srgb;
    t.pixels.assign(bytes.begin(), bytes.end());
    impl->upload(t);
    unsigned id = impl->nextTexture++;
    impl->textures.emplace(id, std::move(t));
    return {id};
}
void Renderer::destroyTexture(TextureId id) {
    auto it = impl->textures.find(id.value);
    if (it != impl->textures.end()) {
        impl->gl.DeleteTextures(1, &it->second.object);
        impl->textures.erase(it);
    }
}
std::pair<int, int> Renderer::textureDimensions(TextureId id) const {
    auto it = impl->textures.find(id.value);
    if (it == impl->textures.end())
        return {};
    return {it->second.uploadedWidth, it->second.uploadedHeight};
}
void Renderer::begin(const Camera &camera, const Environment &environment) {
    auto &i = *impl;
    for (float f : {camera.position.x,          camera.position.y,
                    camera.position.z,          camera.direction.x,
                    camera.direction.y,         camera.direction.z,
                    environment.sunDirection.x, environment.sunDirection.y,
                    environment.sunDirection.z, environment.sunColor.r,
                    environment.sunColor.g,     environment.sunColor.b,
                    environment.skyColor.r,     environment.skyColor.g,
                    environment.skyColor.b,     environment.groundColor.r,
                    environment.groundColor.g,  environment.groundColor.b,
                    environment.sunIntensity,   environment.ambientIntensity,
                    environment.fogDensity,     environment.storm})
        if (!std::isfinite(f))
            throw std::invalid_argument("Nonfinite camera/environment state");
    if (!std::isfinite(camera.fovDegrees) || camera.fovDegrees < 10 || camera.fovDegrees > 150 ||
        !std::isfinite(camera.nearPlane) || camera.nearPlane <= 0 ||
        camera.nearPlane >= i.settings().viewDistance || length(camera.direction) < .001f)
        throw std::invalid_argument("Invalid camera projection");
    auto &a = environment.atmosphere;
    for (float f : {a.time, a.turbidity, a.cloudCoverage, a.cloudDensity, a.cloudBase, a.cloudThickness,
                    a.wind.x, a.wind.y, a.wind.z, environment.shadowDistance, environment.imageRotation})
        if (!std::isfinite(f))
            throw std::invalid_argument("Nonfinite atmosphere");
    if (int(a.clouds) < 0 || int(a.clouds) > 4 || a.cloudCoverage < 0 || a.cloudCoverage > 1 ||
        a.cloudDensity < 0 || a.cloudDensity > 10 || a.cloudThickness < 1 || a.cloudBase < 1 ||
        environment.shadowDistance < 0)
        throw std::invalid_argument("Invalid atmosphere/shadow range");
    i.camera = camera;
    i.camera.direction = normalized(camera.direction);
    i.environment = environment;
    i.commands.clear();
    i.lights.clear();
    i.particleCommands.clear();
    i.uiImages.clear();
    i.ui.clear();
    i.terrainRequested = false;
    i.rendered = false;
    i.stats = {};
    i.stats.terrainWorkers = i.jobs.count();
}
void Renderer::submit(const Draw &draw) {
    auto &i = *impl;
    if (!i.meshes.contains(draw.mesh.value))
        throw std::invalid_argument("Unknown mesh handle");
    if (draw.material.albedoTexture.value && !i.textures.contains(draw.material.albedoTexture.value))
        throw std::invalid_argument("Unknown texture handle");
    auto &t = draw.transform;
    for (float f : {t.position.x,
                    t.position.y,
                    t.position.z,
                    t.scale.x,
                    t.scale.y,
                    t.scale.z,
                    t.yaw,
                    draw.material.roughness,
                    draw.material.metallic,
                    draw.material.albedo.r,
                    draw.material.albedo.g,
                    draw.material.albedo.b,
                    draw.material.albedo.a,
                    draw.material.emissive.r,
                    draw.material.emissive.g,
                    draw.material.emissive.b,
                    t.pitch,
                    t.roll,
                    draw.material.textureScale,
                    draw.material.macroVariation,
                    draw.material.clearcoat,
                    draw.material.clearcoatRoughness,
                    draw.material.windStrength,
                    draw.material.occlusionStrength})
        if (!std::isfinite(f))
            throw std::invalid_argument("Nonfinite draw state");
    if (draw.material.occlusionStrength < 0 || draw.material.occlusionStrength > 1 ||
        draw.material.textureScale <= 0 || draw.material.macroVariation < 0 ||
        draw.material.macroVariation > 1 || draw.material.clearcoat < 0 || draw.material.clearcoat > 1 ||
        draw.material.clearcoatRoughness < 0 || draw.material.clearcoatRoughness > 1 ||
        int(draw.material.projection) < 0 || int(draw.material.projection) > 1)
        throw std::invalid_argument("Invalid material mapping/coating");
    if (t.scale.x <= 0 || t.scale.y <= 0 || t.scale.z <= 0)
        throw std::invalid_argument("Scale must be positive");
    for (auto id : {draw.material.normalTexture, draw.material.ormTexture, draw.material.emissiveTexture})
        if (id.value && !i.textures.contains(id.value))
            throw std::invalid_argument("Unknown PBR texture");
    if (draw.modelMatrix)
        for (float value : draw.modelMatrix->m)
            if (!std::isfinite(value))
                throw std::invalid_argument("Nonfinite model matrix");
    i.commands.push_back(draw);
    i.rendered = false;
}
void Renderer::primitive(Primitive primitive, const Transform &t, const Material &m) {
    int index = int(primitive);
    if (index < 0 || index > 2)
        throw std::invalid_argument("Unknown primitive");
    Draw draw;
    draw.mesh = impl->primitives[index];
    draw.transform = t;
    draw.material = m;
    submit(draw);
}
void Renderer::terrain(const Terrain &t) {
    impl->terrainSpec = t;
    impl->terrainRequested = t.enabled;
    impl->rendered = false;
}
void Renderer::render() {
    if (!impl->rendered) {
        impl->pipeline();
        impl->rendered = true;
    }
}
void Renderer::end() {
    render();
    impl->drawUi();
}
void Renderer::screenshot(const std::string &path) {
    auto &i = *impl;
    i.gl.Finish();
    std::vector<unsigned char> pixels(size_t(i.outputWidth) * i.outputHeight * 4);
    i.gl.ReadPixels(0, 0, i.outputWidth, i.outputHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    std::ofstream f(path, std::ios::binary);
    if (!f)
        throw std::runtime_error("Cannot write capture");
    f << "P6\n" << i.outputWidth << ' ' << i.outputHeight << "\n255\n";
    for (int y = i.outputHeight - 1; y >= 0; y--)
        for (int x = 0; x < i.outputWidth; x++)
            f.write(reinterpret_cast<const char *>(&pixels[(y * i.outputWidth + x) * 4]), 3);
    if (!f)
        throw std::runtime_error("Capture write failed");
}
void Renderer::check() {
    unsigned error = impl->gl.GetError();
    if (error)
        throw std::runtime_error("OpenGL error: " + std::to_string(error));
}
void Renderer::Impl::initializeSmaa() {
    std::string common = "#version 330 core\n#define SMAA_GLSL_3\n#define SMAA_PRESET_HIGH\nuniform vec4 "
                         "rtMetrics;\n#define SMAA_RT_METRICS rtMetrics\n";
    auto vertex = [&](const std::string &outputs, const std::string &body) {
        return common + "#define SMAA_INCLUDE_PS 0\n" + smaaReference + "\n" + outputs +
               "\nvoid "
               "main(){texcoord=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(texcoord*2.-1.,0,1)"
               ";" +
               body + "}\n";
    };
    auto fragment = [&](const std::string &inputs, const std::string &body) {
        return common + "#define SMAA_INCLUDE_VS 0\n" + smaaReference + "\n" + inputs + "\nvoid main(){" +
               body + "}\n";
    };
    smaaEdgeProgram =
        program(vertex("out vec2 texcoord;out vec4 offsets[3];", "SMAAEdgeDetectionVS(texcoord,offsets);"),
                fragment("in vec2 texcoord;in vec4 offsets[3];uniform sampler2D source;out vec4 color;",
                         "color=vec4(SMAAColorEdgeDetectionPS(texcoord,offsets,source),0,1);"));
    smaaWeightProgram = program(vertex("out vec2 texcoord;out vec2 pixcoord;out vec4 offsets[3];",
                                       "SMAABlendingWeightCalculationVS(texcoord,pixcoord,offsets);"),
                                fragment("in vec2 texcoord;in vec2 pixcoord;in vec4 offsets[3];uniform "
                                         "sampler2D edgesTex,areaTex,searchTex;out vec4 color;",
                                         "color=SMAABlendingWeightCalculationPS(texcoord,pixcoord,offsets,"
                                         "edgesTex,areaTex,searchTex,vec4(0));"));
    smaaBlendProgram = program(
        vertex("out vec2 texcoord;out vec4 offsets;", "SMAANeighborhoodBlendingVS(texcoord,offsets);"),
        fragment("in vec2 texcoord;in vec4 offsets;uniform sampler2D source,blendTex;out vec4 color;",
                 "color=SMAANeighborhoodBlendingPS(texcoord,offsets,source,blendTex);"));
    gl.PixelStorei(0x0CF5, 1);
    smaaArea = texture(160, 560, 0x822B, 0x8227, GL_UNSIGNED_BYTE, smaaAreaBytes);
    smaaSearch = texture(64, 16, 0x8229, 0x1903, GL_UNSIGNED_BYTE, searchTexBytes);
    gl.PixelStorei(0x0CF5, 4);
}
} // namespace rf::gfx
extern "C" const char *frontier_engine_version() {
    return rf::gfx::Version;
}

namespace rf::gfx {
void Renderer::captureBuffers(const std::filesystem::path &directory) {
    auto &i = *impl;
    std::filesystem::create_directories(directory);
    i.gl.Finish();
    for (int pass = 0; pass < 5; pass++) {
        i.gl.BindFramebuffer(0x8CA8, pass == 4 ? i.lit.fbo : i.sceneFbo);
        i.gl.ReadBuffer(vl::GL_COLOR_ATTACHMENT0 + (pass == 4 ? 0 : pass));
        std::vector<unsigned char> bytes(size_t(i.width) * i.height * 4);
        i.gl.ReadPixels(0, 0, i.width, i.height, vl::GL_RGBA, vl::GL_UNSIGNED_BYTE, bytes.data());
        const char *name[] = {"scene", "normal", "material", "motion", "lighting"};
        std::ofstream f(directory / (std::string(name[pass]) + ".ppm"), std::ios::binary);
        f << "P6\n" << i.width << ' ' << i.height << "\n255\n";
        for (int y = i.height - 1; y >= 0; y--)
            for (int x = 0; x < i.width; x++)
                f.write(reinterpret_cast<char *>(bytes.data() + (size_t(y) * i.width + x) * 4), 3);
    }
    i.gl.BindFramebuffer(0x8CA8, 0);
}
} // namespace rf::gfx

namespace rf::gfx {
void Renderer::validateFrame() {
    auto &i = *impl;
    i.gl.Finish();
    std::vector<float> pixels(size_t(i.width) * i.height * 4);
    for (int pass = 0; pass < 5; pass++) {
        i.gl.BindFramebuffer(0x8CA8, pass == 4 ? i.lit.fbo : i.sceneFbo);
        i.gl.ReadBuffer(vl::GL_COLOR_ATTACHMENT0 + (pass == 4 ? 0 : pass));
        i.gl.ReadPixels(0, 0, i.width, i.height, vl::GL_RGBA, vl::GL_FLOAT, pixels.data());
        auto bad = std::find_if(pixels.begin(), pixels.end(), [](float f) { return !std::isfinite(f); });
        if (bad != pixels.end()) {
            size_t index = bad - pixels.begin();
            int x = int(index / 4 % i.width), y = int(index / 4 / i.width);
            std::string message = "Nonfinite frame buffer " + std::to_string(pass) + " at " +
                                  std::to_string(x) + "," + std::to_string(y) + " channel " +
                                  std::to_string(index % 4) + " value " + std::to_string(*bad);
            i.gl.BindFramebuffer(0x8CA8, i.sceneFbo);
            for (int attachment : {1, 2}) {
                std::array<float, 4> values{};
                i.gl.ReadBuffer(vl::GL_COLOR_ATTACHMENT0 + attachment);
                i.gl.ReadPixels(x, y, 1, 1, vl::GL_RGBA, vl::GL_FLOAT, values.data());
                message += " attachment " + std::to_string(attachment) + ":";
                for (float value : values)
                    message += " " + std::to_string(value);
            }
            i.gl.BindFramebuffer(0x8CA8, 0);
            throw std::runtime_error(message);
        }
    }
    i.gl.BindFramebuffer(0x8CA8, 0);
}
} // namespace rf::gfx
