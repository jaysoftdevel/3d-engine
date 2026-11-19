#include <chrono>
#include <frontier/frontier.hpp>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>

using namespace rf;
using namespace rf::gfx;
namespace {
template <class T> T parse(const std::string &v, int count) {
    for (int n = 0; n < count; n++)
        if (v == name(T(n)))
            return T(n);
    throw std::invalid_argument("Unknown setting value: " + v);
}
std::string number(float x) {
    return std::isfinite(x) ? std::to_string(int(x)) : "invalid";
}
} // namespace
int main(int argc, char **argv) {
    try {
        GraphicsSettings settings = GraphicsSettings::balanced();
        bool offscreen = false, noUi = false;
        int frames = 0, width = 1280, height = 720;
        std::string capture, config, writeConfig;
        std::vector<std::pair<std::string, std::string>> overrides;
        auto arg = [&](int &i) {
            if (i + 1 >= argc)
                throw std::invalid_argument("Missing option value");
            return std::string(argv[++i]);
        };
        for (int i = 1; i < argc; i++) {
            std::string a = argv[i];
            if (a == "--offscreen")
                offscreen = true;
            else if (a == "--no-ui")
                noUi = true;
            else if (a == "--frames")
                frames = std::stoi(arg(i));
            else if (a == "--width")
                width = std::stoi(arg(i));
            else if (a == "--height")
                height = std::stoi(arg(i));
            else if (a == "--capture")
                capture = arg(i);
            else if (a == "--settings")
                config = arg(i);
            else if (a == "--write-settings")
                writeConfig = arg(i);
            else if (a == "--preset") {
                auto v = arg(i);
                settings = v == "legacy"     ? GraphicsSettings::legacy()
                           : v == "low"      ? GraphicsSettings::low()
                           : v == "balanced" ? GraphicsSettings::balanced()
                           : v == "high"     ? GraphicsSettings::high()
                           : v == "ultra"    ? GraphicsSettings::ultra()
                                             : throw std::invalid_argument("Unknown preset");
            } else if (a == "--help") {
                std::cout
                    << "JSD Game Engine graphics laboratory 0.4.0\n\nWASD move, arrows look, Tab mouse "
                       "capture, Escape quit. Click settings to cycle.\n\n--preset "
                       "legacy|low|balanced|high|ultra\n--aa off|fxaa|smaa|msaa|taa --msaa 2|4|8\n--textures "
                       "low|medium|high|ultra --anisotropy 1|2|4|8|16\n--shadows off|low|medium|high|ultra "
                       "--view-distance METERS\n--ao QUALITY --dof QUALITY --focus METERS\n--gi "
                       "off|ambient-probe|screen-space --gi-quality QUALITY\n--ssr QUALITY --edge-fusion "
                       "QUALITY --tessellation QUALITY\n--terrain-backend "
                       "automatic|mesh|hardware\n--offscreen --frames N --width N --height N "
                       "--no-ui\n--render-scale .5..2 --sharpening 0..1 --bloom QUALITY --bloom-strength "
                       "FLOAT\n"
                       "--auto-exposure 0|1 --exposure EV --tone-mapper aces|reinhard|neutral\n"
                       "--motion-blur QUALITY --volumetric-fog QUALITY --gpu-profile 0|1\n"
                       "--contrast FLOAT --saturation FLOAT --temperature -1..1 --vignette 0..1\n--capture "
                       "FILE.ppm --settings FILE.ini --write-settings FILE.ini\n";
                return 0;
            } else {
                std::string v = arg(i);
                overrides.push_back({a, v});
            }
        }
        std::string error;
        if (!config.empty() && !loadSettings(config, settings, error))
            throw std::runtime_error(error);
        for (auto &[key, v] : overrides) {
            if (key == "--aa")
                settings.antiAliasing = parse<AntiAliasing>(v, 5);
            else if (key == "--motion-blur")
                settings.motionBlur = parse<Quality>(v, 5);
            else if (key == "--volumetric-fog")
                settings.volumetricFog = parse<Quality>(v, 5);
            else if (key == "--render-scale")
                settings.renderScale = std::stof(v);
            else if (key == "--bloom")
                settings.bloom = parse<Quality>(v, 5);
            else if (key == "--bloom-strength")
                settings.bloomStrength = std::stof(v);
            else if (key == "--auto-exposure")
                settings.autoExposure = std::stoi(v) != 0;
            else if (key == "--exposure")
                settings.exposureCompensation = std::stof(v);
            else if (key == "--tone-mapper")
                settings.toneMapper = parse<ToneMapper>(v, 3);
            else if (key == "--sharpening")
                settings.sharpening = std::stof(v);
            else if (key == "--contrast")
                settings.contrast = std::stof(v);
            else if (key == "--saturation")
                settings.saturation = std::stof(v);
            else if (key == "--temperature")
                settings.temperature = std::stof(v);
            else if (key == "--vignette")
                settings.vignette = std::stof(v);
            else if (key == "--gpu-profile")
                settings.gpuProfiling = std::stoi(v) != 0;
            else if (key == "--msaa")
                settings.msaaSamples = std::stoi(v);
            else if (key == "--textures")
                settings.textureQuality = parse<TextureQuality>(v, 4);
            else if (key == "--anisotropy")
                settings.anisotropy = std::stoi(v);
            else if (key == "--shadows")
                settings.shadows = parse<Quality>(v, 5);
            else if (key == "--view-distance")
                settings.viewDistance = std::stof(v);
            else if (key == "--ao")
                settings.ambientOcclusion = parse<Quality>(v, 5);
            else if (key == "--dof")
                settings.depthOfField = parse<Quality>(v, 5);
            else if (key == "--focus")
                settings.focusDistance = std::stof(v);
            else if (key == "--gi")
                settings.globalIllumination = parse<GlobalIllumination>(v, 3);
            else if (key == "--gi-quality")
                settings.illuminationQuality = parse<Quality>(v, 5);
            else if (key == "--ssr")
                settings.reflections = parse<Quality>(v, 5);
            else if (key == "--edge-fusion")
                settings.edgeFusion = parse<Quality>(v, 5);
            else if (key == "--tessellation")
                settings.terrainTessellation = parse<Quality>(v, 5);
            else if (key == "--terrain-backend")
                settings.tessellationBackend = parse<TessellationBackend>(v, 3);
            else
                throw std::invalid_argument("Unknown option: " + key);
        }
        if (width < 320 || height < 180 || width > 3840 || height > 2160 || frames < 0)
            throw std::invalid_argument("Invalid viewport/frame count");
        if (offscreen && !frames)
            frames = 2;
        Platform platform(offscreen, width, height);
        Renderer renderer(platform, settings);
        std::cout << "JSD Game Engine " << frontier_engine_version() << " / " << renderer.capabilities().renderer
                  << " / " << renderer.capabilities().version << '\n';
        for (auto &message : renderer.settings().adjustments)
            std::cout << "Adjusted: " << message << '\n';
        settings = renderer.settings().requested;
        std::vector<unsigned char> pixels(2048 * 2048 * 4);
        for (int y = 0; y < 2048; y++)
            for (int x = 0; x < 2048; x++) {
                bool grid = ((x / 8 + y / 8) % 2) == 0;
                int offset = (y * 2048 + x) * 4;
                pixels[offset] = grid ? 205 : 45;
                pixels[offset + 1] = grid ? 210 : 65;
                pixels[offset + 2] = grid ? 215 : 95;
                pixels[offset + 3] = 255;
            }
        TextureId checker = renderer.createTexture(2048, 2048, pixels);
        float yaw = -.57f, pitch = -.17f;
        V3 eye{15, 5, -18};
        bool run = true, captureMouse = false, click = false;
        int page = 0;
        float mouseX = 0, mouseY = 0;
        std::array<bool, 512> keys{};
        int frame = 0;
        using Clock = std::chrono::steady_clock;
        auto previous = Clock::now(), started = previous;
        while (run && (!frames || frame < frames)) {
            auto now = Clock::now();
            float dt =
                offscreen ? 1.f / 60 : std::min(.1f, std::chrono::duration<float>(now - previous).count());
            previous = now;
            click = false;
            if (!offscreen) {
                SDL_Event e;
                while (SDL_PollEvent(&e)) {
                    if (e.type == vl::sdl::quit)
                        run = false;
                    if (e.type == vl::sdl::keyDown || e.type == vl::sdl::keyUp) {
                        int k = e.key.keysym.scancode;
                        if (k >= 0 && k < 512)
                            keys[k] = e.type == vl::sdl::keyDown;
                        if (e.type == vl::sdl::keyDown && !e.key.repeat) {
                            if (k == 41)
                                run = false;
                            if (k == 43) {
                                captureMouse = !captureMouse;
                                platform.captureMouse(captureMouse);
                            }
                        }
                    }
                    if (e.type == vl::sdl::window && e.window.event == 13) {
                        keys.fill(false);
                        captureMouse = false;
                        platform.captureMouse(false);
                    }
                    if (e.type == vl::sdl::mouseMotion) {
                        int w = width, h = height;
                        SDL_GetWindowSize(platform.window, &w, &h);
                        mouseX = e.motion.x * 1280.f / w;
                        mouseY = e.motion.y * 720.f / h;
                        if (captureMouse) {
                            yaw -= e.motion.xrel * .0025f;
                            pitch = clamp(pitch - e.motion.yrel * .0025f, -1.4f, 1.4f);
                        }
                    }
                    if (e.type == vl::sdl::mouseDown && e.button.button == 1)
                        click = true;
                }
            }
            yaw += (float(keys[80]) - float(keys[79])) * dt;
            pitch = clamp(pitch + (float(keys[82]) - float(keys[81])) * dt, -1.4f, 1.4f);
            float speed = keys[225] ? 14 : 6;
            eye += forward(yaw) * (float(keys[26]) - float(keys[22])) * speed * dt;
            eye += rotate(forward(yaw), -pi / 2) * (float(keys[7]) - float(keys[4])) * speed * dt;
            eye.y += (float(keys[44]) - float(keys[224])) * speed * dt;
            renderer.begin({eye, forward(yaw, pitch), 70, .08f});
            Terrain terrain;
            terrain.seed = 101;
            terrain.height = [](float x, float z) {
                return terrainHeight(x, z) +
                       std::sin(x * 1.7f) * std::cos(z * 1.3f) * .28f * clamp((std::hypot(x, z) - 15) / 20);
            };
            renderer.terrain(terrain);
            auto object = [&](Primitive shape, V3 p, V3 scale, Color c, float rough = .7f, float metal = 0,
                              TextureId tex = {}) {
                Material m;
                m.albedo = c;
                m.roughness = rough;
                m.metallic = metal;
                m.albedoTexture = tex;
                renderer.primitive(shape, {p, scale, 0}, m);
            };
            object(Primitive::Box, {0, .04f, 3}, {28, .12f, 28}, {.6f, .64f, .68f, 1}, .12f, .85f);
            for (int j = 0; j < 6; j++) {
                float x = -9 + j * 3.4f;
                object(Primitive::Box, {x, 1.5f, 5}, {1.7f, 3, 1.7f},
                       {.2f + .13f * j, .25f, .8f - .1f * j, 1}, .15f + j * .11f, .05f);
                object(Primitive::Cylinder, {x, 1.1f, -1}, {1.5f, 2.2f, 1.5f}, {.25f, .7f, .8f, 1}, .2f, .7f);
            }
            object(Primitive::Box, {-8, 2.5f, 11}, {9, 5, .4f}, {.8f, .16f, .12f, 1});
            object(Primitive::Box, {8, 2.5f, 11}, {9, 5, .4f}, {.12f, .55f, .8f, 1});
            object(Primitive::Box, {0, .22f, -5}, {16, .18f, 4}, {1, 1, 1, 1}, .8f, 0, checker);
            for (int j = 0; j < 8; j++)
                object(Primitive::Box, {-6 + j * 1.7f, 1.7f, 15}, {.3f, 3.4f, .5f}, {.85f, .7f, .3f, 1});
            for (int j = 0; j < 16; j++) {
                float a = j * 2.4f, r = 25 + j * 5;
                V3 p{std::sin(a) * r, 0, std::cos(a) * r};
                p.y = terrain.height(p.x, p.z) + 1.5f;
                object(Primitive::Rock, p, {3, 3, 3}, {.6f, .38f, .25f, 1});
            }
            Material glow;
            glow.albedo = {.1f, .5f, 1, 1};
            glow.emissive = {1, 6, 15, 1};
            renderer.primitive(Primitive::Cylinder, {{-2, 3, -3}, {.3f, 2, .3f}, 0}, glow);
            if (!noUi) {
                renderer.rect(20, 20, 640, 79, {.02f, .045f, .06f, .9f});
                renderer.text(36, 36, "JSD ENGINE / GRAPHICS LAB", 2.4f, {.3f, .9f, .8f, 1});
                renderer.text(36, 73, "WASD MOVE / ARROWS LOOK / TAB CAPTURE / ESC QUIT", 1.3f);
                renderer.rect(850, 20, 410, 678, {.02f, .045f, .065f, .96f});
                renderer.text(870, 38, "GRAPHICS SETTINGS / PAGES", 2, {.3f, .9f, .8f, 1});
                bool changed = false;
                float rowY = 81;
                auto row = [&](const std::string &label, const std::string &value, auto change) {
                    bool hover = mouseX > 865 && mouseX < 1245 && mouseY >= rowY && mouseY < rowY + 28;
                    renderer.rect(865, rowY, 380, 28,
                                  hover ? Color{.12f, .27f, .3f, 1} : Color{.055f, .11f, .15f, 1});
                    renderer.text(875, rowY + 9, label, 1.4f);
                    renderer.text(1080, rowY + 9, value, 1.4f, {.3f, .9f, .8f, 1});
                    if (hover && click) {
                        change();
                        changed = true;
                    }
                    rowY += 35;
                };
                auto quality = [](Quality &q) { q = Quality((int(q) + 1) % 5); };
                if (page == 0) {
                    row("ANTI ALIASING", name(settings.antiAliasing),
                        [&] { settings.antiAliasing = AntiAliasing((int(settings.antiAliasing) + 1) % 5); });
                    row("MSAA SAMPLES", number(settings.msaaSamples), [&] {
                        settings.msaaSamples = settings.msaaSamples >= 8 ? 2 : settings.msaaSamples * 2;
                    });
                    row("TEXTURE QUALITY", name(settings.textureQuality), [&] {
                        settings.textureQuality = TextureQuality((int(settings.textureQuality) + 1) % 4);
                    });
                    row("ANISOTROPY", number(settings.anisotropy) + "X", [&] {
                        settings.anisotropy = settings.anisotropy >= 16 ? 1 : settings.anisotropy * 2;
                    });
                    row("SHADOW QUALITY", name(settings.shadows), [&] { quality(settings.shadows); });
                    row("VIEW DISTANCE", number(settings.viewDistance) + "M", [&] {
                        settings.viewDistance =
                            settings.viewDistance >= 500 ? 80 : settings.viewDistance + 60;
                    });
                    row("AMBIENT OCCLUSION", name(settings.ambientOcclusion),
                        [&] { quality(settings.ambientOcclusion); });
                    row("DEPTH OF FIELD", name(settings.depthOfField),
                        [&] { quality(settings.depthOfField); });
                    row("FOCUS DISTANCE", number(settings.focusDistance) + "M", [&] {
                        settings.focusDistance =
                            settings.focusDistance >= 45 ? 5 : settings.focusDistance + 5;
                    });
                    row("GLOBAL ILLUM.", name(settings.globalIllumination), [&] {
                        settings.globalIllumination =
                            GlobalIllumination((int(settings.globalIllumination) + 1) % 3);
                    });
                    row("GI QUALITY", name(settings.illuminationQuality),
                        [&] { quality(settings.illuminationQuality); });
                    row("SCREEN REFLECTIONS", name(settings.reflections),
                        [&] { quality(settings.reflections); });
                    row("EDGE FUSION", name(settings.edgeFusion), [&] { quality(settings.edgeFusion); });
                    row("TERRAIN TESSELL.", name(settings.terrainTessellation),
                        [&] { quality(settings.terrainTessellation); });
                    row("TERRAIN BACKEND", name(settings.tessellationBackend), [&] {
                        settings.tessellationBackend =
                            TessellationBackend((int(settings.tessellationBackend) + 1) % 3);
                    });
                    row("WIREFRAME", settings.wireframe ? "on" : "off",
                        [&] { settings.wireframe = !settings.wireframe; });
                } else {
                    row("RENDER SCALE", number(settings.renderScale * 100) + "%", [&] {
                        settings.renderScale =
                            settings.renderScale >= 1.5f ? .5f : settings.renderScale + .25f;
                    });
                    row("BLOOM", name(settings.bloom), [&] { quality(settings.bloom); });
                    row("BLOOM STRENGTH", number(settings.bloomStrength * 100), [&] {
                        settings.bloomStrength =
                            settings.bloomStrength >= .5f ? 0 : settings.bloomStrength + .1f;
                    });
                    row("AUTO EXPOSURE", settings.autoExposure ? "on" : "off",
                        [&] { settings.autoExposure = !settings.autoExposure; });
                    row("EXPOSURE EV", number(settings.exposureCompensation), [&] {
                        settings.exposureCompensation =
                            settings.exposureCompensation >= 3 ? -3 : settings.exposureCompensation + 1;
                    });
                    row("TONE MAPPER", name(settings.toneMapper),
                        [&] { settings.toneMapper = ToneMapper((int(settings.toneMapper) + 1) % 3); });
                    row("CONTRAST", number(settings.contrast * 100), [&] {
                        settings.contrast = settings.contrast >= 1.5f ? .5f : settings.contrast + .25f;
                    });
                    row("SATURATION", number(settings.saturation * 100), [&] {
                        settings.saturation = settings.saturation >= 2 ? 0 : settings.saturation + .25f;
                    });
                    row("TEMPERATURE", number(settings.temperature * 100), [&] {
                        settings.temperature = settings.temperature >= 1 ? -1 : settings.temperature + .25f;
                    });
                    row("VIGNETTE", number(settings.vignette * 100),
                        [&] { settings.vignette = settings.vignette >= 1 ? 0 : settings.vignette + .25f; });
                    row("SHARPENING", number(settings.sharpening * 100), [&] {
                        settings.sharpening = settings.sharpening >= 1 ? 0 : settings.sharpening + .2f;
                    });
                    row("MOTION BLUR", name(settings.motionBlur), [&] { quality(settings.motionBlur); });
                    row("VOLUMETRIC FOG", name(settings.volumetricFog),
                        [&] { quality(settings.volumetricFog); });
                    row("GPU PROFILING", settings.gpuProfiling ? "on" : "off",
                        [&] { settings.gpuProfiling = !settings.gpuProfiling; });
                }
                if (click && mouseX > 850 && mouseX < 1260 && mouseY > 20 && mouseY < 70)
                    page = 1 - page;
                renderer.text(870, 640, "CLICK HEADER FOR NEXT PAGE", 1.25f, {.5f, .65f, .7f, 1});
                renderer.text(870, 660, "CLICK VALUES TO CYCLE", 1.4f, {.5f, .65f, .7f, 1});
                const auto &effective = renderer.settings().effective;
                renderer.text(870, 683,
                              std::string("ACTIVE: ") + name(effective.antiAliasing) + " / " +
                                  name(effective.tessellationBackend),
                              1.2f, {.5f, .65f, .7f, 1});
                if (changed) {
                    auto report = renderer.setSettings(settings);
                    for (auto &m : report.adjustments)
                        std::cout << "Adjusted: " << m << '\n';
                }
            }
            renderer.end();
            renderer.check();
            frame++;
            if (frame == frames && !capture.empty())
                renderer.screenshot(capture);
            platform.present();
            if (!offscreen)
                std::this_thread::sleep_until(now + std::chrono::milliseconds(8));
        }
        if (!writeConfig.empty() && !saveSettings(writeConfig, settings, error))
            throw std::runtime_error(error);
        auto &s = renderer.statistics();
        std::cout << "Frames=" << frame << " draws=" << s.drawCalls << " triangles=" << s.triangles
                  << " post_passes=" << s.postPasses << " terrain_tiles=" << s.visibleTerrainTiles
                  << " msaa=" << s.msaaSamples << " shadow=" << s.shadowResolution
                  << " tessellation=" << (s.hardwareTessellation ? "hardware" : "mesh") << " elapsed_ms="
                  << std::chrono::duration<double, std::milli>(Clock::now() - started).count() << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "JSD graphics lab: " << e.what() << '\n';
        return 1;
    }
}
