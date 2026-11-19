#pragma once
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>
#include <vector>
namespace jsd {
struct Entity {
    uint32_t index = ~0u, generation = 0;
    bool operator==(const Entity &) const = default;
    uint64_t value() const { return uint64_t(generation) << 32 | index; }
};
class Registry {
    struct Base {
        virtual ~Base() = default;
        virtual void remove(uint32_t) = 0;
    };
    template <class T> struct Pool : Base {
        std::unordered_map<uint32_t, T> data;
        void remove(uint32_t i) override { data.erase(i); }
    };
    std::unordered_map<std::type_index, std::unique_ptr<Base>> pools;
    std::vector<uint32_t> generations, free;
    std::vector<bool> living;
    template <class T> Pool<T> &pool() {
        auto key = std::type_index(typeid(T));
        auto it = pools.find(key);
        if (it == pools.end())
            it = pools.emplace(key, std::make_unique<Pool<T>>()).first;
        return static_cast<Pool<T> &>(*it->second);
    }

  public:
    Entity create() {
        uint32_t i;
        if (free.empty()) {
            i = generations.size();
            generations.push_back(0);
            living.push_back(true);
        } else {
            i = free.back();
            free.pop_back();
            living[i] = true;
        }
        return {i, generations[i]};
    }
    bool valid(Entity e) const {
        return e.index < generations.size() && living[e.index] && generations[e.index] == e.generation;
    }
    void destroy(Entity e) {
        if (!valid(e))
            return;
        for (auto &[type, p] : pools) {
            (void)type;
            p->remove(e.index);
        }
        living[e.index] = false;
        generations[e.index]++;
        free.push_back(e.index);
    }
    template <class T, class... A> T &emplace(Entity e, A &&...args) {
        if (!valid(e))
            throw std::invalid_argument("stale entity");
        auto &data = pool<T>().data;
        auto [it, ok] = data.try_emplace(e.index, std::forward<A>(args)...);
        if (!ok)
            throw std::invalid_argument("component already exists");
        return it->second;
    }
    template <class T> T *get(Entity e) {
        if (!valid(e))
            return nullptr;
        auto it = pools.find(typeid(T));
        if (it == pools.end())
            return nullptr;
        auto &data = static_cast<Pool<T> &>(*it->second).data;
        auto value = data.find(e.index);
        return value == data.end() ? nullptr : &value->second;
    }
    template <class T> void remove(Entity e) {
        if (valid(e))
            pool<T>().remove(e.index);
    }
    template <class... T, class F> void each(F fn) {
        std::vector<Entity> snapshot;
        for (uint32_t i = 0; i < living.size(); i++)
            if (living[i])
                snapshot.push_back({i, generations[i]});
        for (auto e : snapshot)
            if (valid(e) && (get<T>(e) && ...))
                fn(e, *get<T>(e)...);
    }
    size_t size() const { return living.size() - free.size(); }
};
} // namespace jsd
