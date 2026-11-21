#include "frontier/renderer.hpp"
namespace rf::gfx {
namespace {
void triangle(std::vector<Vertex> &v, V3 a, V3 b, V3 c, float au = 0, float av = 0, float bu = 0,
              float bv = 1, float cu = 1, float cv = 1) {
    V3 n = normalized(cross(b - a, c - a));
    v.push_back({a, n, {}, au, av});
    v.push_back({b, n, {}, bu, bv});
    v.push_back({c, n, {}, cu, cv});
}
} // namespace
std::vector<Vertex> makeBox() {
    std::vector<Vertex> v;
    V3 p[] = {{-.5f, -.5f, -.5f}, {.5f, -.5f, -.5f}, {.5f, .5f, -.5f}, {-.5f, .5f, -.5f},
              {-.5f, -.5f, .5f},  {.5f, -.5f, .5f},  {.5f, .5f, .5f},  {-.5f, .5f, .5f}};
    int faces[][4] = {{1, 0, 3, 2}, {4, 5, 6, 7}, {0, 4, 7, 3}, {5, 1, 2, 6}, {3, 7, 6, 2}, {0, 1, 5, 4}};
    for (auto &f : faces) {
        triangle(v, p[f[0]], p[f[1]], p[f[2]], 0, 0, 1, 0, 1, 1);
        triangle(v, p[f[0]], p[f[2]], p[f[3]], 0, 0, 1, 1, 0, 1);
    }
    return v;
}
std::vector<Vertex> makeRock() {
    std::vector<Vertex> v;
    V3 top{.03f, .58f, -.05f}, bottom{0, -.4f, 0};
    for (int j = 0; j < 7; j++) {
        float a = j * 2 * pi / 7, b = (j + 1) * 2 * pi / 7;
        V3 p{std::sin(a) * .57f, -.03f, std::cos(a) * .5f}, q{std::sin(b) * .57f, -.03f, std::cos(b) * .5f};
        triangle(v, top, p, q);
        triangle(v, bottom, q, p);
    }
    return v;
}
std::vector<Vertex> makeCylinder(unsigned sides) {
    std::vector<Vertex> v;
    sides = std::clamp(sides, 3u, 128u);
    for (unsigned j = 0; j < sides; j++) {
        float a = j * 2 * pi / sides, b = (j + 1) * 2 * pi / sides;
        V3 p{std::sin(a) * .5f, -.5f, std::cos(a) * .5f}, q{std::sin(b) * .5f, -.5f, std::cos(b) * .5f},
            r = q + V3{0, 1, 0}, s = p + V3{0, 1, 0};
        triangle(v, p, q, r, float(j) / sides, 0, float(j + 1) / sides, 0, float(j + 1) / sides, 1);
        triangle(v, p, r, s, float(j) / sides, 0, float(j + 1) / sides, 1, float(j) / sides, 1);
        // Smooth only the cylindrical wall; retain hard cap edges and UV seams.
        for(size_t i=v.size()-6;i<v.size();++i)
            v[i].n=normalized(V3{v[i].p.x,0,v[i].p.z});
        triangle(v, {0, .5f, 0}, s, r);
        triangle(v, {0, -.5f, 0}, q, p);
    }
    return v;
}
} // namespace rf::gfx
