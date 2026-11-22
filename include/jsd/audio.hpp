#pragma once
#include "runtime.hpp"
#include <mutex>
namespace jsd {
struct AudioClip {
    unsigned sampleRate = 48000, channels = 1;
    std::vector<float> samples;
};
std::shared_ptr<AudioClip> loadWav(const std::filesystem::path &);
struct Sound {
    uint64_t id = 0;
};
class AudioMixer {
    struct Voice {
        Sound sound;
        std::shared_ptr<const AudioClip> clip;
        double cursor = 0;
        V3 position;
        float gain = 1;
        bool loop = false, spatial = true;
        float left = 0, right = 0;
        bool initialized = false;
    };
    std::mutex mutex;
    std::vector<Voice> voices;
    uint64_t next = 1;
    V3 listener, right{1, 0, 0};

  public:
    Sound play(std::shared_ptr<const AudioClip>, V3 position = {}, float gain = 1, bool loop = false,
               bool spatial = true);
    // Update a live voice without restarting its clip. Returns false after stop/end.
    bool update(Sound, V3 position, float gain, bool spatial = true);
    void stop(Sound);
    void setListener(V3, V3 right = {1, 0, 0});
    void mix(std::span<float> stereo, unsigned rate = 48000);
};
class AudioDevice {
    unsigned device = 0;
    AudioMixer &mixer;

  public:
    explicit AudioDevice(AudioMixer &);
    ~AudioDevice();
    AudioDevice(const AudioDevice &) = delete;
};
} // namespace jsd
