#pragma once
#include "runtime.hpp"
namespace jsd {
enum class VfxPreset { Dust, Steam, Sparks, Sand, Gravel };
struct VfxSettings {
    VfxPreset preset = VfxPreset::Dust;
    V3 origin{}, velocity{0, .4f, 0}, gravity{0, -.2f, 0}, wind{.5f, 0, .1f};
    Color start{.45f, .32f, .2f, .19f}, end{.38f, .31f, .24f, 0};
    float rate = 30, lifetime = 3, lifetimeVariation = .35f, size = .3f, sizeGrowth = 1;
    float spread = .6f, spawnRadius = .15f, drag = .7f, turbulence = .5f;
    float restitution = .3f, friction = .45f, floor = 0, softness = .4f, stretch = 0;
    unsigned capacity = 4096, seed = 101;
    bool collision = false;
    static VfxSettings dust();
    static VfxSettings steam();
    static VfxSettings sparks();
    static VfxSettings sand();
    static VfxSettings gravel();
};
struct VfxParticle {
    V3 position, velocity;
    float age = 0, lifetime = 1, size = .1f, angle = 0, spin = 0;
    unsigned id = 0;
};
class VfxEmitter {
    VfxSettings config;
    std::vector<VfxParticle> particles;
    unsigned sequence = 0, collisionCount = 0;
    uint64_t instanceId = 0;
    double accumulator = 0, emission = 0;
    bool enabled = true;
    std::function<float(float, float)> height;
    void tick(float);
    void spawn();

  public:
    explicit VfxEmitter(VfxSettings);
    VfxEmitter(const VfxEmitter &) = delete;
    VfxEmitter &operator=(const VfxEmitter &) = delete;
    VfxEmitter(VfxEmitter &&) noexcept = default;
    VfxEmitter &operator=(VfxEmitter &&) noexcept = default;
    void update(float seconds);
    unsigned burst(unsigned count);
    void setOrigin(V3);
    void setVelocity(V3);
    void setEmitting(bool value) { enabled = value; }
    // Set a CPU height callback for terrain contact. Jolt owns rigid-body simulation separately.
    void setSurface(std::function<float(float, float)> callback) { height = std::move(callback); }
    void clear();
    void draw(gfx::Renderer &, gfx::MeshId gravelMesh = {}) const;
    const std::vector<VfxParticle> &active() const { return particles; }
    const VfxSettings &settings() const { return config; }
    unsigned collisions() const { return collisionCount; }
};
} // namespace jsd
