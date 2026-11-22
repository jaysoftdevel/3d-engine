#include "jsd/audio.hpp"
#include <cstring>
#include <fstream>
namespace jsd {
std::shared_ptr<AudioClip> loadWav(const std::filesystem::path &path) {
    std::ifstream f(path, std::ios::binary);
    std::vector<unsigned char> b((std::istreambuf_iterator<char>(f)), {});
    if (b.size() < 44 || b.size() > 128 * 1024 * 1024 || std::memcmp(b.data(), "RIFF", 4) ||
        std::memcmp(b.data() + 8, "WAVE", 4))
        throw std::runtime_error("invalid WAV");
    auto u16 = [&](size_t p) { return unsigned(b.at(p)) | (unsigned(b.at(p + 1)) << 8); };
    auto u32 = [&](size_t p) { return u16(p) | (u16(p + 2) << 16); };
    unsigned format = 0, channels = 0, rate = 0, bits = 0;
    size_t offset = 0, count = 0;
    for (size_t p = 12; p + 8 <= b.size();) {
        unsigned size = u32(p + 4);
        if (size > b.size() - p - 8)
            throw std::runtime_error("truncated WAV chunk");
        if (!std::memcmp(b.data() + p, "fmt ", 4)) {
            if (size < 16)
                throw std::runtime_error("short WAV format");
            format = u16(p + 8);
            channels = u16(p + 10);
            rate = u32(p + 12);
            bits = u16(p + 22);
        }
        if (!std::memcmp(b.data() + p, "data", 4)) {
            offset = p + 8;
            count = size;
        }
        p += 8 + size + (size & 1);
    }
    if ((format != 1 && format != 3) || (channels != 1 && channels != 2) || rate < 8000 || rate > 192000 ||
        (format == 1 && bits != 16) || (format == 3 && bits != 32) || !count)
        throw std::runtime_error("WAV requires PCM16 or float32 mono/stereo");
    auto out = std::make_shared<AudioClip>();
    out->sampleRate = rate;
    out->channels = channels;
    size_t step = bits / 8;
    for (size_t p = offset; p + step <= offset + count; p += step) {
        float sample;
        if (format == 1)
            sample = int16_t(u16(p)) / 32768.f;
        else
            std::memcpy(&sample, b.data() + p, 4);
        if (!std::isfinite(sample))
            throw std::runtime_error("nonfinite WAV sample");
        out->samples.push_back(rf::clamp(sample, -1, 1));
    }
    return out;
}
Sound AudioMixer::play(std::shared_ptr<const AudioClip> clip, V3 p, float gain, bool loop, bool spatial) {
    if (!clip || clip->samples.empty() || (clip->channels != 1 && clip->channels != 2) ||
        clip->sampleRate == 0 || !std::isfinite(gain))
        throw std::invalid_argument("invalid sound");
    std::lock_guard lock(mutex);
    if (voices.size() >= 128)
        throw std::runtime_error("voice budget exhausted");
    Sound id{next++};
    voices.push_back({id, std::move(clip), 0, p, gain, loop, spatial});
    return id;
}
bool AudioMixer::update(Sound s, V3 p, float gain, bool spatial) {
    if (!std::isfinite(gain) || !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
        throw std::invalid_argument("invalid voice update");
    std::lock_guard lock(mutex);
    for (auto &v : voices) if (v.sound.id == s.id) {
        v.position = p; v.gain = std::max(0.f, gain); v.spatial = spatial;
        return true;
    }
    return false;
}
void AudioMixer::stop(Sound s) {
    std::lock_guard lock(mutex);
    std::erase_if(voices, [&](auto &v) { return v.sound.id == s.id; });
}
void AudioMixer::setListener(V3 p, V3 r) {
    std::lock_guard lock(mutex);
    listener = p;
    right = rf::normalized(r);
}
void AudioMixer::mix(std::span<float> out, unsigned rate) {
    if (out.size() % 2 || rate == 0)
        throw std::invalid_argument("stereo output required");
    std::fill(out.begin(), out.end(), 0);
    std::lock_guard lock(mutex);
    for (auto &v : voices) {
        size_t frames = v.clip->samples.size() / v.clip->channels;
        V3 delta = v.position - listener;
        float dist = rf::length(delta), pan = v.spatial ? rf::dot(rf::normalized(delta), right) : 0,
              attenuation = v.spatial ? 1 / (1 + .04f * dist * dist) : 1;
        float left = std::sqrt((1 - pan) * .5f) * v.gain * attenuation,
              r = std::sqrt((1 + pan) * .5f) * v.gain * attenuation;
        if (!v.initialized) { v.left = left; v.right = r; v.initialized = true; }
        const float smooth = 1.f - std::exp(-1.f / (rate * .025f));
        for (size_t frame = 0; frame < out.size() / 2; frame++) {
            v.left += (left - v.left) * smooth;
            v.right += (r - v.right) * smooth;
            if (v.cursor >= frames) {
                if (v.loop)
                    v.cursor = std::fmod(v.cursor, double(frames));
                else
                    break;
            }
            size_t a = size_t(v.cursor), b = std::min(a + 1, frames - 1);
            float t = v.cursor - a;
            float l = rf::mix(v.clip->samples[a * v.clip->channels], v.clip->samples[b * v.clip->channels],
                              t),
                  rightSample = v.clip->channels == 2
                                    ? rf::mix(v.clip->samples[a * 2 + 1], v.clip->samples[b * 2 + 1], t)
                                    : l;
            out[frame * 2] += l * v.left;
            out[frame * 2 + 1] += rightSample * v.right;
            v.cursor += double(v.clip->sampleRate) / rate;
        }
    }
    std::erase_if(voices,
                  [](auto &v) { return !v.loop && v.cursor >= v.clip->samples.size() / v.clip->channels; });
    for (auto &sample : out)
        sample = rf::clamp(sample, -1, 1);
}
AudioDevice::AudioDevice(AudioMixer &m) : mixer(m) {
    if (SDL_Init(vl::sdl::audio) < 0)
        throw std::runtime_error(SDL_GetError());
    SDL_AudioSpec desired{}, obtained{};
    desired.freq = 48000;
    desired.format = 0x8120;
    desired.channels = 2;
    desired.samples = 512;
    desired.userdata = &mixer;
    desired.callback = [](void *data, U8 *stream, int bytes) {
        auto *m = static_cast<AudioMixer *>(data);
        m->mix({reinterpret_cast<float *>(stream), size_t(bytes) / sizeof(float)}, 48000);
    };
    device = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    if (!device)
        throw std::runtime_error(SDL_GetError());
    SDL_PauseAudioDevice(device, 0);
}
AudioDevice::~AudioDevice() {
    if (device)
        SDL_CloseAudioDevice(device);
}
} // namespace jsd
