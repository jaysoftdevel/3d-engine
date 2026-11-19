#include "internal.hpp"
#include <stdexcept>
namespace rf::gfx {
using namespace vl;
EnvironmentId Renderer::createEnvironment(int w, int h, std::span<const float> rgb) {
    return createEnvironment(w, h, rgb, 0);
}
EnvironmentId Renderer::createEnvironment(int w, int h, std::span<const float> rgb, float highlightLimit) {
    auto &i = *impl;
    if (w <= 0 || h <= 0 || w > 8192 || h > 4096 || rgb.size() != size_t(w) * h * 3)
        throw std::invalid_argument("invalid HDR environment");
    for (auto value : rgb)
        if (!std::isfinite(value) || value < 0)
            throw std::invalid_argument("invalid HDR radiance");
    if (!std::isfinite(highlightLimit) || highlightLimit < 0)
        throw std::invalid_argument("invalid IBL highlight limit");
    auto source = i.texture(w, h, 0x8815, 0x1907, GL_FLOAT, rgb.data());
    auto cube = [&](int size, int levels) {
        unsigned t;
        i.gl.GenTextures(1, &t);
        i.gl.BindTexture(0x8513, t);
        for (int level = 0; level < levels; level++)
            for (int face = 0; face < 6; face++)
                i.gl.TexImage2D(0x8515 + face, level, GL_RGBA16F, std::max(1, size >> level),
                                std::max(1, size >> level), 0, GL_RGBA, GL_FLOAT, nullptr);
        i.gl.TexParameteri(0x8513, GL_TEXTURE_MIN_FILTER, levels > 1 ? 0x2703 : GL_LINEAR);
        i.gl.TexParameteri(0x8513, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        for (auto edge : {0x2802, 0x2803, 0x8072})
            i.gl.TexParameteri(0x8513, edge, GL_CLAMP_TO_EDGE);
        i.gl.TexParameteri(0x8513, 0x813D, levels - 1);
        return t;
    };
    if (!std::isfinite(highlightLimit) || highlightLimit < 0)
        throw std::invalid_argument("invalid IBL highlight limit");
    unsigned lightingSource = source;
    if (highlightLimit > 0) {
        int lw = std::min(w, 512), lh = std::min(h, 256);
        std::vector<float> filtered(size_t(lw) * lh * 3);
        for (int y = 0; y < lh; y++)
            for (int x = 0; x < lw; x++) {
                int x0 = x * w / lw, x1 = (x + 1) * w / lw, y0 = y * h / lh, y1 = (y + 1) * h / lh;
                for (int c = 0; c < 3; c++) {
                    double sum = 0;
                    for (int sy = y0; sy < y1; sy++)
                        for (int sx = x0; sx < x1; sx++)
                            sum += std::min(rgb[(size_t(sy) * w + sx) * 3 + c], highlightLimit);
                    filtered[(size_t(y) * lw + x) * 3 + c] = sum / ((x1 - x0) * (y1 - y0));
                }
            }
        lightingSource = i.texture(lw, lh, 0x8815, 0x1907, GL_FLOAT, filtered.data());
        i.gl.GenerateMipmap(GL_TEXTURE_2D);
        i.gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 0x2703);
    }
    Impl::EnvMap e;
    e.source = source;
    i.gl.BindTexture(GL_TEXTURE_2D, source);
    i.gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x2901);
    i.gl.GenerateMipmap(GL_TEXTURE_2D);
    i.gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 0x2703);
    e.specular = cube(128, 8);
    e.irradiance = cube(32, 1);
    e.brdf = i.texture(128, 128, 0x822F, 0x8227, GL_FLOAT);
    i.gl.Disable(0x0B71);
    i.gl.Disable(GL_BLEND);
    i.gl.BindFramebuffer(GL_FRAMEBUFFER, i.iblFbo);
    i.gl.DrawBuffer(GL_COLOR_ATTACHMENT0);
    i.gl.BindVertexArray(i.fullscreenVao);
    i.gl.UseProgram(i.iblProgram);
    i.sampler(i.iblProgram, "source", lightingSource, 0);
    for (int mode = 0; mode < 2; mode++) {
        int levels = mode ? 1 : 8, size = mode ? 32 : 128;
        for (int level = 0; level < levels; level++)
            for (int face = 0; face < 6; face++) {
                i.gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0x8515 + face,
                                          mode ? e.irradiance : e.specular, level);
                if (i.gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                    throw std::runtime_error("IBL target incomplete");
                i.gl.Viewport(0, 0, std::max(1, size >> level), std::max(1, size >> level));
                i.integer(i.iblProgram, "face", face);
                i.integer(i.iblProgram, "irradiance", mode);
                i.scalar(i.iblProgram, "roughness", float(level) / 7);
                i.gl.DrawArrays(GL_TRIANGLES, 0, 3);
            }
    }
    i.gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, e.brdf, 0);
    i.gl.Viewport(0, 0, 128, 128);
    i.gl.UseProgram(i.brdfProgram);
    i.gl.DrawArrays(GL_TRIANGLES, 0, 3);
    i.gl.BindFramebuffer(GL_FRAMEBUFFER, 0);

    if (lightingSource != source)
        i.gl.DeleteTextures(1, &lightingSource);
    i.gl.Enable(0x884F);
    auto id = i.nextEnvironment++;
    i.environments[id] = e;
    i.historyValid = false;
    return {id};
}
void Renderer::destroyEnvironment(EnvironmentId id) {
    auto it = impl->environments.find(id.value);
    if (it != impl->environments.end()) {
        for (auto t : {it->second.specular, it->second.irradiance, it->second.brdf, it->second.source})
            impl->gl.DeleteTextures(1, &t);
        impl->environments.erase(it);
    }
}
} // namespace rf::gfx
