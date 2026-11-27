#include <jsd/engine.hpp>
#include <array>
#include <iostream>

namespace {
constexpr char gameTitle[]="YOUR GAME";

enum class Screen { Title, Settings, Credits, Play };

void drawGame(rf::gfx::Renderer& renderer) {
    // Replace this frame with the game's update and rendering.
    renderer.begin({});
    renderer.text(38,38,"ESC  /  RETURN TO MENU",2);
    renderer.end();
}

int run() {
    rf::Platform platform(false,1280,720,false,gameTitle);
    platform.setVSync(false);
    jsd::shell::Window shell(platform,gameTitle);
    shell.showStartup("Starting game");
    rf::gfx::Renderer renderer(platform,rf::gfx::GraphicsSettings::balanced(),shell.rendererStartup());
    shell.showStartup("Preparing game assets");
    // Load game content here and call showStartup between loading phases.

    bool vsync=platform.setVSync(true);
    bool fullscreen=platform.isFullscreen();
    jsd::shell::Menu menu;
    Screen screen=Screen::Title;
    const auto go=[&](Screen next) { menu.reset(); screen=next; };
    bool active=false;
    std::string status;

    for (;;) {
        const auto input=shell.pollInput();
        if (input.quit)
            return 0;
        if (input.back) {
            if (screen!=Screen::Title)
                go(Screen::Title);
            else if (active)
                go(Screen::Play);
            continue;
        }
        switch (screen) {
        case Screen::Title: {
            const jsd::shell::TitleItems title({active});
            const std::array<jsd::shell::ContextLine,1> context{{{"ESC  /  RETURN TO MENU DURING PLAY",0,1,true}}};
            if (const auto action=shell.present(menu,{gameTitle,title.items(),context},input)) {
                switch (title.action(*action)) {
                case jsd::shell::TitleAction::Resume:
                case jsd::shell::TitleAction::NewGame: active=true; go(Screen::Play); break;
                case jsd::shell::TitleAction::Settings: status.clear(); go(Screen::Settings); break;
                case jsd::shell::TitleAction::Credits: go(Screen::Credits); break;
                case jsd::shell::TitleAction::Exit: return 0;
                default: break;
                }
            }
            break;
        }
        case Screen::Settings: {
            const std::array<jsd::shell::Item,3> items{{
                {fullscreen?"DISPLAY  /  FULLSCREEN":"DISPLAY  /  WINDOWED"},
                {vsync?"VERTICAL SYNC  /  ON":"VERTICAL SYNC  /  OFF"},{"BACK"}}};
            if (const auto action=shell.present(menu,{"SETTINGS",items,{}, {},status},input)) {
                if (*action==0) {
                    status=platform.setFullscreen(!fullscreen)?"":"DISPLAY MODE UNAVAILABLE";
                    fullscreen=platform.isFullscreen();
                } else if (*action==1) {
                    if (platform.setVSync(!vsync)) {
                        vsync=!vsync;
                        status.clear();
                    } else {
                        status="VERTICAL SYNC UNAVAILABLE";
                    }
                } else {
                    go(Screen::Title);
                }
            }
            break;
        }
        case Screen::Credits: {
            const std::array<jsd::shell::Item,1> items{{{"BACK"}}};
            const std::array<jsd::shell::ContextLine,1> credits{{{gameTitle,0,1.55f}}};
            if (shell.present(menu,{"CREDITS",items,credits,"JSD GAME ENGINE"},input))
                go(Screen::Title);
            break;
        }
        case Screen::Play:
            if (platform.width>0 && platform.height>0) {
                drawGame(renderer);
                platform.present();
            }
            break;
        }
        if (!vsync || platform.width<=0 || platform.height<=0)
            SDL_Delay(16);
    }
}
} // namespace

int main() {
    try {
        return run();
    } catch (const jsd::shell::StartupCancelled&) {
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
