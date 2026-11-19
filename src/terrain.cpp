#include "internal.hpp"
#include <chrono>
#include <stdexcept>

namespace rf::gfx {
namespace {
TileData generate(TileKey key, int divisions, bool hardware, unsigned seed,
                  std::function<float(float, float)> fn) {
    TileData data;
    data.divisions = divisions;
    float ox = key.first * 32.f, oz = key.second * 32.f;
    auto height = [&](float x, float z) {
        float h = fn ? fn(x, z) : terrainHeight(x, z, seed);
        if (!std::isfinite(h) || std::abs(h) > 10000)
            throw std::invalid_argument("Terrain generator returned invalid height");
        return h;
    };
    auto point = [&](float x, float z) {
        float h = height(x + ox, z + oz), dx = height(x + ox + .25f, z + oz) - height(x + ox - .25f, z + oz),
              dz = height(x + ox, z + oz + .25f) - height(x + ox, z + oz - .25f);
        return Vertex{{x, h, z}, normalized({-dx, .5f, -dz}), {}, (x + ox) * .125f, (z + oz) * .125f};
    };
    if (hardware) {
        for (int z = 0; z < 4; z++)
            for (int x = 0; x < 4; x++)
                for (auto [vx, vz] : {std::pair{x * 8.f, z * 8.f},
                                      {(x + 1) * 8.f, z * 8.f},
                                      {(x + 1) * 8.f, (z + 1) * 8.f},
                                      {x * 8.f, (z + 1) * 8.f}})
                    data.vertices.push_back(point(vx, vz));
        for (int z = 0; z <= 64; z++)
            for (int x = 0; x <= 64; x++)
                data.heights.push_back(height(ox + x * .5f, oz + z * .5f));
        return data;
    }
    float step = 32.f / divisions;
    data.vertices.reserve(divisions * divisions * 6 + divisions * 24);
    for (int z = 0; z < divisions; z++)
        for (int x = 0; x < divisions; x++) {
            auto a = point(x * step, z * step), b = point(x * step, (z + 1) * step),
                 c = point((x + 1) * step, z * step), d = point((x + 1) * step, (z + 1) * step);
            data.vertices.insert(data.vertices.end(), {a, b, c, c, b, d});
        }
    // Skirts close cracks between distance-dependent mesh resolutions.
    for (int side = 0; side < 4; side++)
        for (int k = 0; k < divisions; k++) {
            float t = k * step, next = (k + 1) * step;
            auto a = point(side == 0   ? 0
                           : side == 1 ? 32
                                       : t,
                           side == 2   ? 0
                           : side == 3 ? 32
                                       : t),
                 b = point(side == 0   ? 0
                           : side == 1 ? 32
                                       : next,
                           side == 2   ? 0
                           : side == 3 ? 32
                                       : next);
            auto c = a, d = b;
            c.p.y -= 2;
            d.p.y -= 2;
            data.vertices.insert(data.vertices.end(), {a, b, c, c, b, d});
        }
    return data;
}
} // namespace
void Renderer::Impl::streamTerrain() {
    bool hardware = settings().terrainTessellation != Quality::Off &&
                    settings().tessellationBackend == TessellationBackend::Hardware;
    if (terrainSeed != terrainSpec.seed || terrainRevision != terrainSpec.revision ||
        terrainQuality != settings().terrainTessellation || terrainHardware != hardware) {
        for (auto &[key, t] : tiles) {
            (void)key;
            destroy(t);
        }
        tiles.clear();
        pending.clear();
        terrainSeed = terrainSpec.seed;
        terrainRevision = terrainSpec.revision;
        terrainQuality = settings().terrainTessellation;
        terrainHardware = hardware;
    }
    int cx = std::floor(camera.position.x / 32), cz = std::floor(camera.position.z / 32),
        radius = std::ceil(settings().viewDistance / 32);
    int base = std::array{8, 16, 32, 48, 64}[int(settings().terrainTessellation)];
    stats.terrainSubdivisions = hardware ? base / 4 : base;
    stats.hardwareTessellation = hardware;
    std::vector<TileKey> wanted;
    for (int z = -radius; z <= radius; z++)
        for (int x = -radius; x <= radius; x++) {
            TileKey key{cx + x, cz + z};
            if (flatDistance(camera.position, {key.first * 32.f + 16, 0, key.second * 32.f + 16}) <
                settings().viewDistance + 24)
                wanted.push_back(key);
        }
    std::sort(wanted.begin(), wanted.end(), [&](auto a, auto b) {
        return std::hypot(a.first - cx, a.second - cz) < std::hypot(b.first - cx, b.second - cz);
    });
    int added = 0;
    for (auto key : wanted) {
        float distance = flatDistance(camera.position, {key.first * 32.f + 16, 0, key.second * 32.f + 16});
        int divisions = hardware         ? base
                        : distance > 160 ? std::min(base, 8)
                        : distance > 80  ? std::min(base, 16)
                                         : base;
        auto current = tiles.find(key);
        if (!pending.contains(key) && (current == tiles.end() || current->second.divisions != divisions) &&
            (context.offscreen || added < 8)) {
            unsigned seed = terrainSpec.seed;
            auto fn = terrainSpec.height;
            pending.emplace(key, PendingTile{jobs.submit([key, divisions, hardware, seed, fn] {
                                                 return generate(key, divisions, hardware, seed, fn);
                                             }),
                                             divisions});
            added++;
        }
    }
    for (auto it = pending.begin(); it != pending.end();) {
        if (context.offscreen ||
            it->second.future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            TileData data = it->second.future.get();
            Tile t;
            t.mesh = upload(data.vertices);
            t.divisions = data.divisions;
            if (hardware) {
                t.heightMap = texture(65, 65, 0x822E, 0x1903, vl::GL_FLOAT, data.heights.data());
                auto limits = std::minmax_element(data.heights.begin(), data.heights.end());
                float half = (*limits.second - *limits.first) * .5f;
                t.mesh.center = {16, (*limits.first + *limits.second) * .5f, 16};
                t.mesh.radius = std::sqrt(512 + half * half);
            }
            auto existing = tiles.find(it->first);
            if (existing != tiles.end()) {
                destroy(existing->second);
                existing->second = t;
            } else
                tiles.emplace(it->first, t);
            it = pending.erase(it);
        } else
            ++it;
    }
    for (auto it = tiles.begin(); it != tiles.end();) {
        if (flatDistance(camera.position, {it->first.first * 32.f + 16, 0, it->first.second * 32.f + 16}) >
            settings().viewDistance + 70) {
            destroy(it->second);
            it = tiles.erase(it);
        } else
            ++it;
    }
    stats.residentTerrainTiles = tiles.size();
    stats.pendingTerrainJobs = pending.size();
}
void Renderer::Impl::drawTerrain(bool shadow) {
    if (shadow && !terrainSpec.material.castShadow)
        return;
    unsigned p = terrainHardware ? (shadow ? tessShadowProgram : tessProgram)
                                 : (shadow ? shadowProgram : sceneProgram);
    const Mat4 &matrix = shadow ? lightVP : vp;
    common(p, matrix);
    material(p, terrainSpec.material);
    std::array<GpuInstance, 1> batch;
    batch[0].color=terrainSpec.material.albedo;
    for (auto &[key, tile] : tiles) {
        Transform t{{key.first * 32.f, 0, key.second * 32.f}, {1, 1, 1}, 0};
        if (!visible(tile.mesh, t, matrix))
            continue;
        batch[0].model=model(t);batch[0].previous=batch[0].model;
        if (terrainHardware) {
            sampler(p, "heightMap", tile.heightMap, 5);
            gl.Uniform2f(gl.GetUniformLocation(p, "tileOrigin"), t.position.x, t.position.z);
            gl.Uniform2f(gl.GetUniformLocation(p, "cameraXZ"), camera.position.x, camera.position.z);
            scalar(p, "tessLevel",
                   std::min(float(caps.maxTessellationLevel), stats.terrainSubdivisions * 1.f));
            auto c = terrainSpec.material.albedo;
            gl.Uniform4f(gl.GetUniformLocation(p, "terrainTint"), c.r, c.g, c.b, c.a);
        }
        mesh(tile.mesh, batch, terrainHardware);
        if (!shadow)
            stats.visibleTerrainTiles++;
    }
}
} // namespace rf::gfx
