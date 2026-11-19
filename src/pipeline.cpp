#include "internal.hpp"
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
        oh == outputHeight)
        return;
    releaseTargets();
    width = w;
    outputWidth = ow;
    outputHeight = oh;
    historyValid = false;
    height = h;
    allocatedSamples = samples;
    buffersDirty = false;
    sceneColor = texture(w, h, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    sceneNormal = texture(w, h, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    sceneMotion = texture(w, h, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    sceneSurface = texture(w, h, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
    sceneDepth = texture(w, h, 0x81A6, 0x1902, GL_FLOAT);
    gl.BindTexture(GL_TEXTURE_2D, sceneDepth);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 0x2600);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 0x2600);
    gl.GenFramebuffers(1, &sceneFbo);
    gl.BindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
    unsigned colorTextures[] = {sceneColor, sceneNormal, sceneSurface, sceneMotion};
    unsigned attachments[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT0 + 1, GL_COLOR_ATTACHMENT0 + 2,
                              GL_COLOR_ATTACHMENT0 + 3};
    for (int n = 0; n < 4; n++)
        gl.FramebufferTexture2D(GL_FRAMEBUFFER, attachments[n], GL_TEXTURE_2D, colorTextures[n], 0);
    gl.FramebufferTexture2D(GL_FRAMEBUFFER, 0x8D00, GL_TEXTURE_2D, sceneDepth, 0);
    gl.DrawBuffers(4, attachments);
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
    gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
}
void Renderer::Impl::sampler(unsigned p, const char *name, unsigned tex, int unit) {
    gl.ActiveTexture(GL_TEXTURE0 + unit);
    gl.BindTexture(GL_TEXTURE_2D, tex);
    gl.Uniform1i(gl.GetUniformLocation(p, name), unit);
}
void Renderer::Impl::matrix(unsigned p, const char *name, const Mat4 &m) {
    gl.UniformMatrix4fv(gl.GetUniformLocation(p, name), 1, 0, m.m.data());
}
void Renderer::Impl::scalar(unsigned p, const char *name, float f) {
    gl.Uniform1f(gl.GetUniformLocation(p, name), f);
}
void Renderer::Impl::integer(unsigned p, const char *name, int n) {
    gl.Uniform1i(gl.GetUniformLocation(p, name), n);
}
void Renderer::Impl::vector(unsigned p, const char *name, V3 v) {
    gl.Uniform3f(gl.GetUniformLocation(p, name), v.x, v.y, v.z);
}
void Renderer::Impl::common(unsigned p, const Mat4 &m) {
    gl.UseProgram(p);
    matrix(p, "vp", m);
    matrix(p, "lightVP", lightVP);
    vector(p, "eye", camera.position);
    vector(p, "sun", normalized(environment.sunDirection));
    vector(p, "sunColor", {environment.sunColor.r, environment.sunColor.g, environment.sunColor.b});
    vector(p, "skyColor", {environment.skyColor.r, environment.skyColor.g, environment.skyColor.b});
    vector(p, "groundColor",
           {environment.groundColor.r, environment.groundColor.g, environment.groundColor.b});
    scalar(p, "sunIntensity", environment.sunIntensity);
    scalar(p, "ambientIntensity", environment.ambientIntensity);
    scalar(p, "viewDistance", settings().viewDistance);
    scalar(p, "storm", environment.storm);
    scalar(p, "fogDensity", environment.fogDensity);
    integer(p, "shadowQuality", int(settings().shadows));
    scalar(p, "windTime", environment.atmosphere.time);
    vector(p, "windVector", environment.atmosphere.wind);
    integer(p, "giMode", int(settings().globalIllumination));
    sampler(p, "shadowMap", shadowDepth, 4);
    matrix(p, "previousVP", previousVP);
    matrix(p, "view", view);
    matrix(p, "previousView", previousView);
    bindAdvanced(p);
}
void Renderer::Impl::material(unsigned p, const Material &m) {
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
    integer(p, "hasNormal", m.normalTexture.value != 0);
    integer(p, "hasOrm", m.ormTexture.value != 0);
    integer(p, "hasEmissive", m.emissiveTexture.value != 0);
    scalar(p, "normalScale", m.normalScale);
    scalar(p, "alphaCutoff", m.alphaCutoff);
    integer(p, "alphaMode", int(m.alphaMode));
    integer(p, "doubleSided", m.doubleSided);
    integer(p, "reactive", m.temporalReactive);
    integer(p, "textureProjection", int(m.projection));
    scalar(p, "textureScale", m.textureScale);
    scalar(p, "macroVariation", m.macroVariation);
    scalar(p, "clearcoat", m.clearcoat);
    scalar(p, "clearcoatRoughness", m.clearcoatRoughness);
    scalar(p, "windStrength", m.windStrength);
    scalar(p, "occlusionStrength", m.occlusionStrength);
}
Mat4 Renderer::Impl::model(const Transform &t) const {
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
        gl.DrawArraysInstanced(0x000E, 0, m.count, batch.size());
        stats.triangles += m.count / 4 * stats.terrainSubdivisions * stats.terrainSubdivisions * 2;
    } else {
        gl.DrawArraysInstanced(GL_TRIANGLES, 0, m.count, batch.size());
        stats.triangles += m.count / 3 * batch.size();
    }
    stats.drawCalls++;
}
void Renderer::Impl::drawCommands(unsigned p, const Mat4 &m, bool shadow, bool transparent) {
    common(p, m);
    using Key = std::array<unsigned, 23>;
    auto bits = [](float f) {
        unsigned u;
        std::memcpy(&u, &f, 4);
        return u;
    };
    auto key = [&](const Draw &c) {
        auto &a = c.material;
        return Key{c.mesh.value,
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
    auto instance = [&](const Draw &c) {
        Mat4 x = c.modelMatrix ? *c.modelMatrix : model(c.transform);
        auto old = previousModels.find(c.objectId);
        return GpuInstance{x, historyValid && c.objectId && old != previousModels.end() ? old->second : x,
                           c.material.albedo};
    };
    std::vector<const Draw *> visibleCommands;
    for (auto &c : commands) {
        bool blend = c.material.alphaMode == AlphaMode::Blend || c.material.alphaMode == AlphaMode::Additive;
        if (blend != transparent || (shadow && (!c.material.castShadow || blend)))
            continue;
        auto it = meshes.find(c.mesh.value);
        if (it != meshes.end() &&
            visible(it->second, c.transform, m, c.modelMatrix ? &*c.modelMatrix : nullptr))
            visibleCommands.push_back(&c);
    }
    if (transparent) {
        std::stable_sort(visibleCommands.begin(), visibleCommands.end(), [&](auto *a, auto *b) {
            auto A = a->modelMatrix ? *a->modelMatrix : model(a->transform),
                 B = b->modelMatrix ? *b->modelMatrix : model(b->transform);
            return distance(transformPoint(A, {}), camera.position) >
                   distance(transformPoint(B, {}), camera.position);
        });
        std::vector<GpuInstance> batch;
        size_t i = 0;
        while (i < visibleCommands.size()) {
            auto *c = visibleCommands[i];
            Key k = key(*c);
            batch.clear();
            do {
                batch.push_back(instance(*visibleCommands[i++]));
            } while (i < visibleCommands.size() && key(*visibleCommands[i]) == k);
            material(p, c->material);
            if (p == sceneProgram)
                gl.BlendFunc(GL_SRC_ALPHA,
                             c->material.alphaMode == AlphaMode::Additive ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
            mesh(meshes.at(c->mesh.value), batch);
        }
    } else {
        std::map<Key, std::vector<GpuInstance>> batches;
        std::map<Key, const Material *> materials;
        for (auto *c : visibleCommands) {
            Key k = key(*c);
            batches[k].push_back(instance(*c));
            materials[k] = &c->material;
        }
        for (auto &[k, b] : batches) {
            material(p, *materials[k]);
            mesh(meshes.at(k[0]), b);
        }
    }
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
        gl.Uniform4f(gl.GetUniformLocation(p, "rtMetrics"), 1.f / width, 1.f / height, width, height);
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
        gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
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
    gl.Uniform2f(gl.GetUniformLocation(p, "pixel"), 1.f / width, 1.f / height);
    scalar(p, "sharpening", settings().sharpening);
    gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
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
    if (frameIndex && (distance(lastEye, camera.position) > 10 || dot(lastDirection, camera.direction) < .5f))
        historyValid = false;
    bool query = false;
    unsigned qi = queryIndex % 3;
    if (settings().gpuProfiling) {
        int ready = 0;
        if (queryActive[qi])
            gl.GetQueryObjectiv(gpuQueries[qi], 0x8867, &ready);
        if (ready) {
            unsigned long long ns;
            gl.GetQueryObjectui64v(gpuQueries[qi], 0x8866, &ns);
            stats.gpuMilliseconds = ns / 1000000.;
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
    gl.DrawBuffers(4, outputs);
    gl.ClearColor(environment.skyColor.r * .4f, environment.skyColor.g * .4f, environment.skyColor.b * .4f,
                  1);
    gl.Clear(GL_COLOR_BUFFER_BIT | 0x100);
    if (msaaFbo)
        gl.Enable(0x809D);
    else
        gl.Disable(0x809D);
    drawCommands(sceneProgram, vp, false);
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
    integer(skyProgram, "atmosphereEnabled", environment.atmosphere.enabled);
    integer(skyProgram, "cloudSteps", std::array{0, 8, 12, 20, 32}[int(environment.atmosphere.clouds)]);
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
        matrix(aoProgram, "projection", projection);
        matrix(aoProgram, "invProjection", invProjection);
        matrix(aoProgram, "view", view);
        integer(aoProgram, "samples", std::array{0, 8, 16, 32, 48}[int(settings().ambientOcclusion)]);
        scalar(aoProgram, "radius", 1.5f);
        fullscreen(aoProgram, aoRaw.fbo);
        gl.UseProgram(aoBlurProgram);
        sampler(aoBlurProgram, "depthMap", sceneDepth, 1);
        matrix(aoBlurProgram, "invProjection", invProjection);
        sampler(aoBlurProgram, "source", aoRaw.texture, 0);
        gl.Uniform2f(gl.GetUniformLocation(aoBlurProgram, "direction"), 1.f / width, 0);
        fullscreen(aoBlurProgram, aoA.fbo);
        sampler(aoBlurProgram, "source", aoA.texture, 0);
        gl.Uniform2f(gl.GetUniformLocation(aoBlurProgram, "direction"), 0, 1.f / height);
        fullscreen(aoBlurProgram, aoB.fbo);
    }
    if (settings().reflections != Quality::Off) {
        gl.ActiveTexture(GL_TEXTURE0);
        gl.BindTexture(GL_TEXTURE_2D, sceneColor);
        gl.GenerateMipmap(GL_TEXTURE_2D);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 0x2703);
    }
    gl.UseProgram(lightingProgram);
    sampler(lightingProgram, "source", sceneColor, 0);
    sampler(lightingProgram, "depthMap", sceneDepth, 1);
    sampler(lightingProgram, "normalMap", sceneNormal, 2);
    sampler(lightingProgram, "surfaceMap", sceneSurface, 3);
    sampler(lightingProgram, "aoMap", aoB.texture, 4);
    matrix(lightingProgram, "projection", projection);
    matrix(lightingProgram, "invProjection", invProjection);
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
    gl.BindFramebuffer(GL_FRAMEBUFFER, transparentFbo);
    gl.DrawBuffer(GL_COLOR_ATTACHMENT0);
    gl.Enable(0x0B71);
    gl.DepthMask(0);
    gl.Enable(GL_BLEND);
    drawCommands(sceneProgram, vp, false, true);
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
        gl.UseProgram(dofProgram);
        sampler(dofProgram, "source", current, 0);
        sampler(dofProgram, "depthMap", sceneDepth, 1);
        matrix(dofProgram, "invProjection", invProjection);
        scalar(dofProgram, "focusDistance", settings().focusDistance);
        scalar(dofProgram, "aperture", settings().aperture);
        scalar(dofProgram, "maxBlur", settings().maximumBlur);
        integer(dofProgram, "samples", std::array{0, 8, 16, 24, 32}[int(settings().depthOfField)]);
        gl.Uniform2f(gl.GetUniformLocation(dofProgram, "pixel"), 1.f / width, 1.f / height);
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
    scalar(tonemapProgram, "exposureEV", settings().exposureCompensation);
    integer(tonemapProgram, "autoExposure", settings().autoExposure);
    integer(tonemapProgram, "toneMapper", int(settings().toneMapper));
    scalar(tonemapProgram, "contrast", settings().contrast);
    scalar(tonemapProgram, "saturation", settings().saturation);
    scalar(tonemapProgram, "temperature", settings().temperature);
    scalar(tonemapProgram, "vignette", settings().vignette);
    scalar(tonemapProgram, "fusion", std::array{0.f, .15f, .3f, .5f, .7f}[int(settings().edgeFusion)]);
    gl.Uniform2f(gl.GetUniformLocation(tonemapProgram, "pixel"), 1.f / width, 1.f / height);
    fullscreen(tonemapProgram, display.fbo);
    antialias(display.texture);
    gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
    gl.Viewport(0, 0, outputWidth, outputHeight);
    previousVP = vp;
    previousView = view;
    lastEye = camera.position;
    lastDirection = camera.direction;
    previousModels.clear();
    for (auto &c : commands)
        if (c.objectId)
            previousModels[c.objectId] = c.modelMatrix ? *c.modelMatrix : model(c.transform);
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
