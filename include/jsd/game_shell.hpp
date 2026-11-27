#pragma once
#include <frontier/math.hpp>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace rf {
class Platform;
namespace gfx { struct RendererStartup; struct ShaderBuildProgress; }
}

namespace jsd::shell {

struct Point { float x{}, y{}; };
struct Rect { float x{}, y{}, w{}, h{}; };

struct Theme {
    rf::Color panel{.047f, .065f, .086f, .96f};
    rf::Color control{.085f, .12f, .15f, .96f};
    rf::Color text{.92f, .96f, .98f, 1};
    rf::Color muted{.64f, .72f, .78f, 1};
    rf::Color accent{.39f, .85f, .78f, 1};
    rf::Color hover{.17f, .31f, .35f, 1};
    rf::Color border{.20f, .30f, .35f, 1};
    float fontScale{1.50f};
};

struct Input {
    Point pointer;
    bool click{}, select{}, up{}, down{}, back{}, quit{};
};

struct Item {
    std::string_view label;
    bool enabled{true};
};

enum class TitleAction { Resume, Save, Continue, NewGame, Load, Settings, Credits, Exit };

struct TitleOptions {
    bool activeSession{};
    bool savesSupported{};
    bool hasRecentSave{};
};

class TitleItems {
    std::array<Item,8> entries{};
    std::array<TitleAction,8> actions{};
    std::size_t count{};

  public:
    explicit TitleItems(TitleOptions = {});
    std::span<const Item> items() const { return std::span(entries).first(count); }
    TitleAction action(std::size_t index) const { return actions.at(index); }
};

struct ContextLine {
    std::string_view text;
    float offset{};
    float scale{1};
    bool muted{};
};

struct Page {
    std::string_view title;
    std::span<const Item> items;
    std::span<const ContextLine> context{};
    std::string_view footer{}, status{};
    std::size_t preferred{};
};

class Canvas {
  public:
    virtual ~Canvas() = default;
    virtual void gradient(Rect, rf::Color top, rf::Color bottom) = 0;
    virtual void line(Point, Point, rf::Color, float width) = 0;
    // Positions are in UI coordinates with zero z.
    virtual void triangles(std::span<const rf::V3>, rf::Color) = 0;
    virtual void text(Point, std::string_view, float scale, rf::Color) = 0;
};

class Menu {
    std::optional<std::size_t> keyboardFocus;

  public:
    void reset() { keyboardFocus.reset(); }
    // Returns the selected item's index. The game owns page transitions and actions.
    std::optional<std::size_t> draw(Canvas&, float width, float height, const Page&, const Input&,
                                    const Theme& = {});
};

struct StartupCancelled {};

// Lightweight UI for an active Platform context. Keep it alive through renderer startup.
// Use on the window thread and destroy before destroying the Platform.
class Window {
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    Window(rf::Platform&, std::string_view gameTitle);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    void showStartup(std::string_view phase, std::string_view detail = {}, bool force = false);
    void showShaderProgress(const rf::gfx::ShaderBuildProgress&);
    rf::gfx::RendererStartup rendererStartup(bool cacheShaders = true);
    // Standalone menu input uses SDL window coordinates and adapts to drawable DPI.
    Input pollInput();
    std::optional<std::size_t> present(Menu&, const Page&, const Input&, const Theme& = {});
};

} // namespace jsd::shell
