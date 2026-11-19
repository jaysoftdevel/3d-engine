#include <frontier/frontier.hpp>
#include <iostream>
int main() {
    rf::Platform platform(true, 640, 360);
    auto settings = rf::gfx::GraphicsSettings::balanced();
    settings.viewDistance = 64;
    rf::gfx::Renderer renderer(platform, settings);
    renderer.begin({{5, 3, -8}, rf::normalized(rf::V3{-5, -2, 10}), 70, .08f});
    rf::gfx::Material material;
    material.albedo = {.3f, .8f, .6f, 1};
    renderer.primitive(rf::gfx::Primitive::Box, {{0, 1, 2}, {2, 2, 2}, 0}, material);
    renderer.end();
    renderer.check();
    std::cout << "External consumer linked Frontier " << frontier_engine_version() << '\n';
}
