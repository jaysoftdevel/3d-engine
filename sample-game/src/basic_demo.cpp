#include <chrono>
#include <iostream>
#include <jsd/engine.hpp>
#include <thread>
using namespace jsd;
namespace {
struct Visual {
    Body body;
    BodyState previous, current;
    Color color;
};
} // namespace
int main(int argc, char **argv) {
    try {
        bool headless = false, noAudio = false;
        int frames = 0, width = 1280, height = 720;
        std::string capture;
        std::filesystem::path assets = "assets";
        auto settings = gfx::GraphicsSettings::high();
        settings.viewDistance = 120;
        settings.gpuProfiling = true;
        settings.fogDensity = .003f;
        auto arg = [&](int &i) {
            if (i + 1 >= argc)
                throw std::runtime_error("missing argument");
            return std::string(argv[++i]);
        };
        for (int i = 1; i < argc; i++) {
            std::string a = argv[i];
            if (a == "--offscreen")
                headless = true;
            else if (a == "--no-audio")
                noAudio = true;
            else if (a == "--frames")
                frames = std::stoi(arg(i));
            else if (a == "--capture")
                capture = arg(i);
            else if (a == "--assets")
                assets = arg(i);
            else if (a == "--width")
                width = std::stoi(arg(i));
            else if (a == "--height")
                height = std::stoi(arg(i));
            else if (a == "--settings") {
                std::string error;
                if (!gfx::loadSettings(arg(i), settings, error))
                    throw std::runtime_error(error);
            } else if (a == "--help") {
                std::cout << "JSD sample game: WASD + mouse, Tab capture, E launch box, Space impulse aimed "
                             "body, F1 HUD, F2 AA, F4 render scale, Esc quit.\n--offscreen --frames N "
                             "--capture FILE.ppm --assets PATH --no-audio --settings FILE\n";
                return 0;
            } else
                throw std::runtime_error("unknown argument " + a);
        }
        if (headless && !frames)
            frames = 8;
        if (width < 320 || height < 180 || frames < 0)
            throw std::invalid_argument("invalid frame options");
        if (!std::filesystem::exists(assets))
            assets = std::filesystem::read_symlink("/proc/self/exe").parent_path().parent_path() / "assets";
        Platform platform(headless, width, height);
        gfx::Renderer renderer(platform, settings);
        PhysicsWorld physics;
        Registry registry;
        Input input;
        FixedStep clock;
        AudioMixer audio;
        std::unique_ptr<AudioDevice> device;
        auto hum = loadWav(assets / "ambience.wav");
        audio.play(hum, {-3, 2, 0}, .45f, true, true);
        if (!headless && !noAudio)
            try {
                device = std::make_unique<AudioDevice>(audio);
            } catch (const std::exception &e) {
                std::cerr << "Audio device unavailable: " << e.what() << '\n';
            }
        auto sky = loadHdr(assets / "sky.hdr");
        auto environment = renderer.createEnvironment(sky.width, sky.height, sky.rgb);
        GpuScene model(renderer, loadGltf(assets / "animated-column.gltf"));
        auto color = loadImage(assets / "base-color.png"), normal = loadImage(assets / "normal.png"),
             orm = loadImage(assets / "orm.png");
        auto baseTexture = renderer.createTexture(color.width, color.height, color.rgba),
             normalTexture = renderer.createTexture(normal.width, normal.height, normal.rgba, false),
             ormTexture = renderer.createTexture(orm.width, orm.height, orm.rgba, false);
        auto box = renderer.createMesh(gfx::makeBox());
        BodySettings floor;
        floor.dynamic = false;
        floor.position = {0, -.5f, 4};
        floor.halfExtent = {18, .5f, 18};
        physics.box(floor);
        auto spawn = [&](V3 position, V3 velocity = {}) {
            BodySettings body;
            body.position = position;
            body.halfExtent = {.4f, .4f, .4f};
            body.continuous = true;
            auto id = physics.box(body);
            physics.velocity(id, velocity);
            auto entity = registry.create();
            auto state = physics.state(id);
            registry.emplace<Visual>(
                entity, Visual{id, state, state, {.3f + rf::random01(id.value) * .6f, .35f, .7f, 1}});
        };
        for (int i = 0; i < 18; i++)
            spawn({float(i % 6) * 1.1f - 2.8f, 1.0f + (i / 6) * 1.1f, 3 + float(i % 2) * .1f});
        ParticleSettings emitter;
        emitter.origin = {-3, 1, -2};
        emitter.color = {.12f, .65f, 1, 1};
        emitter.rate = 28;
        ParticleEmitter particles(emitter);
        input.bind("forward", 26, 1);
        input.bind("forward", 22, -1);
        input.bind("side", 7, 1);
        input.bind("side", 4, -1);
        V3 eye{12, 5, -15};
        float yaw = -.58f, pitch = -.12f, seconds = 0;
        bool running = true, captured = !headless, fire = false, kick = false, showHud = true;
        platform.captureMouse(captured);
        int frame = 0;
        auto previous = std::chrono::steady_clock::now();
        while (running && (!frames || frame < frames)) {
            auto now = std::chrono::steady_clock::now();
            float dt =
                headless ? 1.f / 60 : std::min(.1f, std::chrono::duration<float>(now - previous).count());
            previous = now;
            input.beginFrame();
            fire = kick = false;
            if (!headless) {
                SDL_Event event;
                while (SDL_PollEvent(&event)) {
                    input.event(event);
                    if (event.type == vl::sdl::quit)
                        running = false;
                    if (event.type == vl::sdl::mouseDown && event.button.button == 1)
                        fire = true;
                    if (event.type == vl::sdl::window && event.window.event == 13) {
                        captured = false;
                        platform.captureMouse(false);
                    }
                }
                if (input.pressed(41))
                    running = false;
                if (input.pressed(43)) {
                    captured = !captured;
                    platform.captureMouse(captured);
                }
                if (input.pressed(58))
                    showHud = !showHud;
                if (input.pressed(59)) {
                    settings.antiAliasing = gfx::AntiAliasing((int(settings.antiAliasing) + 1) % 5);
                    renderer.setSettings(settings);
                }
                if (input.pressed(61)) {
                    settings.renderScale = settings.renderScale < 1 ? 1 : .67f;
                    renderer.setSettings(settings);
                }
                fire = fire || input.pressed(8);
                kick = input.pressed(44);
            }
            if (captured) {
                auto [dx, dy] = input.mouseDelta();
                yaw -= dx * .0025f;
                pitch = rf::clamp(pitch - dy * .0025f, -1.4f, 1.4f);
            }
            yaw += (input.down(80) - input.down(79)) * dt;
            pitch = rf::clamp(pitch + (input.down(82) - input.down(81)) * dt, -1.4f, 1.4f);
            eye += rf::forward(yaw) * input.axis("forward") * dt * 7;
            eye += rf::rotate(rf::forward(yaw), -rf::pi / 2) * input.axis("side") * dt * 7;
            if (fire)
                spawn(eye + rf::forward(yaw, pitch) * 2, rf::forward(yaw, pitch) * 18);
            if (kick)
                if (auto hit = physics.rayCast(eye, eye + rf::forward(yaw, pitch) * 80))
                    try {
                        physics.impulse(hit->body, rf::forward(yaw, pitch) * 30);
                    } catch (...) {
                    }
            clock.advance(dt, [&](float step) {
                registry.each<Visual>([](Entity, Visual &v) { v.previous = v.current; });
                physics.step(step, 2);
                (void)physics.contacts();
                registry.each<Visual>([&](Entity, Visual &v) { v.current = physics.state(v.body); });
                particles.update(step);
                seconds += step;
            });
            gfx::Environment env;
            env.image = environment;
            env.deltaSeconds = dt;
            env.ambientIntensity = .7f;
            renderer.begin({eye, rf::forward(yaw, pitch), 70, .08f}, env);
            gfx::Terrain terrain;
            terrain.height = [](float x, float z) { return -1.f + rf::terrainHeight(x, z); };
            renderer.terrain(terrain);
            gfx::Material floorMaterial;
            floorMaterial.albedo = {.5f, .58f, .65f, 1};
            floorMaterial.roughness = .22f;
            floorMaterial.metallic = .55f;
            floorMaterial.albedoTexture = baseTexture;
            floorMaterial.normalTexture = normalTexture;
            floorMaterial.ormTexture = ormTexture;
            renderer.primitive(gfx::Primitive::Box, {{0, -.12f, 4}, {36, .2f, 36}, 0}, floorMaterial);
            registry.each<Visual>([&](Entity entity, Visual &v) {
                float a = clock.alpha();
                gfx::Draw draw;
                draw.mesh = box;
                draw.objectId = entity.value() + 1;
                draw.material.albedo = v.color;
                draw.material.roughness = .25f;
                draw.material.metallic = .2f;
                draw.modelMatrix =
                    rf::compose(rf::mix(v.previous.position, v.current.position, a),
                                rf::slerp(v.previous.rotation, v.current.rotation, a), {.8f, .8f, .8f});
                renderer.submit(draw);
            });
            model.draw(seconds, 0, rf::compose({-5, .1f, 2}), 100000);
            gfx::Material glass;
            glass.albedo = {.2f, .7f, .9f, .25f};
            glass.roughness = .1f;
            glass.alphaMode = gfx::AlphaMode::Blend;
            glass.castShadow = false;
            renderer.primitive(gfx::Primitive::Box, {{5, 1.5f, 2}, {2, 3, .12f}, .2f}, glass);
            gfx::Material glow;
            glow.albedo = {.1f, .5f, 1, 1};
            glow.emissive = {1, 8, 20, 1};
            renderer.primitive(gfx::Primitive::Cylinder, {{-3, 1, -2}, {.18f, 2, .18f}, 0}, glow);
            particles.draw(renderer);
            gfx::Light light;
            light.position = {-3, 2, -2};
            light.color = {.1f, .5f, 1, 1};
            light.intensity = 45;
            light.radius = 12;
            renderer.light(light);
            light.position = {5, 4, 5};
            light.color = {1, .3f, .08f, 1};
            light.intensity = 70;
            renderer.light(light);
            if (showHud) {
                renderer.rect(18, 18, 730, 100, {.02f, .04f, .065f, .92f});
                renderer.text(34, 35, "JSD GAME ENGINE / SIBLING GAME", 2.8f, {.3f, .9f, .8f, 1});
                renderer.text(34, 74, "WASD / MOUSE  E LAUNCH BOX  SPACE IMPULSE  TAB CURSOR", 1.5f);
                renderer.text(34, 97, "F2 AA  F4 SCALE  F1 HUD  ESC QUIT", 1.5f);
                renderer.rect(18, 630, 860, 68, {.02f, .04f, .065f, .92f});
                renderer.text(34, 644, "JOLT PHYSICS / GLTF SKINNING / PBR / IBL / LIGHTS / PARTICLES", 1.5f);
                renderer.text(34, 671,
                              std::string(gfx::name(settings.antiAliasing)) + "  RENDER " +
                                  std::to_string(int(settings.renderScale * 100)) + "%  BODIES " +
                                  std::to_string(registry.size()),
                              1.7f, {.3f, .9f, .8f, 1});
            }
            renderer.end();
            renderer.check();
            audio.setListener(eye, rf::rotate(rf::forward(yaw), -rf::pi / 2));
            frame++;
            if (frame == frames && !capture.empty())
                renderer.screenshot(capture);
            platform.present();
            if (!headless)
                std::this_thread::sleep_until(now + std::chrono::milliseconds(8));
        }
        auto stats = renderer.statistics();
        std::cout << "Sample game used JSD " << jsd_game_engine_version() << " / " << PhysicsWorld::backend()
                  << " / frames=" << frame << " bodies=" << registry.size() << " lights=" << stats.localLights
                  << " render=" << stats.renderWidth << 'x' << stats.renderHeight
                  << " output=" << stats.outputWidth << 'x' << stats.outputHeight << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Sample game: " << e.what() << '\n';
        return 1;
    }
}
