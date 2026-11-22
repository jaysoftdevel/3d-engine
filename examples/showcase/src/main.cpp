#include "showcase.hpp"
#include "graphics_presets.hpp"
#include "../../graphics_settings.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>
using namespace jsd;
int main(int argc, char **argv) {
  try {
    bool validate = false;
    bool particles = true;
    bool fullscreen = true, vsync = true, showFps = true;
    bool offscreen = false, noAudio = false, hud = true, freeCamera = false,
         paused = false;
    int frames = 0, width = 1920, height = 1080;
    float start = 0, fps = 60,frameLimit=0,explicitScale=1;
    bool cinematicDof=false,enableMotionBlur=false;
    std::string debugDir, profile;
    std::string capture, sequence, quality = "high";
    std::filesystem::path assets,exportProject;
    std::string config, writeConfig;
    auto value = [&](int &i) {
      if (i + 1 >= argc)
        throw std::runtime_error("missing option value");
      return std::string(argv[++i]);
    };
    for (int i = 1; i < argc; i++) {
      std::string a = argv[i];
      if (a == "--export-project")exportProject=value(i);
      else if (a == "--validate")
        validate = true;
      else if (a == "--windowed")
        fullscreen = false;
      else if (a == "--fullscreen")
        fullscreen = true;
      else if (a == "--vsync")
        vsync = true;
      else if (a == "--no-vsync")
        vsync = false;
      else if (a == "--no-fps")
        showFps = false;
      else if (a == "--profile")
        profile = value(i);
      else if (a == "--debug-dir")
        debugDir = value(i);
      else if (a == "--no-particles")
        particles = false;
      else if(a=="--cinematic-dof")cinematicDof=true;
      else if(a=="--motion-blur")enableMotionBlur=true;
      else if(a=="--render-scale")explicitScale=std::stof(value(i));
      else if(a=="--frame-limit")frameLimit=std::stof(value(i));
      else if (a == "--offscreen")
        offscreen = true;
      else if (a == "--no-audio")
        noAudio = true;
      else if (a == "--no-ui")
        hud = showFps = false;
      else if (a == "--free-camera")
        freeCamera = true;
      else if (a == "--autoplay") {
      } else if (a == "--frames")
        frames = std::stoi(value(i));
      else if (a == "--width")
        width = std::stoi(value(i));
      else if (a == "--height")
        height = std::stoi(value(i));
      else if (a == "--time")
        start = std::stof(value(i));
      else if (a == "--fps")
        fps = std::stof(value(i));
      else if (a == "--capture")
        capture = value(i);
      else if (a == "--sequence")
        sequence = value(i);
      else if (a == "--quality" || a == "--preset")
        quality = value(i);
      else if (a == "--assets")
        assets = value(i);
      else if (a == "--settings")
        config = value(i);
      else if (a == "--write-settings")
        writeConfig = value(i);
      else if (a == "--help") {
        std::cout << "SUNDOG / Recovery at Kestrel Relay — native JSD 0.5.12 "
                     "showcase\n160-second "
                     "autonomous recovery mission; loops automatically.\nSPACE "
                     "pause; TAB explore "
                     "camera; WASD/mouse fly; C graphics settings; R restart; F1 HUD; ESC "
                     "exit.\n--offscreen --frames N "
                     "--time SECONDS --fps N --width N --height N\n--capture "
                     "FILE.ppm --sequence "
                     "DIRECTORY --quality minimal|low|medium|high|max\n--settings FILE.ini "
                     "--write-settings FILE.ini --assets PATH --no-ui "
                     "--no-audio --free-camera\n--no-particles --validate "
                     "--debug-dir DIRECTORY\n"
                     "--fullscreen (default) --windowed --vsync (default) --no-vsync\n"
                     "--frame-limit FPS --render-scale SCALE (default 1) --cinematic-dof --motion-blur\n"
                     "F11 toggles fullscreen; --no-fps hides measured FPS\n"
                     "--profile FILE.csv records frame, CPU submission and GPU "
                     "times\n";
        return 0;
      } else
        throw std::runtime_error("unknown option: " + a);
    }
    gfx::GraphicsPreset preset;
    if (!gfx::parseGraphicsPreset(quality, preset))
      throw std::invalid_argument("unknown quality preset");
    if (width < 320 || height < 180 || width > 3840 || height > 2160 ||
        frames < 0 || !std::isfinite(start + fps) || start < 0 || fps < 1 ||
        fps > 240 || !std::isfinite(frameLimit+explicitScale)||frameLimit<0||frameLimit>360||explicitScale<.5f||explicitScale>2)
      throw std::invalid_argument("invalid capture options");
    if (offscreen && frames == 0)
      frames = 1;
    if (assets.empty()) {
      const auto prefix = rf::executableDirectory().parent_path();
      assets = prefix / JSD_SHOWCASE_ASSET_SUBDIR;
      if (!std::filesystem::exists(assets / "showcase"))
        assets = JSD_SHOWCASE_ASSET_DIR;
    }
    if (!std::filesystem::exists(assets / "showcase"))
      throw std::runtime_error("Showcase assets not found; use --assets PATH");
    auto settings = sundog::graphicsPreset(preset);
    if (!config.empty()) {
      std::string error;
      if (!gfx::loadSettings(config, settings, error))
        throw std::runtime_error(error);
    }
    // Sharp presentation is the showcase default, including when an older
    // saved config contains an aggressive Max lens or a low render scale.
    settings.depthOfField=cinematicDof?gfx::Quality::Medium:gfx::Quality::Off;
    settings.motionBlur=enableMotionBlur?gfx::Quality::Low:gfx::Quality::Off;
    settings.aperture=.35f;settings.maximumBlur=2.5f;settings.focusAdaptation=2.5f;
    settings.temporalWeight=std::min(settings.temporalWeight,.76f);settings.renderScale=explicitScale;
    Platform platform(offscreen, width, height);
    if (!offscreen) {
      if (fullscreen && !platform.setFullscreen(true))
        throw std::runtime_error(std::string("fullscreen unavailable: ") +
                                 SDL_GetError());
      if (!platform.setVSync(vsync)){
        std::cerr << "Swap interval request was not accepted by the driver: " << SDL_GetError() << '\n';
        if(vsync&&frameLimit==0)frameLimit=60;
      }
    }
    gfx::Renderer renderer(platform, settings);
    std::cout << "Renderer: " << renderer.capabilities().renderer << " / "
              << (offscreen                 ? "offscreen"
                  : platform.isFullscreen() ? "fullscreen"
                                            : "windowed")
              << " / " << (vsync ? "VSync requested" : "uncapped requested")
              << '\n';
    if(!exportProject.empty()){
      jsd::project::ProjectCapture capture(exportProject);renderer.observe(&capture);
      {sundog::Showcase scene(renderer,assets,&capture);scene.projectMetadata();unsigned count=unsigned(scene.Duration*120);
       for(unsigned n=0;n<count;n++){if(n)scene.update(1.f/120);float t=n/120.f;capture.frame(t);scene.render(scene.camera(),false,false,-1,{},1.f/120);scene.projectSample(t);capture.endFrame();if(n%960==0)std::cout<<"Project capture "<<t<<" s\n";}
       capture.frame(scene.Duration);scene.render(scene.camera(),false,false,-1,{},1.f/120);scene.projectSample(scene.Duration);capture.endFrame();capture.finish();}
      renderer.observe(nullptr);std::cout<<"Exported complete project: "<<exportProject<<'\n';return 0;
    }
    sundog::Showcase scene(renderer, assets);
    AudioMixer audio;
    std::unique_ptr<AudioDevice> device;
    if (!offscreen && !noAudio)
      try {
        auto hum = loadWav(assets / "ambience.wav");
        audio.play(hum, {6, 1.5f, 12}, .35f, true, true);
        device = std::make_unique<AudioDevice>(audio);
      } catch (const std::exception &e) {
        std::cerr << "Audio: " << e.what() << '\n';
      }
    Input input;
    input.bind("forward", 26);
    input.bind("forward", 22, -1);
    input.bind("side", 7);
    input.bind("side", 4, -1);
    FixedStep fixed(1. / 120, 32);
    for (int n = 0; n < int(std::fmod(start, sundog::Showcase::Duration) * 120);
         n++)
      scene.update(1.f / 120);
    auto camera = scene.camera();
    float yaw = std::atan2(camera.direction.x, camera.direction.z),
          pitch = std::asin(camera.direction.y);
    bool running = true, captured = false, showGraphicsSettings = false;
    rf::examples::GraphicsSettingsOverlay graphicsOverlay;
    int mouseX = 0, mouseY = 0;
    int frame = 0;
    V3 flightVelocity{};
    FrameRateCounter frameRate;
    std::ofstream profileFile;
    if (!profile.empty()) {
      profileFile.open(profile);
      if (!profileFile)
        throw std::runtime_error("cannot open frame profile");
      profileFile
          << "frame,frame_ms,cpu_submission_ms,gpu_ms,draws,triangles,prepare_ms,workers,probe_faces,light_budget,shadow_cascades,pcg_candidates,lod_draws,lod_triangles_saved,fog_volumes,probe_filters,shadowed_spots,spot_shadow_updates\n";
    }
    auto previous = std::chrono::steady_clock::now(), begin = previous;
    if (!sequence.empty())
      std::filesystem::create_directories(sequence);
    auto nextPresent=begin;
    while (running && (!frames || frame < frames)) {
      auto now = std::chrono::steady_clock::now();
      if (frame > 0)
        frameRate.sample(std::chrono::duration<double>(now - previous).count());
      float dt =
          offscreen
              ? 1 / fps
              : std::min(.25f,
                         std::chrono::duration<float>(now - previous).count());
      previous = now;
      input.beginFrame();
      bool settingsClick = false, settingsToggled = false;
      if (!offscreen) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
          input.event(event);
          if (event.type == vl::sdl::mouseMotion) {
            mouseX = event.motion.x;
            mouseY = event.motion.y;
          }
          if (event.type == vl::sdl::mouseDown && event.button.button == 1) {
            mouseX = event.button.x;
            mouseY = event.button.y;
            settingsClick = true;
          }
          if (event.type == vl::sdl::quit)
            running = false;
          if (event.type == vl::sdl::window &&
              event.window.event == vl::sdl::focusLost) {
            captured = false;
            platform.captureMouse(false);
            settingsClick = false;
          }
        }
        if (input.pressed(41))
          running = false;
        if (input.pressed(44))
          paused = !paused;
        if (input.pressed(58))
          hud = !hud;
        if (input.pressed(vl::sdl::key::c)) {
          showGraphicsSettings = !showGraphicsSettings;
          settingsToggled = true;
          settingsClick = false;
          captured = freeCamera && !showGraphicsSettings &&
                     (SDL_GetWindowFlags(platform.window) & vl::sdl::inputFocus);
          platform.captureMouse(captured);
          SDL_GetMouseState(&mouseX, &mouseY);
        }
        if (input.pressed(vl::sdl::key::f11)) {
          if (!platform.setFullscreen(!platform.isFullscreen()))
            std::cerr << "Fullscreen change failed: " << SDL_GetError() << '\n';
          renderer.resetHistory();
        }
        if (input.pressed(21)){
          scene.reset();fixed=FixedStep(1./120,32);camera=scene.camera();flightVelocity={};
          yaw=std::atan2(camera.direction.x,camera.direction.z);pitch=std::asin(camera.direction.y);
        }
        if (input.pressed(43)) {
          freeCamera = !freeCamera;
          flightVelocity={};
          captured = freeCamera && !showGraphicsSettings;
          platform.captureMouse(captured);
          camera = scene.camera();
          yaw = std::atan2(camera.direction.x, camera.direction.z);
          pitch = std::asin(camera.direction.y);
        }
      }
      if (!paused)
        fixed.advance(dt, [&](float h) { scene.update(h); });
      if (!freeCamera)
        {auto desired=scene.viewAt(scene.time()+fixed.alpha()/120.f);desired.position=scene.moveCamera(camera.position,desired.position);camera=desired;}
      else if (!showGraphicsSettings) {
        auto [dx, dy] = input.mouseDelta();
        if (captured && !settingsToggled) {
          yaw -= dx * .002;
          pitch = rf::clamp(pitch - dy * .002f, -1.45f, 1.45f);
        }
        camera.direction = rf::forward(yaw, pitch);
        V3 wanted=camera.direction*input.axis("forward")*8+rf::rotate(rf::forward(yaw),-rf::pi/2)*input.axis("side")*8;
        if(input.down(44))wanted.y+=6;if(input.down(224))wanted.y-=6;
        flightVelocity=rf::mix(flightVelocity,wanted,1-std::exp(-12*dt));
        camera.position=scene.moveCamera(camera.position,camera.position+flightVelocity*dt);
      }
      platform.dimensions();
      scene.render(camera, hud, particles, showFps ? frameRate.fps() : -1, [&] {
        if (showGraphicsSettings) {
          int windowWidth = width, windowHeight = height;
          if (!offscreen)
            SDL_GetWindowSize(platform.window, &windowWidth, &windowHeight);
          graphicsOverlay.draw(renderer, settings, settingsClick,
              mouseX * 1280.f / std::max(1, windowWidth),
              mouseY * 720.f / std::max(1, windowHeight), true, sundog::graphicsPreset);
        }
      }, dt,float(platform.width)/std::max(1,platform.height),fixed.alpha()/120.f);
      if (validate) {
        renderer.check();
        renderer.validateFrame();
      }
      audio.setListener(camera.position,
                        rf::normalized(rf::cross(camera.direction, {0, 1, 0})));
      frame++;
      if (!sequence.empty()) {
        std::ostringstream name;
        name << sequence << "/frame-" << std::setw(5) << std::setfill('0')
             << frame << ".ppm";
        renderer.screenshot(name.str());
      }
      if (frame == frames && !capture.empty())
        renderer.screenshot(capture);
      if (frame == frames && !debugDir.empty())
        renderer.captureBuffers(debugDir);
      platform.present();
      if(!offscreen&&frameLimit>0){
        auto interval=std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(1./frameLimit));
        nextPresent+=interval;auto ready=std::chrono::steady_clock::now();
        if(nextPresent>ready)std::this_thread::sleep_until(nextPresent);else nextPresent=ready;
      }
      double frameSeconds =
          std::chrono::duration<double>(std::chrono::steady_clock::now() - now)
              .count();
      if (profileFile.is_open()) {
        const auto &s = renderer.statistics();
        profileFile << frame << ',' << frameSeconds * 1000 << ','
                    << s.cpuMilliseconds << ',' << s.gpuMilliseconds << ','
                    << s.drawCalls << ',' << s.triangles << ',' << s.preparationMilliseconds
                    << ',' << s.preparationWorkers << ',' << s.reflectionProbeFaces << ',' << s.stochasticLightSamples << ',' << s.shadowCascades << ',' << s.proceduralInstances << ',' << s.lodInstances << ',' << s.lodTrianglesSaved << ',' << s.fogVolumes << ',' << s.reflectionProbeFilters << ',' << s.localShadowMaps << ',' << s.localShadowUpdates << '\n';
      }
      if (offscreen && (frame % 30 == 0 || frame == frames)) {
        auto &s = renderer.statistics();
        std::cout << "Frame " << frame << " t=" << scene.time() << " / "
                  << s.triangles << " triangles / " << s.drawCalls
                  << " draws / " << s.particleSprites << " sprites / "
                  << scene.activeParticles() << " live particles\n"
                  << std::flush;
      }
    }
    if (!writeConfig.empty()) {
      std::string error;
      if (!gfx::saveSettings(writeConfig, settings, error)) throw std::runtime_error(error);
    }
    auto &s = renderer.statistics();
    std::cout << "SUNDOG / JSD " << jsd_game_engine_version() << " / "
              << renderer.capabilities().renderer << " / frames=" << frame
              << " time=" << scene.time() << " render=" << s.renderWidth << 'x'
              << s.renderHeight << " particles=" << scene.activeParticles()
              << " elapsed="
              << std::chrono::duration<double>(
                     std::chrono::steady_clock::now() - begin)
                     .count()
              << "s\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "SUNDOG: " << e.what() << '\n';
    return 1;
  }
}
