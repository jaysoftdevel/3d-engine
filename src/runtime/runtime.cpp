#include "jsd/runtime.hpp"
#include <queue>
#include <set>
namespace jsd {
int SceneGraph::add(std::string name, Mat4 local, int parent) {
    if (parent >= int(nodes.size()) || parent < -1)
        throw std::invalid_argument("invalid scene parent");
    int id = nodes.size();
    nodes.push_back({parent, local, Mat4::identity(), std::move(name)});
    return id;
}
void SceneGraph::reparent(int node, int parent) {
    (void)nodes.at(node);
    if (parent >= int(nodes.size()) || parent < -1)
        throw std::invalid_argument("invalid parent");
    for (int n = parent; n >= 0; n = nodes[n].parent)
        if (n == node)
            throw std::invalid_argument("scene hierarchy cycle");
    nodes[node].parent = parent;
}
void SceneGraph::setLocal(int node, const Mat4 &m) {
    nodes.at(node).local = m;
}
void SceneGraph::update() {
    std::vector<int> state(nodes.size());
    std::function<void(int)> visit = [&](int n) {
        if (state[n] == 2)
            return;
        if (state[n] == 1)
            throw std::logic_error("scene cycle");
        state[n] = 1;
        auto &p = nodes[n];
        if (p.parent >= 0) {
            visit(p.parent);
            p.world = nodes[p.parent].world * p.local;
        } else
            p.world = p.local;
        state[n] = 2;
    };
    for (int n = 0; n < int(nodes.size()); n++)
        visit(n);
}
void FrameGraph::add(std::string name, std::vector<std::string> reads, std::vector<std::string> writes,
                     std::function<void()> fn, std::vector<std::string> after) {
    for (auto &p : passes)
        if (p.name == name)
            throw std::invalid_argument("duplicate pass");
    passes.push_back({std::move(name), std::move(reads), std::move(writes), std::move(after), std::move(fn)});
    order.clear();
}
void FrameGraph::compile() {
    size_t n = passes.size();
    std::vector<std::set<size_t>> edges(n);
    std::map<std::string, size_t> writers;
    std::map<std::string, std::set<size_t>> readers;
    for (size_t i = 0; i < n; i++) {
        for (auto &r : passes[i].reads) {
            if (writers.contains(r) && writers[r] != i)
                edges[writers[r]].insert(i);
            readers[r].insert(i);
        }
        for (auto &w : passes[i].writes) {
            if (writers.contains(w) && writers[w] != i)
                edges[writers[w]].insert(i);
            for (auto j : readers[w])
                if (j != i)
                    edges[j].insert(i);
            readers[w].clear();
            writers[w] = i;
        }
        for (auto &dependency : passes[i].after) {
            auto it =
                std::find_if(passes.begin(), passes.end(), [&](auto &p) { return p.name == dependency; });
            if (it == passes.end())
                throw std::invalid_argument("unknown pass dependency");
            edges[it - passes.begin()].insert(i);
        }
    }
    std::vector<unsigned> degree(n);
    for (auto &e : edges)
        for (auto v : e)
            degree[v]++;
    std::queue<size_t> ready;
    for (size_t i = 0; i < n; i++)
        if (!degree[i])
            ready.push(i);
    std::vector<size_t> compiled;
    while (!ready.empty()) {
        auto i = ready.front();
        ready.pop();
        compiled.push_back(i);
        for (auto j : edges[i])
            if (!--degree[j])
                ready.push(j);
    }
    if (compiled.size() != n)
        throw std::invalid_argument("frame graph cycle");
    order = std::move(compiled);
}
void FrameGraph::execute() {
    if (order.size() != passes.size())
        compile();
    timings.clear();
    for (auto i : order) {
        auto start = std::chrono::steady_clock::now();
        passes[i].run();
        timings.push_back(
            {passes[i].name,
             std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count()});
    }
}
void Input::beginFrame() {
    pushed.fill(false);
    released.fill(false);
    mouseDX = mouseDY = 0;
}
void Input::event(const SDL_Event &e) {
    if (e.type == vl::sdl::keyDown || e.type == vl::sdl::keyUp) {
        int k = e.key.keysym.scancode;
        if (k < 0 || k >= 512)
            return;
        bool value = e.type == vl::sdl::keyDown;
        if (value && !held[k])
            pushed[k] = true;
        if (!value && held[k])
            released[k] = true;
        held[k] = value;
    }
    if (e.type == vl::sdl::mouseMotion) {
        mouseDX += e.motion.xrel;
        mouseDY += e.motion.yrel;
    }
    if (e.type == vl::sdl::window && e.window.event == vl::sdl::focusLost) {
        held.fill(false);
        pushed.fill(false);
    }
}
void Input::bind(std::string action, int key, float scale) {
    if (key < 0 || key >= 512 || !std::isfinite(scale))
        throw std::invalid_argument("invalid binding");
    actions[std::move(action)].push_back({key, scale});
}
float Input::axis(const std::string &action) const {
    auto it = actions.find(action);
    float value = 0;
    if (it != actions.end())
        for (auto [k, s] : it->second)
            if (held[k])
                value += s;
    return rf::clamp(value, -1, 1);
}
gfx::MeshId LodSelector::select(uint64_t id, float distance, std::span<const LodLevel> levels,
                                float hysteresis) {
    if (levels.empty() || !std::isfinite(distance))
        throw std::invalid_argument("invalid LOD group");
    for (size_t i = 1; i < levels.size(); i++)
        if (levels[i].maximumDistance <= levels[i - 1].maximumDistance)
            throw std::invalid_argument("unordered LOD ranges");
    size_t selected = levels.size() - 1;
    for (size_t i = 0; i < levels.size(); i++)
        if (distance < levels[i].maximumDistance) {
            selected = i;
            break;
        }
    auto it = previous.find(id);
    if (it != previous.end() && it->second < levels.size()) {
        size_t old = it->second;
        if (selected > old && distance < levels[old].maximumDistance * (1 + hysteresis))
            selected = old;
        if (selected < old && distance > levels[selected].maximumDistance * (1 - hysteresis))
            selected = old;
    }
    previous[id] = selected;
    return levels[selected].mesh;
}
void ParticleEmitter::update(float dt) {
    if (!std::isfinite(dt) || dt < 0 || dt > 1)
        throw std::invalid_argument("invalid particle delta");
    for (auto &p : particles) {
        p.age += dt;
        p.velocity += settings.gravity * dt;
        p.position += p.velocity * dt;
    }
    std::erase_if(particles, [](auto &p) { return p.age >= p.lifetime; });
    carry += dt * std::max(0.f, settings.rate);
    int count = std::min(int(carry), int(settings.capacity - particles.size()));
    carry -= std::floor(carry);
    for (int n = 0; n < count; n++) {
        auto h = rf::hash(settings.seed + sequence++);
        V3 random{rf::random01(h) * 2 - 1, rf::random01(h + 1) * 2 - 1, rf::random01(h + 2) * 2 - 1};
        particles.push_back({settings.origin, settings.velocity + random * settings.spread, settings.color, 0,
                             std::max(.01f, settings.lifetime), settings.size,
                             uint64_t(settings.seed) << 32 | sequence});
    }
}
void ParticleEmitter::draw(gfx::Renderer &r) const {
    for (auto &p : particles) {
        gfx::Material m;
        m.albedo = {p.color.r, p.color.g, p.color.b, p.color.a * (1 - p.age / p.lifetime)};
        m.emissive = {p.color.r * 5, p.color.g * 5, p.color.b * 5, 1};
        m.alphaMode = gfx::AlphaMode::Additive;
        m.castShadow = false;
        r.primitive(gfx::Primitive::Rock, {p.position, {p.size, p.size, p.size}, 0}, m);
    }
}
bool FileWatcher::changed(const std::filesystem::path &p) {
    std::error_code error;
    auto time = std::filesystem::last_write_time(p, error);
    if (error)
        return false;
    auto it = times.find(p);
    if (it == times.end()) {
        times[p] = time;
        return false;
    }
    if (it->second != time) {
        it->second = time;
        return true;
    }
    return false;
}
} // namespace jsd
extern "C" const char *jsd_game_engine_version() {
    return "0.5.16";
}
