#pragma once
#include "sdl.hpp"
#include <string>
#include <filesystem>

namespace rf {
std::filesystem::path executableDirectory();
class Platform {
    void *sdlContext = nullptr;
    void *eglLib = nullptr;
    void *glLib = nullptr;
    void *display = nullptr;
    void *surface = nullptr;
    void *context = nullptr;

  public:
    SDL_Window *window = nullptr;
    bool offscreen = false;
    int width = 1280, height = 720;
    Platform(bool headless = false, int w = 1280, int h = 720);
    ~Platform();
    Platform(const Platform &) = delete;
    Platform &operator=(const Platform &) = delete;
    void *resolve(const char *name) const;
    void present();
    void captureMouse(bool capture);
    void dimensions();
    // Borderless desktop fullscreen; existing constructors retain windowed behavior.
    bool setFullscreen(bool enabled);
    bool isFullscreen() const;
    bool setVSync(bool enabled);
};
} // namespace rf
