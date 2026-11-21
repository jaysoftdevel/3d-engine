#pragma once
#include <fstream>
#include <jsd/engine.hpp>
#include "process_id.hpp"
inline void cinematicTests(const std::function<void(bool, const char *)> &test) {
    using namespace jsd;
    FrameRateCounter timing;
    for (int i = 0; i < 25; i++) {
        timing.sample(.01);
        timing.sample(.03);
    }
    test(std::abs(timing.fps() - 50) < .0001,
         "FPS averages completed wall time across variable frame durations");
    test(std::abs(timing.milliseconds() - 20) < .0001, "frame duration matches measured FPS");
    timing.reset();
    timing.sample(0);
    test(timing.fps() == 0, "frame rate reset and zero duration do not invent a rate");
    bool badTiming = false;
    try {
        timing.sample(std::numeric_limits<double>::quiet_NaN());
    } catch (const std::invalid_argument &) {
        badTiming = true;
    }
    test(badTiming, "frame timing rejects nonfinite durations");
    for (auto geometry :
         {gfx::makeBeveledBox(), gfx::makeSphere(), gfx::makeTorus(), gfx::makeDetailedRock(412, 12)}) {
        bool healthy = !geometry.empty() && geometry.size() % 3 == 0;
        for (auto &v : geometry)
            healthy =
                healthy && std::isfinite(v.p.x + v.p.y + v.p.z) && std::abs(rf::length(v.n) - 1) < .003f;
        test(healthy, "procedural mesh has finite positions and unit normals");
    }
    std::array<V3, 4> path{{{0, 0, 0}, {0, 1, 0}, {.5f, 1.5f, 0}, {1, 1.5f, 1}}};
    auto tube = gfx::makeTube(path, .08f);
    test(!tube.empty() && tube.size() % 3 == 0, "curved service tube produces renderable geometry");
    std::vector<gfx::Vertex> transformed;
    gfx::appendMesh(transformed, tube, rf::compose({2, 3, 4}, {}, {2, .5f, 3}));
    bool normals = true;
    for (auto &v : transformed)
        normals &= std::abs(rf::length(v.n) - 1) < .003f;
    test(normals, "mesh merge transforms normals under nonuniform scale");
    auto settings = VfxSettings::gravel();
    settings.rate = 0;
    settings.origin = {0, 1, 0};
    settings.velocity = {0, -5, 0};
    settings.capacity = 64;
    VfxEmitter gravel(settings);
    gravel.burst(64);
    gravel.update(.5f);
    bool above = true;
    for (auto &p : gravel.active())
        above &= p.position.y >= 0;
    test(above && gravel.collisions() > 0, "gravel particles bounce against collision surface");
    auto dust = VfxSettings::dust();
    dust.rate = 1000;
    dust.capacity = 16;
    VfxEmitter bounded(dust);
    bounded.update(1);
    test(bounded.active().size() == 16, "VFX capacity bounds continuous emission");
    dust.rate = 0;
    dust.capacity = 128;
    dust.seed = 141;
    VfxEmitter a(dust), b(dust);
    a.burst(30);
    b.burst(30);
    a.update(.5f);
    for (int n = 0; n < 30; n++)
        b.update(1.f / 60);
    test(a.active().size() == b.active().size() &&
             rf::distance(a.active()[0].position, b.active()[0].position) < .002f,
         "VFX fixed step is stable across frame subdivisions");
    a.clear();
    test(a.active().empty() && a.collisions() == 0, "VFX reset retires particles and simulation state");
    auto steamSettings = VfxSettings::steam();
    steamSettings.rate = 0;
    steamSettings.spread = 0;
    VfxEmitter steam(steamSettings);
    steam.burst(4);
    steam.update(.5f);
    test(steam.active()[0].position.y > .5f, "steam buoyancy produces a rising plume");
    auto sparksSettings = VfxSettings::sparks();
    sparksSettings.rate = 0;
    sparksSettings.lifetime = .1f;
    sparksSettings.lifetimeVariation = 0;
    VfxEmitter sparks(sparksSettings);
    sparks.burst(8);
    sparks.update(.2f);
    test(sparks.active().empty(), "spark lifetime retires hot particles");
    bool rejected = false;
    dust.capacity = 0;
    try {
        VfxEmitter invalid(dust);
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    test(rejected, "invalid emitter capacity rejected");
    CameraTrack track(
        {{0, {0, 1, -5}, {0, 1, 0}, 55}, {1, {1, 2, -4}, {0, 1, 0}, 60}, {2, {2, 3, -3}, {0, 1, 0}, 50}});
    auto camera = track.sample(.5f, false);
    test(camera.position.x > 0 && camera.position.x < 1 && std::abs(rf::length(camera.direction) - 1) < .001f,
         "cinematic camera interpolates position and look direction");
    test(rf::distance(track.sample(.3f).position, track.sample(2.3f).position) < .001f,
         "cinematic camera loops at track duration");
    rejected = false;
    try {
        CameraTrack invalid({{0, {0, 1, 0}, {0, 0, 0}, 55}, {0, {1, 1, 0}, {0, 0, 0}, 55}});
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    test(rejected, "cinematic track rejects duplicate key times");
}
inline void cinematicGraphicsTests(const std::function<void(bool, const char *)> &test,
                                   rf::gfx::Renderer &r) {
    using namespace rf;
    using namespace rf::gfx;
    auto settings = GraphicsSettings::legacy();
    settings.viewDistance = 80;
    r.setSettings(settings);
    auto box = r.createMesh(makeBox());
    Camera camera{{0, 2, -8}, normalized({0, -.7f, 8}), 60, .08f};
    auto bytes = [&]() {
        auto file =
            std::filesystem::temp_directory_path() / ("jsd-cinematic-" + std::to_string(jsdTestProcessId()) + ".ppm");
        r.screenshot(file);
        std::ifstream input(file, std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(input)), {});
        std::filesystem::remove(file);
        return content;
    };
    auto draw = [&](std::span<const ParticleSprite> sprites) {
        r.begin(camera);
        Draw d;
        d.mesh = box;
        d.transform = {{0, 1, 0}, {2, 2, 2}, 0};
        d.material.albedo = {.3f, .35f, .4f, 1};
        r.submit(d);
        r.particles(sprites);
        r.end();
        r.check();
        r.validateFrame();
        return bytes();
    };
    auto base = draw({});
    auto moved = makeBox();
    for (auto &vertex : moved)
        vertex.p.x += 3;
    r.updateMesh(box, moved);
    auto shifted = draw({});
    r.updateMesh(box, makeBox());
    test(shifted != base && draw({}) == base,
         "streamed mesh update detaches static indices and restores original pixels");
    ParticleSprite hidden;
    hidden.position = {0, 1, 2};
    hidden.color = {20, 5, .2f, 1};
    hidden.size = .15f;
    hidden.style = ParticleStyle::Spark;
    test(draw(std::span(&hidden, 1)) == base, "particle depth testing hides sprites behind opaque geometry");
    std::array<ParticleSprite, 4> sprites;
    for (int i = 0; i < 4; i++) {
        sprites[i].position = {-2.4f + i * 1.6f, 2.3f, -1};
        sprites[i].color = {2, .6f, .1f, .8f};
        sprites[i].size = .35f;
        sprites[i].velocity = {1, 0, 0};
        sprites[i].style = ParticleStyle(i);
        sprites[i].stretch = i == 2 ? .2f : 0;
    }
    auto visible = draw(sprites);
    test(visible != base, "dust steam sparks and sand change actual rendered pixels");
    test(r.statistics().particleSprites == 4 && r.statistics().particleDrawCalls == 1,
         "sprite styles share one GPU instanced draw");
    ParticleSprite edge;
    edge.position = {0, 1, -1.08f};
    edge.color = {2, .5f, .1f, .8f};
    edge.size = .3f;
    edge.style = ParticleStyle::Steam;
    edge.softness = .01f;
    auto hard = draw(std::span(&edge, 1));
    edge.softness = 1;
    auto soft = draw(std::span(&edge, 1));
    test(hard != soft, "soft particles fade against copied scene depth");
    bool rejected = false;
    auto invalid = makeBox();
    invalid[0].n = {};
    try {
        r.createMesh(invalid);
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    test(rejected, "zero mesh normals rejected before HDR shading");
    r.begin(camera);
    Draw invalidMaterial;
    invalidMaterial.mesh = box;
    invalidMaterial.material.textureScale = std::numeric_limits<float>::quiet_NaN();
    rejected = false;
    try {
        r.submit(invalidMaterial);
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    test(rejected, "nonfinite material mapping rejected before submission");
    // Imported/smoothed normals can be parallel to a triangle's UV tangent.
    // The shader must choose a valid fallback basis instead of normalizing zero.
    std::array<Vertex, 3> tangentParallel{{
        {{-1, 0, 0}, {1, 0, 0}, {}, 0, 0},
        {{1, 0, 0}, {1, 0, 0}, {}, 1, 0},
        {{0, 2, 0}, {1, 0, 0}, {}, .5f, 1},
    }};
    auto unusual = r.createMesh(tangentParallel);
    std::array<unsigned char, 4> neutral{128, 128, 255, 255};
    auto neutralMap = r.createTexture(1, 1, neutral, false);
    r.begin(camera);
    Draw normalMapped;
    normalMapped.mesh = unusual;
    normalMapped.material.normalTexture = neutralMap;
    normalMapped.material.doubleSided = true;
    r.submit(normalMapped);
    r.end();
    r.validateFrame();
    test(true, "normal mapping keeps parallel tangent and normal bases finite");
    r.destroyTexture(neutralMap);
    r.destroyMesh(unusual);
    r.begin(camera);
    for (int i = 0; i < 200; i++) {
        Light light;
        light.position = {0, 1, 0};
        light.radius = 50;
        light.intensity = .01f;
        r.light(light);
    }
    Draw illuminated;
    illuminated.mesh = box;
    r.submit(illuminated);
    r.end();
    r.check();
    r.validateFrame();
    test(r.statistics().localLights == 200 && r.statistics().lightTileOverflows > 0,
         "flat light bins retain bounded ranges when more than 128 lights overlap");
    Environment env;
    env.atmosphere.enabled = true;
    env.atmosphere.clouds = Quality::Medium;
    env.atmosphere.cloudCoverage = .6f;
    auto skyCamera = Camera{{0, 2, 0}, normalized({0, .6f, 1}), 65, .08f};
    r.begin(skyCamera, env);
    r.end();
    r.validateFrame();
    auto cloud = bytes();
    env.atmosphere.clouds = Quality::Off;
    r.begin(skyCamera, env);
    r.end();
    r.validateFrame();
    auto clearSky = bytes();
    test(cloud != clearSky, "ray-marched clouds change atmospheric sky pixels");
    std::array<unsigned char, 16> red{255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255};
    auto texture = r.createTexture(2, 2, red, false);
    r.begin(skyCamera, env);
    r.image(texture, 100, 100, 200, 100);
    r.end();
    r.check();
    test(bytes() != clearSky, "full resolution textured UI overlay renders");
    r.destroyTexture(texture);
    r.destroyMesh(box);
}
