#include "internal.hpp"
#include <utility>
#include <chrono>
#include <cstring>
#include <stdexcept>

namespace rf::gfx {
using namespace vl;
TargetBuffer Renderer::Impl::target(unsigned internal, unsigned format, int w, int h) {
    if (!w)
        w = width;
    if (!h)
        h = height;
    TargetBuffer t;
    t.texture = texture(w, h, internal, format, GL_FLOAT);
    gl.GenFramebuffers(1, &t.fbo);
    gl.BindFramebuffer(GL_FRAMEBUFFER, t.fbo);
    gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.texture, 0);
    gl.DrawBuffer(GL_COLOR_ATTACHMENT0);
    if (gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("Incomplete postprocess target");
    return t;
}
void Renderer::Impl::destroy(TargetBuffer &t) {
    if (t.fbo)
        gl.DeleteFramebuffers(1, &t.fbo);
    if (t.texture)
        gl.DeleteTextures(1, &t.texture);
    t = {};
}
void Renderer::Impl::releaseTargets() {
    for (auto *t : {&aoRaw, &aoA, &aoB, &lit, &dof, &display, &edges, &weights, &fogTarget, &motionTarget})
        destroy(*t);
    for (auto &t : temporal)
        destroy(t);
    for (auto &t : focus)
        destroy(t);
    focusValid = false;
    for (auto &t : exposure)
        destroy(t);
    destroy(luminance);
    for (auto &t : bloomPyramid)
        destroy(t);
    for (auto &t : bloomScratch)
        destroy(t);
    bloomPyramid.clear();
    bloomScratch.clear();
    for (auto *tex :
         {&sceneColor, &sceneNormal, &sceneSurface, &sceneDepth, &shadowDepth, &sceneMotion, &particleDepth})
        if (*tex) {
            gl.DeleteTextures(1, tex);
            *tex = 0;
        }
    for (auto *fbo : {&sceneFbo, &msaaFbo, &shadowFbo, &transparentFbo, &particleDepthFbo})
        if (*fbo) {
            gl.DeleteFramebuffers(1, fbo);
            *fbo = 0;
        }
    gl.DeleteRenderbuffers(5, msaaBuffers.data());
    msaaBuffers.fill(0);
    shadowSize = 0;
}
void Renderer::Impl::allocate() {
    auto [w, h] = context.dimensions();
    w = std::clamp(w, 1, std::min(caps.maxTextureSize, 8192));
    h = std::clamp(h, 1, std::min(caps.maxTextureSize, 8192));
    int ow = w, oh = h;
    w = std::min(caps.maxTextureSize, int(std::round(w * settings().renderScale)));
    h = std::min(caps.maxTextureSize, int(std::round(h * settings().renderScale)));
    int samples = settings().antiAliasing == AntiAliasing::MSAA ? settings().msaaSamples : 1;
    if (!buffersDirty && w == width && h == height && samples == allocatedSamples && ow == outputWidth &&
        oh == outputHeight && compactScene==minimalFrame())
        return;
    releaseTargets();
    compactScene=minimalFrame();
    width = w;
    outputWidth = ow;
    outputHeight = oh;
    historyValid = false;
    height = h;
    allocatedSamples = samples;
    buffersDirty = false;
    sceneColor = texture(w, h, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    if(!compactScene){sceneNormal = texture(w, h, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    sceneMotion = texture(w, h, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    sceneSurface = texture(w, h, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);}
    sceneDepth = texture(w, h, 0x81A6, 0x1902, GL_FLOAT);
    gl.BindTexture(GL_TEXTURE_2D, sceneDepth);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 0x2600);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 0x2600);
    gl.GenFramebuffers(1, &sceneFbo);
    gl.BindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
    unsigned colorTextures[] = {sceneColor, sceneNormal, sceneSurface, sceneMotion};
    unsigned attachments[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT0 + 1, GL_COLOR_ATTACHMENT0 + 2,
                              GL_COLOR_ATTACHMENT0 + 3};
    for (int n = 0; n < (compactScene?1:4); n++)
        gl.FramebufferTexture2D(GL_FRAMEBUFFER, attachments[n], GL_TEXTURE_2D, colorTextures[n], 0);
    gl.FramebufferTexture2D(GL_FRAMEBUFFER, 0x8D00, GL_TEXTURE_2D, sceneDepth, 0);
    gl.DrawBuffers(compactScene?1:4, attachments);
    if (gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("Incomplete scene framebuffer");
    if (samples > 1) {
        gl.GenFramebuffers(1, &msaaFbo);
        gl.BindFramebuffer(GL_FRAMEBUFFER, msaaFbo);
        gl.GenRenderbuffers(5, msaaBuffers.data());
        unsigned formats[] = {GL_RGBA16F, GL_RGBA16F, GL_RGBA8, GL_RGBA16F, 0x81A6};
        for (int n = 0; n < 5; n++) {
            gl.BindRenderbuffer(0x8D41, msaaBuffers[n]);
            gl.RenderbufferStorageMultisample(0x8D41, samples, formats[n], w, h);
            gl.FramebufferRenderbuffer(GL_FRAMEBUFFER, n == 4 ? 0x8D00 : attachments[n], 0x8D41,
                                       msaaBuffers[n]);
        }
        gl.DrawBuffers(4, attachments);
        if (gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("Driver could not allocate requested MSAA framebuffer");
    }
    if (settings().shadows != Quality::Off) {
        shadowSize =
            std::min(caps.maxTextureSize, std::array{0, 512, 1024, 2048, 4096}[int(settings().shadows)]);
        shadowDepth = texture(shadowSize, shadowSize, 0x81A6, 0x1902, GL_FLOAT);
        gl.BindTexture(GL_TEXTURE_2D, shadowDepth);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 0x2600);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 0x2600);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812D);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812D);
        float border[] = {1, 1, 1, 1};
        gl.TexParameterfv(GL_TEXTURE_2D, 0x1004, border);
        gl.GenFramebuffers(1, &shadowFbo);
        gl.BindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
        gl.FramebufferTexture2D(GL_FRAMEBUFFER, 0x8D00, GL_TEXTURE_2D, shadowDepth, 0);
        gl.DrawBuffer(0);
        gl.ReadBuffer(0);
        if (gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("Incomplete shadow framebuffer");
    }
    if (settings().ambientOcclusion != Quality::Off) {
        aoRaw = target(0x822D, 0x1903);
        aoA = target(0x822D, 0x1903);
        aoB = target(0x822D, 0x1903);
    }
    lit = target();
    if (settings().depthOfField != Quality::Off)
        dof = target();
    display = target(GL_RGBA8);
    gl.GenFramebuffers(1, &transparentFbo);
    gl.BindFramebuffer(GL_FRAMEBUFFER, transparentFbo);
    gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, lit.texture, 0);
    gl.FramebufferTexture2D(GL_FRAMEBUFFER, 0x8D00, GL_TEXTURE_2D, sceneDepth, 0);
    gl.DrawBuffer(GL_COLOR_ATTACHMENT0);
    if (gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("transparent framebuffer incomplete");
    gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + 1, GL_TEXTURE_2D, sceneSurface, 0);
    particleDepth = texture(w, h, 0x81A6, 0x1902, GL_FLOAT);
    gl.GenFramebuffers(1, &particleDepthFbo);
    gl.BindFramebuffer(GL_FRAMEBUFFER, particleDepthFbo);
    gl.FramebufferTexture2D(GL_FRAMEBUFFER, 0x8D00, GL_TEXTURE_2D, particleDepth, 0);
    gl.DrawBuffer(0);
    gl.ReadBuffer(0);
    if (gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("particle depth framebuffer incomplete");
    allocateAdvanced();
    if (settings().antiAliasing == AntiAliasing::SMAA) {
        edges = target(GL_RGBA8);
        weights = target(GL_RGBA8);
    }
    gl.BindFramebuffer(GL_FRAMEBUFFER, outputFramebuffer());
}
Renderer::Impl::UniformState &Renderer::Impl::uniformState(unsigned p, std::string_view name) {
    auto &cache = uniforms[p];
    auto found = cache.find(name);
    if (found != cache.end()) return found->second;
    std::string owned(name);
    UniformState value;
    value.location = gl.GetUniformLocation(p, owned.c_str());
    return cache.emplace(std::move(owned), std::move(value)).first->second;
}
int Renderer::Impl::uniform(unsigned p, std::string_view name) { return uniformState(p, name).location; }
bool Renderer::Impl::updateUniform(unsigned p, std::string_view name, unsigned type,
                                  const void *data, size_t bytes, int &location) {
    auto &state = uniformState(p, name);
    location = state.location;
    if (location < 0) return false;
    unsigned char *cached = state.bytes.data();
    if (bytes > state.bytes.size()) {
        if (!state.matrix) state.matrix = std::make_unique<std::array<unsigned char, 64>>();
        cached = state.matrix->data();
    }
    if (state.type == type && std::memcmp(cached, data, bytes) == 0) return false;
    state.type = type;
    std::memcpy(cached, data, bytes);
    return true;
}
void Renderer::Impl::sampler(unsigned p, const char *name, unsigned tex, int unit) {
    // Inactive shader inputs need neither a uniform update nor a resource binding.
    int location;
    bool changed = updateUniform(p, name, 3, &unit, sizeof(unit), location);
    if (location < 0) return;
    gl.ActiveTexture(GL_TEXTURE0 + unit);
    gl.BindTexture(GL_TEXTURE_2D, tex);
    if (changed) gl.Uniform1i(location, unit);
}
void Renderer::Impl::matrix(unsigned p, const char *name, const Mat4 &m) {
    int location;
    if (updateUniform(p, name, 1, m.m.data(), sizeof(float) * 16, location))
        gl.UniformMatrix4fv(location, 1, 0, m.m.data());
}
void Renderer::Impl::scalar(unsigned p, const char *name, float value) {
    int location;
    if (updateUniform(p, name, 2, &value, sizeof(value), location)) gl.Uniform1f(location, value);
}
void Renderer::Impl::integer(unsigned p, const char *name, int value) {
    int location;
    if (updateUniform(p, name, 3, &value, sizeof(value), location)) gl.Uniform1i(location, value);
}
void Renderer::Impl::vector(unsigned p, const char *name, V3 value) {
    const float data[] = {value.x, value.y, value.z};
    int location;
    if (updateUniform(p, name, 4, data, sizeof(data), location))
        gl.Uniform3f(location, value.x, value.y, value.z);
}
void Renderer::Impl::common(unsigned p, const Mat4 &m) {
    gl.UseProgram(p);
    matrix(p, "vp", m);
    if (p == shadowProgram) {
        scalar(p, "windTime", environment.atmosphere.time);
        return;
    }
    matrix(p, "lightVP", lightVP);
    vector(p, "eye", camera.position);
    vector(p, "sun", normalized(environment.sunDirection));
    vector(p, "sunColor", {environment.sunColor.r, environment.sunColor.g, environment.sunColor.b});
    vector(p, "skyColor", {environment.skyColor.r, environment.skyColor.g, environment.skyColor.b});
    vector(p, "groundColor",
           {environment.groundColor.r, environment.groundColor.g, environment.groundColor.b});
    scalar(p, "sunIntensity", environment.sunIntensity);
    integer(p, "physicalLighting", physicalLighting);
    scalar(p, "ambientIntensity", environment.ambientIntensity);
    scalar(p, "viewDistance", settings().viewDistance);
    scalar(p, "storm", environment.storm);
    scalar(p, "fogDensity", environment.fogDensity);
    integer(p, "shadowQuality", int(settings().shadows));
    scalar(p, "windTime", environment.atmosphere.time);
    vector(p, "windVector", environment.atmosphere.wind);
    integer(p, "giMode", int(settings().globalIllumination));
    sampler(p, "shadowMap", shadowDepth, 4);
    sampler(p, "shadowPcf", shadowDepth, 13);
    gl.BindSampler(13, shadowSampler);
    matrix(p, "previousVP", previousVP);
    matrix(p, "view", view);
    matrix(p, "previousView", previousView);
    bindAdvanced(p);
}
void Renderer::Impl::material(unsigned p, const Material &m) {
    if (p == shadowProgram) {
        scalar(p, "windStrength", m.windStrength);
        scalar(p, "alphaCutoff", m.alphaCutoff);
        integer(p, "alphaMode", int(m.alphaMode));
        bool masked = m.alphaMode == AlphaMode::Mask;
        auto texture = masked ? textures.find(m.albedoTexture.value) : textures.end();
        integer(p, "textured", texture != textures.end());
        if (texture != textures.end())
            sampler(p, "albedoMap", texture->second.object, 0);
        return;
    }
    V3 constantOrm{1,1,1};if(settings().textureQuality==TextureQuality::Lowest){auto orm=textures.find(m.ormTexture.value);if(orm!=textures.end()&&orm->second.averageReady)constantOrm={orm->second.average[0],orm->second.average[1],orm->second.average[2]};}
    if(p==minimalProgram){scalar(p,"windStrength",m.windStrength);scalar(p,"alphaCutoff",m.alphaCutoff);integer(p,"alphaMode",int(m.alphaMode));integer(p,"doubleSided",m.doubleSided);integer(p,"textureProjection",int(m.projection));scalar(p,"textureScale",m.textureScale);scalar(p,"roughness",std::clamp(m.roughness*constantOrm.y,.06f,1.f));scalar(p,"metallic",std::clamp(m.metallic*constantOrm.z,0.f,1.f));vector(p,"emissive",{m.emissive.r,m.emissive.g,m.emissive.b});auto a=textures.find(m.albedoTexture.value),e=textures.find(m.emissiveTexture.value);integer(p,"textured",a!=textures.end());integer(p,"hasEmissive",e!=textures.end());sampler(p,"albedoMap",a==textures.end()?0:a->second.object,0);sampler(p,"emissiveMap",e==textures.end()?0:e->second.object,1);return;}
    const bool lowest=settings().textureQuality==TextureQuality::Lowest;
    integer(p,"lowestTextures",lowest);vector(p,"constantOrm",constantOrm);
    scalar(p, "roughness", clamp(m.roughness, .04f, 1));
    scalar(p, "metallic", clamp(m.metallic));
    vector(p, "emissive", {m.emissive.r, m.emissive.g, m.emissive.b});
    integer(p, "receiveShadow", m.receiveShadow);
    auto it = textures.find(m.albedoTexture.value);
    integer(p, "textured", it != textures.end());
    sampler(p, "albedoMap", it == textures.end() ? 0 : it->second.object, 0);
    int unit = 1;
    for (auto pair : {std::pair{m.normalTexture, "normalMap"}, std::pair{m.ormTexture, "ormMap"},
                      std::pair{m.emissiveTexture, "emissiveMap"}}) {
        auto t = textures.find(pair.first.value);
        sampler(p, pair.second, t == textures.end() ? 0 : t->second.object, unit++);
    }
    integer(p, "hasNormal", !lowest&&m.normalTexture.value != 0);
    integer(p, "hasOrm", !lowest&&m.ormTexture.value != 0);
    integer(p, "hasEmissive", m.emissiveTexture.value != 0);
    scalar(p, "normalScale", m.normalScale);
    scalar(p, "alphaCutoff", m.alphaCutoff);
    integer(p, "alphaMode", int(m.alphaMode));
    integer(p, "doubleSided", m.doubleSided);
    integer(p, "reactive", m.temporalReactive);
    integer(p, "textureProjection", int(m.projection));
    scalar(p, "textureScale", m.textureScale);
    scalar(p, "macroVariation", lowest?0:m.macroVariation);
    scalar(p, "clearcoat", lowest?0:m.clearcoat);
    scalar(p, "clearcoatRoughness", m.clearcoatRoughness);
    scalar(p, "windStrength", m.windStrength);
    scalar(p, "occlusionStrength", m.occlusionStrength);
}
Mat4 Renderer::Impl::model(const Transform &t) const {
    if (t.yaw == 0 && t.pitch == 0 && t.roll == 0) return compose(t.position, Quat{}, t.scale);
    auto axis = [](V3 v, float a) {
        return Quat{v.x * std::sin(a * .5f), v.y * std::sin(a * .5f), v.z * std::sin(a * .5f),
                    std::cos(a * .5f)};
    };
    auto mul = [](Quat a, Quat b) {
        return Quat{
            a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
    };
    return compose(t.position,
                   mul(axis({0, 0, 1}, t.roll), mul(axis({1, 0, 0}, t.pitch), axis({0, 1, 0}, t.yaw))),
                   t.scale);
}
bool Renderer::Impl::visible(const GpuMesh &m, const Transform &t, const Mat4 &matrix,
                             const Mat4 *overrideModel) const {
    Mat4 x = overrideModel ? *overrideModel : model(t);
    V3 p = transformPoint(x, m.center);
    float bound = 0;
    for (int c = 0; c < 3; c++)
        for (int r = 0; r < 3; r++)
            bound += x.m[c * 4 + r] * x.m[c * 4 + r];
    float radius = m.radius * std::sqrt(bound);
    if (distance(p, camera.position) > settings().viewDistance + radius)
        return false;
    for (int row = 0; row < 3; row++)
        for (float sign : {-1.f, 1.f}) {
            V3 n{matrix.m[3] + sign * matrix.m[row], matrix.m[7] + sign * matrix.m[4 + row],
                 matrix.m[11] + sign * matrix.m[8 + row]};
            float d = matrix.m[15] + sign * matrix.m[12 + row];
            if (dot(n, p) + d < -radius * length(n))
                return false;
        }
    return true;
}
void Renderer::Impl::mesh(const GpuMesh &m, std::span<const GpuInstance> batch, bool patches) {
    if (batch.empty())
        return;
    gl.BindVertexArray(m.vao);
    gl.BindBuffer(GL_ARRAY_BUFFER, instanceBuffer);
    gl.BufferData(GL_ARRAY_BUFFER, batch.size_bytes(), batch.data(), GL_STREAM_DRAW);
    if (patches) {
        gl.PatchParameteri(0x8E72, 4);
        if (m.ibo)
            gl.DrawElementsInstanced(0x000E, m.count, 0x1405, nullptr, batch.size());
        else
            gl.DrawArraysInstanced(0x000E, 0, m.count, batch.size());
        if (countDraws)
            stats.triangles += m.count / 4 * stats.terrainSubdivisions * stats.terrainSubdivisions * 2;
    } else {
        if (m.ibo)
            gl.DrawElementsInstanced(GL_TRIANGLES, m.count, 0x1405, nullptr, batch.size());
        else
            gl.DrawArraysInstanced(GL_TRIANGLES, 0, m.count, batch.size());
        if (countDraws)
            stats.triangles += m.count / 3 * batch.size();
    }
    if (countDraws)
        stats.drawCalls++;
}
void Renderer::Impl::prepareCommands() {
    // Keep constructed storage across frames instead of initializing every instance twice.
    opaqueOrder.clear();
    transparentOrder.clear();
    const auto started = std::chrono::steady_clock::now();
    prepared.resize(commands.size());
    auto bits = [](float f) {
        unsigned u;
        std::memcpy(&u, &f, 4);
        return u;
    };
    auto key = [&](const Draw &c) {
        auto &a = c.material;
        return BatchKey{c.mesh.value,
                        a.albedoTexture.value,
                        a.normalTexture.value,
                        a.ormTexture.value,
                        a.emissiveTexture.value,
                        bits(a.roughness),
                        bits(a.metallic),
                        bits(a.emissive.r),
                        bits(a.emissive.g),
                        bits(a.emissive.b),
                        unsigned(a.receiveShadow),
                        unsigned(a.alphaMode),
                        bits(a.alphaCutoff),
                        bits(a.normalScale),
                        unsigned(a.doubleSided),
                        unsigned(a.temporalReactive),
                        unsigned(a.projection),
                        bits(a.textureScale),
                        bits(a.macroVariation),
                        bits(a.clearcoat),
                        bits(a.clearcoatRoughness),
                        bits(a.windStrength),
                        bits(a.occlusionStrength)};
    };
    const auto &meshLookup = std::as_const(meshes);
    const auto &motionLookup = std::as_const(previousModels);
    auto prepare = [&](size_t lo, size_t hi) {
      for (size_t index = lo; index < hi; ++index) {
        const auto &c = commands[index];
        prepared[index].source = nullptr;
        auto mesh = meshLookup.find(c.mesh.value);
        if (mesh == meshLookup.end())
            continue;
        Mat4 x = c.modelMatrix ? *c.modelMatrix : model(c.transform);
        auto old = compactScene?motionLookup.end():motionLookup.find(c.objectId);
        auto center = transformPoint(x, mesh->second.center);
        float bound = 0;
        for (int col = 0; col < 3; col++)
            for (int row = 0; row < 3; row++)
                bound += x.m[col * 4 + row] * x.m[col * 4 + row];
        auto delta = center - camera.position;
        auto originDelta = transformPoint(x, {}) - camera.position;
        prepared[index] = 
            {&c, &mesh->second,
             GpuInstance{x, historyValid && c.objectId && old != motionLookup.end() ? old->second : x,
                         c.material.albedo},
             key(c), center, mesh->second.radius * std::sqrt(bound), dot(delta, delta),
             dot(originDelta, originDelta)};
      }
    };
    stats.preparationWorkers = jobs.parallelFor(commands.size(), 2048, prepare);
    for (size_t index = 0; index < prepared.size(); ++index) {
        if (!prepared[index].source) continue;
        const auto &c = *prepared[index].source;
        bool blend = c.material.alphaMode == AlphaMode::Blend || c.material.alphaMode == AlphaMode::Additive;
        (blend ? transparentOrder : opaqueOrder).push_back(index);
    }
    std::stable_sort(opaqueOrder.begin(), opaqueOrder.end(),
                     [&](size_t a, size_t b) { return prepared[a].key < prepared[b].key; });
    std::stable_sort(transparentOrder.begin(), transparentOrder.end(), [&](size_t a, size_t b) {
        return prepared[a].sortDistanceSquared > prepared[b].sortDistanceSquared;
    });
    stats.preparationMilliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
}
void Renderer::Impl::drawCommands(unsigned p, const Mat4 &matrix, bool shadow, bool transparent,
                                  bool depthOnly) {
    common(p, matrix);
    std::array<std::array<float, 4>, 6> planes;
    int count = 0;
    for (int row = 0; row < 3; row++)
        for (float sign : {-1.f, 1.f}) {
            V3 n{matrix.m[3] + sign * matrix.m[row], matrix.m[7] + sign * matrix.m[4 + row],
                 matrix.m[11] + sign * matrix.m[8 + row]};
            float inverseLength = 1.f / std::max(length(n), 1e-12f);
            planes[count++] = {n.x * inverseLength, n.y * inverseLength, n.z * inverseLength,
                               (matrix.m[15] + sign * matrix.m[12 + row]) * inverseLength};
        }
    const PreparedDraw *first = nullptr;
    instanceScratch.clear();
    // After the depth prepass, opaque surfaces only shade the visible fragment; masked ones still resolve alpha.
    const bool lateDepth = depthPrepassed && p == sceneProgram && !transparent;
    auto flush = [&] {
        if (!first)
            return;
        if (lateDepth) {
            bool opaque = first->source->material.alphaMode == AlphaMode::Opaque;
            gl.DepthFunc(opaque ? 0x0202 : 0x0201);
            gl.DepthMask(!opaque);
        }
        material(p, first->source->material);
        if (transparent && p == sceneProgram)
            gl.BlendFunc(GL_SRC_ALPHA, first->source->material.alphaMode == AlphaMode::Additive
                                           ? GL_ONE
                                           : GL_ONE_MINUS_SRC_ALPHA);
        mesh(*first->mesh, instanceScratch);
        instanceScratch.clear();
    };
    for (auto index : transparent ? transparentOrder : opaqueOrder) {
        const auto &draw = prepared[index];
        if (shadow && !draw.source->material.castShadow)
            continue;
        if (depthOnly && draw.source->material.alphaMode != AlphaMode::Opaque)
            continue;
        float range = settings().viewDistance + draw.radius;
        if (draw.distanceSquared > range * range)
            continue;
        bool outside = false;
        for (const auto &plane : planes)
            if (plane[0] * draw.center.x + plane[1] * draw.center.y + plane[2] * draw.center.z + plane[3] <
                -draw.radius) {
                outside = true;
                break;
            }
        if (outside)
            continue;
        // Opaque depth-only draws depend on geometry and wind alone, so batches merge across material differences.
        bool same = first && (first->key == draw.key ||
                              ((depthOnly || shadow) && first->key[0] == draw.key[0] &&
                               first->key[21] == draw.key[21] &&
                               first->source->material.alphaMode == AlphaMode::Opaque &&
                               draw.source->material.alphaMode == AlphaMode::Opaque));
        if (first && !same) {
            flush();
            first = nullptr;
        }
        if (!first)
            first = &draw;
        instanceScratch.push_back(draw.instance);
    }
    flush();
    if (lateDepth) {
        gl.DepthFunc(0x0201);
        gl.DepthMask(1);
    }
}
void Renderer::Impl::projectionTerms(unsigned p, bool xy) {
    // Perspective depth reconstruction needs only these projection entries, not a full inverse matrix multiply.
    if (xy)
        gl.Uniform4f(uniform(p, "projectionXY"), projection.m[0], projection.m[5], projection.m[8],
                     projection.m[9]);
    gl.Uniform2f(uniform(p, "depthTerms"), projection.m[10], projection.m[14]);
}
void Renderer::Impl::fullscreen(unsigned p, unsigned fbo) {
    gl.BindFramebuffer(GL_FRAMEBUFFER, fbo);
    gl.Viewport(0, 0, width, height);
    gl.UseProgram(p);
    gl.BindVertexArray(fullscreenVao);
    gl.DrawArrays(GL_TRIANGLES, 0, 3);
    stats.drawCalls++;
    stats.postPasses++;
}
unsigned Renderer::Impl::antialias(unsigned source) {
    if (settings().antiAliasing == AntiAliasing::TAA) {
        temporalResolve(source);
        return source;
    }

    auto metrics = [&](unsigned p) {
        gl.UseProgram(p);
        gl.Uniform4f(uniform(p, "rtMetrics"), 1.f / width, 1.f / height, width, height);
    };
    if (settings().antiAliasing == AntiAliasing::SMAA) {
        gl.ClearColor(0, 0, 0, 0);
        gl.BindFramebuffer(GL_FRAMEBUFFER, edges.fbo);
        gl.Clear(GL_COLOR_BUFFER_BIT);
        metrics(smaaEdgeProgram);
        sampler(smaaEdgeProgram, "source", source, 0);
        fullscreen(smaaEdgeProgram, edges.fbo);
        gl.BindFramebuffer(GL_FRAMEBUFFER, weights.fbo);
        gl.Clear(GL_COLOR_BUFFER_BIT);
        metrics(smaaWeightProgram);
        sampler(smaaWeightProgram, "edgesTex", edges.texture, 0);
        sampler(smaaWeightProgram, "areaTex", smaaArea, 1);
        sampler(smaaWeightProgram, "searchTex", smaaSearch, 2);
        fullscreen(smaaWeightProgram, weights.fbo);
        metrics(smaaBlendProgram);
        sampler(smaaBlendProgram, "source", source, 0);
        sampler(smaaBlendProgram, "blendTex", weights.texture, 1);
        gl.BindFramebuffer(GL_FRAMEBUFFER, outputFramebuffer());
        gl.Viewport(0, 0, outputWidth, outputHeight);
        gl.UseProgram(smaaBlendProgram);
        gl.BindVertexArray(fullscreenVao);
        gl.DrawArrays(GL_TRIANGLES, 0, 3);
        stats.drawCalls++;
        stats.postPasses++;
        return source;
    }
    unsigned p = settings().antiAliasing == AntiAliasing::FXAA ? fxaaProgram : upscaleProgram;
    gl.UseProgram(p);
    sampler(p, "source", source, 0);
    gl.Uniform2f(uniform(p, "pixel"), 1.f / width, 1.f / height);
    scalar(p, "sharpening", settings().sharpening);
    integer(p, "sameSize", width == outputWidth && height == outputHeight);
    gl.BindFramebuffer(GL_FRAMEBUFFER, outputFramebuffer());
    gl.Viewport(0, 0, outputWidth, outputHeight);
    gl.UseProgram(p);
    gl.BindVertexArray(fullscreenVao);
    gl.DrawArrays(GL_TRIANGLES, 0, 3);
    stats.drawCalls++;
    stats.postPasses++;
    return source;
}
void Renderer::Impl::pipeline() {
    auto cpuStart = std::chrono::steady_clock::now();
    allocate();
    stats.renderWidth = width;
    stats.renderHeight = height;
    stats.outputWidth = outputWidth;
    stats.outputHeight = outputHeight;
    if (frameIndex && (distance(lastEye, camera.position) > 10 || dot(lastDirection, camera.direction) < .5f)) {
        historyValid = false;
        focusValid = false;
    }
    bool query = false;
    unsigned qi = queryIndex % 3;
    if (settings().gpuProfiling) {
        int ready = 0;
        if (queryActive[qi])
            gl.GetQueryObjectiv(gpuQueries[qi], 0x8867, &ready);
        if (ready) {
            unsigned long long ns;
            gl.GetQueryObjectui64v(gpuQueries[qi], 0x8866, &ns);
            stats.gpuMilliseconds = lastGpuMilliseconds = ns / 1000000.;
            queryActive[qi] = false;
        }
        if (!queryActive[qi]) {
            gl.BeginQuery(0x88BF, gpuQueries[qi]);
            query = true;
        }
    }
    view = lookAt(camera.position, camera.position + camera.direction);
    projection = perspective(camera.fovDegrees * pi / 180, float(outputWidth) / outputHeight,
                             camera.nearPlane, settings().viewDistance);
    if (settings().antiAliasing == AntiAliasing::TAA) {
        auto halton = [](unsigned n, unsigned base) {
            float value = 0, factor = 1;
            for (; n; n /= base) {
                factor /= base;
                value += factor * (n % base);
            }
            return value;
        };
        projection.m[8] += (halton(frameIndex % 8 + 1, 2) - .5f) * 2 / width;
        projection.m[9] += (halton(frameIndex % 8 + 1, 3) - .5f) * 2 / height;
    }
    vp = projection * view;
    if (!historyValid) {
        previousVP = vp;
        previousView = view;
    }
    stats.lightweightRendering=compactScene;stats.sceneColorAttachments=compactScene?1:4;
    prepareCommands();
    uploadLights();
    invProjection = inverse(projection);
    stats.msaaSamples = allocatedSamples;
    stats.shadowResolution = shadowSize;
    for (auto &[id, t] : textures) {
        (void)id;
        stats.textureBytes += t.uploadedWidth * t.uploadedHeight * 4 * 4 / 3;
    }
    if (terrainRequested)
        streamTerrain();
    float extent = environment.shadowDistance > 0 ? std::clamp(environment.shadowDistance, 8.f, 150.f)
                                                  : std::min(settings().viewDistance * .7f, 150.f);
    V3 center = camera.position + camera.direction * std::min(35.f, extent * .3f);
    center.y = 0;
    V3 sun = normalized(environment.sunDirection);
    if (length(sun) < .1f)
        sun = {.3f, .8f, .2f};
    auto lightView = lookAt(center + sun * extent * 3, center);
    auto origin = transformPoint(lightView, {});
    float texel = 2 * extent / std::max(1, shadowSize);
    lightView.m[12] += std::round(origin.x / texel) * texel - origin.x;
    lightView.m[13] += std::round(origin.y / texel) * texel - origin.y;
    lightVP = orthographic(-extent, extent, -extent, extent, 1, extent * 6) * lightView;
    gl.Disable(GL_BLEND);
    gl.Enable(0x0B71);
    gl.PolygonMode(0x0408, settings().wireframe ? 0x1B01 : 0x1B02);
    if (shadowFbo) {
        gl.BindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
        gl.Viewport(0, 0, shadowSize, shadowSize);
        gl.Clear(0x100);
        gl.Enable(0x8037);
        gl.PolygonOffset(1.4f, 2.f);
        drawCommands(shadowProgram, lightVP, true);
        if (terrainRequested)
            drawTerrain(true);
        gl.Disable(0x8037);
    }
    gl.BindFramebuffer(GL_FRAMEBUFFER, msaaFbo ? msaaFbo : sceneFbo);
    gl.Viewport(0, 0, width, height);
    unsigned outputs[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT0 + 1, GL_COLOR_ATTACHMENT0 + 2,
                          GL_COLOR_ATTACHMENT0 + 3};
    gl.DrawBuffers(compactScene?1:4, outputs);
    gl.ClearColor(environment.skyColor.r * .4f, environment.skyColor.g * .4f, environment.skyColor.b * .4f,
                  1);
    gl.Clear(GL_COLOR_BUFFER_BIT | 0x100);
    if (msaaFbo)
        gl.Enable(0x809D);
    else
        gl.Disable(0x809D);
    depthPrepassed = !compactScene && !settings().wireframe && !customMaterialShader;
    if (depthPrepassed) {
        gl.DrawBuffer(0);
        countDraws = false;
        drawCommands(shadowProgram, vp, false, false, true);
        if (terrainRequested)
            drawTerrain(false, true);
        countDraws = true;
        gl.DrawBuffers(compactScene?1:4, outputs);
    }
    drawCommands(surfaceProgram(), vp, false);
    if (terrainRequested)
        drawTerrain(false);
    gl.PolygonMode(0x0408, 0x1B02);
    gl.Enable(0x0B71);
    gl.DepthFunc(0x0203);
    gl.DepthMask(0);
    gl.UseProgram(skyProgram);
    matrix(skyProgram, "invProjection", invProjection);
    matrix(skyProgram, "invView", inverse(view));
    matrix(skyProgram, "previousVP", previousVP);
    scalar(skyProgram, "farDistance", settings().viewDistance);
    vector(skyProgram, "skyColor", {environment.skyColor.r, environment.skyColor.g, environment.skyColor.b});
    auto sky = environments.find(environment.image.value);
    integer(skyProgram, "enabled", sky != environments.end());
    sampler(skyProgram, "skyEquirect", sky == environments.end() ? 0 : sky->second.source, 0);
    scalar(skyProgram, "environmentRotation", environment.imageRotation);
    integer(skyProgram, "atmosphereEnabled", environment.atmosphere.enabled&&!compactScene);
    integer(skyProgram, "cloudSteps", compactScene?0:std::array{0, 8, 12, 20, 32}[int(environment.atmosphere.clouds)]);
    vector(skyProgram, "eye", camera.position);
    vector(skyProgram, "sun", normalized(environment.sunDirection));
    vector(skyProgram, "sunColor", {environment.sunColor.r, environment.sunColor.g, environment.sunColor.b});
    vector(skyProgram, "wind", environment.atmosphere.wind);
    scalar(skyProgram, "skyTime", environment.atmosphere.time);
    scalar(skyProgram, "coverage", environment.atmosphere.cloudCoverage);
    scalar(skyProgram, "cloudDensity", environment.atmosphere.cloudDensity);
    scalar(skyProgram, "cloudBase", environment.atmosphere.cloudBase);
    scalar(skyProgram, "cloudThickness", environment.atmosphere.cloudThickness);
    scalar(skyProgram, "turbidity", environment.atmosphere.turbidity);
    bind(skyProgram, "noiseVolume", noiseVolume, 1, 0x806F);
    fullscreen(skyProgram, msaaFbo ? msaaFbo : sceneFbo);
    gl.DepthMask(1);
    gl.DepthFunc(0x0201);
    gl.PolygonMode(0x0408, 0x1B02);
    gl.Disable(0x0B71);
    gl.Disable(0x809D);
    if (msaaFbo) {
        gl.BindFramebuffer(0x8CA8, msaaFbo);
        gl.BindFramebuffer(0x8CA9, sceneFbo);
        for (int n = 0; n < 4; n++) {
            gl.ReadBuffer(outputs[n]);
            gl.DrawBuffer(outputs[n]);
            gl.BlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, 0x2600);
        }
        gl.BlitFramebuffer(0, 0, width, height, 0, 0, width, height, 0x100, 0x2600);
    }
    if (aoRaw.fbo) {
        gl.UseProgram(aoProgram);
        sampler(aoProgram, "depthMap", sceneDepth, 0);
        sampler(aoProgram, "normalMap", sceneNormal, 1);
        projectionTerms(aoProgram);
        const int aoSamples = std::array{0, 8, 16, 32, 48}[int(settings().ambientOcclusion)];
        std::array<float, 48 * 4> aoTable{};
        for (int i = 0; i < aoSamples; i++) {
            float f = (i + .5f) / aoSamples, z = .2f + .8f * f;
            aoTable[i * 4] = z;
            aoTable[i * 4 + 1] = std::sqrt(1 - z * z);
            aoTable[i * 4 + 2] = .15f + .85f * f * f;
        }
        gl.Uniform4fv(uniform(aoProgram, "sampleTable"), 48, aoTable.data());
        matrix(aoProgram, "view", view);
        integer(aoProgram, "samples", aoSamples);
        scalar(aoProgram, "radius", 1.5f);
        fullscreen(aoProgram, aoRaw.fbo);
        gl.UseProgram(aoBlurProgram);
        sampler(aoBlurProgram, "depthMap", sceneDepth, 1);
        projectionTerms(aoBlurProgram, false);
        sampler(aoBlurProgram, "source", aoRaw.texture, 0);
        gl.Uniform2f(uniform(aoBlurProgram, "direction"), 1.f / width, 0);
        fullscreen(aoBlurProgram, aoA.fbo);
        sampler(aoBlurProgram, "source", aoA.texture, 0);
        gl.Uniform2f(uniform(aoBlurProgram, "direction"), 0, 1.f / height);
        fullscreen(aoBlurProgram, aoB.fbo);
    }
    if (settings().reflections != Quality::Off) {
        gl.ActiveTexture(GL_TEXTURE0);
        gl.BindTexture(GL_TEXTURE_2D, sceneColor);
        gl.GenerateMipmap(GL_TEXTURE_2D);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 0x2703);
    }
    if(compactScene){gl.BindFramebuffer(0x8CA8,sceneFbo);gl.ReadBuffer(GL_COLOR_ATTACHMENT0);gl.BindFramebuffer(0x8CA9,lit.fbo);gl.DrawBuffer(GL_COLOR_ATTACHMENT0);gl.BlitFramebuffer(0,0,width,height,0,0,width,height,GL_COLOR_BUFFER_BIT,0x2600);}else{
    gl.UseProgram(lightingProgram);
    sampler(lightingProgram, "source", sceneColor, 0);
    sampler(lightingProgram, "depthMap", sceneDepth, 1);
    sampler(lightingProgram, "normalMap", sceneNormal, 2);
    sampler(lightingProgram, "surfaceMap", sceneSurface, 3);
    sampler(lightingProgram, "aoMap", aoB.texture, 4);
    projectionTerms(lightingProgram);
    matrix(lightingProgram, "view", view);
    integer(lightingProgram, "aoEnabled", aoB.fbo != 0);
    integer(lightingProgram, "giSamples",
            settings().globalIllumination == GlobalIllumination::ScreenSpace
                ? std::array{0, 8, 12, 24, 32}[int(settings().illuminationQuality)]
                : 0);
    integer(lightingProgram, "reflectionSteps", std::array{0, 16, 32, 64, 96}[int(settings().reflections)]);
    vector(lightingProgram, "skyColor",
           {environment.skyColor.r, environment.skyColor.g, environment.skyColor.b});
    fullscreen(lightingProgram, lit.fbo);
    }
    gl.BindFramebuffer(GL_FRAMEBUFFER, transparentFbo);
    gl.DrawBuffer(GL_COLOR_ATTACHMENT0);
    gl.Enable(0x0B71);
    gl.DepthMask(0);
    gl.Enable(GL_BLEND);
    drawCommands(surfaceProgram(), vp, false, true);
    gl.Disable(GL_BLEND);
    if (settings().antiAliasing == AntiAliasing::TAA) {
        gl.BindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
        unsigned reactiveOutputs[] = {0, 0, GL_COLOR_ATTACHMENT0 + 2, 0};
        gl.DrawBuffers(4, reactiveOutputs);
        gl.ColorMask(0, 0, 0, 1);
        drawCommands(reactiveProgram, vp, false, true);
        gl.ColorMask(1, 1, 1, 1);
    }
    gl.DepthMask(1);
    gl.Disable(0x0B71);
    drawParticles();
    unsigned current = postAdvanced(lit.texture);
    if (dof.fbo) {
        if (settings().autoFocus) {
            unsigned next = 1 - focusIndex;
            gl.UseProgram(focusProgram);
            sampler(focusProgram, "depthMap", sceneDepth, 0);
            sampler(focusProgram, "previous", focus[focusIndex].texture, 1);
            projectionTerms(focusProgram, false);
            gl.Uniform2f(uniform(focusProgram, "pixel"), 1.f / width, 1.f / height);
            scalar(focusProgram, "farDistance", settings().viewDistance);
            scalar(focusProgram, "dt", std::clamp(environment.deltaSeconds, 0.f, .25f));
            scalar(focusProgram, "speed", settings().focusAdaptation);
            integer(focusProgram, "valid", focusValid);
            gl.BindFramebuffer(GL_FRAMEBUFFER, focus[next].fbo);
            gl.Viewport(0, 0, 1, 1);
            gl.BindVertexArray(fullscreenVao);
            gl.DrawArrays(GL_TRIANGLES, 0, 3);
            ++stats.drawCalls;
            ++stats.postPasses;
            focusIndex = next;
            focusValid = true;
        }
        gl.UseProgram(dofProgram);
        sampler(dofProgram, "source", current, 0);
        sampler(dofProgram, "depthMap", sceneDepth, 1);
        projectionTerms(dofProgram, false);
        sampler(dofProgram, "focusMap", focus[focusIndex].texture, 2);
        integer(dofProgram, "autoFocus", settings().autoFocus);
        scalar(dofProgram, "focusDistance", settings().focusDistance);
        integer(dofProgram, "physicalLens", camera.optics.has_value());
        if (camera.optics) {
            scalar(dofProgram, "lensFocalLength", camera.optics->focalLengthMetres);
            scalar(dofProgram, "lensFNumber", camera.optics->fNumber);
            scalar(dofProgram, "lensSensorHeight", camera.optics->sensorHeightMetres);
            scalar(dofProgram, "imageHeight", height);
        }
        scalar(dofProgram, "aperture", settings().aperture);
        scalar(dofProgram, "maxBlur", settings().maximumBlur);
        integer(dofProgram, "samples", std::array{0, 8, 16, 24, 32}[int(settings().depthOfField)]);
        gl.Uniform2f(uniform(dofProgram, "pixel"), 1.f / width, 1.f / height);
        fullscreen(dofProgram, dof.fbo);
        current = dof.texture;
    }
    gl.UseProgram(tonemapProgram);
    sampler(tonemapProgram, "source", current, 0);
    sampler(tonemapProgram, "depthMap", sceneDepth, 1);
    sampler(tonemapProgram, "normalMap", sceneNormal, 2);
    sampler(tonemapProgram, "bloomMap", bloomPyramid.empty() ? 0 : bloomPyramid.front().texture, 3);
    sampler(tonemapProgram, "exposureMap", exposure[exposureIndex].texture, 4);
    scalar(tonemapProgram, "bloomStrength", settings().bloom == Quality::Off ? 0 : settings().bloomStrength);
    scalar(tonemapProgram, "exposureEV", settings().exposureCompensation +
        (camera.optics && camera.optics->physicalExposure ? float(std::log2(camera.optics->exposureMultiplier())) : 0));
    integer(tonemapProgram, "autoExposure", settings().autoExposure && !(camera.optics && camera.optics->physicalExposure));
    integer(tonemapProgram, "toneMapper", int(settings().toneMapper));
    scalar(tonemapProgram, "contrast", settings().contrast);
    scalar(tonemapProgram, "saturation", settings().saturation);
    scalar(tonemapProgram, "temperature", settings().temperature);
    scalar(tonemapProgram, "vignette", settings().vignette);
    scalar(tonemapProgram, "fusion", std::array{0.f, .15f, .3f, .5f, .7f}[int(settings().edgeFusion)]);
    gl.Uniform2f(uniform(tonemapProgram, "pixel"), 1.f / width, 1.f / height);
    fullscreen(tonemapProgram, display.fbo);
    antialias(display.texture);
    gl.BindSampler(13, 0);
    gl.BindFramebuffer(GL_FRAMEBUFFER, outputFramebuffer());
    gl.Viewport(0, 0, outputWidth, outputHeight);
    previousVP = vp;
    previousView = view;
    lastEye = camera.position;
    lastDirection = camera.direction;
    previousModels.clear();
    if(!compactScene)for (const auto &c : prepared)
        if (c.source && c.source->objectId)
            previousModels[c.source->objectId] = c.instance.model;
    historyValid = true;
    frameIndex++;
    stats.temporalHistoryValid = settings().antiAliasing == AntiAliasing::TAA && historyValid;
    stats.cpuMilliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - cpuStart).count();
    if (query) {
        gl.EndQuery(0x88BF);
        queryActive[qi] = true;
    }
    queryIndex++;
}
void Renderer::Impl::drawUi() {
    gl.Disable(0x0B71);
    gl.Enable(GL_BLEND);
    gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl.UseProgram(uiProgram);
    gl.BindVertexArray(uiVao);
    gl.BindBuffer(GL_ARRAY_BUFFER, uiVbo);
    gl.BufferData(GL_ARRAY_BUFFER, ui.size() * sizeof(UiVertex), ui.data(), GL_STREAM_DRAW);
    gl.DrawArrays(GL_TRIANGLES, 0, ui.size());
    stats.drawCalls++;
    gl.Disable(GL_BLEND);
    drawImages();
}
} // namespace rf::gfx
