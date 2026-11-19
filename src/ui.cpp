#include "internal.hpp"
#include <cctype>
#include <unordered_map>
namespace rf::gfx {
void Renderer::rect(float x, float y, float w, float h, Color c) {
    UiVertex a{x, y, c}, b{x + w, y, c}, d{x, y + h, c}, e{x + w, y + h, c};
    impl->ui.insert(impl->ui.end(), {a, b, e, a, e, d});
}
void Renderer::line(float x1, float y1, float x2, float y2, Color c, float thickness) {
    float dx = x2 - x1, dy = y2 - y1, len = std::hypot(dx, dy);
    if (len < .01f)
        return;
    float nx = -dy / len * thickness * .5f, ny = dx / len * thickness * .5f;
    impl->ui.insert(impl->ui.end(), {{x1 + nx, y1 + ny, c},
                                     {x2 + nx, y2 + ny, c},
                                     {x2 - nx, y2 - ny, c},
                                     {x1 + nx, y1 + ny, c},
                                     {x2 - nx, y2 - ny, c},
                                     {x1 - nx, y1 - ny, c}});
}
void Renderer::text(float x, float y, std::string_view value, float scale, Color color) {
    static const std::unordered_map<char, std::array<unsigned char, 7>> glyph = {
        {'A', {14, 17, 17, 31, 17, 17, 17}}, {'B', {30, 17, 17, 30, 17, 17, 30}},
        {'C', {14, 17, 16, 16, 16, 17, 14}}, {'D', {30, 17, 17, 17, 17, 17, 30}},
        {'E', {31, 16, 16, 30, 16, 16, 31}}, {'F', {31, 16, 16, 30, 16, 16, 16}},
        {'G', {14, 17, 16, 23, 17, 17, 15}}, {'H', {17, 17, 17, 31, 17, 17, 17}},
        {'I', {14, 4, 4, 4, 4, 4, 14}},      {'J', {7, 2, 2, 2, 2, 18, 12}},
        {'K', {17, 18, 20, 24, 20, 18, 17}}, {'L', {16, 16, 16, 16, 16, 16, 31}},
        {'M', {17, 27, 21, 21, 17, 17, 17}}, {'N', {17, 25, 21, 19, 17, 17, 17}},
        {'O', {14, 17, 17, 17, 17, 17, 14}}, {'P', {30, 17, 17, 30, 16, 16, 16}},
        {'Q', {14, 17, 17, 17, 21, 18, 13}}, {'R', {30, 17, 17, 30, 20, 18, 17}},
        {'S', {15, 16, 16, 14, 1, 1, 30}},   {'T', {31, 4, 4, 4, 4, 4, 4}},
        {'U', {17, 17, 17, 17, 17, 17, 14}}, {'V', {17, 17, 17, 17, 17, 10, 4}},
        {'W', {17, 17, 17, 21, 21, 21, 10}}, {'X', {17, 17, 10, 4, 10, 17, 17}},
        {'Y', {17, 17, 10, 4, 4, 4, 4}},     {'Z', {31, 1, 2, 4, 8, 16, 31}},
        {'0', {14, 17, 19, 21, 25, 17, 14}}, {'1', {4, 12, 4, 4, 4, 4, 14}},
        {'2', {14, 17, 1, 2, 4, 8, 31}},     {'3', {30, 1, 1, 14, 1, 1, 30}},
        {'4', {2, 6, 10, 18, 31, 2, 2}},     {'5', {31, 16, 16, 30, 1, 1, 30}},
        {'6', {14, 16, 16, 30, 17, 17, 14}}, {'7', {31, 1, 2, 4, 8, 8, 8}},
        {'8', {14, 17, 17, 14, 17, 17, 14}}, {'9', {14, 17, 17, 15, 1, 1, 14}},
        {'.', {0, 0, 0, 0, 0, 12, 12}},      {',', {0, 0, 0, 0, 4, 4, 8}},
        {':', {0, 12, 12, 0, 12, 12, 0}},    {'-', {0, 0, 0, 31, 0, 0, 0}},
        {'+', {0, 4, 4, 31, 4, 4, 0}},       {'/', {1, 2, 2, 4, 8, 8, 16}},
        {'(', {2, 4, 8, 8, 8, 4, 2}},        {')', {8, 4, 2, 2, 2, 4, 8}},
        {'[', {14, 8, 8, 8, 8, 8, 14}},      {']', {14, 2, 2, 2, 2, 2, 14}},
        {'=', {0, 0, 31, 0, 31, 0, 0}},      {'>', {16, 8, 4, 2, 4, 8, 16}},
        {'<', {1, 2, 4, 8, 4, 2, 1}},        {'%', {17, 2, 4, 4, 8, 16, 17}},
        {'!', {4, 4, 4, 4, 4, 0, 4}},        {'?', {14, 17, 1, 2, 4, 0, 4}},
        {'_', {0, 0, 0, 0, 0, 0, 31}},       {'|', {4, 4, 4, 4, 4, 4, 4}}};
    float start = x;
    for (char raw : value) {
        char c = std::toupper(static_cast<unsigned char>(raw));
        if (c == '\n') {
            x = start;
            y += scale * 10;
            continue;
        }
        auto it = glyph.find(c);
        if (it != glyph.end())
            for (int row = 0; row < 7; row++)
                for (int col = 0; col < 5; col++)
                    if (it->second[row] & (1 << (4 - col)))
                        rect(x + col * scale, y + row * scale, scale, scale, color);
        x += scale * 6;
    }
}

} // namespace rf::gfx
