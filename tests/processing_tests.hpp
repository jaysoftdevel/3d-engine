#pragma once
#include <frontier/frontier.hpp>
#include <atomic>
#include <fstream>
#include <unistd.h>

template <class Check> void processingTests(Check check) {
    using namespace rf;
    using namespace rf::gfx;
    for (int i = 0; i < 5; ++i) {
        GraphicsPreset preset;
        check(parseGraphicsPreset(name(GraphicsPreset(i)), preset) && int(preset) == i,
              "all five preset names parse without ambiguity");
        auto settings = graphicsPreset(preset);
        check(settings.renderScale == 1 && !settings.wireframe && settings.autoFocus,
              "presets keep native resolution, textured surfaces and autofocus");
    }
    auto maximum = GraphicsSettings::max();
    check(maximum.bloom == Quality::Ultra && maximum.depthOfField == Quality::Ultra &&
          maximum.shadows == Quality::Ultra && maximum.terrainTessellation == Quality::Ultra,
          "max preset selects the highest effect quality");
    GraphicsPreset alias;
    check(parseGraphicsPreset("ultra", alias) && alias == GraphicsPreset::Max &&
          parseGraphicsPreset("balanced", alias) && alias == GraphicsPreset::Medium,
          "legacy preset aliases remain accepted");
    check(!parseGraphicsPreset("unknown", alias), "invalid preset is rejected");
    auto path = std::filesystem::temp_directory_path() / ("jsd-focus-settings-" + std::to_string(getpid()));
    auto settings = GraphicsSettings::max();
    settings.autoFocus = false;
    settings.focusAdaptation = 7.25f;
    std::string error;
    check(saveSettings(path, settings, error), "autofocus settings save");
    auto restored = GraphicsSettings::minimal();
    check(loadSettings(path, restored, error) && restored == settings,
          "preset, manual focus and focus response roundtrip exactly");
    { std::ofstream file(path); file << "version=1\nauto_focus=2\n"; }
    check(!loadSettings(path, restored, error) && restored == settings,
          "invalid autofocus settings leave current configuration unchanged");
    std::filesystem::remove(path);
    Jobs pool(2);
    std::vector<unsigned> values(10000);
    pool.parallelFor(values.size(), 512, [&](size_t lo, size_t hi) {
        for (size_t i = lo; i < hi; ++i) values[i] = unsigned(i * i);
    });
    bool complete = true;
    for (size_t i = 0; i < values.size(); ++i) complete &= values[i] == i * i;
    check(complete, "parallel work partitions every element exactly once");
    auto nested = pool.submit([&] {
        return pool.parallelFor(100, 1, [](size_t, size_t) {});
    });
    check(nested.get() == 1, "nested parallel work avoids worker-pool deadlocks");
    std::atomic<unsigned> visits{0};
    bool propagated = false;
    try {
        pool.parallelFor(10000, 512, [&](size_t lo, size_t) {
            ++visits;
            if (lo == 0) throw std::runtime_error("expected worker error");
        });
    } catch (const std::runtime_error &) { propagated = true; }
    check(propagated && visits > 0, "parallel processing joins work and propagates failures");
}

template <class Check> void autofocusGraphicsTests(Check check, rf::gfx::Renderer &renderer) {
    using namespace rf;
    using namespace rf::gfx;
    auto settings = GraphicsSettings::legacy();
    settings.viewDistance = 64;
    settings.depthOfField = Quality::Low;
    settings.focusAdaptation = 6;
    renderer.setSettings(settings);
    auto draw = [&](float z, float cameraZ = 0, bool box = true) {
        Environment environment;
        environment.deltaSeconds = 1.f / 60;
        renderer.begin({{0, 0, cameraZ}, {0, 0, 1}, 60, .08f}, environment);
        if (box) renderer.primitive(Primitive::Box, {{0, 0, z}, {2, 2, 2}, 0});
        renderer.end();
        renderer.check();
    };
    auto directory = std::filesystem::temp_directory_path() / ("jsd-focus-" + std::to_string(getpid()));
    auto focus = [&] {
        std::filesystem::remove_all(directory);
        renderer.captureBuffers(directory);
        float value = -1;
        std::ifstream(directory / "autofocus.txt") >> value;
        return value;
    };
    draw(8);
    check(std::abs(focus() - 7) < .03f, "autofocus acquires the actual center surface depth");
    draw(24);
    float transition = focus();
    check(transition > 7 && transition < 23, "autofocus adapts smoothly instead of jumping");
    for (int i = 0; i < 60; ++i) draw(24);
    check(std::abs(focus() - 23) < .1f, "autofocus converges on the new subject");
    draw(24, 12);
    check(std::abs(focus() - 11) < .03f, "camera cuts reacquire focus immediately");
    for (int i = 0; i < 90; ++i) draw(0, 12, false);
    check(std::abs(focus() - 64) < .2f, "empty sky converges to the far focus distance");
    unsigned passes = renderer.statistics().postPasses;
    settings.autoFocus = false;
    settings.focusDistance = 11;
    renderer.setSettings(settings);
    draw(24, 12);
    check(renderer.statistics().postPasses + 1 == passes,
          "manual focus omits the autofocus pass while retaining depth of field");
    std::filesystem::remove_all(directory);
    renderer.begin({{0, 0, 0}, {0, 0, 1}, 60, .08f});
    for (int i = 0; i < 8192; ++i)
        renderer.primitive(Primitive::Box, {{1000.f + i, 0, 5}, {1, 1, 1}, 0});
    renderer.end();
    renderer.check();
    check(renderer.statistics().preparationWorkers <= availableHardwareThreads() &&
          (availableHardwareThreads() == 1 || renderer.statistics().preparationWorkers > 1),
          "large scenes prepare draws across available CPU cores");
}
