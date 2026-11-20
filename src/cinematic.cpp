#include "cinematic_shaders.hpp"
#include "internal.hpp"
#include "shaders.hpp"
#include <cstddef>
#include <stdexcept>
namespace rf::gfx {
using namespace vl;
void Renderer::Impl::initializeCinematic() {
    gl.DeleteProgram(skyProgram);
    skyProgram = program(cinematic::skyVertex, cinematic::sky);
    particleProgram = program(cinematic::particleVertex, cinematic::particleFragment);
    imageProgram = program(cinematic::imageVertex, cinematic::imageFragment);
    gl.GenVertexArrays(1, &particleVao);
    gl.GenBuffers(1, &particleVbo);
    gl.BindVertexArray(particleVao);
    gl.BindBuffer(GL_ARRAY_BUFFER, particleVbo);
    for (unsigned n = 0; n < 4; n++) {
        gl.EnableVertexAttribArray(n);
        gl.VertexAttribPointer(n, 4, GL_FLOAT, 0, 64, reinterpret_cast<void *>(uintptr_t(n * 16)));
        gl.VertexAttribDivisor(n, 1);
    }
    constexpr int size = 32;
    std::vector<unsigned char> volume(size * size * size * 4);
    for (int z = 0; z < size; z++)
        for (int y = 0; y < size; y++)
            for (int x = 0; x < size; x++)
                for (int c = 0; c < 4; c++)
                    volume[((z * size + y) * size + x) * 4 + c] =
                        hash(x * 1723 + y * 731 + z * 9431 + c * 17237) & 255;
    gl.GenTextures(1, &noiseVolume);
    gl.BindTexture(0x806F, noiseVolume);
    gl.TexImage3D(0x806F, 0, GL_RGBA8, size, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, volume.data());
    gl.TexParameteri(0x806F, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.TexParameteri(0x806F, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    for (auto p : {GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T, unsigned(0x8072)})
        gl.TexParameteri(0x806F, p, 0x2901);
}

void Renderer::Impl::releaseCinematic() {
    gl.DeleteProgram(particleProgram);
    gl.DeleteProgram(imageProgram);
    gl.DeleteBuffers(1, &particleVbo);
    gl.DeleteVertexArrays(1, &particleVao);
    gl.DeleteTextures(1, &noiseVolume);
}
void Renderer::particles(std::span<const ParticleSprite> particles) {
    auto &i = *impl;
    if (i.particleCommands.size() + particles.size() > 131072)
        throw std::invalid_argument("particle sprite budget exceeded");
    for (auto &p : particles) {
        for (float f :
             {p.position.x, p.position.y, p.position.z, p.velocity.x, p.velocity.y, p.velocity.z, p.color.r,
              p.color.g, p.color.b, p.color.a, p.size, p.rotation, p.stretch, p.softness, p.age})
            if (!std::isfinite(f))
                throw std::invalid_argument("nonfinite particle sprite");
        if (p.size <= 0 || p.softness < 0 || p.stretch < 0 || int(p.style) < 0 || int(p.style) > 3)
            throw std::invalid_argument("invalid particle sprite");
    }
    i.particleCommands.insert(i.particleCommands.end(), particles.begin(), particles.end());
    i.rendered = false;
}
void Renderer::Impl::drawParticles() {
    if (particleCommands.empty())
        return;
    gl.BindFramebuffer(0x8CA8, sceneFbo);
    gl.BindFramebuffer(0x8CA9, particleDepthFbo);
    gl.BlitFramebuffer(0, 0, width, height, 0, 0, width, height, 0x100, 0x2600);
    gl.BindFramebuffer(GL_FRAMEBUFFER, transparentFbo);
    unsigned outputs[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT0 + 1};
    gl.DrawBuffers(2, outputs);
    gl.ColorMaski(1, 0, 0, 0, 1);
    gl.Enable(0x0B71);
    gl.DepthMask(0);
    gl.Enable(GL_BLEND);
    gl.BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    std::stable_sort(particleCommands.begin(), particleCommands.end(), [&](auto &a, auto &b) {
        return dot(a.position - camera.position, camera.direction) >
               dot(b.position - camera.position, camera.direction);
    });
    std::vector<std::array<float, 16>> gpu;
    gpu.reserve(particleCommands.size());
    for (auto &p : particleCommands)
        gpu.push_back({p.position.x, p.position.y, p.position.z, p.size, p.velocity.x, p.velocity.y,
                       p.velocity.z, p.stretch, p.color.r, p.color.g, p.color.b, p.color.a, p.rotation,
                       p.softness, p.age, float(p.style)});
    gl.UseProgram(particleProgram);
    matrix(particleProgram, "vp", vp);
    matrix(particleProgram, "view", view);
    projectionTerms(particleProgram, false);
    vector(particleProgram, "right", {view.m[0], view.m[4], view.m[8]});
    vector(particleProgram, "up", {view.m[1], view.m[5], view.m[9]});
    vector(particleProgram, "eye", camera.position);
    vector(particleProgram, "sun", normalized(environment.sunDirection));
    vector(particleProgram, "sunColor",
           {environment.sunColor.r, environment.sunColor.g, environment.sunColor.b});
    vector(particleProgram, "skyColor",
           {environment.skyColor.r, environment.skyColor.g, environment.skyColor.b});
    scalar(particleProgram, "sunIntensity", environment.sunIntensity);
    gl.Uniform2f(uniform(particleProgram, "viewport"), width, height);
    sampler(particleProgram, "depthMap", particleDepth, 0);
    bind(particleProgram, "noiseVolume", noiseVolume, 1, 0x806F);
    gl.BindVertexArray(particleVao);
    gl.BindBuffer(GL_ARRAY_BUFFER, particleVbo);
    gl.BufferData(GL_ARRAY_BUFFER, gpu.size() * 64, gpu.data(), GL_STREAM_DRAW);
    gl.DrawArraysInstanced(GL_TRIANGLES, 0, 6, gpu.size());
    stats.particleSprites = gpu.size();
    stats.particleDrawCalls = 1;
    stats.drawCalls++;
    stats.triangles += gpu.size() * 2;
    gl.ColorMaski(1, 1, 1, 1, 1);
    gl.DrawBuffer(GL_COLOR_ATTACHMENT0);
    gl.DepthMask(1);
    gl.Disable(GL_BLEND);
    gl.Disable(0x0B71);
}
void Renderer::image(TextureId texture, float x, float y, float w, float h, Color tint) {
    if (!impl->textures.contains(texture.value) || !std::isfinite(x + y + w + h) || w <= 0 || h <= 0)
        throw std::invalid_argument("invalid UI image");
    impl->uiImages.push_back({texture, x, y, w, h, tint});
}
void Renderer::Impl::drawImages() {
    if (uiImages.empty())
        return;
    gl.Disable(0x0B71);
    gl.Enable(GL_BLEND);
    gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl.UseProgram(imageProgram);
    gl.BindVertexArray(fullscreenVao);
    for (auto &i : uiImages) {
        auto it = textures.find(i.texture.value);
        if (it == textures.end())
            continue;
        sampler(imageProgram, "source", it->second.object, 0);
        gl.Uniform4f(uniform(imageProgram, "rectangle"), i.x, i.y, i.w, i.h);
        gl.Uniform4f(uniform(imageProgram, "tint"), i.tint.r, i.tint.g, i.tint.b, i.tint.a);
        gl.DrawArrays(GL_TRIANGLES, 0, 6);
        stats.drawCalls++;
    }
    gl.Disable(GL_BLEND);
}
} // namespace rf::gfx
