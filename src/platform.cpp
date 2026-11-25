#include "jsd/filesystem.hpp"
#include "frontier/platform.hpp"
#include "gl.hpp"
#ifndef _WIN32
#include <dlfcn.h>
#endif
#include <filesystem>
#include <stdexcept>

namespace vl {
void GL::load(const std::function<void *(const char *)> &resolve) {
#define RF_LOAD(result, name, args)                                                                          \
    name = reinterpret_cast<decltype(name)>(resolve("gl" #name));                                            \
    if (!name)                                                                                               \
        throw std::runtime_error("Missing OpenGL function: gl" #name);
    VL_GL_FUNCTIONS(RF_LOAD)
    JSD_GL_FUNCTIONS(RF_LOAD)
#undef RF_LOAD
    BlendFuncSeparate = reinterpret_cast<decltype(BlendFuncSeparate)>(resolve("glBlendFuncSeparate"));
    if(!BlendFuncSeparate)throw std::runtime_error("Missing OpenGL function: glBlendFuncSeparate");
    ProgramParameteri = reinterpret_cast<decltype(ProgramParameteri)>(resolve("glProgramParameteri"));
    GetProgramBinary = reinterpret_cast<decltype(GetProgramBinary)>(resolve("glGetProgramBinary"));
    ProgramBinary = reinterpret_cast<decltype(ProgramBinary)>(resolve("glProgramBinary"));
    PatchParameteri = reinterpret_cast<decltype(PatchParameteri)>(resolve("glPatchParameteri"));
}
} // namespace vl
namespace rf {
namespace {
#ifndef _WIN32
template <class F> F symbol(void *lib, const char *name) {
    auto p = reinterpret_cast<F>(dlsym(lib, name));
    if (!p)
        throw std::runtime_error(std::string("Missing platform entry: ") + name);
    return p;
}
#endif
} // namespace
std::filesystem::path executableDirectory() {
    char *base = SDL_GetBasePath();
    if (!base) throw std::runtime_error(std::string("Cannot locate executable: ") + SDL_GetError());
    std::filesystem::path result=jsd::utf8_path(base);
    SDL_free(base);
    return result;
}
Platform::Platform(bool headless, int w, int h, bool fullscreen, const std::string& title) : offscreen(headless), width(w), height(h) {
    if (!headless
#ifdef _WIN32
        || headless
#endif
    ) {
        SDL_SetMainReady();
        if (SDL_Init(vl::sdl::video) < 0)
            throw std::runtime_error(SDL_GetError());
        SDL_GL_SetAttribute(5, 1);
        SDL_GL_SetAttribute(6, 24);
        SDL_GL_SetAttribute(17, 3);
        SDL_GL_SetAttribute(18, 3);
        SDL_GL_SetAttribute(21, 1);
        window = SDL_CreateWindow(title.c_str(), vl::sdl::centered, vl::sdl::centered, w, h,
                                  vl::sdl::openGL | vl::sdl::highDPI |
                                  (headless ? vl::sdl::hidden : fullscreen ? vl::sdl::fullscreen : vl::sdl::resizable));
        if (!window) {
            SDL_Quit();
            throw std::runtime_error(SDL_GetError());
        }
        SDL_SetWindowMinimumSize(window, 960, 540);
        sdlContext = SDL_GL_CreateContext(window);
        if (!sdlContext) {
            SDL_DestroyWindow(window);
            SDL_Quit();
            throw std::runtime_error(std::string("OpenGL 3.3 required: ") + SDL_GetError());
        }
        SDL_GL_SetSwapInterval(1);
        return;
    }
#ifndef _WIN32
    eglLib = dlopen("libEGL.so.1", RTLD_NOW | RTLD_LOCAL);
    glLib = dlopen("libGL.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!eglLib)
        throw std::runtime_error("Offscreen verification requires Mesa EGL");
    auto proc = symbol<void *(*)(const char *)>(eglLib, "eglGetProcAddress");
    auto platformDisplay =
        reinterpret_cast<void *(*)(unsigned, void *, const int *)>(proc("eglGetPlatformDisplayEXT"));
    if (!platformDisplay)
        throw std::runtime_error("Surfaceless EGL unavailable");
    display = platformDisplay(0x31DD, nullptr, nullptr);
    int major = 0, minor = 0;
    if (!symbol<unsigned (*)(void *, int *, int *)>(eglLib, "eglInitialize")(display, &major, &minor))
        throw std::runtime_error("EGL initialization failed");
    symbol<unsigned (*)(unsigned)>(eglLib, "eglBindAPI")(0x30A2);
    int cfgAttrib[] = {0x3024, 8, 0x3023, 8, 0x3022, 8, 0x3021, 8, 0x3025, 24, 0x3033, 1, 0x3040, 8, 0x3038};
    void *config = nullptr;
    int count = 0;
    if (!symbol<unsigned (*)(void *, const int *, void **, int, int *)>(eglLib, "eglChooseConfig")(
            display, cfgAttrib, &config, 1, &count) ||
        !count)
        throw std::runtime_error("EGL config unavailable");
    int pb[] = {0x3057, w, 0x3056, h, 0x3038};
    surface = symbol<void *(*)(void *, void *, const int *)>(eglLib, "eglCreatePbufferSurface")(display,
                                                                                                config, pb);
    int ca[] = {0x3098, 3, 0x30FB, 3, 0x30FD, 1, 0x3038};
    context = symbol<void *(*)(void *, void *, void *, const int *)>(eglLib, "eglCreateContext")(
        display, config, nullptr, ca);
    if (!surface || !context ||
        !symbol<unsigned (*)(void *, void *, void *, void *)>(eglLib, "eglMakeCurrent")(display, surface,
                                                                                        surface, context))
        throw std::runtime_error("Cannot create OpenGL 3.3 offscreen context");
#endif
}
Platform::~Platform() {
#ifndef _WIN32
    if (eglLib) {
        auto makeCurrent =
            reinterpret_cast<unsigned (*)(void *, void *, void *, void *)>(dlsym(eglLib, "eglMakeCurrent"));
        if (makeCurrent)
            makeCurrent(display, nullptr, nullptr, nullptr);
        if (context)
            symbol<unsigned (*)(void *, void *)>(eglLib, "eglDestroyContext")(display, context);
        if (surface)
            symbol<unsigned (*)(void *, void *)>(eglLib, "eglDestroySurface")(display, surface);
        if (display)
            symbol<unsigned (*)(void *)>(eglLib, "eglTerminate")(display);
        dlclose(eglLib);
        if (glLib)
            dlclose(glLib);
    } else
#endif
    {
        if (sdlContext)
            SDL_GL_DeleteContext(sdlContext);
        if (window)
            SDL_DestroyWindow(window);
        SDL_Quit();
    }
}
void *Platform::resolve(const char *name) const {
#ifdef _WIN32
    return SDL_GL_GetProcAddress(name);
#else
    if (!offscreen)
        return SDL_GL_GetProcAddress(name);
    auto get = reinterpret_cast<void *(*)(const char *)>(dlsym(eglLib, "eglGetProcAddress"));
    void *p = get(name);
    return p ? p : (glLib ? dlsym(glLib, name) : nullptr);
#endif
}
void Platform::present() {
    if (!offscreen)
        SDL_GL_SwapWindow(window);
}
void Platform::captureMouse(bool capture) {
    if (!offscreen) {
        SDL_SetRelativeMouseMode(capture ? 1 : 0);
        SDL_ShowCursor(capture ? 0 : 1);
    }
}
void Platform::dimensions() {
    if (!offscreen)
        SDL_GL_GetDrawableSize(window, &width, &height);
}
bool Platform::setFullscreen(bool enabled) {
    if (offscreen)
        return false;
    if (SDL_SetWindowFullscreen(window, enabled ? vl::sdl::fullscreen : 0) < 0)
        return false;
    dimensions();
    return true;
}
bool Platform::isFullscreen() const {
    return !offscreen && (SDL_GetWindowFlags(window) & vl::sdl::fullscreen) == vl::sdl::fullscreen;
}
bool Platform::setVSync(bool enabled) {
    return !offscreen && SDL_GL_SetSwapInterval(enabled ? 1 : 0) == 0;
}
} // namespace rf
