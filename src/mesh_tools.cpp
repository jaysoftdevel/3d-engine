#include "frontier/renderer.hpp"
#include <stdexcept>
namespace rf::gfx {
namespace {
void emit(std::vector<Vertex> &out, Vertex a, Vertex b, Vertex c) {
    if (dot(cross(b.p - a.p, c.p - a.p), a.n + b.n + c.n) < 0)
        std::swap(b, c);
    out.insert(out.end(), {a, b, c});
}
struct Face {
    V3 n, u, v;
};
const Face faces[] = {{{1, 0, 0}, {0, 0, -1}, {0, 1, 0}}, {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},
                      {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}}, {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}},
                      {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}},  {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}}};
float noise(V3 p, unsigned seed) {
    int x = std::floor(p.x), y = std::floor(p.y), z = std::floor(p.z);
    V3 f{p.x - x, p.y - y, p.z - z};
    f = {f.x * f.x * (3 - 2 * f.x), f.y * f.y * (3 - 2 * f.y), f.z * f.z * (3 - 2 * f.z)};
    float result = 0;
    for (int k = 0; k < 2; k++)
        for (int j = 0; j < 2; j++)
            for (int i = 0; i < 2; i++)
                result += random01(hash(unsigned(x + i) * 73856093u ^ unsigned(y + j) * 19349663u ^
                                        unsigned(z + k) * 83492791u ^ seed)) *
                          (i ? f.x : 1 - f.x) * (j ? f.y : 1 - f.y) * (k ? f.z : 1 - f.z);
    return result;
}
} // namespace
std::vector<Vertex> makeBeveledBox(float r, unsigned segments) {
    if (!std::isfinite(r) || r <= 0 || r >= .5f || segments < 1 || segments > 12)
        throw std::invalid_argument("invalid bevel");
    if (r < .0001f)
        return makeBox();
    std::vector<float> coord;
    for (unsigned i = 0; i <= segments; i++)
        coord.push_back(-.5f + r * i / segments);
    for (unsigned i = 0; i <= segments; i++)
        coord.push_back(.5f - r + r * i / segments);
    std::vector<Vertex> out;
    for (auto f : faces) {
        auto vertex = [&](float x, float y) {
            V3 p = f.n * .5f + f.u * x + f.v * y,
               core{clamp(p.x, -.5f + r, .5f - r), clamp(p.y, -.5f + r, .5f - r),
                    clamp(p.z, -.5f + r, .5f - r)},
               normal = normalized(p - core);
            return Vertex{core + normal * r, normal, {}, x + .5f, y + .5f};
        };
        for (size_t j = 1; j < coord.size(); j++)
            for (size_t i = 1; i < coord.size(); i++) {
                auto a = vertex(coord[i - 1], coord[j - 1]), b = vertex(coord[i], coord[j - 1]),
                     c = vertex(coord[i], coord[j]), d = vertex(coord[i - 1], coord[j]);
                emit(out, a, b, c);
                emit(out, a, c, d);
            }
    }
    return out;
}
std::vector<Vertex> makeSphere(unsigned segments, unsigned rings) {
    if (segments < 3 || segments > 256 || rings < 2 || rings > 128)
        throw std::invalid_argument("invalid sphere resolution");
    std::vector<Vertex> out;
    auto vertex = [&](unsigned x, unsigned y) {
        float a = x * 2 * pi / segments, b = y * pi / rings;
        V3 n{std::sin(a) * std::sin(b), std::cos(b), std::cos(a) * std::sin(b)};
        return Vertex{n * .5f, n, {}, float(x) / segments, float(y) / rings};
    };
    for (unsigned y = 0; y < rings; y++)
        for (unsigned x = 0; x < segments; x++) {
            auto a = vertex(x, y), b = vertex(x + 1, y), c = vertex(x + 1, y + 1), d = vertex(x, y + 1);
            if (y)
                emit(out, a, b, c);
            if (y + 1 < rings)
                emit(out, a, c, d);
        }
    return out;
}
std::vector<Vertex> makeTorus(float R, float r, unsigned segments, unsigned sides) {
    if (!std::isfinite(R + r) || r <= 0 || R <= r || segments < 3 || segments > 256 || sides < 3 ||
        sides > 64)
        throw std::invalid_argument("invalid torus");
    std::vector<Vertex> out;
    auto vertex = [&](unsigned x, unsigned y) {
        float a = x * 2 * pi / segments, b = y * 2 * pi / sides;
        V3 radial{std::cos(a), 0, std::sin(a)}, n = radial * std::cos(b) + V3{0, std::sin(b), 0};
        return Vertex{radial * R + n * r, n, {}, float(x) / segments, float(y) / sides};
    };
    for (unsigned y = 0; y < sides; y++)
        for (unsigned x = 0; x < segments; x++) {
            auto a = vertex(x, y), b = vertex(x + 1, y), c = vertex(x + 1, y + 1), d = vertex(x, y + 1);
            emit(out, a, b, c);
            emit(out, a, c, d);
        }
    return out;
}
std::vector<Vertex> makeDetailedRock(unsigned seed, unsigned divisions) {
    if (divisions < 2 || divisions > 64)
        throw std::invalid_argument("invalid rock resolution");
    std::vector<Vertex> out;
    for (auto f : faces) {
        auto position = [&](float x, float y) {
            V3 d = normalized(f.n * .5f + f.u * x + f.v * y);
            float n = noise(d * 3.5f, seed), fine = noise(d * 13, seed + 5);
            float radius = .43f * (.7f + .6f * n + .13f * fine);
            return V3{d.x * radius, d.y * radius * (.85f + .15f * noise(d * 2, seed + 7)), d.z * radius};
        };
        auto vertex = [&](float x, float y) {
            V3 p = position(x, y),
               n = normalized(cross((position(x + .001f, y) - position(x - .001f, y)) * 500.f,
                                    (position(x, y + .001f) - position(x, y - .001f)) * 500.f));
            float shade = .85f + .15f * noise(p * 16, seed);
            p={std::round(p.x*1000000)/1000000,std::round(p.y*1000000)/1000000,std::round(p.z*1000000)/1000000};
            return Vertex{p, n, {shade, shade, shade, 1}, x + .5f, y + .5f};
        };
        for (unsigned y = 0; y < divisions; y++)
            for (unsigned x = 0; x < divisions; x++) {
                float u = float(x) / divisions - .5f, v = float(y) / divisions - .5f, u1=float(x+1)/divisions-.5f,v1=float(y+1)/divisions-.5f;
                auto a = vertex(u, v), b = vertex(u1, v), c = vertex(u1, v1), d = vertex(u, v1);
                emit(out, a, b, c);
                emit(out, a, c, d);
            }
    }
    return out;
}
std::vector<Vertex> makeTube(std::span<const V3> path, float radius, unsigned sides) {
    if (path.size() < 2 || path.size() > 65536 || (path.size() - 1) * sides * 6 > 3000000 || radius <= 0 ||
        !std::isfinite(radius) || sides < 3 || sides > 128)
        throw std::invalid_argument("invalid tube");
    std::vector<Vertex> out;
    std::vector<V3> right, up;
    float length = 0;
    std::vector<float> along{0};
    for (size_t i = 0; i < path.size(); i++) {
        auto dir = normalized(path[std::min(i + 1, path.size() - 1)] - path[i ? i - 1 : 0]);
        if (rf::length(dir) < .1f)
            throw std::invalid_argument("degenerate tube path");
        V3 reference = i ? right.back() : V3{1, 0, 0};
        auto R = normalized(reference - dir * dot(reference, dir));
        if (rf::length(R) < .1f)
            R = normalized(cross(dir, {0, 1, 0}));
        right.push_back(R);
        up.push_back(normalized(cross(dir, R)));
        if (i) {
            length += distance(path[i], path[i - 1]);
            along.push_back(length);
        }
    }
    auto vertex = [&](size_t i, unsigned j) {
        float a = j * 2 * pi / sides;
        V3 n = right[i] * std::cos(a) + up[i] * std::sin(a);
        return Vertex{path[i] + n * radius, n, {}, float(j) / sides, along[i]};
    };
    for (size_t i = 1; i < path.size(); i++)
        for (unsigned j = 0; j < sides; j++) {
            auto a = vertex(i - 1, j), b = vertex(i, j), c = vertex(i, j + 1), d = vertex(i - 1, j + 1);
            emit(out, a, b, c);
            emit(out, a, c, d);
        }
    return out;
}
void appendMesh(std::vector<Vertex> &out, std::span<const Vertex> mesh, const Mat4 &matrix) {
    auto inverseMatrix = inverse(matrix).m;
    out.reserve(out.size() + mesh.size());
    for (auto v : mesh) {
        v.p = transformPoint(matrix, v.p);
        auto n = v.n;
        v.n = normalized({inverseMatrix[0] * n.x + inverseMatrix[1] * n.y + inverseMatrix[2] * n.z,
                          inverseMatrix[4] * n.x + inverseMatrix[5] * n.y + inverseMatrix[6] * n.z,
                          inverseMatrix[8] * n.x + inverseMatrix[9] * n.y + inverseMatrix[10] * n.z});
        out.push_back(v);
    }
}
} // namespace rf::gfx
