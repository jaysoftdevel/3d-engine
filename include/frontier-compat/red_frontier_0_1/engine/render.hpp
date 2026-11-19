#pragma once
// Source-compatibility adapter. World remains owned and defined by the consumer.
#include "game/game.hpp"
#include <frontier/frontier.hpp>
#include <numeric>
#include <optional>
namespace rf {
struct Vertex {
    V3 p, n;
    Color c;
};
struct Instance {
    V3 pos, scale;
    Color color;
    float yaw = 0;
};
struct Mesh {
    unsigned vao = 0, vbo = 0;
    int count = 0;
};
struct UiVertex {
    float x, y;
    Color c;
};
class Renderer {
    gfx::Renderer core;

  public:
    int draws = 0, triangles = 0, visibleTiles = 0;
    bool wireframe = false;
    explicit Renderer(Platform &p) : core(p, gfx::GraphicsSettings::legacy()) {}
    gfx::Renderer &graphics() { return core; }
    const gfx::Renderer &graphics() const { return core; }
    gfx::SettingsReport setGraphicsSettings(gfx::GraphicsSettings s) {
        wireframe = s.wireframe;
        return core.setSettings(s);
    }
    void box(V3 p, V3 size, Color c, float yaw = 0) { shape(0, p, size, c, yaw); }
    void shape(int type, V3 p, V3 size, Color c, float yaw = 0) {
        gfx::Material m;
        m.albedo = c;
        core.primitive(gfx::Primitive(type), {p, size, yaw}, m);
    }
    void rect(float x, float y, float w, float h, Color c) { core.rect(x, y, w, h, c); }
    void line(float a, float b, float c, float d, Color color, float thickness = 1) {
        core.line(a, b, c, d, color, thickness);
    }
    void text(float x, float y, std::string_view text, float size = 2, Color color = {.86f, .91f, .93f, 1}) {
        core.text(x, y, text, size, color);
    }
    void finish() {
        core.end();
        auto &s = core.statistics();
        draws = s.drawCalls;
        triangles = s.triangles;
        visibleTiles = s.visibleTerrainTiles;
    }
    void screenshot(const std::string &path) { core.screenshot(path); }
    void check() { core.check(); }
    std::string driver() const { return core.capabilities().renderer + " / " + core.capabilities().version; }
    unsigned workers() const { return core.statistics().terrainWorkers; }
    void world(const World &w, std::optional<std::pair<int, V3>> ghost = {}, float ghostYaw = 0,
               bool validGhost = false, bool overview = false) {
        auto settings = core.settings().requested;
        settings.wireframe = wireframe;
        core.setSettings(settings);
        V3 eye = w.player.pos + V3{0, w.player.driving >= 0 ? 2.5f : 1.65f, 0},
           dir = forward(w.player.yaw, w.player.pitch);
        if (overview) {
            eye = {25, 13, -30};
            dir = normalized(V3{2, 1, 0} - eye);
        }
        gfx::Environment environment;
        float daylight = w.dayLight(), storm = w.storm();
        environment.sunIntensity = .35f + daylight * 2;
        environment.storm = storm;
        environment.skyColor = {.25f + daylight * .18f, .19f + daylight * .1f, .16f + daylight * .06f, 1};
        environment.groundColor = {.15f, .1f, .07f, 1};
        core.begin({eye, dir, 75, .08f}, environment);
        gfx::Terrain terrain;
        terrain.seed = w.seed;
        terrain.material.albedo = {.42f, .23f, .15f, 1};
        core.terrain(terrain);
        for (auto &n : w.nodes)
            if (n.remaining > 0 && distance(n.pos, eye) < 210) {
                shape(1, n.pos + V3{0, n.size * .55f, 0}, {n.size * 1.8f, n.size * 1.6f, n.size * 1.6f},
                      itemColor(n.item), random01(n.id) * pi);
                if (n.scanned)
                    box(n.pos + V3{0, n.size * 1.55f, 0}, {.14f, .14f, .14f}, {.2f, .9f, .75f, 1});
            }
        for (auto &s : w.sites)
            if (distance(s.pos, eye) < 220) {
                box(s.pos + V3{0, .35f, 0}, {4, .7f, 2.5f}, {.24f, .29f, .31f, 1}, .3f);
                box(s.pos + V3{1, 1, 0}, {1, 1.4f, 1}, {.8f, .43f, .16f, 1}, .6f);
                if (!s.looted)
                    box(s.pos + V3{0, 2.5f, 0}, {.2f, 2, .2f}, {.2f, .8f, .8f, 1});
            }
        auto structure = [&](const Build &b, Color overrideColor, bool override) {
            Color hull{.63f, .67f, .66f, 1}, dark{.12f, .18f, .22f, 1}, orange{.85f, .38f, .12f, 1},
                blue{.10f, .32f, .48f, 1},
                green = b.powered ? Color{.24f, .9f, .64f, 1} : Color{.9f, .21f, .15f, 1};
            auto part = [&](V3 p, V3 size, Color c, int shapeId = 0, float yaw = 0) {
                shape(shapeId, b.pos + rotate(p, b.yaw), size, override ? overrideColor : c, b.yaw + yaw);
            };
            if (b.progress < 1 && !override) {
                part({0, .08f, 0}, {def(b.kind).half.x * 2, .15f, def(b.kind).half.z * 2},
                     {.1f, .6f, .65f, 1});
                part({0, .5f + b.progress, 0}, {.12f, 1 + b.progress * 2, .12f}, {.3f, .85f, .9f, 1});
                return;
            }
            switch (b.kind) {
            case Habitat:
                part({0, 0, 0}, {8.4f, .3f, 8.4f}, dark);
                part({0, 3.1f, 0}, {8.4f, .2f, 8.4f}, hull);
                for (float x : {-4.f, 4.f}) {
                    part({x, .65f, 0}, {.22f, 1.3f, 8}, hull);
                    part({x, 2.7f, 0}, {.22f, .7f, 8}, hull);
                    for (float z : {-4.f, -2.f, 0.f, 2.f, 4.f})
                        part({x, 1.9f, z}, {.25f, 1.2f, .15f}, orange);
                    for (float z : {-3.f, -1.f, 1.f, 3.f})
                        part({x, 1.9f, z}, {.12f, 1.1f, 1.8f}, blue);
                }
                part({0, 1.5f, 4}, {8, 3, .22f}, hull);
                for (float x : {-2.6f, 2.6f})
                    part({x, 1.5f, -4}, {2.8f, 3, .22f}, hull);
                part({0, 2.95f, -4}, {2.4f, .3f, .25f}, orange);
                part({0, .02f, -5.6f}, {2.6f, .3f, 3.4f}, dark);
                part({0, 3, -5.6f}, {2.6f, .22f, 3.4f}, orange);
                for (float x : {-1.2f, 1.2f})
                    part({x, 1.5f, -5.6f}, {.18f, 3, 3.4f}, hull);
                part({b.outerOpen ? 2.3f : 0, 1.4f, -7.2f}, {2.3f, 2.8f, .16f}, orange);
                part({b.innerOpen ? 2.3f : 0, 1.4f, -4}, {2.3f, 2.8f, .16f}, hull);
                part({-.99f, 1.4f, -5.6f}, {.2f, .65f, .5f}, b.cycle > 0 ? orange : Color{.2f, .8f, .7f, 1});
                for (float z : {-7.1f, -4.1f})
                    part({.95f, 1.35f, z}, {.2f, .45f, .12f}, dark);
                for (float x : {-3.7f, 3.7f})
                    part({x, 3.3f, 0}, {.12f, .4f, 8}, orange);
                break;
            case Solar:
                part({0, .6f, 0}, {.25f, 1.2f, .25f}, hull);
                part({0, 1.1f, 0}, {4, .12f, 2.6f}, dark);
                for (int x = -2; x < 2; x++)
                    for (int z = -1; z < 1; z++)
                        part({x + .5f, 1.19f, z * .95f + .45f}, {.92f, .06f, .86f},
                             {.12f + .15f * b.dust, .29f, .38f, 1});
                break;
            case Battery:
                part({0, 1, 0}, {1.5f, 2, 1.3f}, dark);
                part({0, 1.2f, -.68f}, {1.2f, .14f, .06f}, green);
                part({0, .7f, -.68f}, {1.1f, .08f, .07f}, orange);
                break;
            case Oxygenator:
                part({0, .4f, 0}, {1.2f, .8f, 1}, dark);
                for (float x : {-.32f, .32f})
                    part({x, 1.15f, 0}, {.5f, 1.3f, .5f}, {.35f, .72f, .77f, 1}, 2);
                part({0, .9f, -.55f}, {.3f, .2f, .08f}, green);
                break;
            case Smelter:
                part({0, .5f, 0}, {2, 1, 2}, dark);
                part({0, 1.25f, 0}, {1.5f, 1.2f, 1.5f}, hull);
                part({0, 1.3f, -.8f}, {1, .65f, .08f}, b.queued > 0 && b.powered ? orange : dark);
                part({.65f, 2.2f, .5f}, {.35f, .9f, .35f}, dark);
                break;
            case Fabricator:
                part({0, .7f, 0}, {2, 1.4f, 1.3f}, hull);
                part({0, 1.35f, -.25f}, {1.6f, .15f, .65f}, blue);
                part({0, .8f, -.7f}, {1.2f, .6f, .05f}, dark);
                part({.65f, 1.5f, 0}, {.15f, .3f, .15f}, green);
                break;
            case Planter:
                part({0, .35f, 0}, {2, .7f, 1.6f}, hull);
                part({0, .75f, 0}, {1.8f, .12f, 1.4f}, {.22f, .13f, .08f, 1});
                if (b.crop)
                    for (float x : {-.5f, .5f})
                        for (float z : {-.35f, .35f}) {
                            part({x, 1 + b.growth * .35f, z}, {.08f, .5f + b.growth * .7f, .08f},
                                 {.22f, .55f, .25f, 1});
                            part({x, 1.1f + b.growth * .5f, z}, {.4f + b.growth * .3f, .12f, .4f},
                                 {.3f, .64f, .26f, 1}, 1);
                        }
                break;
            case Research:
                part({0, .7f, 0}, {1.6f, .15f, 1.2f}, hull);
                for (float x : {-.6f, .6f})
                    part({x, .35f, 0}, {.12f, .7f, .8f}, dark);
                part({0, 1.15f, .15f}, {1, .7f, .1f}, blue);
                break;
            case Survey:
                part({0, .2f, 0}, {1.4f, .4f, 1.4f}, dark);
                part({0, 1.7f, 0}, {.16f, 3, .16f}, hull);
                part({0, 3.15f, 0}, {1, .15f, .4f}, orange);
                part({0, 3.5f, 0}, {.14f, .14f, .14f}, green);
                break;
            case Drill:
                part({0, .3f, 0}, {2.4f, .6f, 2.4f}, dark);
                part({0, 1.3f, 0}, {1.2f, 2, 1.2f}, orange);
                part({0, 2.5f, 0}, {.4f, .7f, .4f}, hull);
                break;
            case Rover:
                part({0, .8f, 0}, {2.2f, .6f, 3.8f}, orange);
                part({0, 1.5f, -.3f}, {1.8f, 1.1f, 1.8f}, hull);
                part({0, 1.6f, .65f}, {1.55f, .65f, .07f}, blue);
                for (float x : {-1.15f, 1.15f})
                    for (float z : {-1.3f, 1.3f})
                        part({x, .5f, z}, {.45f, .9f, .9f}, dark);
                part({0, 1.4f, 1.3f}, {1.6f, .3f, .7f}, dark);
                break;
            case Rocket:
                part({0, .2f, 0}, {4, .4f, 4}, dark);
                part({0, 3.3f, 0}, {2.6f, 6.2f, 2.6f}, hull, 2);
                part({0, 7, 0}, {2.5f, 2.4f, 2.5f}, orange, 1);
                for (float x : {-1.5f, 1.5f})
                    part({x, 1.2f, 0}, {.6f, 2.4f, 1.5f}, orange);
                break;
            }
            if (b.kind != Habitat && b.kind != Solar) {
                part({.2f, .2f, -def(b.kind).half.z - .025f}, {.2f, .15f, .08f}, green);
                if (std::accumulate(b.output.begin(), b.output.end(), 0) > 0)
                    part({0, def(b.kind).half.y * 2 + .25f, 0}, {.2f, .2f, .2f}, {.3f, .95f, .8f, 1});
            }
        };
        for (auto &b : w.builds)
            if (distance(b.pos, eye) < 230)
                structure(b, {}, false);
        for (auto &l : w.links) {
            auto *a = w.building(l.a);
            auto *b = w.building(l.b);
            if (!a || !b)
                continue;
            V3 diff = b->pos - a->pos;
            float len = flatDistance(a->pos, b->pos);
            if (len > .01f) {
                V3 p = (a->pos + b->pos) * .5f;
                p.y = std::max(a->pos.y, b->pos.y) + .25f;
                box(p, {.045f, .045f, len}, {.78f, .41f, .16f, 1}, std::atan2(diff.x, diff.z));
            }
        }
        if (ghost) {
            Build b;
            b.kind = ghost->first;
            b.pos = ghost->second;
            b.yaw = ghostYaw;
            b.progress = 1;
            structure(b, validGhost ? Color{.15f, .9f, .65f, .45f} : Color{.95f, .2f, .16f, .4f}, true);
        }
    }
};
} // namespace rf
