#pragma once
#include "runtime.hpp"
namespace jsd {
struct CameraKey {
    float time = 0;
    V3 position, target;
    float fov = 55;
};
class CameraTrack {
    std::vector<CameraKey> keys;

  public:
    explicit CameraTrack(std::vector<CameraKey>);
    gfx::Camera sample(float seconds, bool loop = true) const;
    float duration() const { return keys.back().time; }
};
} // namespace jsd
