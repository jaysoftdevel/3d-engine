#include "internal.hpp"
#include <chrono>
#include <stdexcept>

namespace rf::gfx {
namespace {
TileData generate(TileKey key, int divisions, bool hardware, unsigned seed,float span,
                  std::function<float(float, float)> fn,std::function<Color(float,float)> color) {
    TileData data;
    data.divisions = divisions;
    float ox = key.first * span, oz = key.second * span;
    auto height = [&](float x, float z) {
        float h = fn ? fn(x, z) : terrainHeight(x, z, seed);
        if (!std::isfinite(h) || std::abs(h) > 10000)
            throw std::invalid_argument("Terrain generator returned invalid height");
        return h;
    };
    auto point = [&](float x, float z) {
        float h = height(x + ox, z + oz), dx = height(x + ox + .25f, z + oz) - height(x + ox - .25f, z + oz),
              dz = height(x + ox, z + oz + .25f) - height(x + ox, z + oz - .25f);
        return Vertex{{x, h, z}, normalized({-dx, .5f, -dz}), color?color(x+ox,z+oz):Color{}, (x + ox) * .125f, (z + oz) * .125f};
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
    float step = span / divisions;
    data.vertices.reserve(divisions * divisions * 6 + divisions * 24);
    // Retain the original triangles and seams while evaluating shared vertices once.
    std::vector<Vertex> grid(size_t(divisions + 1) * (divisions + 1));
    for (int z = 0; z <= divisions; ++z)
        for (int x = 0; x <= divisions; ++x)
            grid[z * (divisions + 1) + x] = point(x * step, z * step);
    for (int z = 0; z < divisions; z++)
        for (int x = 0; x < divisions; x++) {
            auto a = grid[z * (divisions + 1) + x], b = grid[(z + 1) * (divisions + 1) + x],
                 c = grid[z * (divisions + 1) + x + 1], d = grid[(z + 1) * (divisions + 1) + x + 1];
            data.vertices.insert(data.vertices.end(), {a, b, c, c, b, d});
        }
    // Skirts close cracks between distance-dependent mesh resolutions.
    for (int side = 0; side < 4; side++)
        for (int k = 0; k < divisions; k++) {
            auto a=grid[(side==2?0:side==3?divisions:k)*(divisions+1)+
                        (side==0?0:side==1?divisions:k)],
                 b=grid[(side==2?0:side==3?divisions:k+1)*(divisions+1)+
                        (side==0?0:side==1?divisions:k+1)];
            auto c = a, d = b;
            c.p.y -= span==64?8:2;
            d.p.y -= span==64?8:2;
            data.vertices.insert(data.vertices.end(), {a, b, c, c, b, d});
        }
    return data;
}
} // namespace
void Renderer::Impl::streamTerrain() {
    bool hardware = settings().terrainTessellation != Quality::Off &&
                    settings().tessellationBackend == TessellationBackend::Hardware && !terrainSpec.color;
    const int span=compactScene?64:32;
    if (terrainSeed != terrainSpec.seed || terrainRevision != terrainSpec.revision ||
        terrainQuality != settings().terrainTessellation || terrainHardware != hardware || terrainTileSize!=span) {
        for (auto &[key, t] : tiles) {
            (void)key;
            destroy(t);
        }
        tiles.clear();
        pending.clear();
        terrainSeed = terrainSpec.seed;
        terrainRevision = terrainSpec.revision;
        terrainQuality = settings().terrainTessellation;
        terrainHardware = hardware;terrainTileSize=span;
    }
    int cx = std::floor(camera.position.x / span), cz = std::floor(camera.position.z / span),
        radius = std::ceil(settings().viewDistance / span);
    int base = std::array{8, 16, 32, 48, 64}[int(settings().terrainTessellation)];
    stats.terrainSubdivisions = hardware ? base / 4 : base;
    stats.hardwareTessellation = hardware;
    std::vector<TileKey> wanted;
    for (int z = -radius; z <= radius; z++)
        for (int x = -radius; x <= radius; x++) {
            TileKey key{cx + x, cz + z};
            if (flatDistance(camera.position, {key.first * float(span) + span*.5f, 0, key.second * float(span) + span*.5f}) <
                settings().viewDistance + span*.75f)
                wanted.push_back(key);
        }
    std::sort(wanted.begin(), wanted.end(), [&](auto a, auto b) {
        return (a.first - cx) * (a.first - cx) + (a.second - cz) * (a.second - cz) <
               (b.first - cx) * (b.first - cx) + (b.second - cz) * (b.second - cz);
    });
    int added = 0;
    for (auto key : wanted) {
        float distance = flatDistance(camera.position, {key.first * float(span) + span*.5f, 0, key.second * float(span) + span*.5f});
        int divisions = compactScene? (distance>256?1:distance>160?2:distance>80?4:8) : hardware         ? base
                        : distance > 160 ? std::min(base, 8)
                        : distance > 80  ? std::min(base, 16)
                                         : base;
        auto current = tiles.find(key);
        if (!pending.contains(key) && (current == tiles.end() || current->second.divisions != divisions) &&
            (context.offscreen || added < 8)) {
            unsigned seed = terrainSpec.seed;
            auto fn = terrainSpec.height;auto color=terrainSpec.color;
            pending.emplace(key, PendingTile{jobs.submit([key, divisions, hardware, seed, fn,color,span] {
                                                 return generate(key, divisions, hardware, seed,span, fn,color);
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
        if (flatDistance(camera.position, {it->first.first * float(span) + span*.5f, 0, it->first.second * float(span) + span*.5f}) >
            settings().viewDistance + 70) {
            destroy(it->second);
            it = tiles.erase(it);
        } else
            ++it;
    }
    stats.residentTerrainTiles = tiles.size();
    stats.pendingTerrainJobs = pending.size();
}
void Renderer::Impl::drawTerrain(bool shadow, bool depthOnly) {
    if (shadow && !terrainSpec.material.castShadow)
        return;
    const bool opaque = terrainSpec.material.alphaMode == AlphaMode::Opaque;
    if (depthOnly && !opaque)
        return;
    const bool lateDepth = depthPrepassed && opaque && !shadow && !depthOnly;
    const bool depthProgram = shadow || depthOnly;
    unsigned p = terrainHardware ? (depthProgram ? tessShadowProgram : tessProgram)
                                 : (depthProgram ? shadowProgram : surfaceProgram());
    const Mat4 &matrix = shadow ? lightVP : vp;
    common(p, matrix);
    material(p, terrainSpec.material);
    if (lateDepth) {
        gl.DepthFunc(0x0202);
        gl.DepthMask(0);
    }
    std::array<GpuInstance, 1> batch;
    batch[0].color=terrainSpec.material.albedo;
    for (auto &[key, tile] : tiles) {
        Transform t{{key.first * float(terrainTileSize), 0, key.second * float(terrainTileSize)}, {1, 1, 1}, 0};
        if (!visible(tile.mesh, t, matrix))
            continue;
        batch[0].model=model(t);batch[0].previous=batch[0].model;
        if (terrainHardware) {
            sampler(p, "heightMap", tile.heightMap, 5);
            gl.Uniform2f(uniform(p, "tileOrigin"), t.position.x, t.position.z);
            gl.Uniform2f(uniform(p, "cameraXZ"), camera.position.x, camera.position.z);
            scalar(p, "tessLevel",
                   std::min(float(caps.maxTessellationLevel), stats.terrainSubdivisions * 1.f));
            auto c = terrainSpec.material.albedo;
            gl.Uniform4f(uniform(p, "terrainTint"), c.r, c.g, c.b, c.a);
        }
        mesh(tile.mesh, batch, terrainHardware);
        if (!shadow && !depthOnly)
            stats.visibleTerrainTiles++;
    }
    if (lateDepth) {
        gl.DepthFunc(0x0201);
        gl.DepthMask(1);
    }
}
} // namespace rf::gfx
