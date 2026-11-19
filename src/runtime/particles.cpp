#include "jsd/particles.hpp"
#include <atomic>
#include <stdexcept>
namespace jsd {
VfxSettings VfxSettings::dust() {
    return {};
}
VfxSettings VfxSettings::steam() {
    VfxSettings s;
    s.preset = VfxPreset::Steam;
    s.start = {.7f, .79f, .82f, .2f};
    s.end = {.7f, .76f, .8f, 0};
    s.velocity = {0, 1.7f, 0};
    s.gravity = {0, .28f, 0};
    s.wind = {.28f, 0, .1f};
    s.rate = 28;
    s.lifetime = 3;
    s.size = .16f;
    s.sizeGrowth = 2;
    s.drag = .5f;
    s.turbulence = .65f;
    s.softness = .6f;
    return s;
}
VfxSettings VfxSettings::sparks() {
    VfxSettings s;
    s.preset = VfxPreset::Sparks;
    s.start = {35, 12, 1, .95f};
    s.end = {5, .12f, .01f, 0};
    s.velocity = {2, 3, 0};
    s.gravity = {0, -9.81f, 0};
    s.wind = {};
    s.rate = 70;
    s.lifetime = .9f;
    s.size = .018f;
    s.sizeGrowth = -.005f;
    s.drag = .15f;
    s.spread = 2.3f;
    s.collision = true;
    s.restitution = .38f;
    s.turbulence = 0;
    s.stretch = .11f;
    s.softness = .06f;
    return s;
}
VfxSettings VfxSettings::sand() {
    VfxSettings s;
    s.preset = VfxPreset::Sand;
    s.start = {.49f, .3f, .13f, .65f};
    s.end = {.52f, .38f, .21f, 0};
    s.velocity = {1, 0, 0};
    s.gravity = {0, -4.5f, 0};
    s.wind = {2, 0, .3f};
    s.rate = 160;
    s.lifetime = 1.8f;
    s.size = .016f;
    s.sizeGrowth = 0;
    s.drag = .3f;
    s.spread = .8f;
    s.collision = true;
    s.restitution = .18f;
    s.softness = .025f;
    return s;
}
VfxSettings VfxSettings::gravel() {
    auto s = sand();
    s.preset = VfxPreset::Gravel;
    s.start = {.31f, .26f, .19f, 1};
    s.end = s.start;
    s.gravity = {0, -9.81f, 0};
    s.wind = {};
    s.rate = 25;
    s.lifetime = 5;
    s.size = .055f;
    s.drag = .05f;
    s.restitution = .35f;
    s.friction = .7f;
    s.spread = .5f;
    return s;
}
VfxEmitter::VfxEmitter(VfxSettings s) : config(s) {
    static std::atomic<uint64_t> nextId{1};
    instanceId = nextId.fetch_add(1);
    for (float f : {s.rate, s.lifetime, s.lifetimeVariation, s.size, s.sizeGrowth, s.spread, s.spawnRadius,
                    s.drag, s.turbulence, s.restitution, s.friction, s.floor, s.softness, s.stretch})
        if (!std::isfinite(f))
            throw std::invalid_argument("nonfinite emitter setting");
    if (s.rate < 0 || s.rate > 100000 || s.lifetime <= 0 || s.lifetime > 120 || s.size <= 0 ||
        s.capacity == 0 || s.capacity > 65536 || s.drag < 0 || s.spawnRadius < 0 || s.spread < 0 ||
        s.restitution < 0 || s.restitution > 1 || s.friction < 0 || s.friction > 1)
        throw std::invalid_argument("invalid emitter setting");
    setOrigin(s.origin);
    setVelocity(s.velocity);
    particles.reserve(s.capacity);
}
void VfxEmitter::setOrigin(V3 p) {
    if (!std::isfinite(p.x + p.y + p.z))
        throw std::invalid_argument("invalid emitter origin");
    config.origin = p;
}
void VfxEmitter::setVelocity(V3 p) {
    if (!std::isfinite(p.x + p.y + p.z))
        throw std::invalid_argument("invalid emitter velocity");
    config.velocity = p;
}
void VfxEmitter::spawn() {
    if (particles.size() >= config.capacity)
        return;
    auto random = [&](unsigned n) {
        return rf::random01(rf::hash(config.seed + sequence * 7919 + n * 1777));
    };
    V3 jitter{random(1) * 2 - 1, random(2) * 2 - 1, random(3) * 2 - 1};
    V3 offset{random(4) * 2 - 1, random(5) * 2 - 1, random(6) * 2 - 1};
    particles.push_back(
        {config.origin + offset * config.spawnRadius, config.velocity + jitter * config.spread, 0,
         config.lifetime * (1 + (random(7) * 2 - 1) * std::clamp(config.lifetimeVariation, 0.f, .9f)),
         config.size * (.6f + random(8) * .8f), random(9) * 2 * rf::pi, (random(10) * 2 - 1) * 3,
         sequence++});
}
unsigned VfxEmitter::burst(unsigned count) {
    auto before = particles.size();
    for (unsigned i = 0; i < std::min(count, config.capacity - unsigned(before)); i++)
        spawn();
    return particles.size() - before;
}
void VfxEmitter::tick(float dt) {
    for (auto &p : particles) {
        p.age += dt;
        float t = p.age;
        V3 curl{std::sin(t * 2.1f + p.id * .71f), std::sin(t * 1.7f + p.id * 1.17f) * .4f,
                std::cos(t * 2.3f + p.id * .57f)};
        p.velocity +=
            (config.gravity + curl * config.turbulence + (config.wind - p.velocity) * config.drag) * dt;
        p.position += p.velocity * dt;
        p.angle += p.spin * dt;
        if (config.collision) {
            float ground = height ? height(p.position.x, p.position.z) : config.floor;
            if (!std::isfinite(ground))
                throw std::runtime_error("nonfinite particle collision surface");
            if (p.position.y < ground + p.size * .4f) {
                p.position.y = ground + p.size * .4f;
                if (p.velocity.y < -.1f) {
                    p.velocity.y = -p.velocity.y * config.restitution;
                    collisionCount++;
                } else
                    p.velocity.y = 0;
                p.velocity.x *= 1 - config.friction;
                p.velocity.z *= 1 - config.friction;
            }
        }
    }
    std::erase_if(particles, [](auto &p) { return p.age >= p.lifetime; });
    if (enabled) {
        emission += config.rate * dt;
        auto count = unsigned(emission);
        emission -= count;
        burst(count);
    }
}
void VfxEmitter::update(float dt) {
    if (!std::isfinite(dt) || dt < 0 || dt > 1)
        throw std::invalid_argument("invalid emitter delta");
    accumulator += dt;
    constexpr double step = 1. / 120;
    while (accumulator + 1e-9 >= step) {
        tick(float(step));
        accumulator -= step;
    }
}
void VfxEmitter::clear() {
    particles.clear();
    accumulator = emission = 0;
    collisionCount = 0;
    sequence = 0;
}
void VfxEmitter::draw(gfx::Renderer &r, gfx::MeshId gravel) const {
    std::vector<gfx::ParticleSprite> sprites;
    sprites.reserve(particles.size());
    for (auto &p : particles) {
        float t = rf::clamp(p.age / p.lifetime);
        Color color{rf::mix(config.start.r, config.end.r, t), rf::mix(config.start.g, config.end.g, t),
                    rf::mix(config.start.b, config.end.b, t), rf::mix(config.start.a, config.end.a, t)};
        float radius = std::max(.002f, p.size + config.sizeGrowth * p.age);
        if (config.preset == VfxPreset::Gravel) {
            gfx::Material m;
            m.albedo = color;
            m.roughness = .9f;
            float variation = .75f + rf::random01(rf::hash(p.id)) * .5f;
            m.albedo.r *= variation;
            m.albedo.g *= variation;
            m.albedo.b *= variation;
            gfx::Transform transform{p.position,
                                     {radius * 2, radius * 1.5f, radius * 1.7f},
                                     p.angle,
                                     p.angle * .7f,
                                     p.angle * .3f};
            if (gravel.value) {
                gfx::Draw d;
                d.mesh = gravel;
                d.transform = transform;
                d.material = m;
                d.objectId = 0x7000000000000000ull + (instanceId << 32) + p.id;
                r.submit(d);
            } else
                r.primitive(gfx::Primitive::Rock, transform, m);
            continue;
        }
        color.a *= rf::clamp(p.age * 12);
        gfx::ParticleStyle style = config.preset == VfxPreset::Steam    ? gfx::ParticleStyle::Steam
                                   : config.preset == VfxPreset::Sparks ? gfx::ParticleStyle::Spark
                                   : config.preset == VfxPreset::Sand   ? gfx::ParticleStyle::Sand
                                                                        : gfx::ParticleStyle::Dust;
        sprites.push_back(
            {p.position, p.velocity, color, radius, p.angle, config.stretch, config.softness, p.age, style});
    }
    r.particles(sprites);
}
} // namespace jsd
