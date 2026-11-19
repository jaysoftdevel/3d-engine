#pragma once
#include "ecs.hpp"
#include <chrono>
#include <filesystem>
#include <frontier/frontier.hpp>
#include <map>
#include <unordered_map>
namespace jsd {
using rf::Color;
using rf::Mat4;
using rf::Quat;
using rf::V3;
namespace gfx = rf::gfx;
class FixedStep {
    double accumulator = 0, step;
    unsigned maximum;

  public:
    explicit FixedStep(double h = 1. / 60, unsigned max = 8) : step(h), maximum(max) {
        if (h <= 0 || !std::isfinite(h) || !max)
            throw std::invalid_argument("invalid fixed step");
    }
    template <class F> unsigned advance(double delta, F callback) {
        if (!std::isfinite(delta) || delta < 0)
            throw std::invalid_argument("invalid frame delta");
        accumulator += std::min(delta, step * maximum);
        unsigned n = 0;
        while (accumulator >= step && n < maximum) {
            callback(float(step));
            accumulator -= step;
            n++;
        }
        return n;
    }
    float alpha() const { return accumulator / step; }
};
struct SceneNode {
    int parent = -1;
    Mat4 local = Mat4::identity(), world = Mat4::identity();
    std::string name;
};
class SceneGraph {
    std::vector<SceneNode> nodes;

  public:
    int add(std::string name, Mat4 local = Mat4::identity(), int parent = -1);
    void reparent(int node, int parent);
    void setLocal(int node, const Mat4 &);
    void update();
    const SceneNode &get(int node) const { return nodes.at(node); }
    size_t size() const { return nodes.size(); }
};
struct PassTiming {
    std::string name;
    double milliseconds = 0;
};
class FrameGraph {
    struct Pass {
        std::string name;
        std::vector<std::string> reads, writes, after;
        std::function<void()> run;
    };
    std::vector<Pass> passes;
    std::vector<size_t> order;
    std::vector<PassTiming> timings;

  public:
    void add(std::string name, std::vector<std::string> reads, std::vector<std::string> writes,
             std::function<void()> run, std::vector<std::string> after = {});
    void compile();
    void execute();
    const std::vector<PassTiming> &profile() const { return timings; }
};
class Input {
    std::array<bool, 512> held{}, pushed{}, released{};
    std::map<std::string, std::vector<std::pair<int, float>>> actions;
    float mouseDX = 0, mouseDY = 0;

  public:
    void beginFrame();
    void event(const SDL_Event &);
    void bind(std::string action, int scancode, float scale = 1);
    bool down(int key) const { return key >= 0 && key < 512 && held[key]; }
    bool pressed(int key) const { return key >= 0 && key < 512 && pushed[key]; }
    float axis(const std::string &action) const;
    std::pair<float, float> mouseDelta() const { return {mouseDX, mouseDY}; }
};
struct LodLevel {
    gfx::MeshId mesh;
    float maximumDistance = 100;
};
class LodSelector {
    std::unordered_map<uint64_t, size_t> previous;

  public:
    gfx::MeshId select(uint64_t object, float distance, std::span<const LodLevel>, float hysteresis = .1f);
};
struct Particle {
    V3 position, velocity;
    Color color{1, .5f, .1f, 1};
    float age = 0, lifetime = 1, size = .06f;
    uint64_t id = 0;
};
struct ParticleSettings {
    V3 origin, velocity{0, 2, 0}, gravity{0, -1, 0};
    Color color{1, .35f, .06f, 1};
    float rate = 40, lifetime = 2, spread = 1, size = .06f;
    unsigned capacity = 4096, seed = 101;
};
class ParticleEmitter {
    ParticleSettings settings;
    std::vector<Particle> particles;
    float carry = 0;
    uint32_t sequence = 0;

  public:
    explicit ParticleEmitter(ParticleSettings s = {}) : settings(s) {}
    void update(float dt);
    void draw(gfx::Renderer &) const;
    const std::vector<Particle> &active() const { return particles; }
};
class FileWatcher {
    std::map<std::filesystem::path, std::filesystem::file_time_type> times;

  public:
    bool changed(const std::filesystem::path &);
};
} // namespace jsd
