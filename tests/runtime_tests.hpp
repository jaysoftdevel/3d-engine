#pragma once
#include "process_id.hpp"
#include <fstream>
#include <jsd/engine.hpp>
inline void runtimeTests(const std::function<void(bool, const char *)> &test) {
    using namespace jsd;
    Registry registry;
    auto first = registry.create();
    registry.emplace<int>(first, 42);
    test(*registry.get<int>(first) == 42, "ECS stores typed components");
    registry.destroy(first);
    auto second = registry.create();
    test(!registry.valid(first) && second.index == first.index && second.generation != first.generation,
         "ECS generations reject stale entity handles");
    registry.emplace<int>(second, 7);
    registry.each<int>([&](Entity e, int &) { registry.destroy(e); });
    test(registry.size() == 0, "ECS iteration tolerates entity removal");
    SceneGraph scene;
    int parent = scene.add("parent", rf::compose({2, 0, 0})),
        child = scene.add("child", rf::compose({0, 3, 0}), parent);
    scene.update();
    test(rf::distance(rf::transformPoint(scene.get(child).world, {}), {2, 3, 0}) < .001f,
         "scene graph composes parent transforms");
    bool failed = false;
    try {
        scene.reparent(parent, child);
    } catch (const std::invalid_argument &) {
        failed = true;
    }
    test(failed, "scene graph rejects hierarchy cycles");
    FrameGraph graph;
    std::vector<int> order;
    graph.add("simulate", {}, {"world"}, [&] { order.push_back(1); });
    graph.add("render", {"world"}, {"frame"}, [&] { order.push_back(2); });
    graph.execute();
    test(order == std::vector<int>{1, 2} && graph.profile().size() == 2,
         "frame graph orders hazards and profiles passes");
    FrameGraph cycle;
    cycle.add("A", {}, {}, [] {}, {"B"});
    cycle.add("B", {}, {}, [] {}, {"A"});
    failed = false;
    try {
        cycle.compile();
    } catch (...) {
        failed = true;
    }
    test(failed, "frame graph detects dependency cycles");
    FixedStep fixed;
    int steps = 0;
    fixed.advance(.05, [&](float) { steps++; });
    test(steps == 3, "fixed-step runtime decouples simulation from presentation");
    Input input;
    input.bind("forward", 26);
    SDL_Event key{};
    key.type = vl::sdl::keyDown;
    key.key.keysym.scancode = 26;
    input.event(key);
    test(input.pressed(26) && input.axis("forward") == 1, "input maps actions and detects key edges");
    input.beginFrame();
    test(!input.pressed(26) && input.down(26), "input retains held state across frames");
    LodSelector lod;
    std::array<LodLevel, 2> levels{{{{1}, 20}, {{2}, 100}}};
    test(lod.select(1, 10, levels).value == 1 && lod.select(1, 21, levels).value == 1 &&
             lod.select(1, 25, levels).value == 2,
         "LOD selection includes boundary hysteresis");
    ParticleSettings ps;
    ps.rate = 20;
    ps.lifetime = 1;
    ps.capacity = 64;
    ParticleEmitter a(ps), b(ps);
    a.update(.5f);
    b.update(.5f);
    test(a.active().size() == 10 && a.active()[0].velocity.x == b.active()[0].velocity.x,
         "particle emitters are seeded and bounded");
    a.update(1);
    test(a.active().size() <= 64, "particle lifetimes retire old particles");
    AudioMixer mixer;
    auto clip = std::make_shared<AudioClip>();
    clip->samples.assign(4800, 1);
    auto sound = mixer.play(clip, {-10, 0, 0});
    std::array<float, 128> pcm{};
    mixer.mix(pcm);
    test(pcm[0] > pcm[1] && pcm[0] > 0, "spatial audio applies stereo panning and distance attenuation");
    mixer.stop(sound);
    mixer.mix(pcm);
    test(std::all_of(pcm.begin(), pcm.end(), [](float x) { return x == 0; }),
         "stopped audio voices produce silence");
    SceneAsset asset;
    asset.nodes.resize(1);
    AnimationClip animation;
    animation.duration = 1;
    AnimationTrack track;
    track.node = 0;
    track.property = TrackProperty::Translation;
    track.interpolation = Interpolation::Linear;
    track.times = {0, 1};
    track.values = {std::array<float, 4>{0, 0, 0, 1}, std::array<float, 4>{2, 0, 0, 1}};
    animation.tracks.push_back(track);
    asset.animations.push_back(animation);
    auto pose = sampleAnimation(asset, 0, .5f, false);
    test(std::abs(pose[0].m[12] - 1) < .001f, "animation samples linear translation tracks");
    MeshAsset mesh;
    mesh.vertices.push_back({{0, 0, 0}, {0, 1, 0}, {}, 0, 0});
    mesh.weights.push_back({{0, 1, 0, 0}, {.5f, .5f, 0, 0}});
    std::array<Mat4, 2> palette{Mat4::identity(), rf::compose({2, 0, 0})};
    test(std::abs(skinMesh(mesh, palette)[0].p.x - 1) < .001f,
         "skeletal skinning blends weighted joint matrices");
    PhysicsWorld physics;
    BodySettings ground;
    ground.dynamic = false;
    ground.position = {0, -1, 0};
    ground.halfExtent = {10, 1, 10};
    physics.box(ground);
    BodySettings box;
    box.position = {0, 4, 0};
    auto body = physics.box(box);
    for (int i = 0; i < 180; i++)
        physics.step(1.f / 60);
    auto state = physics.state(body);
    test(state.position.y > .45f && state.position.y < .6f,
         "Jolt rigid body rests on static collision geometry");
    test(!physics.contacts().empty(), "Jolt contact listener delivers collision events");
    auto hit = physics.rayCast({0, 5, 0}, {0, -5, 0});
    test(hit && hit->body == body, "physics ray casts identify actual bodies");
    physics.impulse(body, {0, 3, 0});
    physics.step(1.f / 60);
    test(physics.state(body).velocity.y > 0, "physics impulses wake and accelerate bodies");
    physics.remove(body);
    failed = false;
    try {
        physics.state(body);
    } catch (const std::invalid_argument &) {
        failed = true;
    }
    test(failed, "physics rejects retired body handles");
    BodySettings wall;
    wall.dynamic = false;
    wall.position = {0, 2, 0};
    wall.halfExtent = {5, 2, .05f};
    physics.box(wall);
    BodySettings bullet;
    bullet.position = {0, 2, -5};
    bullet.continuous = true;
    auto fast = physics.sphere(bullet, .1f);
    physics.velocity(fast, {0, 0, 900});
    physics.step(1.f / 60);
    test(physics.state(fast).position.z < 1, "continuous collision prevents fast-body tunneling");
    auto prefix = rf::executableDirectory().parent_path();
    auto assetPath = prefix / "share/jsd-game-engine/test-fixtures";
    if (!std::filesystem::exists(assetPath / "animated-column.gltf"))
        assetPath = JSD_TEST_FIXTURE_DIR;
    if (!std::filesystem::exists(assetPath / "animated-column.gltf"))
        throw std::runtime_error("Engine test fixtures are missing; rebuild/install the engine test target");
    {
        auto image = loadImage(assetPath / "base-color.png");
        test(image.width == 256 && image.rgba.size() == 256 * 256 * 4,
             "PNG asset decoder reads engine-owned fixture texture");
        auto hdr = loadHdr(assetPath / "sky.hdr");
        test(hdr.width == 128 && *std::max_element(hdr.rgb.begin(), hdr.rgb.end()) > 1,
             "Radiance HDR asset preserves values above one");
        auto wav = loadWav(assetPath / "ambience.wav");
        test(wav->sampleRate == 48000 && !wav->samples.empty(),
             "WAV asset decoder loads sample-rate metadata and PCM");
        auto gltf = loadGltf(assetPath / "animated-column.gltf");
        test(gltf.meshes.size() == 1 && gltf.skins.size() == 1 && gltf.animations.size() == 1,
             "glTF loader imports actual images, skin and animation");
        auto unicodeLeaf = std::filesystem::path(u8"jsd-assets-ä-月-");
        unicodeLeaf += std::to_string(jsdTestProcessId());
        auto unicodeRoot = std::filesystem::temp_directory_path() / unicodeLeaf;
        std::filesystem::create_directories(unicodeRoot);
        struct CleanupAssets { std::filesystem::path p; ~CleanupAssets() {
            std::error_code error; std::filesystem::remove_all(p, error);
        } } cleanup{unicodeRoot};
        for (const auto &entry : std::filesystem::directory_iterator(assetPath))
            if (entry.is_regular_file()) std::filesystem::copy_file(entry.path(), unicodeRoot / entry.path().filename(),
                std::filesystem::copy_options::overwrite_existing);
        auto unicodeGltf = loadGltf(unicodeRoot / "animated-column.gltf");
        test(unicodeGltf.meshes.size() == gltf.meshes.size() && unicodeGltf.images.size() == gltf.images.size(),
             "glTF and external resources load from Unicode directories");
        auto p0 = sampleAnimation(gltf, 0, 0, false), p1 = sampleAnimation(gltf, 0, 1, false);
        bool animatedJoint=false;for(size_t i=0;i<p0.size();i++)animatedJoint=animatedJoint||p0[i].m!=p1[i].m;
        test(animatedJoint, "loaded quaternion animation changes joint pose");
        failed = false;
        try {
            loadGltf(assetPath / "animated-column.gltf", assetPath / "restricted");
        } catch (const std::runtime_error &) {
            failed = true;
        }
        test(failed, "asset loader enforces allowed resource root");
    }
    test(std::string(jsd_game_engine_version()) == jsd::Version, "JSD engine exports its standalone version API");
}
