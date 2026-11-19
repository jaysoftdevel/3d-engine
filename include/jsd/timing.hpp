#pragma once
#include <cmath>
#include <stdexcept>
namespace jsd {
// Use completed wall-clock frame durations, including presentation, not the
// simulation delta or requested offline capture cadence.
class FrameRateCounter {
    double elapsed = 0, measured = 0;
    unsigned frames = 0;

  public:
    void sample(double seconds) {
        if (!std::isfinite(seconds) || seconds < 0)
            throw std::invalid_argument("invalid frame duration");
        if (seconds == 0)
            return;
        elapsed += seconds;
        frames++;
        if (elapsed >= .5) {
            measured = frames / elapsed;
            elapsed = 0;
            frames = 0;
        }
    }
    double fps() const { return measured; }
    double milliseconds() const { return measured > 0 ? 1000 / measured : 0; }
    void reset() {
        elapsed = measured = 0;
        frames = 0;
    }
};
} // namespace jsd
