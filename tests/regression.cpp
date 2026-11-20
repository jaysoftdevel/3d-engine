#include "cinematic_tests.hpp"
#include "runtime_tests.hpp"
#include <frontier/frontier.hpp>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <unistd.h>
using namespace rf;
using namespace rf::gfx;
int main(int argc, char **argv) {
    try {
        int checks = 0;
        auto test = [&](bool value, const char *name) {
            if (!value)
                throw std::runtime_error(name);
            checks++;
            std::cout << "PASS " << name << '\n';
        };
        Capabilities caps;
        caps.maxSamples = 4;
        caps.maxAnisotropy = 4;
        caps.anisotropicFiltering = true;
        caps.hardwareTessellation = false;
        auto s = GraphicsSettings::high();
        s.antiAliasing = AntiAliasing::MSAA;
        s.msaaSamples = 8;
        s.anisotropy = 16;
        s.tessellationBackend = TessellationBackend::Hardware;
        auto r = resolveSettings(s, caps);
        test(r.effective.msaaSamples == 4 && r.requested.msaaSamples == 8,
             "requested MSAA preserved and effective samples clamped");
        test(r.effective.anisotropy == 4, "anisotropy uses driver limit");
        test(r.effective.tessellationBackend == TessellationBackend::Mesh,
             "hardware tessellation falls back on 3.3");
        caps.maxSamples = 1;
        r = resolveSettings(s, caps);
        test(r.effective.antiAliasing == AntiAliasing::FXAA,
             "unsupported MSAA selects reported FXAA fallback");
        s.viewDistance = std::numeric_limits<float>::quiet_NaN();
        r = resolveSettings(s, caps);
        test(std::isfinite(r.effective.viewDistance), "nonfinite view distance normalized");
        auto path = std::filesystem::temp_directory_path() /
                    ("frontier-settings-" + std::to_string(::getpid()) + ".ini");
        std::string error;
        s = GraphicsSettings::ultra();
        test(saveSettings(path, s, error), "settings save");
        auto restored = GraphicsSettings::legacy();
        test(loadSettings(path, restored, error) && restored == s, "all graphics settings roundtrip");
        {
            std::ofstream f(path);
            f << "version=1\nanti_aliasing=made-up\n";
        }
        auto before = restored;
        test(!loadSettings(path, restored, error) && before == restored,
             "invalid configuration leaves current settings unchanged");
        std::filesystem::remove(path);
        Jobs jobs(2);
        auto a = jobs.submit([] { return 42; });
        test(a.get() == 42, "terrain job pool completes futures");
        Mat4 p = perspective(1.2f, 1.7f);
        auto identity = p * inverse(p);
        test(std::abs(identity.m[0] - 1) < .001f && std::abs(identity.m[15] - 1) < .001f,
             "projection inverse reconstructs identity");
        auto vertical = lookAt({0, 0, 0}, {0, 1, 0});
        test(std::abs(vertical.m[0]) + std::abs(vertical.m[8]) > .9f,
             "vertical view keeps a valid camera basis");
        test(std::string(frontier_engine_version()) == "0.4.4", "exported engine version symbol");
        runtimeTests(test);
        cinematicTests(test);
        if (argc > 1 && std::string(argv[1]) == "--graphics") {
            Platform platform(true, 640, 360);
            Renderer renderer(platform, GraphicsSettings::legacy());
            std::vector<unsigned char> pixels(2048 * 1024 * 4, 180);
            auto texture = renderer.createTexture(2048, 1024, pixels);
            s = renderer.settings().requested;
            s.textureQuality = TextureQuality::Low;
            renderer.setSettings(s);
            test(renderer.textureDimensions(texture) == std::pair{512, 256},
                 "texture quality changes actual GPU allocation");
            s.textureQuality = TextureQuality::High;
            renderer.setSettings(s);
            test(renderer.textureDimensions(texture) == std::pair{2048, 1024},
                 "raising quality restores retained source texture");
            bool rejected = false;
            try {
                renderer.createMesh(std::vector<Vertex>(2));
            } catch (const std::invalid_argument &) {
                rejected = true;
            }
            test(rejected, "invalid triangle mesh rejected");
            rejected = false;
            Camera invalidCamera;
            invalidCamera.position.x = std::numeric_limits<float>::quiet_NaN();
            try {
                renderer.begin(invalidCamera);
            } catch (const std::invalid_argument &) {
                rejected = true;
            }
            test(rejected, "nonfinite camera rejected before rendering");
            Camera camera{{8, 5, -12}, normalized(V3{-8, -3, 15}), 70, .08f};
            auto draw = [&] {
                renderer.begin(camera);
                Material m;
                m.albedo = {.7f, .3f, .2f, 1};
                m.albedoTexture = texture;
                renderer.primitive(Primitive::Box, {{0, 1, 2}, {3, 2, 3}, 0}, m);
                Terrain terrain;
                terrain.height = [](float, float) { return 0.f; };
                renderer.terrain(terrain);
                renderer.end();
                renderer.check();
            };
            for (auto aa : {AntiAliasing::Off, AntiAliasing::FXAA, AntiAliasing::SMAA, AntiAliasing::MSAA,
                            AntiAliasing::TAA}) {
                s = GraphicsSettings::legacy();
                s.viewDistance = 64;
                s.antiAliasing = aa;
                renderer.setSettings(s);
                draw();
                test(renderer.statistics().drawCalls > 0, "AA mode renders without GL errors");
            }
            s = GraphicsSettings::high();
            s.viewDistance = 64;
            s.depthOfField = Quality::High;
            renderer.setSettings(s);
            draw();
            test(renderer.statistics().postPasses >= 8, "combined effects execute multipass pipeline");
            test(renderer.statistics().shadowResolution >= 1024, "shadow quality allocates actual map");
            s.tessellationBackend = TessellationBackend::Mesh;
            renderer.setSettings(s);
            draw();
            test(!renderer.statistics().hardwareTessellation, "explicit mesh terrain backend renders");
            if (renderer.capabilities().hardwareTessellation) {
                s.tessellationBackend = TessellationBackend::Hardware;
                renderer.setSettings(s);
                draw();
                test(renderer.statistics().hardwareTessellation,
                     "hardware terrain tessellation executes patch path");
            }
            s = GraphicsSettings::legacy();
            s.viewDistance = 64;
            renderer.setSettings(s);
            auto distanceScene = [&] {
                renderer.begin({{0, 2, -5}, {0, 0, 1}, 70, .08f});
                renderer.primitive(Primitive::Box, {{0, 1, 2}, {2, 2, 2}, 0});
                renderer.primitive(Primitive::Box, {{0, 1, 150}, {2, 2, 2}, 0});
                renderer.end();
                renderer.check();
                return renderer.statistics().triangles;
            };
            auto nearCount = distanceScene();
            s.viewDistance = 256;
            renderer.setSettings(s);
            auto farCount = distanceScene();
            test(farCount > nearCount, "view distance changes submitted scene geometry");
            s = GraphicsSettings::high();
            s.viewDistance = 64;
            s.antiAliasing = AntiAliasing::TAA;
            s.renderScale = .67f;
            renderer.setSettings(s);
            draw();
            draw();
            test(renderer.statistics().renderWidth == 429 && renderer.statistics().outputWidth == 640 &&
                     renderer.statistics().temporalHistoryValid,
                 "temporal upscaling keeps full output resolution and history");
            s = GraphicsSettings::legacy();
            s.viewDistance = 64;
            renderer.setSettings(s);
            renderer.begin(camera);
            Light lamp;
            lamp.position = {0, 4, 0};
            renderer.light(lamp);
            renderer.primitive(Primitive::Box, {{0, 1, 2}, {2, 2, 2}, 0});
            renderer.end();
            renderer.check();
            test(renderer.statistics().localLights == 1, "tiled local lighting submits real light data");
            std::vector<float> hdr(16 * 8 * 3, 2);
            auto environment = renderer.createEnvironment(16, 8, hdr);
            Environment env;
            env.image = environment;
            renderer.begin(camera, env);
            renderer.primitive(Primitive::Box, {{0, 1, 2}, {2, 2, 2}, 0});
            renderer.end();
            renderer.check();
            test(environment.value != 0, "HDR environment prefilter and BRDF lookup render successfully");
            renderer.destroyEnvironment(environment);
            std::string reloadError;
            test(!renderer.reloadMaterialShaders("invalid shader", "invalid shader", reloadError) &&
                     !reloadError.empty(),
                 "shader reload rejects invalid programs transactionally");
            draw();
            auto motionBox = renderer.createMesh(makeBox());
            auto movingFrame = [&](Quality blur, const char *label) {
                auto option = GraphicsSettings::legacy();
                option.viewDistance = 64;
                option.motionBlur = blur;
                renderer.setSettings(option);
                for (int frame = 0; frame < 2; frame++) {
                    renderer.begin(camera);
                    Draw object;
                    object.mesh = motionBox;
                    object.objectId = 1234;
                    object.transform = {{frame ? 1.f : -1.f, 1, 2}, {3, 2, 3}, 0};
                    object.material.albedo = {.9f, .15f, .1f, 1};
                    renderer.submit(object);
                    renderer.end();
                    renderer.check();
                }
                auto file = std::filesystem::temp_directory_path() /
                            (std::string(label) + std::to_string(::getpid()) + ".ppm");
                renderer.screenshot(file);
                std::ifstream input(file, std::ios::binary);
                std::string bytes((std::istreambuf_iterator<char>(input)), {});
                std::filesystem::remove(file);
                return bytes;
            };
            auto sharp = movingFrame(Quality::Off, "jsd-sharp-"),
                 blurred = movingFrame(Quality::High, "jsd-blur-");
            test(sharp != blurred, "object motion vectors produce visible motion blur");
            auto overlapCenter = [&](bool nearFirst) {
                auto option = GraphicsSettings::legacy();
                option.viewDistance = 64;
                renderer.setSettings(option);
                renderer.begin({{0, 1, -6}, {0, 0, 1}, 60, .08f});
                auto box = [&](float z, Color color) {
                    Draw draw;
                    draw.mesh = motionBox;
                    draw.transform = {{0, 1, z}, {2, 2, .5f}, 0};
                    draw.material.albedo = color;
                    renderer.submit(draw);
                };
                Color red{.9f, .05f, .05f, 1}, green{.05f, .9f, .05f, 1};
                if (nearFirst) {
                    box(0, red);
                    box(4, green);
                } else {
                    box(4, green);
                    box(0, red);
                }
                renderer.end();
                renderer.check();
                auto file = std::filesystem::temp_directory_path() /
                            ("jsd-overlap-" + std::to_string(::getpid()) + ".ppm");
                renderer.screenshot(file);
                std::ifstream input(file, std::ios::binary);
                std::string bytes((std::istreambuf_iterator<char>(input)), {});
                std::filesystem::remove(file);
                int width = 0, height = 0;
                std::istringstream header(bytes);
                std::string magic;
                int range = 0;
                header >> magic >> width >> height >> range;
                size_t offset = size_t(header.tellg()) + 1 + (size_t(height / 2) * width + width / 2) * 3;
                std::array<int, 3> center{};
                if (offset + 2 < bytes.size())
                    center = {uint8_t(bytes[offset]), uint8_t(bytes[offset + 1]), uint8_t(bytes[offset + 2])};
                return center;
            };
            for (bool nearFirst : {true, false}) {
                auto center = overlapCenter(nearFirst);
                test(center[0] > center[1] * 2,
                     "depth prepass keeps the nearest opaque surface regardless of draw order");
            }
            cinematicGraphicsTests(test, renderer);
            renderer.destroyMesh(motionBox);
            renderer.destroyTexture(texture);
            test(renderer.textureDimensions(texture) == std::pair{0, 0},
                 "destroyed texture handle is retired");
        }
        std::cout << checks << " checks passed.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAILED: " << e.what() << '\n';
        return 1;
    }
}
