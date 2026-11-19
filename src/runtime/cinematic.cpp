#include "jsd/cinematic.hpp"
namespace jsd {
CameraTrack::CameraTrack(std::vector<CameraKey> k) : keys(std::move(k)) {
    if (keys.size() < 2 || keys.front().time != 0)
        throw std::invalid_argument("camera track requires zero origin and two keys");
    for (size_t i = 0; i < keys.size(); i++) {
        auto &k = keys[i];
        if (!std::isfinite(k.time + k.position.x + k.position.y + k.position.z + k.target.x + k.target.y +
                           k.target.z + k.fov) ||
            k.fov < 10 || k.fov > 150 || rf::distance(k.position, k.target) < .01f ||
            (i && k.time <= keys[i - 1].time))
            throw std::invalid_argument("invalid camera key");
    }
}
gfx::Camera CameraTrack::sample(float time, bool loop) const {
    if (!std::isfinite(time))
        throw std::invalid_argument("invalid camera time");
    time = loop ? std::fmod(std::max(0.f, time), duration()) : rf::clamp(time, 0, duration());
    size_t j = std::upper_bound(keys.begin(), keys.end(), time, [](float t, auto &k) { return t < k.time; }) -
               keys.begin();
    j = std::clamp(j, size_t(1), keys.size() - 1);
    size_t i = j - 1, a = i ? i - 1 : i, b = std::min(j + 1, keys.size() - 1);
    float delta = keys[j].time - keys[i].time, u = rf::clamp((time - keys[i].time) / delta), u2 = u * u,
          u3 = u2 * u;
    auto blend = [&](V3 A, V3 I, V3 J, V3 B) {
        V3 left = (J - A) / (keys[j].time - keys[a].time), right = (B - I) / (keys[b].time - keys[i].time);
        return I * (2 * u3 - 3 * u2 + 1) + left * ((u3 - 2 * u2 + u) * delta) + J * (-2 * u3 + 3 * u2) +
               right * ((u3 - u2) * delta);
    };
    auto p = blend(keys[a].position, keys[i].position, keys[j].position, keys[b].position),
         target = blend(keys[a].target, keys[i].target, keys[j].target, keys[b].target);
    return {p, rf::normalized(target - p), rf::mix(keys[i].fov, keys[j].fov, u * u * (3 - 2 * u)), .08f};
}
} // namespace jsd
